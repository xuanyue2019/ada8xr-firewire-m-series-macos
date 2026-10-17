// bebob.h — ADA-8XR 火线卡的 BeBoB 协议常量与命令构造器
// 依据（对源码核对，不是凭记忆）：
//   - Linux sound/firewire/cmp.c        PCR 位段与建连算法
//   - Linux sound/firewire/fcp.c        AV/C 信号格式命令字节
//   - Linux sound/firewire/bebob/bebob.c  Prism 设备条目（spec_normal，无 quirk）
//   - IEEE 1212/61883-1 CSR 寄存器布局
#pragma once
#include <stdint.h>
#include <stddef.h>

// ---- CSR 地址（总线 48 位定址：hi=0xFFFF, lo=0xF000xxxx） ----
#define FW_ADDR_CSR(hi, lo) ((hi) == 0xFFFF && (lo) >= 0xF0000000u)
#define BEBOB_LO_IMPR   0xF0000700u  // 输入主插寄存器（iMPR）
#define BEBOB_LO_OMPR   0xF0000780u  // 输出主插寄存器（oMPR）
#define BEBOB_LO_IPCR(n) (0xF0000800u + 4u * (n))  // 输入点对点连接寄存器
#define BEBOB_LO_OPCR(n) (0xF0000900u + 4u * (n))  // 输出点对点连接寄存器

// ---- PCR 位段（Linux cmp.c 同款） ----
#define PCR_ONLINE            0x80000000u
#define PCR_BCAST_CONN        0x40000000u
#define PCR_P2P_CONN_MASK     0x3f000000u
#define PCR_P2P_CONN_SHIFT    24
#define PCR_CHANNEL_MASK      0x003f0000u
#define PCR_CHANNEL_SHIFT     16
// oPCR 专属
#define OPCR_XSPEED_MASK      0x00C00000u
#define OPCR_XSPEED_SHIFT     22
#define OPCR_SPEED_MASK       0x0000C000u
#define OPCR_SPEED_SHIFT      14
#define OPCR_OVERHEAD_ID_MASK 0x00003C00u
#define OPCR_OVERHEAD_ID_SHIFT 10
// MPR 位段
#define MPR_SPEED_MASK        0xC0000000u
#define MPR_SPEED_SHIFT       30
#define MPR_XSPEED_MASK       0x00000060u
#define MPR_XSPEED_SHIFT      5
#define MPR_PLUGS_MASK        0x0000001fu

// ---- AV/C 帧常量 ----
#define AVC_CTYPE_CONTROL   0x00
#define AVC_CTYPE_STATUS    0x01
#define AVC_SUBUNIT_UNIT    0xFF
#define AVC_OP_INPUT_PLUG_SIG_FMT   0x19   // 输入插信号格式（设/读采样率）
#define AVC_OP_OUTPUT_PLUG_SIG_FMT  0x18   // 输出插信号格式
#define AVC_RESP_NOT_IMPLEMENTED 0x08
#define AVC_RESP_ACCEPTED        0x09
#define AVC_RESP_REJECTED        0x0A
#define AVC_RESP_STABLE          0x0C     // STATUS 查询成功
#define AVC_RESP_IMPLEMENTED     0x0C

// AM824 频率编号（AV/C 信号格式 operand[5] 低 3 位；Linux amdtp_rate_table 顺序）
static const unsigned bebob_sfc_rates[8] = {
    32000, 44100, 48000, 88200, 96000, 176400, 192000, 0 /*undefined*/
};

static inline int bebob_rate_to_sfc(unsigned hz) {
    for (int i = 0; i < 7; i++) if (bebob_sfc_rates[i] == hz) return i;
    return -1;
}

// 构造 AV/C 插信号格式命令（8 字节）。
// dir_out: 1=OUTPUT 插(0x18) 0=INPUT 插(0x19)；get: 1=查询(STATUS) 0=设置(CONTROL)
// sfc: 0-6（设置时有效）
static inline size_t bebob_build_sig_fmt(uint8_t *cmd, int dir_out, unsigned plug,
                                         int get, int sfc) {
    cmd[0] = get ? AVC_CTYPE_STATUS : AVC_CTYPE_CONTROL;
    cmd[1] = AVC_SUBUNIT_UNIT;
    cmd[2] = dir_out ? AVC_OP_OUTPUT_PLUG_SIG_FMT : AVC_OP_INPUT_PLUG_SIG_FMT;
    cmd[3] = (uint8_t)plug;
    cmd[4] = 0x90;                 // EOH_1 Form_1 FMT=AM824
    cmd[5] = (uint8_t)(sfc & 0x07);// FDF-hi: AM824 频率
    cmd[6] = 0xFF;                 // SYT hi（不用）
    cmd[7] = 0xFF;                 // SYT lo（不用）
    return 8;
}

// ---- AMDTP/CIP 包头布局（等时收发阶段用，先立此存照） ----
// CIP q0: sid(6) dbl(2) | dbs(6) fn(2) qpc(3) sph(1) rsv(2)
// CIP q1: dbc(8) fmt(6) fdf(8) syt(16)   —— AM824: fmt=0x10
// BeBoB 流属性（Linux bebob_stream.c discover 阶段经 AV/C 插描述符读取）
#define AMDTP_FMT_AM824 0x10
