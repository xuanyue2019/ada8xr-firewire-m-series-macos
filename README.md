# ADA-8XR on Apple M-series Macs

**Use the Prism Sound ADA-8XR FireWire card on Apple M-series Macs (Apple Silicon: M1/M2/M3/M4…) running macOS 26 (Tahoe) — where Apple removed FireWire support entirely.**

Verified working on an **M4 Max** MacBook Pro: the ADA-8XR enumerates as a CoreAudio device (**8 in / 8 out @ 48 kHz over FireWire**, shown as *"Dream ADA-8"*) and plays audio through its analog outputs. No kernel code, no reverse engineering, no Prism Sound software required.

```
M-series Mac (macOS 26 Tahoe) ──Thunderbolt──▶ ASFireWire (DriverKit FW stack)
                                                    │  IEEE 1394
                                                    ▼
                                            ADA-8XR FireWire card
                                            (standard BeBoB protocol)
                                                    │  analog
                                                    ▼
                                              your monitors 🎧
```

## Why this works (and why it was "impossible")

1. **The official path is dead.** On the Mac, the ADA-8XR FireWire card never had a Prism-written driver — it relied on Apple's built-in CoreAudio-over-FireWire. Apple removed that in macOS 13 Ventura and removed even the FireWire bus driver in macOS 26 Tahoe. Prism Sound confirms the FW card tops out at macOS 12. ([docs/RESEARCH.md](docs/RESEARCH.md))
2. **The protocol is not proprietary.** The ADA-8XR FireWire card is a standard **BridgeCo BeBoB** device (AV/C + CMP + AMDTP). Linux's `snd-bebob` has supported it since 2014 — vendor `0x001198`, model `0x0000ada8`, no quirks. Nothing to reverse-engineer.
3. **The missing piece now exists.** [ASFireWire](https://github.com/mrmidi/ASFireWire) (Apache-2.0) is a community DriverKit FireWire stack built precisely because Tahoe killed FireWire. Unknown BeBoB devices fall into its **Generic AV/C** family automatically — the ADA-8XR is recognized with zero code changes.

This repo packages the **verified end-to-end recipe**, the **diagnostic tools**, the **research evidence**, and an **upstream patch sketch** to give the ADA-8XR a proper catalog entry in ASFireWire.

## Status

| Capability | Status |
|---|---|
| Device enumeration (8×8, FireWire) | ✅ verified |
| Playback 44.1/48 kHz → analog outs | ✅ verified |
| Stream stability (30 s+ monitored, no dropouts) | ✅ verified |
| First stream start (one-time dext restart observed) | ⚠️ recovers by itself |
| Input / simultaneous 8×8 recording | ⬜ untested |
| 88.2–192 kHz | ⬜ untested (device reports 32k–96k formations) |
| Long-run (hours) stability | ⬜ untested |

## Quick start

**Hardware required**

| Item | Notes |
|---|---|
| Apple TB3 (USB-C)→TB2 adapter | A1621, discontinued — second-hand |
| Apple TB2→FireWire 800 adapter | A1463, same |
| FW800 (9-pin) → FW400 (6-pin) cable | the ADA-8XR FW card has a 6-pin port |
| ADA-8XR with FireWire card | powered on |

(A Thunderbolt dock with an FW800 port — older OWC/Sonnet TB2/3 models — replaces the two adapters.)

**Steps** (details in [docs/INSTALL.md](docs/INSTALL.md))

1. Disable SIP: Recovery mode → Terminal → `csrutil disable` → reboot. *(Required for unsigned experimental DriverKit extensions — see safety notes below.)*
2. Download [ASFireWire](https://github.com/mrmidi/ASFireWire/releases) (verified with `ASFW-0.5.0-beta.1`), extract, run its `install.sh`.
3. Plug in the adapter chain and power on the ADA-8XR.
4. Approve the driver extension: **System Settings → General → Login Items & Extensions → scroll to the bottom → Driver Extensions → enable ASFW** (on macOS 26 the approval banner is *not* in Privacy & Security — this cost us an evening).
5. Verify: `system_profiler SPAudioDataType` should list **Dream ADA-8** (8 in / 8 out, FireWire).
6. Select it as the output device (`tools/setout "Dream ADA-8"` after `make`, or System Settings → Sound) and play something.
7. No sound but the ADA-8XR's front-panel meters dance? The FireWire side is fine — fix the device's internal routing ([docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)).

## Repository layout

```
├── preflight.sh              one-shot environment check (SIP/FW chip/dext/audio device)
├── Makefile                  builds all tools
├── tools/
│   ├── fwprobe/              FireWire bus prober (classic Apple stack; see caveat)
│   └── setout.c              switch the default CoreAudio output device by name
├── driver/                   user-space BeBoB control-plane console (ada8xrctl)
│   └── ref/                  Linux snd-bebob sources (GPL-2.0, reference only)
├── revive/                   FireWireRevive.kext — alternative "wake Apple's dormant stack" route
└── docs/
    ├── INSTALL.md            step-by-step setup (verified 2026-10)
    ├── TROUBLESHOOTING.md    live-debugging playbook from the bring-up session
    ├── RESEARCH.md           evidence file: official status, protocol, macOS stack forensics
    ├── PLAN.md               phased roadmap with acceptance gates
    ├── NOTES-API.md          IOFireWireLib API archaeology (classic-stack tooling)
    └── ASFW_UPSTREAM_PATCH.md patch sketch: add a proper ADA-8XR entry to ASFireWire
```

## Tools

- **`setout`** — `./setout "Dream ADA-8"`: tiny CoreAudio HAL utility to switch the default output device by name (handy for scripting; no third-party deps).
- **`ada8xrctl`** — BeBoB control console (list devices, dump Config ROM / iMPR / oMPR / PCR, raw AV/C transactions, get/set sample rate). Talks to the **classic Apple FireWire stack** (IOFireWireLib) — relevant on macOS ≤15 or with `revive/` loaded.
- **`fwprobe`** — read-only FireWire bus prober with Config ROM parsing and Prism/BeBoB auto-detection. Same classic-stack caveat.

> **Caveat:** under ASFireWire (the macOS 26 route) `fwprobe`/`ada8xrctl` will always report "no controller" — they enumerate the old kernel classes (`IOFireWireNub` …), which a DriverKit stack does not publish. For verification use `system_profiler SPAudioDataType` or the ASFW app.

## Upstream contribution

`docs/ASFW_UPSTREAM_PATCH.md` contains a ready sketch (~60 lines, four files) to add a first-class **Prism Sound ADA-8XR** entry to ASFireWire's device catalog: proper name, Supported disposition, and a slot for per-device quirks if any show up. Sending this PR upstream makes the device show as "Prism Sound ADA-8XR" instead of the generic name and helps the next person.

## Safety notes

- **SIP stays disabled while using ASFW** (unsigned experimental system extension). This lowers your Mac's defenses — treat the machine accordingly and re-enable SIP (`csrutil enable`) if you stop using the FireWire chain.
- `revive/FireWireRevive.kext` and ASFireWire **claim the same PCI device and must never be active at the same time**.
- `ada8xrctl write` writes FireWire CSRs directly — debug use only.

## Credits & licenses

- [ASFireWire](https://github.com/mrmidi/ASFireWire) by mrmidi and contributors (**Apache-2.0**) — the DriverKit FireWire stack that made all of this possible. Not bundled in this repo; download from its releases.
- Linux kernel `sound/firewire/bebob` (**GPL-2.0**) — the protocol reference implementation; verbatim copies of a few files are kept in `driver/ref/` for offline study, see the notice there.
- Prism Sound ADA-8XR is a product of Prism Media Products Limited. This is an independent community effort, not affiliated with or endorsed by Prism Sound.
- **Tooling**: this project's bring-up session, tools, and documentation were developed with [ZCode](https://z.ai) — the GLM-powered AI coding agent by Z.ai.

This repository's own code and docs are released under the **MIT License** (see [LICENSE](LICENSE)).
