# Troubleshooting — everything we actually hit during bring-up

Ordered by how likely you are to meet it.

## 1. "No sound" but the ADA-8XR's meters are moving

The stream is fine; the device's internal routing isn't. Checklist (see also INSTALL.md §5):

- [ ] Listening on the **main analog outputs 1–2**, or via the **Monitor section**? The latter additionally needs Path 2 selected + monitor level up in the Monitor panel.
- [ ] Path 2 (D/A path) source = the FireWire module's slots (front panel, Mimic/Path selection).
- [ ] Analog output levels (software-controlled) not at −∞/minimum.
- [ ] Fastest reset: load a **factory preset** (Bank + Store → Factory Store → Enter/Accept).

If the meters are **not** moving while `system_profiler SPAudioDataType` shows the device and playback runs, see #4.

## 2. Can't find where to approve the driver extension (macOS 26)

The approval prompt is **not** the Privacy & Security banner (that's macOS ≤14 lore). Go to:

**System Settings → General → Login Items & Extensions → scroll to the very bottom → Driver Extensions.**

The page is long — the section sits below the whole "Allow in the Background" list. Enable the ASFW toggle there (password/Touch ID). If a "Privacy & Security" banner also exists, using it works too, but don't wait for one.

Confirm with `systemextensionsctl list | grep net.asfw.driver` → state must read `[activated enabled]`.

## 3. `fwprobe` / `ada8xrctl` report "no FireWire controller / no devices"

**Expected under ASFireWire.** These tools enumerate the classic Apple kernel classes (`IOFireWireController`, `IOFireWireNub`, `IOFireWireAVCUnit`), which a DriverKit userspace stack does not publish. They are useful on macOS ≤15, or on Tahoe with the `revive/` kext loaded.

On the ASFW route, verify with:

```bash
system_profiler SPAudioDataType | grep -A5 Dream
ioreg -l | grep -c ASFWDriver          # > 0 = dext attached
```

## 4. The audio device disappears right after first playback starts

Observed once during bring-up: the first stream start triggered a DriverKit extension restart — the CoreAudio device vanished, `systemextensionsctl list` showed a `[terminating for upgrade via delegate]` line, and everything came back by itself within seconds (device re-appeared, dext re-attached). Subsequent streams were stable through 30 s of 3-second-interval monitoring with zero dropouts.

Action: wait ~15 s, re-check `system_profiler SPAudioDataType`. If it does not come back, replug the FireWire chain (bus reset) and re-check. File what you see — this is exactly the kind of datapoint ASFW upstream wants.

A stability watch loop, if you want to reproduce/verify:

```bash
afplay /tmp/tone.wav &
for i in $(seq 1 10); do
  sleep 3
  echo "t=$((i*3))s dev=$(system_profiler SPAudioDataType | grep -c Dream) \
dext=$(ioreg -l | grep -c ASFWDriver)"
done
```

Test-tone generator (48 kHz stereo, −20 dBFS, no deps):

```bash
python3 - <<'EOF'
import wave, math, struct
f = wave.open('/tmp/tone.wav','w'); f.setnchannels(2)
f.setsampwidth(2); f.setframerate(48000)
for i in range(48000*30):
    v=int(3000*math.sin(2*math.pi*440*i/48000)); f.writeframes(struct.pack('<hh',v,v))
f.close()
EOF
```

## 5. `clang`/`codesign` fail with "You have not agreed to the Xcode license agreements"

Xcode is installed but its license wasn't accepted, and `xcode-select` points at Xcode. Two fixes:

- `sudo xcodebuild -license` (interactive, needs your password), **or**
- prefix builds with `DEVELOPER_DIR=/Library/Developer/CommandLineTools` — Command Line Tools alone compiles `fwprobe`, `ada8xrctl`, `setout` and runs ASFW's `install.sh` (including its ad-hoc `codesign`) just fine.

## 6. `install.sh` fails at "System Integrity Protection is not disabled"

The installer hard-stops before touching anything if SIP is on — by design. Do INSTALL.md §1 (Recovery mode, `csrutil disable`), reboot, rerun.

## 7. No FireWire chip on PCI (`ioreg … | grep 0c0010` empty)

- Adapter chain not fully seated, or a hub that isn't actually Thunderbolt. A plain USB-C hub cannot carry FireWire — the controller needs PCIe over Thunderbolt. The OHCI chip appears on PCI only when the Thunderbolt side enumerates; check **System Information → Thunderbolt** — the two adapters (Route A) or the dock (Route B) should be listed there.
- On macOS 26 the FireWire *page* of System Information is gone; that is expected and meaningless here.

## 8. Device present but sample rate won't switch / stays at one rate

Untested territory as of writing — the device reports 32k–96k formations and 48k playback is verified. If a switch misbehaves, capture `ioreg -l | grep -A3 ASFWRateFormations` before/after and compare; the BeBoB rate-set command (`avc 01 FF 18 00 …`, see `driver/ada8xrctl.c` header) is what would be sent on the classic stack.

## 9. Reboot survival

Not yet validated across reboots as of writing. Expectations: SIP stays off, the dext stays registered, ASFW.app auto-starts the streams (`ASFWAutoStartAudioStreams=Yes`). If the device is missing after a reboot, launch ASFW.app and re-check; replug the chain as a last resort.
