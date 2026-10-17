#!/bin/bash
# FireWireRevive 安装脚本 —— 前提：已按 INSTALL.md 把启动安全性降为 Reduced Security
# 并勾选"允许用户管理来自身份不明开发者的内核扩展"。
set -euo pipefail
cd "$(dirname "$0")"

echo "[1/4] ad-hoc 签名"
codesign -s - --force --timestamp=none FireWireRevive.kext

echo "[2/4] 安装到 /Library/Extensions"
sudo mkdir -p /Library/Extensions
sudo rm -rf /Library/Extensions/FireWireRevive.kext
sudo cp -R FireWireRevive.kext /Library/Extensions/

echo "[3/4] 重建 AuxKC"
sudo kmutil install --volume-root / --update-all

echo "[4/4] 完成。"
echo "  → 如果终端提示需要在 系统设置-隐私与安全性 里批准，先批准；"
echo "  → 重启后运行验证： ioreg -r -c IOFireWireController   （应看到 AppleFWOHCI 实例）"
echo "  →              以及 ../tools/fwprobe/fwprobe           （应看到 ADA-8XR 的 GUID）"
