# IPS — todos

Known gaps between the design in `questions.md` and the code in
`src/Slap/IPS/`. Each item is work slap has committed to doing;
priority and shape are noted where they're not obvious. Speculative
stuff lives in `notebook.md`.

## Bugs — silent corruption today

### 1. `createIPS` target-size guard

`createIPS` is infallible: it accepts any source and target without
checking their sizes against the variant's addressable range. When
sizes exceed the range, `encodeOffset Offset24` truncates offsets
via `fromIntegral` and silently produces a wrong patch. Fix: add an
explicit guard that rejects when `max(sourceLen, targetLen)` exceeds
the variant's addressable range. `createIPS` becomes
`SourceFileContents -> TargetFileContents -> Either SlapError
PatchFileContents`, aligning with `createIPS32` and `createEBP`.

### 2. `createIPS` doesn't emit truncation marker on shrink

When `target < source`, `createIPS` passes `Nothing` unconditionally
as the truncation marker, silently losing the shrinkage. The design
(questions.md truncation entry) says emit the marker when
`target < source`. Fix is one branch at `createIPS`'s top, computing
the marker value from the target size and threading it into
`encodeIPSPatch`.

## Structural cleanup — invariants not type-enforced

### 3. Unify sentinel resolution

Today, sentinel handling is split: `avoidSentinel` (in
`Slap.IPS.Create`) fixes collisions on the with-source path;
`narrowHunks` (in `Slap.Measure`) rejects them on the source-less
path via a `sentinelOffset` field in `EncodingLimits`. The
with-source path strips the sentinel from the shared
`EncodingLimits` structure to stop `narrowHunks` from rejecting what
`avoidSentinel` is about to fix. `avoidSentinel` has a silent
`| otherwise = record` passthrough for the "source too short" case,
safety of which depends on a comment-enforced invariant.

The fix replaces all of this with one function. Sketch, not the
implementation to do — just here to get the idea across:

```haskell
resolveSentinelCollisions
  :: Offset          -- sentinel offset for the variant
  -> ByteString      -- source bytes (may be empty)
  -> [EncodedHunk]
  -> Either SlapError [EncodedHunk]
```

Both paths use it. With-source gets the shift-and-prepend fix;
source-less gets a `Left` with a clear error. `sentinelOffset` comes
out of `EncodingLimits` entirely — `narrowHunks` is no longer
IPS-aware. The silent passthrough dies; the compiler enforces
"source present implies fix; source absent implies error." Exact name
and shape are open.

## Warning infrastructure — 4 is a prerequisite for 5, 6, 7, and 11

### 4. Warning channel in `parseIPS` return type

`parseIPS :: PatchFileContents -> Either SlapError (Either IPSPatch
EBPPatch)`. There is no way to return "parse succeeded, but emit
this warning." Items 5, 6, 7, and 11 all need that capability.
Options: (a) change the return type to carry a warnings list
alongside the successful result, (b) thread warnings via a
writer-like monad in the parse layer, (c) borrow the shape
`Slap.Error`'s `CreateResult` already uses on the create side and
use it on parse too. Pick one; then 5, 6, 7, and 11 become
single-function changes each.

### 5. RLE count = 0 — accept-and-warn

`Parse.hs` rejects zero-count RLE records as `MalformedRecordField`.
The design (questions.md RLE entry) says accept-and-warn: a no-op,
not corruption. One line becomes a warn instead of a reject, once
item 4 is in place.

### 6. Warn on overlap in apply

Design (questions.md overlap entry): overlap is permitted, later
writes clobber, and we warn the user because overlap is unusual.
Today no warning is emitted. One linear pass over the record vector
tracking covered intervals. Depends on item 4.

### 7. Warn on unsorted records in apply

Design (questions.md record-order entry): wire-order apply, warn
when records aren't in offset order because unsorted is unusual.
Today no warning is emitted. One linear pass comparing each
record's offset against the prior record's. Depends on item 4.

### 11. Parse-time sanity bound on truncation marker value

Today a 3-byte post-`EOF` trailer is accepted as a truncation marker
without sanity-checking the value. Nonsense values get through
silently. Design: reject or warn when the declared target size is
implausible. Specific policy TBD. `truncate < maxRecordEnd` is
already caught at apply time by `ApplyWritesPastTarget`; parse-time
sanity is defensive cover for values that are well-formed but
absurd. Depends on item 4 if we pick warn; independent if we pick
reject.

## Slappy polish — committed, not urgent

### 8. `IPSBody` / `IPSPatch` split

BPS and UPS split their parse results into a "body" type
(raw-decoded fields yielded by `Get`) and a "patch" type (finalized
— CRC-validated, `Vector`-materialized). IPS has only `IPSPatch`;
`parseRecordsAndCaptureTrailer` returns an ad-hoc tuple. Introducing
`IPSBody` as the intermediate and keeping `IPSPatch` as the
finalized shape would match the structural parallel. Functional
no-op; purely an aesthetic / parallel-structure improvement.

### 9. `--require-smc-shaped-target-size` flag (draft name)

Optional create-time flag that refuses to emit a truncation marker
whose declared target size doesn't satisfy `(size & 0xFFF) ==
0x200`, making the emitted patch acceptable to SNESTool's parser.
No-op for patches that don't need a marker (target ≥ source). See
questions.md truncation entry for the scope caveat (necessary but
not sufficient for end-to-end SNESTool success).

### 10. Typeful reshape of trailer-state

The "did we see a trailer, and what kind?" state exists in the
program but isn't named at the type level — it's reconstructed in
`SomePatch` via pattern-matching on `ParseError LabelIPS` and
synthesizing a fake `IPSPatch`. Replace the reconstruction with a
named parse-result sum that makes the trailer-state a typed
variant.

**This item is less certain than the others.** The direction is
clear — make the three real post-parse shapes (clean IPS, clean
EBP, trailer-missing) visible at the type level instead of
distinguishing them via error-channel pattern-matches. The specific
constructor shape is open. Something along the lines of `ParsedIPS
IPSPatch | ParsedEBP EBPPatch | ParsedTruncated (Vector IPSRecord)`
is a plausible sketch but hasn't been ratified. Listed here because
the direction is committed-to, not because the exact shape is.
