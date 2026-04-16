# APS (N64)

The "Advanced Patching System" for Nintendo 64 ROMs. Created by Silo
and Fractal of Blackbag, an N64 demoscene/homebrew group, in July
1998. Distributed as `bb-aps12.zip` on dextrose.com (the central hub
of the Doctor V64 / CD64 backup copier scene).

Byte-replacement format with optional run-length encoding and
N64-specific source verification (cart ID, country code, CRC). Solved
IPS's 16MB addressing limit by using 4-byte offsets (claims support
up to 2GB).

**Completely unrelated** to APS GBA despite the shared extension.
APS N64 uses `APS10` as its magic; APS GBA uses `APS1`. The formats
share nothing else.

## Files

- `spec.md` — the on-disk structure, reflecting the recovered C source
  (not the spec text in `upstream/aps.txt`, which has byte-range typos)
- `proposal.md` — slap's design decisions for APS N64: edge case
  handling, byte-order support (Z64/V64/.n64), type 0 vs type 1 logic
- `upstream/` — the complete bb-aps 1.2 archive and its extracted
  contents. This is the authoritative reference for the format.

## The recovered archive

`bb-aps12.zip` was retrieved from the icequake.net mirror of
dextrose.com at:

    http://n64.icequake.net/mirror/www.dextrose.com/files/n64/ips_tools/bb-aps12.zip

The mirror has an expired TLS certificate and may not remain
accessible. We preserve the archive here against that risk.

The archive contains:

- `aps.txt` — the format specification. Has typos in the byte ranges
  of the N64-specific header (claims 6 bytes of padding + 5 bytes of
  size, but the code does 5 + 4). Trust the source code over this.
- `n64aps.c` — apply tool source. This is the authoritative reference
  for how APS N64 apply behavior works.
- `n64caps.c` — create tool source. Authoritative for creation.
- `n64aps.exe` / `n64caps.exe` — compiled DOS/Win32 binaries.
- `readme.txt` — user-facing readme including the spec verbatim.
- `makefile`, `file_id.diz`, `BLACKBAG.NFO` — build rules, BBS
  archive description, group nfo.

## The Blackbag scene

Blackbag was an N64 demoscene/homebrew group active in 1998. Known
members: Stumble, Ste, Silo, Shroomz, Nagra, McBain, Jovis, Fractal,
Datawiz, Breakpoint, Snake. They placed 3rd at Presence of Mind '98
with a demo called *Psychodelic* (code by Ste, graphics by Fractal);
McBain and Snake's Game Boy emulator for N64 placed 2nd at the same
competition. Silo and Fractal co-authored APS.

Contact addresses embedded in the spec: `silo@blackbag.org` and
`fractal@blackbag.org`.

## Known format issues

- **The spec text has errata.** `upstream/aps.txt` claims the padding
  field spans bytes 69-74 (6 bytes) and the destination size spans
  75-79 (5 bytes). The actual C source does 5 bytes of padding at
  69-73 and 4 bytes of size at 74-77. Our `spec.md` documents what
  the code does.

- **Type 0 (Simple) is in the spec but not implemented.** `n64aps.c`
  rejects any patch that isn't type 1. `n64caps.c` is hardcoded to
  emit type 1. RomPatcher.js and slap support type 0 but bb-aps
  doesn't.

- **The tool only knows about Z64 and V64 byte orders.** The third
  common N64 byte order (.n64, 32-bit swapped, little-endian) isn't
  recognized. slap handles it by normalizing internally.

- **Description field encoding is unspecified.** The format stores 50
  raw bytes space-padded with no encoding declaration. bb-aps uses
  system locale; slap passes bytes through.
