# FireWireRevive：在 macOS 26 Tahoe 上"复活"苹果火线栈的实验

## 这是什么

Tahoe 移除火线的方式不是删代码，而是**删入口**：

- `IOFireWireFamily.kext`（4.8.3）和 `AppleFWOHCI.kext`（5.7.6，2025 年还用 macosx26.6.internal SDK 编译过）仍在系统内核缓存里；
- 但 `AppleFWOHCI.kext` 的 `IOKitPersonalities`（PCI 匹配规则）被整个删掉了，所以雷电→火线适配器里的 TI OHCI 芯片**永远没有驱动来认领**。

`FireWireRevive.kext` 是一个 **codeless kext**（只有 Info.plist、没有代码），
做的事只有一件：把匹配规则补回去——

```
PCI class 0x0C0010（火线 OHCI 控制器）→ 交给苹果现成的 com.apple.driver.AppleFWOHCI
```

成功的话，整条旧火线栈（IOFireWireNub / AVC / SBP2 / 用户态 IOFireWireLib）原地复活，
后续的用户态 BeBoB 驱动就能像在 Sequoia 上一样跑。

## 前提

1. Mac 已降为 **Reduced Security** 并允许第三方 kext：
   关机 → 长按电源键进恢复模式 → 实用工具-启动安全性实用工具 → 选本系统盘 →
   "降低安全性" → 勾选"允许用户管理来自身份不明开发者的内核扩展"。
2. 已插好转接链：**TB3→TB2 适配器 + TB2→FW800 适配器**（都是苹果原厂件，已停产，二手有）。

## 安装

```bash
cd revive
./install.sh        # 签名 + 拷贝 + kmutil 重建 AuxKC
# 按提示在 系统设置-隐私与安全性 批准（如出现）
sudo reboot
```

## 验证（按顺序）

```bash
ioreg -r -c IOFireWireController        # 应出现 AppleFWOHCI 实例
system_profiler SPFireWireDataType      # 应列出总线与设备
../tools/fwprobe/fwprobe                # 应打出 ADA-8XR 的 GUID 00:11:98:...
```

三条都过 = 复活成功，总线层 OK（**注意：此时仍然没有声音** —— CoreAudio 的火线
音频层从 Ventura 起就没了，复活的只是"路"，不是"车"。车 = 我们要写的 BeBoB 音频驱动）。

## 回滚

```bash
sudo rm -rf /Library/Extensions/FireWireRevive.kext
sudo kmutil install --volume-root / --update-all
sudo reboot
```

## 可能的失败方式（诚实清单）

| 失败点 | 现象 | 对策 |
|---|---|---|
| kmutil 拒收 codeless kext | 安装时报错 | 换路：直接走 ASFireWire（不依赖苹果栈） |
| AppleFWOHCI 起不来 | ioreg 无实例，dmesg/日志有 start 失败 | 同上，放弃此路 |
| 栈起了但用户态接口受限 | fwprobe 看不到设备/属性 | 看 `log show --predicate 'senderImagePath contains "FireWire"'` |
| 升级 macOS 后失效 | 26.7/27 挪走了二进制 | 重新评估，代码可能真被删 |

## 与 ASFireWire 路线的关系

- **本实验（revive）**：赌苹果代码还在 → 代价极小（一个 plist），成了就白赚一条完整的、
  经过 25 年打磨的火线总线栈；败了也没损失。
- **ASFireWire**：社区 DriverKit 从零写的火线栈，不依赖苹果代码，是长期正路。
- 两条路**不冲突**，可以都试：revive 成了 → 用户态驱动方案（免内核、免 SIP）；
  revive 败了 → ASFireWire fork + 设备条目。
