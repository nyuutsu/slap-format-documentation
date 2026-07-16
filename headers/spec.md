# ROM headers and byte orders — layouts

The file-level envelopes this document covers come in three shapes: a fixed-size block prepended to the ROM body (iNES, fwNES, LNX, A78, SNES copier, SMD), a rewrite of that block at fixed size (iNES 1.0 ↔ NES 2.0), and a whole-body permutation with no prepend at all (N64 byte orders, SMD interleave). Layouts are restated in our own words from the sources named per section; quoted fragments are verbatim.

## iNES (NES, 16 bytes)

Magic `4E 45 53 1A` ("NES" + MS-DOS EOF). File order after the header: optional 512-byte trainer, PRG ROM, optional CHR ROM, optional PlayChoice-10 data.

| Byte | Meaning |
|------|---------|
| 0-3 | magic |
| 4 | PRG ROM size in 16 KB units |
| 5 | CHR ROM size in 8 KB units; 0 means the board uses CHR RAM (no CHR section) |
| 6 | flags: mirroring, battery, trainer (bit 2: 512-byte trainer between header and PRG), mapper low nibble |
| 7 | flags: VS Unisystem, PlayChoice-10 (bit 1: 8 KB hint-screen data after CHR), NES 2.0 identifier (bits 2-3), mapper high nibble |
| 8-10 | rarely-used extensions (PRG-RAM size, TV system) |
| 11-15 | unused padding, should be zero |

Fields coupled to the body's size and shape: bytes 4 and 5, the trainer bit, and the PC10 bit.

Ripper garbage: old tools wrote signature strings across bytes 7-15 — commonly `DiskDude!`, "which results in 64 being added to the mapper number" (the stamp's first byte lands on byte 7, whose upper nibble is the mapper high nibble). The community rule of thumb: if the last 4 bytes are not all zero and the header is not NES 2.0, mask the mapper's upper nibble or refuse.

Source: nesdev wiki, https://www.nesdev.org/w/index.php?title=INES&oldid=23194. The 16-byte strip rule is also defined by NINJA2 (`../ninja2/upstream/ninja2-convroms.txt`).

## NES 2.0 (NES, 16 bytes, same magic)

Identified by `(header[7] & 0x0C) == 0x08`. Bytes 0-7 keep their iNES meanings; bytes 8-15 are redefined: byte 8 mapper bits 8-11 + submapper, byte 9 low/high nibbles are the PRG/CHR size MSBs (an exponent-multiplier notation applies when an MSB nibble is $F), bytes 10-15 carry RAM sizes, timing, console type, miscellaneous ROMs, and default expansion device.

Downgrade to iNES 1.0 (derived from the identification rule and the variant table; field-verified — see `findings.md`): clear byte 7 bits 2-3 and zero bytes 8-15. Only meaningful when the mapper fits 8 bits and the PRG/CHR sizes fit their one-byte LSBs; the information in bytes 8-15 is lost.

Source: nesdev wiki, https://www.nesdev.org/w/index.php?title=NES_2.0&oldid=23808.

## fwNES (FDS, 16 bytes)

Magic `46 44 53 1A` ("FDS" + EOF); byte 4 is the number of disk sides; bytes 5-15 are zero padding. The body is exactly 65500 bytes per side, so the side count is derivable from the body size. Headerless `.fds` images are explicitly acknowledged: "Some .FDS images may omit the header."

Source: nesdev wiki, https://www.nesdev.org/w/index.php?title=FDS_file_format&oldid=22826.

## LNX (Atari Lynx, 64 bytes)

The layout is the `LYNX_HEADER` struct in `upstream/libretro-handy-cart.h` (the format was created for the Handy emulator, so its source is the defining document):

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | magic `LYNX` |
| 4 | 2 | page_size_bank0 (little-endian) |
| 6 | 2 | page_size_bank1 (little-endian) |
| 8 | 2 | version — Handy requires 1 |
| 10 | 32 | cartname |
| 42 | 16 | manufname |
| 58 | 1 | rotation: 0 none, 1 left, 2 right |
| 59 | 1 | aud_bits |
| 60 | 1 | eeprom |
| 61 | 3 | spare |

A bank's byte size is its page size × 256 (Handy's cart.cpp maps page sizes 0x100/0x200/0x400/0x800 to 64K/128K/256K/512K banks), so the expected body size is `(page_size_bank0 + page_size_bank1) × 256`. Those two fields are the size-coupled ones; cartname, manufname, and rotation are free metadata. Handy-0.90-era tools treat bytes 59-63 as opaque spare. Headerless Lynx images circulate as `.lyx`.

## A78 (Atari 7800, 128 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 1 | header version (current spec is v4) |
| 0x01 | 16 | magic text `ATARI7800` (space-padded) |
| 0x11 | 32 | cart title — free metadata |
| 0x31 | 4 | ROM size without header — the size-coupled field |
| 0x35 | 2 | cart type |
| 0x37, 0x38 | 1 each | controller 1 / controller 2 |
| 0x39 | 1 | TV type |
| 0x3A | 1 | save device |
| 0x3F | 1 | slot passthrough device |
| 0x40+ | | v4 mapper, mapper options, audio, interrupt |
| 0x64 | 28 | trailing magic text — `ACTUAL CART DATA STARTS HERE` per the v1 document |

Unverified as of this writing: the endianness of the 4-byte size field, and whether the current spec's trailing magic matches the v1 wording. Both need to be read directly from the spec before an implementation hard-codes them.

Source: A78 Header Specification, http://7800.8bitdev.org/index.php/A78_Header_Specification (v4, living spec); v1 corroboration at https://sites.google.com/site/atari7800wiki/a78-header.

## SNES copier (512 bytes)

There is no universal magic — detection is size-shaped (a headered file's size leaves 512 after mod 32 KiB). Specific copiers do carry marks:

- SWC / Super Magicom: bytes 0-1 = body size in 8 KiB units, low byte first; byte 2 = emulation-mode flags; bytes 8/9 = `AA`/`BB`; byte 10 = file type (`04` = SWC/SSM game); bytes 3-7 and 11-511 should be zero.
- Pro Fighter (.fig): byte 3 = `80` for HiROM, `00` for LoROM; bytes 4-5 carry DSP/SRAM emulation codes.
- Game Doctor SF3 / Super UFO / NSRT write recognizable text (`GAME DOCTOR SF 3` at 0, `SUPERUFO` at 8, `NSRT` at 0x1E8, per NINJA2's conversion notes).

Modern tools treat copier-header contents as metadata for the original copier device and generally ignore them. Whether an all-zeros header is accepted everywhere is not asserted by any written source we found; verify empirically before relying on it.

Sources: `../ninja2/upstream/ninja2-convroms.txt` (tracked); https://wiki.superfamicom.org/super-wild-card; uCON64's `src/console/snes.c` (GPL) constructs both SWC and FIG headers and cross-checks the above.

## N64 byte orders (whole-body permutations)

| Name | Order | First 4 bytes | Transform to native | Constraint |
|------|-------|---------------|---------------------|------------|
| z64 (native) | ABCD | `80 37 12 40` | identity | |
| v64 (byteswapped) | BADC | `37 80 40 12` | swap bytes within each 16-bit unit | file size even |
| n64 (little-endian, rare) | DCBA | `40 12 37 80` | reverse bytes within each 32-bit unit | file size divisible by 4 |

Detect by leading bytes, never extension — `.n64` has historically meant both byteswapped and little-endian files.

Sources: http://n64dev.org/romformats.html (fetch over plain HTTP; their HTTPS certificate is broken); mupen64plus-core `src/main/rom.c` (the three signatures and both swap loops, with the size constraints). NINJA2's conversion notes cover the z64/v64 pair only.

## SMD (Genesis/Mega Drive, 512-byte header + 16 KiB block interleave)

Header, for 68000 program files: byte 0 = file size in 16 KiB blocks; byte 1 = `03`; byte 2 = split-file flag (`00` single or last file, `40` earlier part of a split set); bytes 8/9 = `AA`/`BB`; byte 0xA = `06`; everything else zero.

Interleave: each 16 KiB block after the header splits the decoded bytes by parity. The implementation consensus (0-based, `i` in 0..8191):

```
decoded[2*i]     = block[8192 + i]
decoded[2*i + 1] = block[i]
```

**Erratum, load-bearing:** prose sources state the opposite parity — including the pseudocode in Charles MacDonald's own smdform.txt — but MAME, Genesis Plus GX, PicoDrive, and blastem all implement the rule above, and MAME's GoodGEN-derived detection heuristic (which finds the `SEGA` marker's bytes split across the two halves) only works under the rule above. Follow the implementations, not the prose.

Sources: smdform.txt and smdtech.txt by Charles MacDonald, via Wayback (http://web.archive.org/web/20140723052248/http://cgfm2.emuviews.com/txt/smdform.txt, http://web.archive.org/web/20160404052849/http://cgfm2.emuviews.com/txt/smdtech.txt); MAME `src/devices/bus/megadrive/md_slot.cpp` (BSD-3-Clause) for the transform.
