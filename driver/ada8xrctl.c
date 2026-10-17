// ada8xrctl — 我们自己的 ADA-8XR 火线驱动控制台（驱动控制面 + 调试台）
//
// 用法（总线点亮后，可加 sudo）：
//   ada8xrctl list                 列出火线设备
//   ada8xrctl rom                  打印 Prism 设备 Config ROM（hex）
//   ada8xrctl mpr                  读 iMPR/oMPR（插数量、速度）
//   ada8xrctl pcr [n]              dump iPCR(n)/oPCR(n)
//   ada8xrctl read <loHex> [n]     从 CSR 读 n 个 quadlet（lo 如 F0000900）
//   ada8xrctl write <loHex> <vHex> 写一个 quadlet（危险，仅调试）
//   ada8xrctl avc <hex...>         裸 AV/C 命令控制台（例：avc 01 FF 18 00 FF FF FF FF）
//   ada8xrctl rate                 查询当前采样率
//   ada8xrctl rate <hz>            设置采样率（44100/48000/88200/96000/176400/192000）
//
// 命令字节依据 Linux fcp.c（avc_general_set/get_sig_fmt），见 bebob.h。
#include "fwio.h"
#include "bebob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void hex_dump(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        printf("%02X%s", p[i], (i % 16 == 15 || i + 1 == n) ? "\n" : " ");
        if (i >= 255) { printf("...(截断)\n"); break; }
    }
}

static void pcr_print(const char *tag, uint32_t v) {
    printf("  %-6s = %08X  online=%d bcast=%d p2p=%u ch=%u", tag, v,
           !!(v & PCR_ONLINE), !!(v & PCR_BCAST_CONN),
           (v & PCR_P2P_CONN_MASK) >> PCR_P2P_CONN_SHIFT,
           (v & PCR_CHANNEL_MASK) >> PCR_CHANNEL_SHIFT);
    if (tag[0] == 'o')
        printf(" xspeed=%u speed=%u ovh_id=%u",
               (v & OPCR_XSPEED_MASK) >> OPCR_XSPEED_SHIFT,
               (v & OPCR_SPEED_MASK) >> OPCR_SPEED_SHIFT,
               (v & OPCR_OVERHEAD_ID_MASK) >> OPCR_OVERHEAD_ID_SHIFT);
    printf("\n");
}

static void die(fwio_ctx_t *ctx, const char *msg) {
    fprintf(stderr, "✗ %s\n", msg);
    fwio_close(ctx);
    exit(1);
}

static int parse_hex_arg(const char *s, uint32_t *out) {
    return sscanf(s, "%x", out) == 1 ? 0 : -1;
}

static int avc_transaction(fwio_ctx_t *c, uint8_t *frame, uint32_t len) {
    uint32_t rlen = 512;
    uint8_t resp[512];
    printf("→ "); hex_dump(frame, len);
    int kr = fwio_avc(c, frame, len, resp, &rlen);
    if (kr) { printf("✗ AVCCommand 失败: 0x%x\n", kr); return kr; }
    printf("← "); hex_dump(resp, rlen);
    switch (resp[0]) {
        case AVC_RESP_ACCEPTED: printf("  [ACCEPTED]\n"); return 0;
        case AVC_RESP_STABLE:   printf("  [STABLE/当前值]\n"); return 0;
        case AVC_RESP_REJECTED: printf("  [REJECTED]\n"); return -2;
        case AVC_RESP_NOT_IMPLEMENTED: printf("  [NOT IMPLEMENTED]\n"); return -3;
        default: printf("  [IN TRANSITION/其他响应 0x%02X]\n", resp[0]); return 1;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法见源码顶部注释\n"); return 2; }
    const char *cmd = argv[1];

    if (strcmp(cmd, "list") == 0) {
        // 复用 fwprobe 的枚举（避免重复造轮子，简单起见这里只列 Nub+GUID）
        io_iterator_t it;
        if (IOServiceGetMatchingServices(kIOMainPortDefault,
                IOServiceMatching("IOFireWireNub"), &it)) {
            fprintf(stderr, "无火线栈/无设备。先跑 tools/fwprobe/fwprobe\n"); return 1;
        }
        io_object_t e; int n = 0;
        while ((e = IOIteratorNext(it)) != MACH_PORT_NULL) {
            CFTypeRef g = IORegistryEntrySearchCFProperty(
                e, kIOServicePlane, CFSTR("GUID"), kCFAllocatorDefault, 0);
            char gs[64] = "?";
            if (g && CFGetTypeID(g) == CFStringGetTypeID())
                CFStringGetCString(g, gs, 64, kCFStringEncodingUTF8);
            if (g) CFRelease(g);
            printf("  FW 设备: GUID=%s%s\n", gs,
                   strncmp(gs, "001198", 6) == 0 ? "   <== Prism Sound" : "");
            IOObjectRelease(e); n++;
        }
        IOObjectRelease(it);
        printf(n ? "%d 台设备\n" : "没发现火线设备（控制器活着吗？链路插了吗？）\n", n);
        return n ? 0 : 1;
    }

    // 以下命令都要打开设备
    char err[256];
    fwio_ctx_t *c = fwio_open("001198", err, sizeof err);
    if (!c && argc > 2) c = fwio_open(NULL, err, sizeof err);   // 指定 --any 时
    if (!c) { fprintf(stderr, "✗ %s\n", err); return 1; }
    printf("设备: GUID=%s\n\n", fwio_guid(c));

    if (strcmp(cmd, "rom") == 0) {
        uint8_t buf[1024];
        size_t l = fwio_config_rom(c, buf, sizeof buf);
        if (!l) die(c, "读不到 Config ROM 属性（试 sudo）");
        printf("Config ROM (%zu bytes):\n", l);
        hex_dump(buf, l);
    } else if (strcmp(cmd, "mpr") == 0) {
        uint32_t impr, opr;
        if (fwio_read32(c, BEBOB_LO_IMPR, &impr)) die(c, "读 iMPR 失败");
        if (fwio_read32(c, BEBOB_LO_OMPR, &opr)) die(c, "读 oMPR 失败");
        printf("iMPR=%08X  plugs=%u speed=%u\n", impr,
               impr & MPR_PLUGS_MASK, (impr & MPR_SPEED_MASK) >> MPR_SPEED_SHIFT);
        printf("oMPR=%08X  plugs=%u speed=%u\n", opr,
               opr & MPR_PLUGS_MASK, (opr & MPR_SPEED_MASK) >> MPR_SPEED_SHIFT);
    } else if (strcmp(cmd, "pcr") == 0) {
        uint32_t n = 0;
        if (argc > 2) parse_hex_arg(argv[2], &n);
        uint32_t v;
        if (fwio_read32(c, BEBOB_LO_IPCR(n), &v) == 0) pcr_print("iPCR", v);
        else printf("  iPCR%u 读失败\n", n);
        if (fwio_read32(c, BEBOB_LO_OPCR(n), &v) == 0) pcr_print("oPCR", v);
        else printf("  oPCR%u 读失败\n", n);
    } else if (strcmp(cmd, "read") == 0) {
        if (argc < 3) die(c, "read <loHex> [count]");
        uint32_t lo, n = 1;
        parse_hex_arg(argv[2], &lo);
        if (argc > 3) parse_hex_arg(argv[3], &n);
        if (n > 64) n = 64;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t v;
            if (fwio_read32(c, lo + 4 * i, &v)) { printf("%08X: ✗\n", lo + 4 * i); break; }
            printf("FFFF%08X: %08X\n", lo + 4 * i, v);
        }
    } else if (strcmp(cmd, "write") == 0) {
        if (argc < 4) die(c, "write <loHex> <valHex>");
        uint32_t lo, v;
        parse_hex_arg(argv[2], &lo);
        parse_hex_arg(argv[3], &v);
        int kr = fwio_write32(c, lo, v);
        printf(kr ? "✗ 写失败 0x%x\n" : "✓ 已写\n", kr);
    } else if (strcmp(cmd, "avc") == 0) {
        if (argc < 3) die(c, "avc <hex...>  例: avc 01 FF 18 00 FF FF FF FF");
        uint8_t f[64]; uint32_t n = 0;
        for (int i = 2; i < argc && n < 64; i++) {
            unsigned b;
            if (sscanf(argv[i], "%x", &b) != 1) die(c, "参数不是 hex");
            f[n++] = (uint8_t)b;
        }
        avc_transaction(c, f, n);
    } else if (strcmp(cmd, "rate") == 0) {
        uint8_t f[8];
        bebob_build_sig_fmt(f, 1, 0, 1, 0);
        printf("[OUTPUT plug 0]\n");
        int kr = avc_transaction(c, f, 8);
        if (kr == 0) {
            unsigned sfc = f[5] & 0x07;      // STATUS 回显里带当前格式
            printf("  当前采样率: %u Hz (sfc=%u)\n", bebob_sfc_rates[sfc], sfc);
        }
        if (argc > 2) {                       // rate set
            uint32_t hz = atoi(argv[2]);
            int sfc = bebob_rate_to_sfc(hz);
            if (sfc < 0) die(c, "不支持的采样率（44100/48000/88200/96000/176400/192000）");
            printf("[设 OUTPUT→INPUT，sfc=%d]\n", sfc);
            bebob_build_sig_fmt(f, 1, 0, 0, (unsigned)sfc);
            if (avc_transaction(c, f, 8)) die(c, "OUTPUT 设采样率被拒");
            usleep(300 * 1000);               // Linux：设备切换需要 ~300ms
            bebob_build_sig_fmt(f, 0, 0, 0, (unsigned)sfc);
            if (avc_transaction(c, f, 8)) fprintf(stderr, "⚠ INPUT 设采样率被拒（部分设备只认 OUTPUT）\n");
            printf("✓ 采样率已设 %u Hz\n", hz);
        }
    } else {
        fprintf(stderr, "未知命令: %s\n", cmd);
        fwio_close(c);
        return 2;
    }
    fwio_close(c);
    return 0;
}
