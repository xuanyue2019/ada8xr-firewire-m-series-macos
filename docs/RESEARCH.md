# 调研证据集（2026-10-10）

三路并行核查：①官方支持现状 ②设备协议与开源先例 ③macOS 火线栈与驱动开发路径。
另有本机（M4 Max / macOS 26.6.2 / 25G83）直接取证。

## ① 官方支持现状（Prism Sound）

**结论：ADA-8XR 火线卡在 Apple Silicon 上没有任何官方路径；官方上限 macOS 12.6。**

- 官方支持页 https://www.prismsound.io/support ：
  - "Apple has since discontinued support for Core Audio over FireWire starting with macOS 13 Ventura."
  - "the Orpheus and ADA-8XR **FireWire cards are not compatible with macOS versions later than macOS 12 Monterey**."
  - 2022-10-31 通告："DO NOT update your OS to Ventura."
- ADA-8XR FAQ https://www.prismsound.com/products/ada-8xr/faq/ ：
  FW 模块 Mac 支持范围 **macOS 10.4.11 – 12.6**；机制是 "Orpheus interfaces directly
  to Core Audio"（即**苹果系统自带 FW CoreAudio**，Prism 在 Mac 上只有控制面板 + 固件
  加载器，从未有自己的 Mac 音频驱动——Windows 才有）。
- ADA-8XR 下载页 https://www.prismsound.com/products/ada-8xr/downloads/ ：
  Mac 侧只有"控制面板 + 固件加载器"，无版本号无日期；Windows 侧才有"driver"。
- Apple Silicon 原生支持仅限 **X-platform USB 产品**（Atlas/Titan/Lyra，V2.1.6，
  https://www.prismsound.com/products/titan/downloads/ ），与 FW 无关。
- 产品线命运：Orpheus = Legacy Product；经销商 DMT Pro 2025-05 公告 Prism 停止开发
  ADA8 系列、由 DREAM-ADA 接替（快照级证据）。
- 论坛实况：Apple Discussions 254731518（2023-03）"Monterey 可用（TB3→TB2→FW 链），
  Ventura 起全灭"；未找到任何 ADA-8XR 在 M 系成功的报告。

## ② 设备协议与开源先例

**结论：ADA-8XR 火线卡 = 标准 BridgeCo BeBoB；协议有两份 GPL/开源实现可抄；无需逆向。**

- Linux 内核主线 `sound/firewire/bebob/bebob.c`（torvalds 树核对）：
  ```c
  #define VEN_PRISMSOUND 0x00001198
  SND_BEBOB_DEV_ENTRY(VEN_PRISMSOUND, 0x00010048, &spec_normal), /* Orpheus */
  SND_BEBOB_DEV_ENTRY(VEN_PRISMSOUND, 0x0000ada8, &spec_normal), /* ADA-8XR */
  ```
  `spec_normal` = 无任何专有 quirk。Kconfig 帮助文本把 "PrismSound Orpheus/ADA-8XR"
  列为已支持设备。
- 内核提交 c4d860a0d256（2015）：**Orpheus**（BeBoB v3）起流时发不连续 DBC 包，
  加了 `CIP_EMPTY_HAS_WRONG_DBC / CIP_SKIP_DBC_ZERO_CHECK` quirk——**注意这是
  Orpheus 的 quirk，ADA-8XR 条目没有**，但同厂设备，实测时留意。
- FFADO 2.5.0 源码无 Prism 条目（无正式支持），但用户实测 juju+ffado+jackd 可跑
  Orpheus（alsa-devel 2015-04，Mike Ood）；Orpheus 编成实测：44.1 kHz = **10 PCM + 1 MIDI**。
- OUI：`00:11:98 = Prism Media Products Limited`（Wireshark manuf 表）。
- 具体硅片（DM1000/1100/1500）无公开拆解佐证——对写驱动无影响（协议层已定）。
- 早期论坛流传"Orpheus 用 DICE"的说法与内核证据矛盾，**不可信**。

## ③ macOS 火线栈与驱动开发路径（含本机取证）

**结论：Sequoia(15) 火线全活；Tahoe(26) 被"删入口式"移除；本机可做低成本复活实验；
长期正路 = ASFireWire（DriverKit）。**

### 本机取证（macOS 26.6.2, 25G83, arm64 T6041=M4 Max）

- `/System/Library/Extensions/` 仍有 `IOFireWireFamily.kext`（4.8.3）、
  `IOFireWireAVC/SBP2/SerialBusProtocolTransport.kext`、
  `IOFireWireFamily.kext/Contents/PlugIns/AppleFWOHCI.kext`（5.7.6，
  `DTSDKName=macosx26.6.internal`、`DTPlatformVersion=26.6.1` —— **2025 年仍在为 26.6 编译**）。
- **但** `AppleFWOHCI.kext` 的 Info.plist 里 `IOKitPersonalities` 整个被删（无 IOPCIMatch），
  系统里也搜不到 TI OHCI 的 PCI 匹配串 → **OHCI 驱动永远不会绑定转接器芯片**。
  这就是 Tahoe"移除火线"的确切机制，也是 `revive/` 实验的依据。
- 用户态 `IOFireWireLib.plugin`（4.8.3）仍在；CLT 16.4（MacOSX15.5.sdk）里
  `IOKit.framework/Headers/firewire/` 头文件齐全（fwprobe 已用它编译通过）。
- 佐证（外部）：TidBITS 2025-09-19《Support for FireWire Removed from macOS 26 Tahoe》；
  RME 官方"Tahoe 起 Thunderbolt 转接方案无解"；MOTU 同口径。

### 驱动架构选项

| 选项 | 状态 |
|---|---|
| kext：IOFireWireFamily+IOAudioFamily | Sequoia 可行（RME/MOTU 先例）；Tahoe 无控制器绑定，断供 |
| DriverKit：FireWire 传输层 | **Apple 从未提供**（无 IOFireWire DEXT API）|
| ASFireWire：PCIDriverKit 自研 OHCI + AudioDriverKit | 社区已走通，2026-10 活跃，多台 BeBoB/DICE 实测 |
| 用户态：IOFireWireLib + AudioServerPlugIn | Sequoia 直接可行（libASPL 开源库）；Tahoe 需先复活总线 |

### ASFireWire 关键事实（仓库快照 2026-10-10）

- Apache-2.0；动机即"Tahoe 杀了火线"。已实现 OHCI bring-up、总线复位、SelfID、
  拓扑、异步事务、AV/C FCP + CMP、实验性音频发布与起流。
- 维护者实测硬件：Apogee Duet FW、TerraTec PHASE 88、Saffire Pro 24 DSP、M-Audio
  FW1814（48k）；贡献者实测：Saffire Pro 40（20×20 全双工）、StudioLive 16.0.2、Venice F32。
- **目录外 BeBoB 设备自动落入 GenericAvc 家族**（`AudioDeviceResolver.cpp:106-181`）
  → ADA-8XR 理论上零代码即可被通用路径接管。
- 设备目录加条目 = `AudioDeviceIds.hpp` 常量 + `Definitions/<Vendor>.hpp` +
  `AudioDeviceCatalog.cpp` 登记（现 55 条）——见 `asfirewire/ADA-8XR_NOTES.md`。
- 本机构建限制：只有 CLT、无 Xcode → dext 编译需装完整 Xcode 26；
  本地开发要 SIP off + `systemextensionsctl developer on`。

### kext 个人加载流程（AS，2026 仍有效）

恢复模式 → 启动安全性实用工具 → Reduced Security + 允许未知开发者 kext →
ad-hoc 签名（`codesign -s -`）→ 拷 /Library/Extensions →
`sudo kmutil install --volume-root /` → 批准/重启。

## 参考源

- Linux bebob：https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/sound/firewire/bebob
- Orpheus quirk 提交：https://github.com/torvalds/linux/commit/c4d860a0d256663937b51468ea13de3c2cd2a09d
- alsa-devel 2015 Orpheus 实测：https://mailman.alsa-project.org/pipermail/alsa-devel/2015-April/090967.html
- ASFireWire：https://github.com/mrmidi/ASFireWire
- FFADO：https://ffado.org （libffado-2.5.0.tgz 已检查无 Prism 条目）
- TidBITS Tahoe 移除 FW：https://tidbits.com/2025/09/19/support-for-firewire-removed-from-macos-26-tahoe/
- RME macOS 26 口径（转 Production Expert 2026-06）：
  https://www.production-expert.com/production-expert-1/how-to-get-a-firewire-audio-interface-working-with-usb-c-in-2026
- Prism 官方：https://www.prismsound.io/support 、
  https://www.prismsound.com/products/ada-8xr/faq/ 、
  https://www.prismsound.com/products/ada-8xr/downloads/
- libASPL（用户态 HAL 驱动库）：https://github.com/gavv/libASPL
