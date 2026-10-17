# 施工计划（分阶段、带验收门槛）

## Phase 0 —— 购物 + 环境（无硬件也能做）

- [ ] 购入：TB3→TB2 转接器 + TB2→FW800 转接器 + FW800→FW400(6针) 线
- [ ] 装完整 Xcode 26（路线 A 编译 dext 用；App Store / developer.apple.com）
- [ ] （可选）备份系统 + 确认可接受 Reduced Security / SIP off（实验机心态）

**门槛**：转接链插上后，系统信息-雷电图里能看到两枚转接器（此时火线页仍是空的，正常）。

## Phase 1 —— 总线点亮（硬件到手当天）

1. 插好 ADA-8XR（开机状态），跑：
   ```bash
   cd tools/fwprobe && ./fwprobe      # 无输出属性时用 sudo
   ```
2. 期望输出：`GUID: 001198:xxxxxx…  <== Prism Sound!!!` + Unit_Spec_Id `0x00A02D` +
   Unit_Sw_Version `0x010001`（BeBoB 确认）。

**分岔**：
- **Tahoe 上看不到任何火线节点（预期如此）** → 进 Phase 2。
- 若 fwprobe 直接看到设备（例如你已刷回 Sequoia）→ 跳到 Phase 3 用户态路线或直接 ASFireWire。

## Phase 2 —— 让总线在 Tahoe 上活过来（两条子路，可都试）

### 2a. FireWireRevive（先试，成本 10 分钟）

- [ ] 按 `revive/INSTALL.md`：Reduced Security → `./install.sh` → 重启
- [ ] `ioreg -r -c IOFireWireController` 有 AppleFWOHCI 实例？
- [ ] `fwprobe` 能看到 ADA-8XR？

**成了**：总线层免费到手。后续两条任选：
  - 2a-1（轻）：**driver/ada8xrctl 已经写好并编译通过**——插上设备先跑
    `list → rom → mpr → pcr → rate 44100`，这五个命令全绿 = 控制面（AV/C+CMP）
    打通；剩下的是等时收发（IOFireWireLibIsoch 的 DCL 程序）+ AudioServerPlugIn。
  - 2a-2（重）：仍走 ASFireWire（不依赖苹果栈，更抗系统升级）。
**败了**：删 kext 回滚（INSTALL.md 末尾），转 2b。

### 2b. ASFireWire（主线）

- [ ] 装 Xcode 26 → `cd asfirewire/ASFireWire && ./build.sh` 编译通过
- [ ] 开发者模式装 dext（SIP off + `systemextensionsctl developer on` + `./install.sh`）
- [ ] 看 ASFW App/日志：ADA-8XR 是否被 GenericAvc 路径发现
- [ ] 顺手用它的 MCP 工具发 AV/C 命令读设备状态（它有整套 BeBoB 工具）

**门槛**：设备被枚举、日志里看到 CMP/AMDTP 协商痕迹。

## Phase 3 —— 出声与长稳（真正的验收）

- [ ] 系统出现音频设备；afplay/Logic 放 44.1k 测试音 → ADA-8XR 模拟口出声 🎉
- [ ] 8 进 8 出同录（Logic 16 轨 arm）
- [ ] 采样率切换 44.1/48/88.2/96/176.4/192（注意 Orpheus 曾需 Win/Mac 预设采样率的坑）
- [ ] 连续 30 分钟无爆音（若有周期性咔哒 → 查 DBC quirk，Linux 只给 Orpheus 打过，
      ADA-8XR 按理不需要，但同厂设备要防）
- [ ] 拔插 FW 线触发总线复位 → 能自恢复
- [ ] 时钟源：ADA-8XR 面板手动设 WordClock/AES 同步（不依赖驱动）

## Phase 4 —— 收尾

- [ ] 把 `Definitions/PrismSound.hpp` 补丁整理干净，给 mrmidi 提 PR（回馈上游）
- [ ] 写一篇施工记（用了哪些坑、fwprobe 输出长什么样）

## 风险登记

| 风险 | 概率 | 缓解 |
|---|---|---|
| AppleFWOHCI 在 Tahoe 起不来 | 中 | 无损失，转 ASFireWire |
| ASFireWire 等时层对 8×8@192k 吃不消 | 中 | 先 44.1/48k 跑通；高采样率跟进上游 |
| ADA-8XR FW 卡固件过老，AV/C 响应怪 | 低 | fwprobe 输出对拍 Linux 源码；必要时升固件（官网 loader） |
| macOS 27 再拆火线残留代码 | 中 | ASFireWire 不依赖苹果栈，是长期避风港 |
| DBC 不连续爆音 | 低-中 | Linux quirk 先例现成（c4d860a0d256） |
