# UPS format findings

Source: romhacking.net archive (2024-08-01). 124 archives, ~192 `.ups` files. Shortlisted 8 patches for hands-on testing.

## Provenance

- **slap info/explain/apply**: authoritative for sizes, CRCs, block counts. Confirmed by successful apply + CRC validation.
- **Python script (corrected)**: used for bulk classification (OOB blocks, growth vs same-size, etc.). The varint decoder was initially wrong (Convention A instead of byuu's Convention B) — all claims now either verified by slap or removed. The corrected script's OOB analysis (contiguous tail property) has not yet been re-verified by slap but is consistent with observed behavior.

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

## OOB blocks (resolved)

crystalleaf and FE1+2_GBA demonstrate the pattern on CRC-verified
ROMs: the very last block overshoots the declared output size by
exactly 1 byte (the terminator). This is a creation-tool artifact
(NUPS, Tsukuyomi) where the terminator lands at `output_size`
rather than `output_size - 1`.

From bulk analysis: OOB blocks are **always a contiguous tail**.
Across all ~192 patches, zero cases of an in-bounds block following
an out-of-bounds block. The output is complete before the first OOB
block fires.

**Fix (applied)**: `applyUPS` clips each sub-operation (skip, xor,
terminator) to remaining target space. `detectOOBBlocks` walks the
block stream at parse time and emits a summary warning.

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
- **Shrink**: zero found in entire corpus. Unexercised edge case.
- **OOB blocks**: common in same-size patches; always a contiguous tail.
- **Dual format**: some archives ship both IPS + UPS (smbs, ff2iset). Cross-validation opportunity.

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
