# Installation — verified 2026-10 on an Apple M-series Mac (M4 Max, macOS 26 Tahoe)

End-to-end setup for the ADA-8XR FireWire card on **Apple M-series (Apple Silicon) Macs** via [ASFireWire](https://github.com/mrmidi/ASFireWire). Verified with `ASFW-0.5.0-beta.1` on an M4 Max running macOS 26.6.

If anything misbehaves, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — it is distilled from a real bring-up session and covers every failure we actually hit.

## 0. Hardware

Two ways onto the FireWire bus:

- **Route A — Apple adapter chain** (what this guide was verified with): TB3 (USB-C)→TB2 adapter (A1621), TB2→FW800 adapter (A1463), then a FW800 (9-pin)→FW400 (6-pin) cable. Both adapters are discontinued; find them second-hand.
- **Route B — one box**: a Thunderbolt dock with a built-in FW800 port (several OWC and Sonnet models have one) plus the same FW400 cable. It must be a Thunderbolt dock, a plain USB-C hub won't work — the FireWire controller needs PCIe over Thunderbolt. This route is expected to behave identically (the same PCI OHCI controller class appears on the bus), but the adapter chain is what has been verified here.

Either way the far end is `FW400 cable → ADA-8XR`, ADA-8XR powered on (the FW card lives in a rear digital I/O slot). Monitors/headphones connect to the ADA-8XR's **analog outputs** — sound comes out of the converter, not the Mac.

## 1. Disable SIP

Unsigned experimental DriverKit extensions require it.

1. Shut down → press and hold the power button until **Startup Options** appear (Apple Silicon).
2. Options → Utilities → Terminal → run `csrutil disable` → reboot.

Keep SIP disabled for as long as you use ASFW. Re-enable with `csrutil enable` when you retire the chain (same Recovery-mode procedure). Understand that a disabled SIP lowers system security.

## 2. Install ASFireWire

1. Download the release ZIP from <https://github.com/mrmidi/ASFireWire/releases> (verified: `ASFW-0.5.0-beta.1`).
2. Extract, then run the installer **from the extracted folder**:

   ```bash
   cd ASFW-0.5.0-beta.1/ASFW-0.5.0-beta.1
   ./install.sh
   ```

   The script checks SIP, enables system-extension developer mode, ad-hoc signs the dext and app, installs to `/Applications`, and launches ASFW.app. No Apple Developer account needed. `/Applications` is admin-writable, so no sudo prompt in the usual case.

   > Xcode license quirk: if `codesign`/`clang` fail with *"You have not agreed to the Xcode license agreements"* while you have Xcode installed but Command Line Tools working, prefix the command with `DEVELOPER_DIR=/Library/Developer/CommandLineTools` — the whole install then runs on CLT alone.

## 3. Approve the driver extension

The dext activation request fires as soon as ASFW.app launches, but macOS requires your approval. **On macOS 26 the approval lives in a different place than older guides suggest:**

**System Settings → General → Login Items & Extensions → scroll all the way down → Driver Extensions → enable the ASFW entry** (password/Touch ID to confirm).

It is *not* the yellow "system software was blocked" banner on the Privacy & Security page (that's where macOS ≤14 put it), and the page is long — the Driver Extensions section is at the very bottom, below the "Allow in the Background" list.

Verify from a terminal:

```bash
systemextensionsctl list | grep net.asfw.driver
#   *  *  -  net.asfw.driver (0.5.0/14)  ASFireWire Driver  [activated enabled]
```

`[activated waiting for user]` means the approval step above hasn't happened yet.

## 4. Plug in and verify enumeration

With the chain connected and the ADA-8XR on:

```bash
ioreg -r -c IOPCIDevice | grep -i 0c0010     # FireWire OHCI controller present
system_profiler SPAudioDataType              # look for the device
```

Expected device entry:

```
Dream ADA-8:
    Input Channels: 8
    Manufacturer: ASFireWire
    Output Channels: 8
    Current SampleRate: 48000
    Transport: FireWire
```

"Dream ADA-8" is the generic-name rendering until the upstream catalog patch ([ASFW_UPSTREAM_PATCH.md](ASFW_UPSTREAM_PATCH.md)) lands. `ioreg -l | grep ASFWDriver` shows the DriverKit stack attached to the controller, including the reported rate formations (32k–96k, 8×8 PCM each).

> `tools/fwprobe` and `driver/ada8xrctl` will **not** see the device under ASFW — they enumerate the classic kernel classes. `system_profiler SPAudioDataType` is the source of truth on this route.

## 5. Route audio and get sound

```bash
make                        # builds setout (and the classic-stack tools)
tools/setout "Dream ADA-8"  # switch the default output device
afplay /System/Library/Sounds/Glass.aiff
```

(or pick the device in System Settings → Sound.)

**No sound but the ADA-8XR's front-panel meters move?** The FireWire stream is working; the converter's internal routing just isn't pointed at the FireWire input. On the ADA-8XR front panel:

- Path 2 (the D/A path) should take its source from the FireWire module's channel slots;
- the analog output levels (software-controlled) should not be at −∞;
- if monitoring via the Monitor section/headphones, select Path 2 there and raise the monitor level;
- quickest fix in practice: load a **factory preset** (Bank + Store → Factory Store → Enter/Accept), which restores the standard "digital in → D/A → analog out" routing, then re-check.

## 6. Uninstall / rollback

```bash
sudo rm -rf /Applications/ASFW.app
systemextensionsctl reset    # clears developer-mode extension registrations
```

Then re-enable SIP via Recovery mode. (ASFW's own README may carry more current uninstall guidance.)
