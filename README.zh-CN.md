# ADA-8XR 火线接苹果 M 系列芯片 Mac

**在 Apple M 系列芯片（Apple Silicon：M1/M2/M3/M4……）+ macOS 26 (Tahoe) 的 Mac 上使用 Prism Sound ADA-8XR 火线卡——在苹果彻底移除火线支持的系统上。**

已在 **M4 Max** MacBook Pro 上实测跑通：ADA-8XR 被枚举为 CoreAudio 设备（**火线 8 进 8 出 @ 48 kHz**，显示名 *"Dream ADA-8"*），并从模拟口正常出声。无需内核代码、无需逆向工程、无需 Prism 官方软件。

```
M 系列 Mac (macOS 26 Tahoe) ──雷电──▶ ASFireWire（社区 DriverKit 火线栈）
                                           │  IEEE 1394
                                           ▼
                                   ADA-8XR 火线卡
                                   （标准 BeBoB 协议）
                                           │  模拟
                                           ▼
                                     你的监听设备 🎧
```

[English README](README.md)

## 为什么"不可能"的事能成

1. **官方路径确实死了**：ADA-8XR 火线卡在 Mac 上从来没有 Prism 写的驱动——靠的是苹果自带的 CoreAudio-over-FireWire。苹果 macOS 13 Ventura 删了音频层，macOS 26 Tahoe 连火线总线驱动都删了。官方确认火线卡上限 macOS 12。（完整证据见 [docs/RESEARCH.md](docs/RESEARCH.md)）
2. **协议不是私有的**：ADA-8XR 火线卡 = 标准 BridgeCo **BeBoB**（AV/C + CMP + AMDTP）。Linux 内核 `snd-bebob` 2014 年起原生支持（vendor `0x001198` / model `0x0000ada8`，无 quirk）——不用逆向。
3. **缺的地基有人打好**：[ASFireWire](https://github.com/mrmidi/ASFireWire)（Apache-2.0）是社区为"Tahoe 杀火线"写的 DriverKit 火线栈。目录外的 BeBoB 设备自动落入其 **Generic AV/C** 家族——ADA-8XR 零代码即被认出。

本仓库打包的是：**验证过的端到端配方** + **诊断工具** + **调研证据** + **给上游的正名补丁草图**。

## 当前状态（2026-10-17）

| 能力 | 状态 |
|---|---|
| 设备枚举（8×8，火线） | ✅ 已验证 |
| 44.1/48 kHz 播放 → 模拟出声 | ✅ 已验证 |
| 流稳定性（30 秒+轮询监控，无掉线） | ✅ 已验证 |
| 首次起流（观察到一次 dext 自愈重启） | ⚠️ 无需干预 |
| 输入 / 8×8 同录 | ⬜ 未测 |
| 88.2–192 kHz | ⬜ 未测（设备上报 32k–96k 档位） |
| 长稳（小时级） | ⬜ 未测 |

## 快速开始

**硬件清单**

| 物品 | 说明 |
|---|---|
| 苹果 TB3(USB-C)→TB2 转接器 | A1621，已停产，二手有 |
| 苹果 TB2→FireWire 800 转接器 | A1463，同上 |
| FW800(9针)→FW400(6针) 线 | ADA-8XR 火线卡是 6 针口 |
| ADA-8XR（火线卡版） | 开机状态 |

（替代：带 FW800 口的雷电坞，OWC/Sonnet 旧款 TB2/3 坞一盒搞定。）

**步骤**（细节见 [docs/INSTALL.md](docs/INSTALL.md)）

1. 关 SIP：恢复模式 → 终端 → `csrutil disable` → 重启。（无签名实验 dext 的硬性要求，安全影响见下。）
2. 下载 [ASFireWire](https://github.com/mrmidi/ASFireWire/releases)（本方案在 `ASFW-0.5.0-beta.1` 上验证），解压跑它的 `install.sh`。
3. 插好转接链，ADA-8XR 开机。
4. 批准驱动扩展：**系统设置 → 通用 → 登录项与扩展 → 滚动到最底部 → 驱动程序扩展 → 打开 ASFW 开关**（macOS 26 的批准入口**不在**「隐私与安全性」横幅——这个坑我们踩了一晚上）。
5. 验证：`system_profiler SPAudioDataType` 应列出 **Dream ADA-8**（8 进 8 出，FireWire）。
6. 选它为输出设备（`make` 后 `tools/setout "Dream ADA-8"`，或系统设置-声音），放音乐。
7. 没声音但 ADA-8XR 面板计量条在动？火线侧一切正常——去修设备内部路由（[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)）。

## 仓库结构

```
├── preflight.sh              一键体检（SIP/火线芯片/dext 状态/音频设备）
├── Makefile                  编译全部工具
├── tools/
│   ├── fwprobe/              火线总线探测（经典苹果栈，注意适用范围）
│   └── setout.c              按名字切换系统默认输出设备
├── driver/                   用户态 BeBoB 控制面控制台（ada8xrctl）
│   └── ref/                  Linux snd-bebob 源码（GPL-2.0，仅供参考）
├── revive/                   FireWireRevive.kext——「唤醒苹果休眠栈」备选路线
└── docs/
    ├── INSTALL.md            分步安装指南（2026-10 实测验证）
    ├── TROUBLESHOOTING.md    通宵调机实录沉淀的排障手册
    ├── RESEARCH.md           证据集：官方口径/协议判定/macOS 栈取证
    ├── PLAN.md               分阶段路线图（含验收门槛）
    ├── NOTES-API.md          IOFireWireLib API 考古笔记（经典栈工具开发用）
    └── ASFW_UPSTREAM_PATCH.md 给 ASFW 加 ADA-8XR 正名条目的补丁草图
```

## 工具说明

- **`setout`**——`./setout "Dream ADA-8"`：迷你 CoreAudio HAL 工具，按名字切默认输出设备，无第三方依赖。
- **`ada8xrctl`**——BeBoB 控制台（列设备、dump Config ROM / iMPR / oPCR、裸 AV/C 事务、采样率读写）。走**经典苹果火线栈**（IOFireWireLib），适用于 macOS ≤15 或装了 `revive/` 的场景。
- **`fwprobe`**——只读火线总线探测，带 Config ROM 解析和 Prism/BeBoB 自动判定。同样只认经典栈。

> **注意**：在 ASFireWire（macOS 26 路线）下 `fwprobe`/`ada8xrctl` 永远报「未找到」——它们枚举的是旧内核类（`IOFireWireNub` 等），DriverKit 栈不发布这些类。验证用 `system_profiler SPAudioDataType` 或 ASFW app。

## 上游贡献

`docs/ASFW_UPSTREAM_PATCH.md` 是一份现成草图（~60 行、四个文件），给 ASFireWire 设备目录加一条正式的 **Prism Sound ADA-8XR** 条目：正确设备名、Supported 等级、以及将来发现 quirk 时的挂载位。提上去之后设备会显示为 "Prism Sound ADA-8XR" 而非通用名，也让后来人少走弯路。

## 安全须知

- **使用 ASFW 期间 SIP 保持关闭**（无签名实验系统扩展）。这会降低系统防御——请相应对待这台机器；不用火线链时建议 `csrutil enable` 恢复。
- `revive/FireWireRevive.kext` 与 ASFireWire **抢同一 PCI 设备，绝不能同时启用**。
- `ada8xrctl write` 直接写火线 CSR——仅调试用。

## 致谢与许可

- [ASFireWire](https://github.com/mrmidi/ASFireWire)，作者 mrmidi 及贡献者（**Apache-2.0**）——让这一切成为可能的 DriverKit 火线栈。本仓库不打包它，请到其 releases 下载。
- Linux 内核 `sound/firewire/bebob`（**GPL-2.0**）——协议参考实现；`driver/ref/` 存了几份原文供离线研读，见该目录内的许可说明。
- Prism Sound ADA-8XR 是 Prism Media Products Limited 的产品。本项目是独立社区行为，与 Prism Sound 无关、未获其背书。
- **开发工具**：本项目的调机、工具开发与文档撰写使用 [ZCode](https://z.ai)（Z.ai 出品的 GLM 大模型 AI 编程助手）完成。

本仓库自有代码与文档以 **MIT License** 发布（见 [LICENSE](LICENSE)）。
