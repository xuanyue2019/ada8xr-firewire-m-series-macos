# Linux kernel reference sources (GPL-2.0)

The files in this directory are **verbatim copies** of Linux kernel sources,
kept for offline protocol reference only. They are **not compiled into
anything** in this repository and carry **no code relationship** to the MIT-
licensed tools outside this directory.

| File | Upstream |
|---|---|
| `fcp.c`, `fcp.h`, `lib.c` | `sound/firewire/fcp.c`, `fcp.h`, `lib.c` |
| `cmp.c` | `sound/firewire/cmp.c` |
| `bebob.c`, `bebob_stream.c` | `sound/firewire/bebob/bebob.c`, `bebob_stream.c` |
| `amdtp-am824.c` | `sound/firewire/amdtp-am824.c` |

Source: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/sound/firewire/

These files are licensed under the **GNU General Public License version 2**
and are redistributed here in unmodified form with their original copyright
and license headers intact, per that license. The protocol constants used by
this project's own tools (`driver/bebob.h`) were cross-checked against these
files.
