// fwio.c — IOFireWireLib/AVCLib 封装实现
//
// 关键实现笔记（都是本机头文件/插件 Info.plist 里核出来的，别凭记忆改）：
//   * IOFireWireLib.plugin 只登记一个工厂：A1478010-F197-11D4-A28B-000502072F80，
//     SDK 头文件没导出这个宏，下面手写。
//   * FWAddress = {UInt16 nodeID; UInt16 addressHi; UInt32 addressLo;}
//     （nodeID 在结构里！IOFireWireFamilyCommon.h:385 附近）
//   * ReadQuadlet/WriteQuadlet/CompareSwap 都是阻塞式；device 参数传
//     GetDevice() 即为 48 位设备相对寻址（此时 nodeID 字段置 0）。
//   * AVC 走 IOFireWireAVCUnit 服务的 AVCLib 插件（AVCCommand 阻塞，10s 超时）。
#include "fwio.h"
#include "bebob.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/firewire/IOFireWireLib.h>
#include <IOKit/AVC/IOFireWireAVCLib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// IOFireWireLibFactory UUID（IOFireWireLib.plugin CFPlugInFactories 登记，SDK 未导出）
#define kIOFireWireLibFactoryID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault, \
    0xA1, 0x47, 0x80, 0x10, 0xF1, 0x97, 0x11, 0xD4, \
    0xA2, 0x8B, 0x00, 0x05, 0x02, 0x07, 0x2F, 0x80)

struct fwio_ctx {
    io_object_t nubService;                 // IOFireWireNub
    IOCFPlugInInterface **fwPlugin;
    IOFireWireLibNubRef dev;                // 已 Open 的 nub 接口
    io_object_t avcService;                 // IOFireWireAVCUnit
    IOCFPlugInInterface **avcPlugin;
    IOFireWireAVCLibUnitInterface *avc;     // 已 open 的 AV/C 单元接口
    io_object_t deviceForAddressing;        // GetDevice() 结果
    UInt32 generation;
    char guid[17];
};

// ---- GUID 属性读取：兼容 CFString / CFNumber 两种形态 ----
static int get_guid_string(io_object_t svc, char out[17]) {
    CFTypeRef v = IORegistryEntrySearchCFProperty(
        svc, kIOServicePlane, CFSTR("GUID"), kCFAllocatorDefault, 0);
    if (!v) return -1;
    int ok = -1;
    if (CFGetTypeID(v) == CFStringGetTypeID()) {
        ok = CFStringGetCString(v, out, 17, kCFStringEncodingUTF8) ? 0 : -1;
    } else if (CFGetTypeID(v) == CFNumberGetTypeID()) {
        long long n = 0;
        if (CFNumberGetValue(v, kCFNumberLongLongType, &n)) {
            snprintf(out, 17, "%016llX", n);
            ok = 0;
        }
    }
    CFRelease(v);
    return ok;
}

// 在 IOFireWireNub 里按 GUID 前缀找设备；也顺带找它的 AVC 单元。
static io_object_t find_nub(const char *prefix, io_object_t *avcOut, char *guidOut) {
    io_iterator_t it = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                     IOServiceMatching("IOFireWireNub"), &it))
        return MACH_PORT_NULL;

    io_object_t hit = MACH_PORT_NULL;
    io_object_t e;
    while (!hit && (e = IOIteratorNext(it)) != MACH_PORT_NULL) {
        char g[17] = "";
        if (get_guid_string(e, g) != 0) { IOObjectRelease(e); continue; }
        if (!prefix || strncmp(g, prefix, strlen(prefix)) == 0) {
            hit = e;
            if (guidOut) memcpy(guidOut, g, 17);
        } else {
            IOObjectRelease(e);
        }
    }
    IOObjectRelease(it);
    if (!hit) return MACH_PORT_NULL;

    // 找挂在同一父链下的 AVC 单元：直接按 GUID 前缀过滤 AVCUnit
    if (avcOut) {
        *avcOut = MACH_PORT_NULL;
        if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                         IOServiceMatching("IOFireWireAVCUnit"), &it) == KERN_SUCCESS) {
            io_object_t a;
            while ((a = IOIteratorNext(it)) != MACH_PORT_NULL) {
                char ag[17] = "";
                if (get_guid_string(a, ag) == 0 &&
                    strncmp(ag, guidOut ? guidOut : "", 16) == 0) {
                    *avcOut = a;
                    break;
                }
                IOObjectRelease(a);
            }
            IOObjectRelease(it);
        }
    }
    return hit;
}

fwio_ctx_t *fwio_open(const char *guid_prefix_hex, char *err, size_t errlen) {
    fwio_ctx_t *c = calloc(1, sizeof *c);
    SInt32 score = 0;
    HRESULT hr;
    IOReturn kr;

    c->nubService = find_nub(guid_prefix_hex, &c->avcService, c->guid);
    if (!c->nubService) {
        snprintf(err, errlen, "总线上没找到%s火线设备（GUID 前缀 %s）。\n"
                 "  - 火线控制器活着吗？先跑 tools/fwprobe/fwprobe\n"
                 "  - macOS 26：需先装 revive/FireWireRevive.kext 或 ASFireWire",
                 guid_prefix_hex ? "符合前缀的" : "任何", guid_prefix_hex ? guid_prefix_hex : "*");
        free(c);
        return NULL;
    }

    kr = IOCreatePlugInInterfaceForService(c->nubService, kIOFireWireLibTypeID,
                                           kIOFireWireLibFactoryID,
                                           &c->fwPlugin, &score);
    if (kr != KERN_SUCCESS) {
        snprintf(err, errlen, "IOCreatePlugInInterfaceForService(FW) 失败: 0x%x", kr);
        fwio_close(c);
        return NULL;
    }
    hr = (*c->fwPlugin)->QueryInterface(c->fwPlugin,
            CFUUIDGetUUIDBytes(kIOFireWireNubInterfaceID_v5), (LPVOID *)&c->dev);
    if (hr != S_OK || !c->dev) {
        snprintf(err, errlen, "QueryInterface(NubInterfaceID_v5) 失败: 0x%lx", (long)hr);
        fwio_close(c);
        return NULL;
    }
    kr = (*c->dev)->Open(c->dev);
    if (kr != KERN_SUCCESS) {
        snprintf(err, errlen, "Open(nub) 失败: 0x%x（设备可能被占用）", kr);
        fwio_close(c);
        return NULL;
    }
    c->deviceForAddressing = (*c->dev)->GetDevice(c->dev);
    UInt16 localNode = 0;
    (*c->dev)->GetGenerationAndNodeID(c->dev, &c->generation, &localNode);

    if (c->avcService != MACH_PORT_NULL) {
        kr = IOCreatePlugInInterfaceForService(c->avcService, kIOFireWireAVCLibUnitTypeID,
                                               kIOFireWireAVCLibUnitFactoryID,
                                               &c->avcPlugin, &score);
        if (kr == KERN_SUCCESS && c->avcPlugin) {
            (*c->avcPlugin)->QueryInterface(c->avcPlugin,
                CFUUIDGetUUIDBytes(kIOFireWireAVCLibUnitInterfaceID), (LPVOID *)&c->avc);
            if (c->avc) c->avc->open(c->avc);
        }
    }
    return c;
}

void fwio_close(fwio_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->dev) {
        (*ctx->dev)->Close(ctx->dev);
        (*ctx->dev)->Release(ctx->dev);
    }
    if (ctx->fwPlugin) IODestroyPlugInInterface(ctx->fwPlugin);
    if (ctx->avc) ctx->avc->Release(ctx->avc);
    if (ctx->avcPlugin) IODestroyPlugInInterface(ctx->avcPlugin);
    if (ctx->avcService != MACH_PORT_NULL) IOObjectRelease(ctx->avcService);
    if (ctx->nubService != MACH_PORT_NULL) IOObjectRelease(ctx->nubService);
    free(ctx);
}

static FWAddress dev_addr(uint32_t lo) {
    FWAddress a = {0, 0xFFFF, lo};   // 48 位设备相对寻址（nodeID=0，device 参数生效）
    return a;
}

int fwio_read32(fwio_ctx_t *ctx, uint32_t lo, uint32_t *val) {
    FWAddress a = dev_addr(lo);
    IOReturn kr = (*ctx->dev)->ReadQuadlet(ctx->dev, ctx->deviceForAddressing,
                                           &a, val, true, ctx->generation);
    if (kr != KERN_SUCCESS) {         // 总线代际变了就刷新重试一次
        UInt16 node;
        (*ctx->dev)->GetGenerationAndNodeID(ctx->dev, &ctx->generation, &node);
        kr = (*ctx->dev)->ReadQuadlet(ctx->dev, ctx->deviceForAddressing,
                                      &a, val, true, ctx->generation);
    }
    return (int)kr;
}

int fwio_write32(fwio_ctx_t *ctx, uint32_t lo, uint32_t val) {
    FWAddress a = dev_addr(lo);
    IOReturn kr = (*ctx->dev)->WriteQuadlet(ctx->dev, ctx->deviceForAddressing,
                                            &a, val, true, ctx->generation);
    if (kr != KERN_SUCCESS) {
        UInt16 node;
        (*ctx->dev)->GetGenerationAndNodeID(ctx->dev, &ctx->generation, &node);
        kr = (*ctx->dev)->WriteQuadlet(ctx->dev, ctx->deviceForAddressing,
                                       &a, val, true, ctx->generation);
    }
    return (int)kr;
}

int fwio_cmpswap32(fwio_ctx_t *ctx, uint32_t lo, uint32_t expect, uint32_t want,
                   uint32_t *cur) {
    FWAddress a = dev_addr(lo);
    IOReturn kr = (*ctx->dev)->CompareSwap(ctx->dev, ctx->deviceForAddressing,
                                           &a, expect, want, true, ctx->generation);
    // 简化版 CompareSwap 不回旧值：无论成败都读回当前值供调用方判断
    uint32_t now = 0;
    fwio_read32(ctx, lo, &now);
    if (cur) *cur = now;
    return (int)kr;
}

int fwio_avc(fwio_ctx_t *ctx, const uint8_t *cmd, uint32_t cmd_len,
             uint8_t *resp, uint32_t *resp_len) {
    if (!ctx->avc) return -1;
    return (int)ctx->avc->AVCCommand(ctx->avc, cmd, cmd_len, resp, resp_len);
}

const char *fwio_guid(const fwio_ctx_t *ctx) { return ctx->guid; }

size_t fwio_config_rom(const fwio_ctx_t *ctx, uint8_t *buf, size_t buflen) {
    CFDataRef rom = IORegistryEntrySearchCFProperty(
        ctx->nubService, kIOServicePlane, CFSTR("Config ROM"),
        kCFAllocatorDefault, 0);
    if (!rom) return 0;
    size_t l = (size_t)CFDataGetLength(rom);
    if (buf) {
        size_t n = l < buflen ? l : buflen;
        memcpy(buf, CFDataGetBytePtr(rom), n);
    }
    CFRelease(rom);
    return l;
}
