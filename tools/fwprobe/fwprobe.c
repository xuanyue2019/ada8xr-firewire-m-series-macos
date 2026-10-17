// fwprobe — FireWire 总线体检 + Config ROM 解析器（Phase 1 工具）
//
// 用途：把雷电→火线转接链 + ADA-8XR 接上 Mac 后，跑一次本工具，
//       确认 (1) 火线总线活着  (2) 对端 GUID 厂商号 00:11:98 = Prism Sound
//             (3) Unit Spec/Version = BeBoB(BridgeCo) 0x00A02D/0x010001
//
// 实现方式：只读 IORegistry（IOFireWireNub 的 "Config ROM" 属性由内核缓存），
//           不需要任何驱动、不写任何寄存器，对设备零风险。
//
// 适用范围：仅经典苹果火线栈——macOS ≤15（原生在）；macOS 26 Tahoe 且已装
//           revive/ 实验 kext。注意：ASFireWire(DriverKit) 路线下本工具永远
//           报"未找到"（dext 不发布 IOFireWireNub 等内核类），验证请改看
//           `system_profiler SPAudioDataType` 是否出现 FireWire 音频设备。
//
// Build: make && sudo ./fwprobe   （root 能看到更多属性；普通用户也能跑）

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- Config ROM 常量 ----------
// 厂商 OUI（IEEE 1394 bus info block 里的 EUI-64 高 24 位）
#define OUI_PRISM_SOUND 0x001198u   // Prism Media Products Ltd (Wireshark manuf: PrismMed)
// 标准 AV/C 音频（BridgeCo BeBoB）unit 目录签名
#define UNIT_SPEC_AVC_AUDIO 0x00A02Du
#define UNIT_VER_BEBOB      0x010001u

static const char *key_name(unsigned key) {
    switch (key) {
        case 0x01: return "Descriptor";
        case 0x02: return "Bus_Dependent_Info";
        case 0x03: return "Vendor";
        case 0x04: return "Hardware_Version";
        case 0x0C: return "Node_Capabilities";
        case 0x12: return "Unit_Spec_Id";
        case 0x13: return "Unit_Sw_Version";
        case 0x14: return "Logical_Unit_Number";
        case 0x17: return "Model";
        case 0x18: return "Instance";
        case 0x19: return "Keyword";
        case 0x1A: return "Feature";
        case 0x3D: return "EUI_64";
        default:   return NULL;
    }
}

// 大端 quadlet 读
static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// 文本描述符 leaf：数据区前 8 字节是 CRC+minimal/width+语言，其后是文本。
// BeBoB 设备常见 UTF-16BE（隔字节为 0）或 ASCII，两种都兜住。
static void print_text_leaf(const uint32_t *leaf, size_t nq, int indent) {
    if (nq < 3) return;
    const uint8_t *b = (const uint8_t *)leaf;
    size_t off = 8, len = nq * 4 - 8;
    // UTF-16BE 启发式
    int utf16 = 0, ascii = 0;
    for (size_t i = 0; i + 1 < len && i < 64; i += 2)
        if (b[off + i] == 0 && b[off + i + 1] != 0) utf16++;
    for (size_t i = 0; i < len && i < 64; i++)
        if (isprint(b[off + i])) ascii++;
    printf("%*stext: \"", indent, "");
    if (utf16 * 2 > ascii) {
        for (size_t i = off + 1, k = 0; k < len / 2 && i < len; i += 2, k++) {
            int c = b[i];
            putchar(isprint(c) ? c : '?');
        }
    } else {
        for (size_t i = off; i < off + len; i++) {
            int c = b[i];
            putchar(isprint(c) ? c : (c == 0 ? ' ' : '?'));
        }
    }
    printf("\"\n");
}

// 递归解析目录。base = ROM 起始 quadlet 指针，idx = 目录起始下标，nq = 总 quadlet 数。
// 返回是否在子树里发现了 BeBoB 签名。
static int parse_dir(const uint32_t *rom, size_t nq, size_t idx, int depth, int indent) {
    int bebob = 0;
    if (idx + 1 > nq || depth > 4) return 0;
    uint16_t dlen = (uint16_t)(rom[idx] >> 16);  // 目录长度（不含头）
    if (idx + 1 + dlen > nq) dlen = (uint16_t)(nq - idx - 1);
    const char *kind = depth == 0 ? "Root Directory" : "Directory";
    printf("%*s[%zu] %s (%u entries)\n", indent, "", idx, kind, dlen);

    for (unsigned i = 0; i < dlen; i++) {
        uint32_t q = rom[idx + 1 + i];
        unsigned type = q >> 30, key = (q >> 24) & 0x3F;
        uint32_t val = q & 0xFFFFFF;
        const char *kn = key_name(key);
        printf("%*s  %-20s", indent, "", kn ? kn : "(key)");
        if (!kn) printf("0x%02x", key);

        if (type == 0) {  // Immediate
            printf(" = 0x%06x", val);
            if (key == 0x12) {
                printf("  %s", val == UNIT_SPEC_AVC_AUDIO ? "<== AV/C audio (BridgeCo/BeBoB 生态)" : "");
                if (val == UNIT_SPEC_AVC_AUDIO) bebob |= 2;  // 候选，等 version 确认
            }
            if (key == 0x13 && (bebob & 2)) {
                if (val == UNIT_VER_BEBOB) { bebob |= 1; printf("  <== BeBoB v%u 确认", (val >> 16) & 0xff); }
            }
            printf("\n");
        } else if (type == 1) {  // Offset (CSR 地址)
            printf(" @ offset 0x%06x\n", val);
        } else {  // Leaf (2) / Directory (3)，val = 相对偏移
            size_t t = idx + 1 + i + val;
            if (t >= nq) { printf(" (越界，跳过)\n"); continue; }
            if (type == 2) {
                uint16_t ll = (uint16_t)(rom[t] >> 16);
                if (t + 1 + ll > nq) ll = (uint16_t)(nq - t - 1);
                printf(" leaf[%u] ->\n", ll);
                if (key == 0x03 || key == 0x01) print_text_leaf(&rom[t + 1], ll, indent + 6);
            } else {
                printf(" dir ->\n");
                bebob |= parse_dir(rom, nq, t, depth + 1, indent + 4);
            }
        }
    }
    return bebob;
}

// 解析整棵 Config ROM。返回：bit0 = BeBoB 确认，bit1 = 厂商是 Prism Sound。
static int parse_rom(const uint8_t *data, size_t len) {
    int flags = 0;
    size_t nq = len / 4;
    if (nq < 5) { printf("  (ROM 太短， %zu 字节)\n", len); return 0; }
    const uint32_t *rom = (const uint32_t *)data;

    printf("  -- 原始 quadlet --\n");
    for (size_t i = 0; i < nq; i++) {
        printf("  %04zx: %08x %s", i * 4, rd32(data + i * 4), (i % 4 == 3) ? "\n" : " ");
        if (i >= 63) { printf(" ... (共 %zu quadlet，截断)\n", nq); break; }
    }
    if (nq <= 64) printf("\n");

    uint32_t q1 = rd32(data + 4);
    if (q1 != 0x31333934u) {
        printf("  !! bus name 不是 \"1394\" (0x%08x)，ROM 可能不完整\n", q1);
        return 0;
    }
    uint32_t info_len = (rd32(data) >> 24) & 0xFF;
    uint32_t opts = rd32(data + 8);
    uint64_t guid = ((uint64_t)rd32(data + 12) << 32) | rd32(data + 16);
    uint32_t oui = (uint32_t)(guid >> 40);

    printf("  -- 总线信息块 --\n");
    printf("  bus options 0x%08x  [IRMC:%d CMC:%d BMC:%d PMC:%d]\n",
           opts, (int)(opts >> 31 & 1), (int)(opts >> 30 & 1),
           (int)(opts >> 28 & 1), (int)(opts >> 27 & 1));
    printf("  GUID: %08"PRIX64"-%08"PRIX64"  (", guid >> 32, guid & 0xFFFFFFFFu);
    for (int i = 7; i >= 0; i--) printf("%02X%s", (unsigned)(guid >> (i * 8)) & 0xFF, i ? ":" : "");
    printf(")\n");
    printf("  vendor OUI: 0x%06x%s\n", oui, oui == OUI_PRISM_SOUND ? "  <== Prism Sound!!!" : "");

    if (oui == OUI_PRISM_SOUND) flags |= 2;
    size_t root = 1 + info_len;
    if (root < nq) {
        printf("  -- 目录树 --\n");
        if (parse_dir(rom, nq, root, 0, 2)) flags |= 1;
    }
    if ((flags & 3) == 3)
        printf("\n  >>> 判定：Prism Sound + BeBoB 协议。可以直接走 ASFireWire/自研驱动路径。\n");
    else if (flags & 2)
        printf("\n  >>> 判定：Prism Sound 设备，但未看到标准 BeBoB 签名（把上面的 hex 发回来人工分析）。\n");
    return flags;
}

static void dump_props(io_registry_entry_t entry) {
    CFMutableDictionaryRef props = NULL;
    if (IORegistryEntryCreateCFProperties(entry, &props, kCFAllocatorDefault, 0) != KERN_SUCCESS || !props)
        return;
    CFIndex n = CFDictionaryGetCount(props);
    const void **keys = malloc(sizeof(void *) * (size_t)n);
    const void **vals = malloc(sizeof(void *) * (size_t)n);
    CFDictionaryGetKeysAndValues(props, keys, vals);
    for (CFIndex i = 0; i < n; i++) {
        const char *k = CFStringGetCStringPtr(keys[i], kCFStringEncodingUTF8);
        if (!k) continue;
        // 只挑我们关心的短属性，避免刷屏
        if (strstr(k, "GUID") || strstr(k, "Node ID") || strstr(k, "Speed") ||
            strstr(k, "Unit Spec") || strstr(k, "Unit Sw") || strstr(k, "Model") ||
            strstr(k, "Vendor")) {
            CFTypeID t = CFGetTypeID(vals[i]);
            if (t == CFStringGetTypeID()) {
                char buf[256];
                if (CFStringGetCString(vals[i], buf, sizeof buf, kCFStringEncodingUTF8))
                    printf("    %-24s = %s\n", k, buf);
            } else if (t == CFNumberGetTypeID()) {
                long v = 0;
                if (CFNumberGetValue(vals[i], kCFNumberLongType, &v))
                    printf("    %-24s = 0x%lx (%ld)\n", k, v, v);
            } else if (t == CFDataGetTypeID()) {
                CFIndex l = CFDataGetLength(vals[i]);
                printf("    %-24s = <data %ld bytes>\n", k, (long)l);
            }
        }
    }
    free(keys); free(vals);
    CFRelease(props);
}

static int scan_class(const char *class_name, int want_rom) {
    int found = 0, prism_or_bebob = 0;
    io_iterator_t it = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMainPortDefault, IOServiceMatching(class_name), &it);
    if (kr != KERN_SUCCESS) return 0;
    io_object_t e;
    while ((e = IOIteratorNext(it)) != MACH_PORT_NULL) {
        found++;
        char name[128] = "";
        IORegistryEntryGetName(e, name);
        printf("  • %s  \"%s\"\n", class_name, name);
        if (want_rom) {
            dump_props(e);
            CFDataRef rom = IORegistryEntrySearchCFProperty(
                e, kIOServicePlane, CFSTR("Config ROM"), kCFAllocatorDefault, 0);
            if (rom) {
                const UInt8 *d = CFDataGetBytePtr(rom);
                CFIndex l = CFDataGetLength(rom);
                printf("    Config ROM (%ld bytes):\n", (long)l);
                prism_or_bebob |= parse_rom(d, (size_t)l);
                CFRelease(rom);
            } else {
                printf("    (无 Config ROM 属性 —— 设备可能未完成枚举，试试 sudo)\n");
            }
        }
        IOObjectRelease(e);
    }
    IOObjectRelease(it);
    return found ? (found | (prism_or_bebob << 16)) : 0;
}

int main(void) {
    printf("fwprobe — FireWire 总线体检 (只读)\n");
    printf("====================================\n\n");

    int ctl = scan_class("IOFireWireController", 0);
    if (ctl) scan_class("IOFireWireLocalNode", 0);
    printf("\n");
    int nub = scan_class("IOFireWireNub", 1);
    int avc = scan_class("IOFireWireAVCUnit", 0);

    printf("\n---- 结论 ----\n");
    if (!ctl) {
        printf("火线控制器：未找到。\n"
               "  • 走 ASFireWire(DriverKit) 路线时这是预期结果——本工具只认经典内核栈，\n"
               "    验证请用 `system_profiler SPAudioDataType` 看 FireWire 音频设备。\n"
               "  • macOS ≤15 / 已装 revive kext：请检查转接链（TB3→TB2→FW800）和线。\n");
    } else {
        printf("火线控制器：%d 个，总线活着。\n", ctl);
        if (!(nub & 0xFFFF))
            printf("但没有发现远端设备 —— 检查 FW 线/ADA-8XR 电源/接口卡是否插紧。\n");
    }
    if (nub & 0xFFFF) printf("远端节点：%d 个。\n", nub & 0xFFFF);
    if (avc & 0xFFFF) printf("AV/C 单元：%d 个（设备已通过 AV/C 枚举）。\n", avc & 0xFFFF);
    int verdict = nub >> 16;
    if ((verdict & 3) == 3)
        printf(">>> 目标确认：Prism Sound + BeBoB。把本输出发回来，进入驱动接入阶段。\n");

    return (verdict & 3) == 3 ? 0 : 1;
}
