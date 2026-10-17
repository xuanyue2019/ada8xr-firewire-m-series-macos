# Field notes — API archaeology & project decisions

Technical notes accumulated while building this project. Kept because they save
the next person from re-deriving them. (Historical note: the decisions below
were locked in on 2026-10-10 and validated by a successful bring-up on
2026-10-17.)

## The three cornerstones

1. **Official path is dead.** The ADA-8XR FW card on Mac never had a Prism-written driver — it used Apple's CoreAudio-over-FireWire. Ventura (macOS 13) removed the audio layer; Tahoe (26) removed even the OHCI binding (see RESEARCH.md §③ for the forensic details: `AppleFWOHCI.kext` 5.7.6 was still compiled against the 26.6 SDK in 2025, but its `IOKitPersonalities` were deleted). Official ceiling: macOS 12.6.
2. **Protocol needs no reverse engineering.** Standard BridgeCo BeBoB — vendor `0x001198`, model `0x0000ada8`, Linux `snd-bebob` `spec_normal`, zero quirks. The same vendor's Orpheus (`0x00010048`) *does* carry BeBoB v3 DBC quirks (`CIP_EMPTY_HAS_WRONG_DBC`, commit `c4d860a0d256`) — same factory, so watch for DBC weirdness on the ADA-8XR if periodic clicks show up.
3. **The foundation exists.** ASFireWire (Apache-2.0) = community PCIDriverKit OHCI stack. Out-of-catalog BeBoB devices fall into the GenericAvc family automatically; adding a proper entry is ~4 file changes (see [ASFW_UPSTREAM_PATCH.md](ASFW_UPSTREAM_PATCH.md)).

## IOFireWireLib API archaeology (for classic-stack tooling)

Pitfalls found while writing `driver/fwio.c` against `IOKit/firewire/IOFireWireLib.h`:

- **The plugin factory UUID is not exported by any header.** `IOFireWireLib.plugin`'s Info.plist registers exactly one factory: `A1478010-F197-11D4-A28B-000502072F80`. Build it yourself with `CFUUIDGetConstantUUIDWithBytes`.
- **`kIOFireWireLibVersion` does not exist.** The third argument to `IOCreatePlugInInterfaceForService` is the *factory UUID*, not a version constant.
- **`FWAddress` is three fields** — `{nodeID, addressHi, addressLo}` (`IOFireWireFamilyCommon.h` ~line 385). The node ID lives *inside* the struct; forgetting it sends your transaction to the local node.
- **The AV/C unit interface's real type name is `IOFireWireAVCLibUnitInterface`** (an anonymous typedef in the header). You must call `open()` on the unit interface before `AVCCommand` works.
- **The user-space CompareSwap does not return the old value** (unlike the kernel variant). If you need it, follow the swap with a read.

## macOS kext loading on Apple Silicon (2026, still valid)

Recovery mode → Startup Security Utility → Reduced Security + "allow user-managed kexts from unidentified developers" → ad-hoc sign (`codesign -s -`) → copy to `/Library/Extensions` → `sudo kmutil install --volume-root /` → approve in System Settings → reboot. This is what `revive/install.sh` automates.

## Tahoe's "removal" is entry-point deletion, not code deletion

`IOFireWireFamily.kext` (4.8.3) and `AppleFWOHCI.kext` are still in the kernel collection on macOS 26.6 — only the PCI matching personalities were stripped. That's the entire premise of `revive/` (re-add the match → the dormant stack wakes), and it's why that route is a cheap gamble rather than a rewrite.

## What actually happened at bring-up (calibration data)

Predictions made beforehand vs. outcome:

| Prediction | Outcome (2026-10-17) |
|---|---|
| ASFW installs and lights up the bus (~85%) | ✅ first try |
| ADA-8XR enumerates (~75%, spec_normal but ~20-year-old firmware) | ✅ first try, via GenericAvc |
| Sound the same day (~40%, audio layer marked experimental) | ✅ same evening |
| Eventual success incl. patches/route changes (~85%) | ✅ not even needed |

The only real fight was macOS 26's relocated dext-approval UI (TROUBLESHOOTING.md #2) and the device's own routing matrix — neither was a driver problem.

## Isochronous stability outlook

Engineering polish, not a dead end: 44.1/48k playback was stable on day one; long-run/full-duplex/96k is expected to track upstream ASFW releases. Bandwidth is a non-issue (8×8 @192k ≈ 26% of S400; OHCI moves packets with autonomous DMA). Two reference implementations exist to diff against (Linux `amdtp`, the classic Apple stack). If ASFW stalls and the `revive/` route works, the Apple stack's isochronous path is a generation more battle-tested.

## Misc environment notes

- Fetching kernel.org / raw.githubusercontent can be flaky from some networks; the `api.github.com` contents endpoint + base64 decode is a reliable fallback.
- dext compilation needs full Xcode; the *tools* in this repo need only Command Line Tools (`xcode-select --install`).
- The ADA-8XR FW port is FW400 6-pin; the chain is TB3→TB2→FW800→FW400.
