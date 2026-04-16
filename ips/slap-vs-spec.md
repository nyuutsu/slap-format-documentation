# slap's IPS implementation vs the spec

Comparison of `src/Slap/IPS/` against `docs/ips/spec.md` and the
resolved design decisions in `docs/ips/proposal.md`. Produced April
2026 during an accuracy review of the IPS documentation.

slap is not authoritative here. It is a program in progress. This
document records what the code currently does and where it diverges
from what the documentation implies it should do.

## Correct

- Parse returns `Either SlapError`; apply returns
  `Either SlapError TargetFileContents` (BPS/UPS-aligned shape).
- Parse-time ceiling check: `offset + payload` vs
  `ipsVariantMaxRecordEnd` (`validateRecordList`).
- EOF sentinel: parse uses peek-then-branch (same effective behavior
  as Flips's `while (offset != 0x454F46)`); create has
  `avoidSentinel` matching the Flips/archiveteam convention.
- Truncation: accepted for StandardIPS only; IPS32 rejects all
  trailing bytes.
- EBP trailing JSON: shape-recognized by leading `{`, captured as
  opaque bytes, not validated against the EBPatcher schema.
- Target size derivation: `max(sourceSize, maxRecordEnd)` or
  truncation marker if present.
- Apply: `unsafePerformIO` + `create` + `IORef` error channel
  (BPS/UPS strict-apply pattern).
- Overlapping records: allowed, last-write-wins in wire order.

## Needs change: RLE zero-length

`Parse.hs:274` rejects RLE records with zero run length as
`MalformedRecordField`. Decision (April 2026): accept and warn
instead. A zero-length RLE is a no-op ("write this byte zero
times"), not a reason to reject the entire patch. Flips rejects;
RomPatcher.js and lua-ips silently no-op. We will process and warn.

Note: `parseIPS` currently returns
`Either SlapError (Either IPSPatch EBPPatch)` — there is no
warning channel in the return type. Implementing warn-and-continue
requires a way to carry warnings alongside a successful parse
result. Check whether slap's architecture already has a warning
mechanism elsewhere.

## Needs change: createIPS doesn't emit truncation marker

`createIPS` passes `Nothing` for truncation unconditionally
(`Create.hs:104`). If target < source, the shrinkage is silently
lost — the applied patch yields `max(sourceSize, maxRecordEnd)`
bytes, not the actual target size.

Meanwhile `createIPS32` and `createEBP` return
`Left CannotExpressTargetShrinkage` when target < source. And
`parseIPS` already accepts truncation markers. And
`encodeIPSPatch` already has `encodeTruncationMarker`. The
create/parse pair is asymmetric: parse accepts what create never
produces.

We haven't thought this through enough and don't have enough data
on what other tools do, which would inform our decision. The
parser supports truncation. The encoder has the machinery. The
creator doesn't use it.

**Design space:** if we can exercise a feature (parse truncation)
then we should be able to create with it. But some consumers may
not support truncation. Options:

- Always emit truncation when target < source.
- Have a flag or mode to control it.
- Treat "IPS with truncation" and "maximally vanilla IPS" as
  distinct output modes, similar to how rfc-vcdiff and xdelta3
  codify "extensionless VCDIFF" as a separate thing. The problem
  shape might call for a layer where "maximally vanilla IPS" is
  a mode.

## Sentinel avoidance: works, but the plumbing is wrong

The sentinel collision (offset 0x454F46 is byte-identical to the
EOF footer) is a hard format limitation. Converting a patch to IPS
is impossible when the patch has a record at exactly that offset
and you don't have the ROM — you can't shift the record back by
one byte without knowing what byte to prepend. This is ~1 offset
out of 16.7 million. Rare, but a hard wall inherent to the format.

The code handles this correctly in terms of outcomes, but the
mechanism is split across two systems in a way that's fragile:

**`narrowHunks`** (`Measure.hs`) is a general-purpose function
used by `Convert.hs` for every format that has encoding limits.
It checks whether records fit within offset ranges. But
`EncodingLimits` also carries a `sentinelOffset` field that is
IPS-specific — no other format has a sentinel. The
direct-conversion path (`convertDirect`) passes limits with the
sentinel included, so `narrowHunks` rejects sentinel collisions.
The with-source path (`createFromMemory`) strips the sentinel
from the same structure to avoid a false rejection, because
`avoidSentinel` handles it instead. This selective
enabling/disabling per call path is choreography that shouldn't
be necessary.

**`avoidSentinel`** (`Create.hs`) is the IPS-specific fix-up: it
shifts colliding records back by one byte and prepends the
preceding source byte. It's total
(`[EncodedHunk] -> [EncodedHunk]`) — if it can't fix the
collision (source too short), it silently passes through. Its
contract (documented in the comment at Create.hs:316-330) says
it's only called from the with-source path where the optimizer
guarantees the source is large enough.

### What to fix: `narrowHunks` is doing too much

`narrowHunks` is a general function in `Measure.hs`. It should
ask a general question: "does this record fit within this format's
offset range?" The sentinel is an IPS-specific concern that
doesn't belong in `EncodingLimits`, a structure shared by PPF,
NINJA, PMSR, and every other format.

1. **Remove `sentinelOffset` from `EncodingLimits`.** Let
   `narrowHunks` be purely about offset ranges.

2. **Add an IPS-specific sentinel resolution step.** After
   `narrowHunks` validates offset ranges, a new IPS-only step
   checks for sentinel collisions. It either resolves them (if
   source bytes available) or rejects with a useful message:
   "record at offset 0x454F46 collides with the IPS EOF marker;
   conversion requires source bytes to resolve this." This step
   doesn't exist yet; the work is currently split between
   `narrowHunks` (generic reject) and `avoidSentinel` (fix-up).

3. **The direct-conversion error message improves for free.** The
   current error comes from `narrowHunk` and reads as a generic
   encoding-limits violation. The new IPS-specific step would
   explain *why* and *what would fix it*.

### What to fix: `avoidSentinel` safety is comment-enforced

`avoidSentinel` is total. If it can't fix a sentinel collision,
it silently produces a corrupt patch. Its safety depends on a
contract written in a comment: "only called from the with-source
path where the optimizer guarantees the source covers the
sentinel offset."

The contract holds today. But it's the kind of contract you have
to read the comment and hold in your head. Someone working on the
optimizer, or calling `avoidSentinel` from a new context, wouldn't
know they're responsible for maintaining this invariant unless
they find and read that comment.

Options for making it structural:

- **Make it fallible.** Return `Either SlapError [EncodedHunk]`.
  If it encounters a sentinel collision it can't fix, it fails.
  The with-source path always gets `Right` (the precondition
  still holds), but if someone breaks the precondition, they get
  a compile-time-visible `Either` they must handle, not silent
  corruption. Cost: callers handle an `Either` that currently
  can't fail. This is small.

- **Encode the precondition in the type.** Instead of a raw
  `ByteString` for source, take a type that witnesses "this
  source covers the sentinel offset." Heavier — adds a type that
  exists only to carry a proof. Makes the invariant unbreakable
  but costs readability.

- **Assert at the call site.** Guard in `encodeIPSPatch` that
  checks before calling `avoidSentinel`. Lightest touch — the
  function stays total, the invariant is checked once. But
  someone adding a new call site could skip the guard.

The fallible option is probably the sweet spot. The `Either` in
the signature is self-documenting: anyone calling
`avoidSentinel` sees it might fail, the compiler forces them to
handle that, and a new call path that breaks the precondition
gets a clear error instead of a corrupt patch.
