# slap PPF implementation audit

Triage of bugs / gaps / weaknesses in `src/Slap/PPF/*.hs` as of the
review date. Scoped to the existing code, pre-refactor. For the
structural rewrite case, see `slap-implementation-notes.md`. For
what the format actually is, see `spec.md`.

Each item categorised by whether it would produce wrong output on a
reasonable real patch, or whether it's a missing validation / type
cleanliness issue.

## Priority 1: output correctness on real input

### PPF1 RLE record mode is not implemented

`Slap.PPF.Parse.parseRecords32` (used for both PPF1 and PPF2) reads
a 4-byte offset, a 1-byte count N, and N bytes of data.

Per `ppf.txt` (the PPF1 spec), when N == 0 the record's payload is
**two bytes**: `(data_byte, repeat_count)`, and the applier must
write `repeat_count` copies of `data_byte` at the offset. This is
the PPF1 RLE mode. The era-contemporary `applyppf.c` implements
this branch explicitly:

```c
if (Size != 0) { /* normal literal write */ }
else {
    unsigned char data, len;
    fread(&data, 1, 1, APSFile);
    fread(&len, 1, 1, APSFile);
    for (i = 0; i < len; i++) fputc(data, ORGFile);
}
```

slap's `parseRecords32` has no such branch. For a PPF1 record with
N=0, slap reads 0 data bytes (silently), records an empty PPFRecord,
and then misinterprets the next 2 bytes (which are the RLE payload)
as the start of the next record's offset. The resulting patch state
is garbage from that point forward.

**Practical scope**: no PPF1 patch in our 339-file corpus uses RLE.
The reference PPF1 creator (`makeppf.c`) never emits RLE records.
But the format permits them; a hand-crafted or non-reference-built
PPF1 could use them legitimately. On such a patch slap silently
produces wrong output.

**Fix**: `parseRecords32` must branch on N==0 for PPF1. PPF2 does
not have RLE (per PPF2.txt and the PPF3-era applier); so the branch
is PPF1-only. After the planned four-way split, this becomes a
PPF1-local concern, not a shared parser.

## Priority 1.5: undo coherence — PPF3 create + undo must enforce same-size

PPF3's undo data is stored inline per record: each record with undo
contains N replacement bytes followed by N "original" bytes, at the
same offset. Applying the undo means writing those N original bytes
at the offset, effectively reversing the record's write. For this
to produce the pre-patch file byte-for-byte, **every byte the patch
changed must have had a pre-patch value at that offset** — which is
only true if the pre-patch file extended at least as far as the
patch's highest-offset record.

In other words, PPF3 undo is only coherent when source and target
files are the same size. If a "growth" PPF3 were constructed (records
writing past the original EOF) with undo data enabled, the undo
bytes for those out-of-EOF records would be lying — "restoring"
bytes that never existed.

The PPF3 format does not intrinsically forbid such a patch (no
truncate/size-declaration directive enforces anything). But the
reference creator only diffs same-size inputs, so the issue never
arises there. It becomes a slap concern only when slap permits
creating/converting-to PPF3 from size-mismatched sources with undo
requested.

### What slap must enforce

1. **Create → PPF3 with undo**: require source size == target size.
   Refuse with a clean error otherwise (pointing the user at either
   removing `--undo` or using a different format).
2. **Convert → PPF3 with undo**: since convert ultimately calls
   create, enforcement is the same. If the conversion uses
   `--with original.rom`, slap can compare original.rom's size to
   the patch's effective output size and decide. If the AST came
   from a growth-capable source format (PPF4 with ADD, BPS with
   append, etc.), this comparison will fail.
3. **Create → PPF3 without undo**: no constraint from undo
   semantics. The general "PPF3 is same-size by format design"
   consideration (records writing past original EOF is weird even
   without undo) is a separate, weaker concern.

### What currently happens

`Slap.PPF.Create.encodePPF3` does not check source vs. target size.
`Slap.Convert` has no PPF3-specific size-match logic either. Today
slap would happily accept a growth-producing AST and emit a PPF3
with undo, producing an incoherent undo stream.

### How undo data flows through convert

slap's AST represents undo as `UndoHunk` — a format-agnostic triple
of `(offset, payload, original bytes)`. This is defined in
`Slap.Measure`, not in any format module; the PPF3 parser lifts its
inline per-record undo into this generic shape, and the PPF3 encoder
lowers generic `UndoHunk`s back into the inline format on write.

So "carried through" means: lifted from PPF3's wire format into the
generic AST at parse time, held as `UndoHunk` in `contentsUndoData`,
and lowered back to PPF3's wire format at encode time. The
representation in transit is not PPF-specific; the AST doesn't
commit to any particular format's encoding. That's what makes the
field "transportable" — its internal shape is meaningful to any
consumer that wants to reason about per-byte-original-state.

Per-conversion behaviour:

- **PPF3 → PPF3** with undo carried through: parse into `UndoHunk`s,
  re-encode from `UndoHunk`s. Round-trips the format's inline undo
  faithfully.
- **Target format can't store undo** (PPF1/2/4, IPS, UPS,
  xdelta-family, etc.): `contentsUndoData` is ignored by the target's
  encoder and a `UndoDataDropped` warning is emitted. The generic
  AST still carries the undo; the target just refuses to use it.
- **Target format needs undo but source didn't carry it**: slap
  calls `computeUndo source patchHunks`, reading original bytes from
  `--with original.rom` for each hunk's byte range. This produces
  `UndoHunk`s from nothing, working as long as the supplied ROM is
  the correct pre-patch source.

None of this is wrong. The gap is the size-match enforcement in
step 1/2 above, not the undo transport mechanism itself.

## Priority 2: validation gaps (accepts malformed input that reference rejects)

### PPF4 REPLACE-after-ADD ordering not enforced

Per `patcher.lua`, once the applier sees the first ADD command, it
enters append mode and rejects any subsequent REPLACE with
`ERROR_BAD_REPLACE`. slap's `applyPatchMemory` processes records
individually with no state machine; it silently accepts any
interleaving of Replace and Append commands.

On valid reference-produced patches this is a no-op (they put all
REPLACEs first). On malformed patches slap is more permissive than
the reference.

**Fix**: track ADD-mode state; reject REPLACE after the first ADD.

### PPF4 encoding byte not validated

`Slap.PPF.Parse.checkEncoding` has `checkEncoding PPF4 _ = Right ()`
— accepts any byte. Per `patcher.lua`, the encoding byte must be
`0xFF`; otherwise `ERROR_BAD_VERSION`.

**Fix**: require `0xFF` for PPF4.

### PPF4 reserved flag bytes (offsets 56–59) not validated

`parsePPF4` does `skip ppf4PostDescriptionLength`. `patcher.lua`
reads those four bytes as a u32 and rejects the patch if nonzero
(`ERROR_BAD_FLAGS`). The image_type / validation_flag / undo_flag /
expansion fields are semantically "must be 0" and structurally
present for possible future use.

**Fix**: read and validate zero before skipping.

## Priority 3: type cleanliness / labelling

### PPF2 validation hardcodes `PPFImageType = BIN`

`parsePPF2` constructs `PPFValidation BIN validationBlock`. PPF2's
header has no imagetype field — that's a PPF3 addition. The `BIN`
label is incidentally correct (PPF2's block is always from offset
`0x9320`, which is PPF3's BIN offset), but the type is carrying
information that wasn't in the source patch.

This doesn't produce wrong output. It's type-level conflation that
the four-way split would naturally resolve (PPF2's validation type
has no imagetype; PPF3's does).

### Error labelling when version byte is unknown

`detectVersion` returns `BadVersion LabelPPF1 ...` for any file
starting with `"PPF"` but not `"PPFN"` for N∈{1,2,3,4}. The PPF1
label is arbitrary — there's no version to attach yet. Symptom of
the 3-byte `"PPF"` dispatch prefix; resolved by the planned
four-way split, where an unknown-version file simply doesn't match
any of the four 5-byte probes and falls through to the real
"unknown format" path.

### `PPFPatch`'s `Maybe`-gated fields

`ppfFileSize`, `ppfValidation`, `ppfHasUndo`, `ppfImageType`,
`ppfFileId` are each meaningful for only some versions. Today
they're all `Maybe`/`Bool` and every parser sets the ones it
doesn't use to `Nothing`/`False`. A sum-type-per-version or
per-version module (see the four-way split note) would remove
this without changing any behaviour.

## Non-issues

These looked like potential concerns but turned out to be fine.

### PPF1 termination is EOF-based (matches era-contemporary reference)

`parseRecords32` terminates when `remaining < 5`. Same robustness
as `pdx-ppf1/sources/applyppf.c`'s EOF-driven loop. slap does
**not** inherit the PPF3-era reimplementation's count-arithmetic
bug.

### PPF3 undo uses `recordOffset` for the undo write

Correct — undo writes at the same offset as apply, using the
`recordUndo` bytes instead of `recordData`. No issue.

### PPF4 ADD offset field ignored

Reference behaviour. ppfmaker.cpp writes 0, patcher.lua ignores
the offset field for ADD commands. slap parses the offset and
silently ignores it during apply. Matches reference.

## Summary

| # | Priority | Item | Fix scope |
|---|----------|------|-----------|
| 1 | P1 | PPF1 RLE not implemented | PPF1 parser; add N==0 branch |
| 2 | P1.5 | PPF3 create+undo doesn't enforce same-size | PPF3 create; add size check guarded by undo flag; extend convert to propagate constraint |
| 3 | P2 | PPF4 REPLACE-after-ADD not enforced | PPF4 apply; state tracking |
| 4 | P2 | PPF4 encoding byte not validated | PPF4 parser; require 0xFF |
| 5 | P2 | PPF4 reserved bytes not validated | PPF4 parser; require zero |
| 6 | P3 | PPF2 validation hardcodes BIN | PPF2 types; optional imagetype |
| 7 | P3 | Error label for unknown PPF version | dispatch-layer; four-way split resolves |
| 8 | P3 | Maybe-soup in PPFPatch | four-way split resolves |

P1 produces wrong output on possible-but-unseen real patches.
P1.5 permits constructing a patch whose undo data is incoherent.
P2s are validation strictness that matches the reference and
makes malformed PPF4s fail cleanly. P3s are cleanliness that the
four-way split resolves naturally.
