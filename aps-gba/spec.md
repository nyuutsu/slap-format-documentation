# APS (GBA) — Format Specification

This is a clean writeup of the on-disk structure of an APS GBA patch.
It's derived from the UniPatcher wiki spec, cross-checked against
RomPatcher.js's implementation, the unofficial Gamer2020 VB6 fork of
A-Ptch, and binary analysis of HackMew's original A-Ptch.exe.

Where this document and a specific implementation disagree, this
document describes what the **format can coherently represent**, not
necessarily what every tool does with it. The `proposal.md` sibling
file covers slap's design decisions for edge cases.

## Identity

- **Magic**: `APS1` (4 bytes, ASCII)
- **Endianness**: little-endian throughout
- **Author**: HackMew (Andrea Sartori)
- **Reference tool**: A-Ptch.exe (~2010)
- **Scene**: PokeCommunity GBA ROM hacking

## Overall structure

```
[ header (12 bytes) ]
[ record 0 (65544 bytes) ]
[ record 1 (65544 bytes) ]
...
[ record N-1 (65544 bytes) ]
```

Total file size is always `12 + N * 65544`. Every record is exactly
the same size. There is no EOF marker; records run until end of file.

## Header (12 bytes)

| Offset | Size | Field        | Notes |
|--------|------|--------------|-------|
| 0x00   | 4    | Magic        | ASCII `APS1` |
| 0x04   | 4    | Source size  | uint32 LE, byte length of the original file |
| 0x08   | 4    | Target size  | uint32 LE, byte length of the patched file |

## Record (65544 bytes)

| Offset | Size  | Field         | Notes |
|--------|-------|---------------|-------|
| 0x00   | 4     | Offset        | uint32 LE, byte offset into the file where this block begins. In practice always a multiple of 65536. |
| 0x04   | 2     | Source CRC16  | uint16 LE, CRC16-CCITT over the source block |
| 0x06   | 2     | Target CRC16  | uint16 LE, CRC16-CCITT over the target block |
| 0x08   | 65536 | XOR data      | `source[i] ^ target[i]` for each byte in the 64KB block |

The record encodes a single 64KB block's worth of changes. A patch
emits one record per changed block and omits unchanged blocks entirely.

### CRC16

CRC16-CCITT with polynomial `0x1021`, initial value `0xFFFF`, computed
over exactly 65536 bytes per block. Short blocks (when the file is not
a multiple of 64KB) are zero-padded to 64KB for CRC computation.

## Diff semantics

XOR-based. Per byte: `target = source ^ xor_data`. Because XOR is
self-inverse, the same XOR bytes also recover the source from the
target: `source = target ^ xor_data`.

When source and target file sizes differ, the shorter file is treated
as zero-padded for XOR purposes. In growth blocks (beyond the end of
the source), the XOR data equals the target bytes verbatim. In
shrinkage blocks (beyond the end of the target), the XOR data equals
the source bytes verbatim.

## Reversibility

The format is inherently bidirectional:

1. XOR is self-inverse.
2. Both source and target CRC16s are stored per block, enabling
   direction detection at apply time (compute pre-XOR CRC, compare
   against both stored values).
3. Both source and target sizes are stored in the header.

Apply logic:

1. For each record, read the block at the given offset from the
   input file.
2. Compute CRC16 of the block (before XOR).
3. If it matches `source CRC16`, this is a forward apply. If it
   matches `target CRC16`, this is an undo. If neither, the file
   doesn't match the patch.
4. XOR the block with the record's XOR data (the operation is
   identical in both directions).
5. Write back.
6. After all records: truncate/extend the output file. For forward
   apply, target size. For undo, source size.

## Expressible, in-practice-rare conditions

The format can represent these cases, though no known creation tool
produces them:

- File size changes (source size != target size). Growth and shrinkage
  both work mathematically. See `proposal.md` for how slap handles
  these.
- Non-64KB-aligned file sizes. The last block overshoots the file
  boundary; implementations zero-pad. GBA ROMs are always
  64KB-aligned, so this doesn't arise in practice.

## Format-legal but structurally malformed

The format lacks structural constraints that no reasonable patch would
violate, but that a hand-crafted or corrupted patch could:

- Record offsets not aligned to 64KB boundaries
- Overlapping records (two records covering overlapping byte ranges)
- Duplicate offsets (two records at the same offset)
- Records addressing bytes beyond both file sizes
- RLE-style compounding when two blocks share a CRC16 collision

See `proposal.md` for slap's decisions on each.

## Limitations

- **16-bit CRC for direction detection.** Probability of collision
  per changed block is ~1/65536. For patches with many changed
  blocks, at least one collision becomes plausible.
- **No patch-file checksum.** Corruption of the patch itself is
  undetectable.
- **No extension mechanism.** The format has no reserved bytes,
  version field, or type byte for future features.
- **No metadata beyond the file size pair.** No description, no
  author, no comment.

## Historical note on the original tool

HackMew's A-Ptch.exe (circa 2010) has an inverted truncation bug for
size-changing patches: when applying forward, it truncates to the
source size instead of the target size, and vice versa. This was
never observed in practice because GBA ROMs are power-of-two sizes
and patches don't change file size.

The bug was confirmed by disassembly at addresses 0x40e9d9-0x40ea1f
of the unpacked binary. The unofficial Gamer2020 VB6 fork inherits
the same bug. UniPatcher's independent implementation has the correct
behavior.

Any tool implementing APS GBA today should implement the logically
correct truncation (forward → target size, undo → source size), not
the reference tool's buggy behavior.
