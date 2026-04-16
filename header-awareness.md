# Header Awareness — Cross-Format Design Note

## Problem

Some ROM platforms prepend headers that shift every byte offset:

| Platform | Header | Size | Status |
|----------|--------|------|--------|
| NES | iNES / NES 2.0 | 16 bytes | universal, load-bearing |
| SNES | copier (SWC, Pro Fighter) | 512 bytes | dead, No-Intro strips |
| FDS | fwNES | 16 bytes | common but not universal |
| Genesis/MD | SMD copier | 512 bytes | dead |

A patch created against a headered ROM writes to wrong offsets when
applied to a headerless ROM, and vice versa. This is not a CRC
mismatch — the output is silently corrupt. The header bytes
themselves are irrelevant to the diff; the problem is purely that
offset 0 in the patch maps to different ROM data depending on
whether a header is present.

## Affected formats

Every offset-based format is affected: IPS, IPS32, BPS, UPS, PPF,
xdelta1, VCDIFF, BSDiff, GDIFF, etc. The issue is not
format-specific.

Platform-aware formats (NINJA1, RUP/ninja2) have built-in header
handling — they tag the ROM platform and define per-platform
normalization steps. For these formats, slap already has the
infrastructure (though implementation is currently `raw`-only per
README).

## Real-world impact

Surfaced during romhacking.net corpus research:

- **UPS**: ff2iset ships separate `(Header).ups` and `(No Header).ups`
  for the same hack. The format can't express "strip 512 bytes first."
- **BPS**: NES patches break when No-Intro migrates dumps from iNES
  1.0 to NES 2.0 headers (same ROM data, different header bytes,
  CRC changes).
- **xdelta1**: not observed in corpus, but same structural problem
  applies.

## Proposed feature

A flag on `slap apply` and `slap create`:

```
--header-offset N
```

Meaning: "the first N bytes of this file are a header; they are not
part of the patchable data."

### Apply behavior

1. Read ROM file
2. Split at offset N: header = first N bytes, body = rest
3. Run CRC/MD5/size checks against body (not the full file)
4. Apply patch to body
5. Prepend header to patched body
6. Write output

### Create behavior

1. Read both ROMs
2. Split both at offset N
3. Diff the bodies
4. Emit patch targeting the body

### Platform sugar (optional)

```
--header-offset nes     →  --header-offset 16
--header-offset snes    →  --header-offset 512
--header-offset fds     →  --header-offset 16
--header-offset genesis →  --header-offset 512
```

Header sizes are fixed per platform. No platform has variable-size
external headers.

### Auto-detection (optional, advisory)

slap could detect headers and advise:

- NES: file starts with `NES\x1a` → "this ROM has a 16-byte iNES header"
- SNES: `file_size % 1024 == 512` → "this ROM may have a 512-byte copier header"
- FDS: file starts with `FDS\x1a` → "this ROM has a 16-byte fwNES header"

When CRC mismatches, slap could suggest: "CRC mismatch — try
`--header-offset 16`?" This is heuristic, not automatic.

### What the flag does NOT do

- It does not examine or validate header contents
- It does not add headers that aren't there (if N > file size, error)
- It does not auto-strip (always explicit)

### Priority

Low. The romhacking community has worked around this for decades by
distributing separate patches and documenting which dump to use.
RomPatcher.js exposes a "simulate header" checkbox; most users never
touch it. Format-specific bugs (xdelta1 sequential offsets, etc.)
are higher priority.
