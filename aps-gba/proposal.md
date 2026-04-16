# APS (GBA) — Research and Proposal

Slap's design decisions for APS GBA. For the on-disk structure of the
format itself, see `spec.md`. For the upstream reference binary
(HackMew's A-Ptch.exe), see `upstream/`. This document is slap-specific:
what we warn on, what we reject, how we handle edge cases the format
leaves under-specified, and why.

**Quick orientation**: APS GBA is an XOR-based, block-structured patch
format. 64KB blocks, dual CRC16 per block enabling direction detection,
both source and target sizes stored in the 12-byte header. Inherently
reversible. Magic is `APS1`. The reference tool is HackMew's VB6
A-Ptch.exe (circa 2010).

## Implementations

### A-Ptch (HackMew, 2010) -- the original

- **Language**: VB6 (native compiled, UPX packed)
- **Operations**: Create, Apply (with auto-undo), Get Patch Info
- **Source**: Lost. An unofficial fork by Gamer2020 exists on GitHub
  (`Gamer2020/Unofficial-A-ptch`) with VB6 source, but it's a modification
  (adds GB/GBC support), not a faithful copy.
- **Truncation bug (CONFIRMED in original binary)**: Disassembly of the original
  A-Ptch.exe at 0x40e9d9-0x40ea1f shows:
  - `fIsOriginal` (Boolean at [ebp-0x40]) tested
  - True branch: pushes `[ebp-0x54]` (lFileSize1 = original size) → wrong
  - False branch: pushes `[ebp-0x58]` (lFileSize2 = modified size) → also wrong
  Both directions truncate to the wrong size. The unofficial fork inherited this
  exactly. Never observed because GBA ROMs don't change size.

### UniPatcher (Boris Timofeev, 2017)

- **Language**: Java (Android app)
- **Operations**: Apply (with auto-undo)
- **Truncation**: Correct. `isOriginal => truncate(fileSize2)`,
  `isModified => truncate(fileSize1)`.
- **Source**: `btimofeev/UniPatcher` on GitHub, `APS_GBA.java`

### RomPatcher.js (Marc Robledo, 2023)

- **Language**: JavaScript (web/Node.js)
- **Operations**: Apply only (forward, no undo)
- **Create**: Not supported for APS GBA (the `'aps'` format option creates APS N64)
- **Truncation**: N/A -- allocates `targetSize` buffer upfront, correct for
  forward-only apply.
- **Source**: `marcrobledo/RomPatcher.js` on GitHub, `RomPatcher.format.aps_gba.js`
- **Added**: March 2023, prompted by issue #58 (user with an FFTA X patch)

## Edge cases and ambiguities

### Verified behaviors

1. **Undo**: Works. Auto-detected via CRC16 matching. All implementations agree on
   the mechanism (XOR is always applied; direction only affects truncation).

2. **File growth** (targetSize > sourceSize): The format represents this correctly.
   Blocks beyond sourceSize have implicit source bytes of zero; XOR data = target
   data verbatim. Creation tool iterates over `max(size1, size2) / 65536` blocks.
   Undo of growth (= shrinkage) works: XOR recovers zeros, truncation discards.

3. **File shrinkage** (targetSize < sourceSize): Represented correctly. Blocks beyond
   targetSize have implicit target bytes of zero; XOR data = source data. Forward
   apply: XOR produces zeros, truncation discards. Undo of shrinkage (= growth):
   XOR recovers original source bytes.

4. **BUT**: The original tool's truncation bug means size-changing patches produce
   wrong output in A-Ptch. UniPatcher handles them correctly.

### Unspecified / ambiguous — full inventory with decisions

Each edge case below gets a verdict: what to do, and why.

#### 5. File growth (targetSize > sourceSize)

**What happens**: Blocks beyond sourceSize have no source data. The creation tool
treats missing source bytes as zero. XOR data for growth blocks = target data
verbatim. The format represents this correctly: both sizes stored, records emitted
for all differing blocks including growth blocks.

**Is it stupid?** Not really. The format handles it cleanly. The original tool's
truncation bug means growth patches don't work correctly in A-Ptch, but that's the
tool's fault, not the format's.

**Decision**: Support it. Apply forward: extend output to targetSize, zero-fill
beyond source, XOR normally. Undo: XOR recovers zeros in growth blocks, truncate
to sourceSize discards them. Warn if sourceSize != targetSize (unusual for APS GBA;
no known patch in the wild does this).

#### 6. File shrinkage (targetSize < sourceSize)

**What happens**: Blocks beyond targetSize have no target data. The creation tool
treats missing target bytes as zero. XOR data for shrinkage blocks = source data
verbatim. Forward apply produces zeros at those offsets, then truncation to
targetSize discards them. Undo recovers the original source data from the XOR.

**Is it stupid?** Same as growth — the format handles it, the tool doesn't.

**Decision**: Same as growth. Support it, warn about it.

#### 7. Non-aligned file sizes (not a multiple of 64KB)

**What happens**: The last record's 64KB block overshoots the file boundary. When
reading the source block at that offset, only `fileSize - offset` bytes exist; the
rest are absent. When writing the XOR result, the 64KB output extends past the
intended file size.

**How existing tools handle it**:
- A-Ptch: VB6's `Get` into a pre-allocated 64KB byte array reads what exists and
  leaves the rest as zero. After the loop, `TruncateFile` sets the correct size.
  So the overshoot bytes are written and then truncated away.
- UniPatcher: explicitly zero-fills short reads. `truncateFile` at the end.
- RomPatcher.js: allocates `targetSize` buffer upfront. Writes 64KB at the offset,
  which may extend the internal array past targetSize. No truncation afterward.

**Is it stupid?** A little. Fixed 64KB blocks with non-64KB file sizes means the
last block always wastes space in the patch (the overshoot XOR bytes are
meaningless). But it's a coherent design: always 64KB, always zero-pad, truncate
at the end. The "spec" is just "zero-pad short blocks." All three implementations
agree on this.

**Decision**: Zero-pad short source reads to 64KB before XOR. After all records,
truncate/set output to the correct target size (forward: targetSize, undo:
sourceSize). This matches all implementations. Warn if file sizes are not 64KB-
aligned (no known real patch has this).

**For create**: If the input files aren't 64KB-aligned, zero-pad the short final
blocks and compute CRC16 over the full 64KB (matching what A-Ptch does). The
CRC16 is always over 64KB — this is not ambiguous, all implementations agree.

#### 8. Non-aligned record offsets (not a multiple of 64KB)

**What happens**: The format field is a bare uint32. Nothing constrains it to 64KB
alignment. If offset is e.g. 0x8000, the 64KB block covers 0x8000–0x17FFF, which
overlaps with what a 64KB-aligned scheme would consider two separate blocks.

**Does any tool produce this?** No. A-Ptch walks the file in sequential 64KB-aligned
steps. No other creation tool exists.

**Is it stupid?** This isn't a design decision — it's a non-decision. The author
never considered non-aligned offsets. The format doesn't forbid them because it
doesn't have a mechanism to forbid anything. There is no semantic for "what happens
when blocks overlap."

**Decision**: Reject at parse time. A non-aligned offset means the patch was not
produced by any known tool and has no defined semantics. This is not "unusual but
valid" — it is genuinely undefined behavior. Error, not warning.

#### 9. Duplicate record offsets

**What happens**: Two records at the same offset. The XOR compounds:
`source ^ xor1 ^ xor2 = source ^ (xor1 ^ xor2)`. This is mathematically
well-defined but almost certainly not what was intended — it means the second record
was computed against the ORIGINAL source data, but it's being applied to the ALREADY
PATCHED data from the first record.

**Does any tool produce this?** No. A-Ptch walks sequentially and emits at most one
record per offset.

**Is it stupid?** It's not a design decision. Same as non-aligned offsets: the format
doesn't forbid it because it doesn't have a mechanism to forbid anything.

**Decision**: Reject at parse time. Same reasoning as non-aligned offsets. If someone
wants to compound XOR, they can do it in one record.

#### 10. Overlapping records (distinct non-aligned offsets whose 64KB ranges overlap)

**What happens**: Consequence of non-aligned offsets. Same compounding problem.

**Decision**: Already covered by rejecting non-aligned offsets (which is the only way
to get overlap). If offsets are all 64KB-aligned, 64KB blocks can never overlap.

#### 11. Out-of-bounds records (offset >= max(sourceSize, targetSize))

**What happens**: A record addresses bytes that don't exist in either file. Source
bytes are zero (no data), target bytes are zero (no data), so the XOR data should
be all zeros (pointless). If the XOR data is NOT all zeros, the record is
contradictory — it claims changes in a region that both files agree is empty.

**Does any tool produce this?** No.

**Is it stupid?** More malformed than stupid. This can't arise from correct creation.

**Decision**: Reject at parse time. A record whose offset + 64KB is entirely beyond
both file sizes is structurally invalid.

Note: a record that PARTIALLY extends beyond a file size is NOT out of bounds — that's
the non-aligned file size case (#7), handled by zero-padding. The test is whether the
record's START is beyond both sizes.

#### 12. CRC16 collision (sourceCrc16 == targetCrc16 for a changed block)

**What happens**: Direction detection is ambiguous. The pre-XOR CRC matches both
the original and modified CRC for a given block. The tool can't tell which direction
it's going.

**How likely?** CRC16 is 16 bits, so ~1/65536 per block that actually changed. For
a patch with few changed blocks, very unlikely. For a patch with many changed blocks,
still unlikely for any individual block, but the probability of at least one collision
grows.

**How existing tools handle it**: UniPatcher tracks direction globally and throws if
blocks disagree. A-Ptch does the same (the `fIsOriginal And fIsModified` check).

**Is it stupid?** Yes, but it's the format's fundamental limitation. 16-bit CRC for
direction detection on 64KB blocks is weak. A 32-bit CRC or a hash would have been
better. But it's what we have.

**Decision**: Track direction per-block. If all blocks agree: use that direction. If
blocks disagree: error. If a single block matches BOTH CRCs: it doesn't cast a vote
(skip it for direction purposes, since the XOR is applied regardless and is correct
in either direction). If NO block casts a vote (every block collides): the direction
is genuinely ambiguous. Error, unless the user provides a direction hint.

The subtle case: if sourceCrc16 == targetCrc16 for a block, the XOR is still correct
(it's self-inverse either way). The CRC collision only matters for truncation — you
need to know which size to use. If sourceSize == targetSize (the overwhelmingly common
case), direction doesn't matter for truncation either, so collisions are harmless.

#### 13. Record ordering

**What happens**: The format doesn't specify that records must be in offset order.
A-Ptch produces them in order (it walks the file sequentially), but the format
structure is just "records until EOF."

**Decision**: Accept any order. Sorting is not required for correctness — each record
is self-contained (offset + XOR data). But warn if out of order, since it suggests
the patch wasn't produced by A-Ptch.

#### 14. All-zero XOR data in a record

**What happens**: A record exists but the XOR data is all zeros. This means the
source and target blocks are identical at this offset. The record is a no-op —
applying it changes nothing. Including it wastes 65544 bytes of patch file.

**Does any tool produce this?** A-Ptch checks `InStrB` (binary comparison) and
only emits records for differing blocks. So no.

**Decision**: Accept but warn. It's valid (XOR with zeros is identity) but wasteful
and not produced by any known tool.

#### 15. Patch file size not congruent to 12 mod 65544

**What happens**: The file can't be cleanly parsed into header + N records. There's
a partial record at the end, or extra trailing bytes.

**Decision**: Reject at parse time. This is a structural validity check, same as
what all three implementations do (RomPatcher.js checks `(fileSize-12) % recordSize
!== 0`).

## Design recommendations for slap

### Overall stance: Option 4 (correct behavior + parse-time warnings)

Apply logically correct behavior derived from the format's structure, not from the
original tool's bugs. Emit warnings for anything no known tool would produce. Reject
the truly malformed. Accept but flag the suspicious.

This is the most slap-y option: the code is correct, the types are honest, and the
system communicates clearly about what it's seeing. The warning says "I'm handling
this correctly, but you should know this is unusual."

### Why not the other options

1. **Bug-compatible** ("do what A-Ptch does"): Encoding a known bug as a feature
   contradicts slap's ethos. You'd need a comment explaining why you're doing the
   wrong thing, and that comment would be the ugliest line in the codebase.

2. **Logically correct with no warnings**: Correct but silent. Misses the chance to
   tell the user "this patch is unusual" when it genuinely is.

3. **Strict rejection**: Refusing valid format data because the original author had
   a bug is the wrong kind of strictness. "Impossible" should mean "the format can't
   express this," not "the original tool couldn't handle this."

### Summary of decisions by category

**Supported (correct behavior, warn if unusual)**:
- File growth / shrinkage (#5, #6): support, correct truncation, warn
- Non-aligned file sizes (#7): zero-pad, truncate at end, warn
- Record ordering (#13): accept any order, warn if unsorted
- All-zero XOR records (#14): accept, warn
- One-sided zero file size (#16): accept, warn

**Rejected at parse time (structurally invalid / undefined semantics)**:
- Non-aligned offsets (#8): reject
- Duplicate offsets (#9): reject
- Overlapping records (#10): covered by #8
- Out-of-bounds records (#11): reject
- Bad file size / partial records (#15): reject
- Both file sizes zero (#16): reject
- Old format misidentification (#17): covered by structural check

**Handled with special logic**:
- Truncation (#4): follow UniPatcher (logically correct), not A-Ptch (buggy)
- CRC16 collision (#12): per-block voting; colliding blocks abstain; error if
  ambiguous; optional user direction hint as escape hatch

### Truncation detail

Follow UniPatcher's logic (which is the logically correct one):
- Forward apply (input matches original CRCs) → truncate to modified size
- Undo (input matches modified CRCs) → truncate to original size

The original A-Ptch.exe has these inverted (confirmed via disassembly at
0x40e9d9-0x40ea1f). This is a bug, not a design decision. No size-changing APS GBA
patch is known to exist in the wild, so there is no compatibility concern.

### CRC16 collision detail

16 bits is 16 bits. Track direction per-block:
- Block matches only sourceCrc → votes "forward"
- Block matches only targetCrc → votes "undo"
- Block matches both (collision) → abstains
- Block matches neither → error (file not compatible with patch)

If all voting blocks agree: use that direction. If they disagree: error. If no block
votes (all collide): direction is genuinely ambiguous; error unless user provides a
direction hint. Note: if sourceSize == targetSize, direction only affects CRC
validation, not truncation, so collisions are less consequential.

#### 16. Zero file size

**What happens**: sourceSize = 0 or targetSize = 0. This would mean patching an
empty file into something, or patching something into nothing. The format can
represent it (bare uint32).

**Does any tool produce this?** No. A-Ptch is GBA-specific; GBA ROMs are never empty.

**Is it stupid?** A patch that produces an empty file is pointless (just delete the
file). A patch that takes an empty file and produces content is technically possible
but nonsensical for the format's intended use.

**Decision**: Reject if BOTH are zero (empty→empty is meaningless). Accept if one is
zero, but warn. The math works: all blocks XOR against implicit zeros. It's just
extremely unusual.

#### 17. Old format ambiguity (APS1 v1 vs v2)

**What happens**: The commented-out older format in the A-Ptch source also uses the
`APS1` magic. It has a different structure: variable-length records, single XOR'd
size field, optional RLE compression. A patch in the old format would have the same
first 4 bytes as the current format.

**Could this cause misidentification?** No. The structural validity check
(`(fileSize - 12) % 65544 == 0`) would fail for old-format patches because their
records are variable-length. The old format would be rejected as structurally invalid,
which is correct — there is no known tool that produces old-format patches (the code
was commented out before release).

**Decision**: No special handling needed. The structural check covers it. If we ever
want to support the old format, it would need separate detection logic.

## Commented-out older format

The unofficial fork source contains a commented-out older format version:

- Variable-length records (1-byte size field, up to 255 bytes per record)
- RLE compression (size == 0 means next two bytes are fill-byte + count)
- File size stored as `LOF(original) XOR LOF(modified)` (single XOR'd value)
- No per-block CRC16

The active format abandoned all of this for fixed 64KB blocks, no compression,
explicit dual sizes, and per-block dual CRC16s. The older format could not support
undo (no dual CRCs, and the XOR'd size field loses information when sizes differ).
