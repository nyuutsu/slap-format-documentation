# UPS format findings

Source: romhacking.net archive (2024-08-01). 124 archives, ~192 `.ups` files. Shortlisted 8 patches for hands-on testing. A curated subset of 25 `.ups` files lives in `roms/curated/ups/`, `roms/curated/ninja2/`, and `test/data/`.

## Provenance

- **slap info/explain/apply**: authoritative for sizes, CRCs, block counts. Confirmed by successful apply + CRC validation.
- **Independent Python wire-format analyzer** (`/tmp/ups_analyze.py`, `/tmp/ups_max_overshoot.py`): does not use slap. Reads varint and walks blocks directly, classifying each block as fits / partial-overshoot / fully-phantom against an arbitrary `output_size`. Used for bulk classification, cross-direction analysis, and per-block-overshoot measurement. CRC32 of every walked patch matches a `zlib.crc32` of the raw file; no drift between slap and the wire.
- **Three encoder cross-check**: byuu's reference encoder source (`tools/byuu.org-beat/nall/ups.hpp`), tsukuyomi v0.01 (`tools/tsukuyomi_v01/tsukuyomi`, disassembled with `objdump -d -M intel`; cannot run on macOS arm64 without qemu + 32-bit GTK chroot), and go-ups (`tools/go-ups/operations/diff.go` + `writer/writer.go`). flips' `ups_create` is unimplemented (returns `ups_broken`).
- The corrected Python analyzer (Convention B varint) matches slap exactly. The initial Convention A scan was wrong and produced wildly inflated sizes; see "Varint decoder incident" below.

> Where slap's strict reading disagrees with every other tool's behavior, the likelier explanation is that slap is too strict, not that the patches are broken.

## Apply results

All tested via `slap apply`:

| Patch | Platform | Blocks | ROM match | Result |
|-------|----------|--------|-----------|--------|
| CFC2English | NGPC | 9,811 | size+CRC | **applied, output CRC verified** |
| gen6Typing | GB | 26 | size+CRC | **applied, output CRC verified** |
| FE1+2_GBA | GBA | 660,119 | size+CRC | **applied, OOB warning** (1 block, step 660118, 1 byte overshoot) |
| crystalleaf | GBC | 25,423 | size+CRC | **applied, OOB warning** (1 block, step 25422, 1 byte overshoot) |
| crystalleaf_alt | GBC | 25,493 | size+CRC | **applied, OOB warning** (1 block, step 25492, 1 byte overshoot). V1.1 ROM. |
| smbs-1.0~rc1 | NES | 1,699 | size+CRC | **applied, OOB warning** (1 block, step 1698, 1 byte overshoot). Growth patch: 41K → 74K. |
| ff2iset | SNES | 1,082 | size+CRC | **applied, output CRC verified** (Rev 1 ROM) |
| vay (2352-sector) | Sega CD | 1 | size+CRC | **applied, output CRC verified** |

**Summary**: 8/8 CRC-matched patches apply successfully (4 clean,
4 with OOB clipping warning). All curated patches verified.

## OOB blocks

There are two structurally distinct OOB phenomena. They look
identical to a direction-blind detector (any block that writes
past `output_size`) but have different causes, different
upper-bound magnitudes, and different fixes.

### Apply-direction: the +1-terminator quirk

byuu's canonical encoder walks `output_length = max(source,
target)`. The inner loop terminates as soon as
`offset >= output_length`, and on that exit path it writes one
synthetic `0x00` without advancing `offset`. The result: when
the last differing byte lands at `output_length`, the wire
carries a final block whose terminator byte sits at
`output_length` rather than `output_length - 1`.

Three encoder implementations confirm this is the only path
producing apply-direction OOB:

- **byuu's beat** (`nall/ups.hpp`): `create()` walks
  `max(source, target)`; on `offset >= output_length` writes
  `0x00` and exits. No multi-block phantom path.
- **tsukuyomi v0.01** (32-bit ELF, 2008): varint encode at
  `0x8050b44` matches `ups.hpp`'s `encode()` instruction-for-
  statement (`and eax, 0x7f`; `shrd eax, edx, 7`;
  `or eax, -0x80` on final byte). Create body at `0x8050cbf`
  walks the same outer/inner loop structure with the identical
  `offset >= output_length -> write 0x00 -> break` exit path.
  Algorithmically equivalent to byuu's reference.
- **go-ups** (`operations/diff.go` + `writer/writer.go`):
  different code structure (linear scan over target, append to
  blocks list, close any open block at end-of-target) but the
  wire output produces the same +1-terminator under the same
  condition.

Per-block-overshoot analysis on the curated 25 patches:

- 21 patches: zero apply-direction OOB.
- 4 patches (crystalleaf, crystalleaf_alt, smbs, FE1+2_GBA):
  every OOB block overshoots by **exactly 1 byte**, regardless
  of the block's length. FE1+2_GBA's OOB block is 1024 bytes
  long; only its terminator (the last byte) is past
  `target_size`.
- **Maximum apply-direction single-block overshoot anywhere: 1
  byte.** This matches the theoretical upper bound from the
  encoder algorithms above.

Growth is not a prerequisite. FE1+2_GBA and smbs are growth
patches; crystalleaf is same-size; all four exhibit the same
+1-byte quirk.

### Phantom-tail: writes-the-smaller-side

Because byuu's `create()` walks `max(source, target)`, the
block stream describes positions up to the **larger** of the
two sides. When the walker on the apply/undo side writes the
**smaller** side, every block whose declared output position
lies past the smaller side's EOF is structurally OOB.

This is symmetric across direction:

- **Undoing a growth patch** (target > source): the undo
  walker writes the source size; blocks describing target-side
  positions past `source_size` are phantom.
- **Applying a shrink patch** (target < source): the apply
  walker writes the target size; blocks describing source-side
  positions past `target_size` are phantom.

It is **not** an undo-only phenomenon. The original framing
that put OOB squarely on the undo path missed that the same
structural rule produces it in apply direction whenever the
target is the smaller side.

Magnitudes from the patches we have (undo direction; no shrink patches
exist among them to exercise the apply-of-shrink direction):

- **smbs undone** (target 73,744 → source 40,976): 897 OOB
  blocks; max single-block overshoot **8,222 bytes** (block 803
  starts 1 byte past `source_size` and spans 8,222 bytes
  entirely past).
- **FE1+2_GBA undone** (target ~33 MB → source 16 MB): 621,580
  OOB blocks; max single-block overshoot **593,563 bytes**
  (block 566,135 entirely past `source_size`).

For comparison, the SMBS patch in each direction:

| direction | output_size | fits-fully | partial OOB | fully phantom | total overshoot |
|-----------|-------------|------------|-------------|---------------|-----------------|
| apply     | 73,744      | 1,698      | 1           | 0             | 1 byte          |
| undo      | 40,976      | 802        | 1           | 896           | 32,769 bytes    |

### Contiguous-tail property

OOB blocks are always a contiguous tail in either direction:
once a block fails to fit, every subsequent block also fails
to fit, because the walker advances monotonically. Across the
~192-patch archive, zero cases of an in-bounds block following
an out-of-bounds block. The output is complete before the
first OOB block fires.

### Fix (applied)

`applyUPS` clips each sub-operation (skip, xor, terminator) to
remaining target space. `detectOOBBlocks` walks the block
stream per direction and emits a summary warning measured
against that direction's output size — forward against
`upsTargetSize`, reverse against `upsSourceSize`.

## ROMs on hand

**Matched** (size + CRC confirmed):

| ROM | Size | CRC | Patches |
|-----|------|-----|---------|
| SNK vs. Capcom - Card Fighters 2 (Japan).ngc | 2,097,152 | CCBCFDA7 | CFC2English |
| Pokemon - Crystal Version (USA).gbc | 2,097,152 | EE6F5188 | crystalleaf (V1.0) |
| Pokemon - Crystal Version (UE) (V1.1).gbc | 2,097,152 | 3358E30A | crystalleaf_alt (V1.1) |
| Pokemon - Red Version (USA).gb | 1,048,576 | 9F7FDD53 | gen6Typing |
| Fire Emblem - Seima no Kouseki (Japan).gba | 16,777,216 | 9D76826F | FE1+2_GBA |
| Final Fantasy II (USA) (Rev 1).sfc | 1,048,576 | 23084FCD | ff2iset (No Header) |
| Super Mario Bros. (JU) (PRG0).nes | 40,976 | 3337EC46 | smbs (growth: 41K → 74K) |
| Pokemon - Blue Version (USA).gb | 1,048,576 | D6DA8A1A | (no patch targets this ROM) |
| Vay (Un-Working Designs).bin | 475,238,064 | BAF8F8D5 | vay 2352-sector variants |

## Structural properties

- **Growth**: common (ROM expansion). FE1+2_GBA grows 16 MB to 33 MB.
- **Shrink**: zero found among all the patches we have. The apply-of-shrink
  path for phantom-tail OOB is therefore unexercised in real data,
  even though it is structurally identical to undo-of-growth.
- **Block-stream underfill**: all 25 curated patches' block streams
  underfill the declared target before tail-copy. Range: 0%
  reached (`stadium2/size-change`, 0 blocks) to 99.5% (`mother3`).
  The 12 `vay_battle_rate_reduction_*` family patches each have
  exactly 1 block reaching ~43% of declared target; legal but
  unusual — tail-copy does almost all the work.
- **Apply-direction OOB blocks**: 4 of 25 curated patches; always
  the +1-terminator quirk, always exactly 1 byte; always a
  contiguous tail (in practice, the single final block).
- **Phantom-tail OOB**: structurally produced whenever
  `output_size < max(source, target)`. Unmeasured in apply
  direction (no shrink patches among them). Massive in undo
  direction for growth patches.
- **Dual format**: some archives ship both IPS + UPS (smbs,
  ff2iset). Cross-validation opportunity.

## Header relevance

Headers shift byte offsets. A patch against a headered ROM corrupts a headerless ROM. Per-platform:

| Platform | Header | Size | Status |
|----------|--------|------|--------|
| NES | iNES | 16 bytes | universal, load-bearing |
| SNES | copier | 512 bytes | dead (headerless is norm) |
| FDS | fwNES | 16 bytes | may or may not be present |
| GB/GBC/GBA | none | n/a | no external header concept |
| N64 | none | n/a | byte-order varies |

slap has no header awareness. Low-priority feature: `--header-offset N` to shift the patch frame of reference. Not a stripping operation — the header bytes stay in the file, untouched.

## Varint decoder incident

The initial Python analysis script used the wrong byuu varint decoding convention, producing wildly inflated sizes (e.g., 34 GB for a 453 MB Sega CD image, 4 MB for a 2 MB NGPC ROM). This led to incorrect claims about overdumped ROMs and bogus declared sizes. All such claims were retracted once slap's parser (which agrees with byuu's spec) showed the correct values. The corrected decoder (Convention B) matches slap exactly.
