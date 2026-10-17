// fwio.h — macOS IOFireWireLib 用户态火线访问封装（我们自己的驱动底座）
#pragma once
#include <IOKit/IOKitLib.h>
#include <stdint.h>

typedef struct fwio_ctx fwio_ctx_t;

// 在总线上找 Prism Sound 设备（GUID 厂商号前缀 001198）。
// guid_prefix_hex: 形如 "001198"；传 NULL 则取第一个远端设备。
// 成功返回上下文（已完成 Open），失败返回 NULL 并在 err[256] 里留原因。
fwio_ctx_t *fwio_open(const char *guid_prefix_hex, char *err, size_t errlen);
void fwio_close(fwio_ctx_t *ctx);

// 同步 quadlet 读写 + lock compare-swap（hi 固定 0xFFFF，lo 是 CSR 低 32 位）。
// compare-swap 语义与 Linux cmp.c 的 pcr_modify 对齐：无论成败都把
// 寄存器当前值写回 *cur，由调用方判断是否换成功。
int fwio_read32(fwio_ctx_t *ctx, uint32_t lo, uint32_t *val);
int fwio_write32(fwio_ctx_t *ctx, uint32_t lo, uint32_t val);
int fwio_cmpswap32(fwio_ctx_t *ctx, uint32_t lo, uint32_t expect, uint32_t want,
                   uint32_t *cur);

// AV/C：阻塞式发命令。返回 IOReturn；resp 为响应帧，*resp_len 进出。
int fwio_avc(fwio_ctx_t *ctx, const uint8_t *cmd, uint32_t cmd_len,
             uint8_t *resp, uint32_t *resp_len);

// 设备 GUID（16 进位数字符串，16 字符），供展示
const char *fwio_guid(const fwio_ctx_t *ctx);
// Config ROM 原始数据（返回长度，buf 可为 NULL 只取长度）
size_t fwio_config_rom(const fwio_ctx_t *ctx, uint8_t *buf, size_t buflen);
