# 把 ADA-8XR 接进 ASFireWire —— 补丁草图（Phase 3 施工图）

> 上游仓库：https://github.com/mrmidi/ASFireWire （Apache-2.0，2026-10 活跃）
> 本文件基于 2026-10-10 快照写成；动工前先 `git pull` 并核对行号/枚举名。

## 为什么工作量很小

ADA-8XR 的火线卡 = **标准 BeBoB（BridgeCo）**设备：

- Linux 内核 `sound/firewire/bebob/bebob.c` 里两个 Prism 条目都用 `spec_normal`
  （无任何专有 quirk）：
  - `VEN_PRISMSOUND 0x00001198` + Orpheus `0x00010048`
  - `VEN_PRISMSOUND 0x00001198` + ADA-8XR `0x0000ada8`（"ADA8" 助记）
- BeBoB 的信令三件套（AV/C 发现 → CMP 接管 iPCR/oPCR → AMDTP 流）ASFireWire 已全部实现，
  且**目录外的 BeBoB 设备会自动落到 `GenericAvc` 家族**（`AudioDeviceResolver.cpp:106-181`
  的 fallback 链）。也就是说：**不加任何代码，ADA-8XR 很可能接上就能被枚举成
  "Generic AV/C audio"并尝试起流**。

加目录条目的意义：正确的设备名、Supported 等级、（必要时）per-device 编成/quirk。

## 补丁内容（四处改动）

### 1) `ASFWDriver/DeviceProfiles/Audio/AudioDeviceIds.hpp` —— 加 Prism 常量块

仿照文件里 TerraTec（BeBoB 家族）的写法：

```cpp
// ---- Prism Sound (BridgeCo / BeBoB family) ----
// Linux sound/firewire/bebob/bebob.c:
//   SND_BEBOB_DEV_ENTRY(VEN_PRISMSOUND /*0x00001198*/, 0x00010048, &spec_normal) // Orpheus
//   SND_BEBOB_DEV_ENTRY(VEN_PRISMSOUND, 0x0000ada8, &spec_normal)                // ADA-8XR
inline constexpr uint32_t kPrismVendorId     = 0x001198;
inline constexpr uint32_t kPrismOrpheusModelId = 0x00010048;
inline constexpr uint32_t kPrismAda8xrModelId  = 0x0000ada8;
inline constexpr const char* kPrismVendorName       = "Prism Sound";
inline constexpr const char* kPrismOrpheusModelName = "Orpheus";
inline constexpr const char* kPrismAda8xrModelName  = "ADA-8XR";
```

### 2) 新文件 `ASFWDriver/DeviceProfiles/Audio/Definitions/PrismSound.hpp`

仿 `Definitions/TerraTec.hpp`（同家族，逐字段对齐）：

```cpp
#pragma once
#include "CatalogHelpers.hpp"
#include <array>

namespace ASFW::DeviceProfiles::Audio::Definitions {

inline constexpr std::array kPrismDefinitions{
    Definition(DeviceDefinitionId::PrismAda8XR, kPrismVendorId,
               kPrismAda8xrModelId, AudioFamilyProviderId::BeBoB,
               ProbePolicyId::BeBoBPlug0, ProfileBuilderId::GenericAvc,
               ProtocolImplementationId::GenericAvc,
               SupportDisposition::Supported, kPrismVendorName,
               kPrismAda8xrModelName, std::nullopt, BootloaderCuePolicy::None,
               kCmpBlockingTraits),
    // Orpheus 同款一条（用户群里 Orpheus 存量更大）
};

} // namespace
```

注意核对：
- `DeviceDefinitionId` 枚举要加 `PrismAda8XR`（`AudioDeviceCatalog.hpp:19` 起，别撞退役编号）；
- `ProfileBuilderId::GenericAvc` / `ProtocolImplementationId::GenericAvc` 是通用路径的取值
  （枚举见 `AudioDeviceCatalog.hpp:129/184`；"BeBoBGeneric" 已退役，注释里写明统一走 GenericAvc）；
- `ProbePolicyId::BeBoBPlug0` 是普通 BeBoB 探测策略（TerraTec/M-Audio 同款）。

### 3) `ASFWDriver/DeviceProfiles/Audio/AudioDeviceCatalog.cpp` —— 登记

```cpp
#include "Definitions/PrismSound.hpp"
...
constexpr auto kDefinitions = ConcatArrays(
    ...,
    Definitions::kPrismDefinitions,
    Definitions::kAvidDefinitions
);
static_assert(kDefinitions.size() == 57U, "...");  // 55 → 57
```

### 4) （视实测再定）编成与 quirk

- **通道编成**：Orpheus 实测 44.1 kHz = 10 PCM + 1 MIDI（alsa-devel 2015-04）。
  ADA-8XR 预期 8 进 8 出（是否有 MIDI 待实测）。ASFireWire 用 `StreamGeometryResolver`
  从设备侧读取编成，通常不用硬编码；若设备上报不准，再仿 M-Audio 加静态编成表。
- **DBC 不连续 quirk**：Linux 只给 *Orpheus*（BeBoB v3）打过
  `CIP_EMPTY_HAS_WRONG_DBC / CIP_SKIP_DBC_ZERO_CHECK`（commit c4d860a0d256），
  **ADA-8XR 条目没有任何 quirk** —— 预期流是干净的；若实测出现周期性爆音/丢块，
  在 ASFW 的 CIP 校验层找对应开关（搜 `Isoch` 目录下的 DBC/CIP 校验）。
- **采样率/时钟源**：BeBoB 通用 AV/C 采样率命令（OUTPUT PLUG signal format）一般可用；
  Ubuntu 时代 Orpheus 曾要求先在 Win/Mac 上预设采样率——若复现，就是驱动要补的第一个控制点。

## 构建与部署（本机现状：只有 CLT，没有 Xcode）

```bash
# 需要完整 Xcode 26（DriverKit/DEXT 开发 CLT 搞不定）→ 装 Xcode 后：
cd ASFireWire
./build.sh          # 或开 ASFW.xcodeproj 直接 build
# 本地开发模式安装（实验 dext）：
sudo csrutil disable          # 恢复模式下
systemextensionsctl developer on
./install.sh
```

或直接用上游预编译 build 当"白盒试验台"，先把 ADA-8XR 插上看日志（README 有
"Installing a prebuilt build (testers)" 一节 + 日志收集方法）。

## 硬件到手后的验证清单（按序）

1. `fwprobe` 确认 GUID=00:11:98:…、Unit Spec 0x00A02D/Version 0x010001；
2. ASFireWire 装上后看它日志：应出现 GenericAV/C 发现 → CMP 接管 → AMDTP 协商；
3. `system_profiler SPAudioDataType` 应出现新音频设备（名字先不管对不对）；
4. Logic/afplay 放 44.1k 测试音 → ADA-8XR 模拟口出声 = **主路打通**；
5. 再逐项：48k/96k/192k 采样率切换、8×8 同时进录（afrecord/Logic）、连续 30 分钟
   无爆音（盯 DBC 连续性）、总线复位恢复（拔插 FW 线）；
6. 控制面：ADA-8XR 面板上的时钟源/矩阵照常用手操作（不依赖驱动），
   驱动侧只要"采样率+流"正确，音质路径就正确（数字直通）。

## 回馈上游

补丁干净的话直接给 mrmidi 提 PR（README 有 Contributing 一节）——
ADA-8XR/Orpheus 是"标准 spec_normal BeBoB"，是很受欢迎的低成本新增设备。
