#!/bin/bash
# preflight — one-shot health check: how far are you from sound?
# Exit codes: 0 = audio device present; 1 = something still missing.
cd "$(dirname "$0")"

echo "======== ADA-8XR FireWire on macOS — preflight ========"

FAIL=0

echo -n "[1] SIP: "
SIP=$(csrutil status 2>&1)
if echo "$SIP" | grep -q "disabled"; then
    echo "disabled ✓ (required for the experimental dext)"
elif echo "$SIP" | grep -q "enabled"; then
    echo "ENABLED ✗ — Recovery mode → Terminal → csrutil disable → reboot"
    FAIL=1
else
    echo "unknown: $SIP"
fi

echo -n "[2] FireWire OHCI controller (PCI class 0c0010): "
FWPCI=$(ioreg -r -c IOPCIDevice 2>/dev/null | grep -c '"class-code" = <10000c00>')
if [ "$FWPCI" -gt 0 ]; then
    echo "present ✓ (adapter chain enumerated)"
else
    echo "not found — connect TB3→TB2→FW800 chain, power on the ADA-8XR"
    FAIL=1
fi

echo -n "[3] ASFW driver extension: "
if systemextensionsctl list 2>/dev/null | grep -q "net.asfw.driver.*activated enabled"; then
    echo "activated enabled ✓"
elif systemextensionsctl list 2>/dev/null | grep -q "net.asfw.driver.*waiting for user"; then
    echo "WAITING FOR APPROVAL ✗ — System Settings → General → Login Items &"
    echo "    Extensions → scroll to the very bottom → Driver Extensions → enable ASFW"
    FAIL=1
elif [ -d /Applications/ASFW.app ]; then
    echo "app installed but dext not registered — run ASFW's install.sh, then approve"
    FAIL=1
else
    echo "not installed — get it from https://github.com/mrmidi/ASFireWire/releases"
    FAIL=1
fi

echo -n "[4] Classic Apple FW stack (revive route only): "
if ioreg -r -c IOFireWireController 2>/dev/null | grep -q AppleFWOHCI; then
    echo "alive ✓ (fwprobe/ada8xrctl usable)"
else
    echo "not active (normal unless the revive kext is loaded)"
fi

echo -n "[5] Toolchain: "
if CLANG=$(xcrun --find clang 2>/dev/null) || CLANG=$(DEVELOPER_DIR=/Library/Developer/CommandLineTools xcrun --find clang 2>/dev/null); then
    echo "clang ✓"
else
    echo "missing — run: xcode-select --install"
fi

echo -n "[6] CoreAudio device: "
AUDIO=$(system_profiler SPAudioDataType 2>/dev/null)
if echo "$AUDIO" | grep -q "Transport: FireWire"; then
    echo "$AUDIO" | grep -B1 "Transport: FireWire" | head -1 | sed 's/^ *//' | tr -d ':'
    echo "    ✓ FIREWIRE AUDIO DEVICE PRESENT — you're done. Select it as output."
    exit 0
else
    echo "no FireWire audio device yet — walk steps 1-3 above (docs/INSTALL.md)"
    exit 1
fi
