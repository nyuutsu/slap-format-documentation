# IPS rewrite — audit brief

Reference material for the `Slap.IPS` rewrite. Not a design doc: a gathering
of what the spec actually says, what BPS/UPS do that IPS should inherit,
and what the existing module gets right, wrong, or approximate.

The existing `Slap.IPS` tree is months-old and carries no design authority.
Where spec and existing code disagree, the spec wins. Where the spec is
silent, I flag the call as a judgment and give the data points I found.

-------------------------------------------------------------------------------

## 1. Wire format reconstruction

IPS has no canonical spec. The ZeroSoft document
(`zerosoft.zophar.net/ips.php`, now mirrored in
`upstream/zerosoft-source.md`) is short and covers only the base record
format — and has garbled unit conversions in its preamble, though the
tables themselves are sound. Everything else — 32-bit offsets, truncation
marker, EBP metadata, EOF sentinel avoidance — is de-facto-from-one-
implementation, codified by community convention across two or three
downstream tools.

### 1.1 StandardIPS (`PATCH` / `EOF`)

Authority: ZeroSoft spec; Archiveteam `fileformats.archiveteam.org`
wiki; Flips `libips.cpp`; SnesLab wiki.

```
  magic        5 bytes  "PATCH"                         (not NUL-terminated)
  records      *        see below, zero or more
  trailer      3 bytes  "EOF"                           (not NUL-terminated)
  [truncate]   3 bytes  big-endian target size          (Flips extension)
```

Regular record:

```
  offset       3 bytes  big-endian, 0..0xFFFFFF
  size         2 bytes  big-endian, 1..0xFFFF
  data         `size`   payload bytes
```

RLE record (size == 0):

```
  offset       3 bytes  big-endian, 0..0xFFFFFF
  size         2 bytes  big-endian, 0              (sentinel)
  rle_size     2 bytes  big-endian, 1..0xFFFF      (ZeroSoft: "Any nonzero value")
  value        1 byte   fill byte
```

Hard limits (arithmetic on field widths, not values in the format):
- Max offset is `0xFFFFFF` = 16 MiB − 1.
- Max record payload is `0xFFFF` = 65535 bytes.
- Therefore the highest byte a single record can touch is at
  position `0xFFFFFF + 0xFFFF - 1 = 0x100FFFD = 16,842,749`
  (the last of 0xFFFF bytes written starting at offset 0xFFFFFF).
  The Archiveteam wiki phrases the same limit as an exclusive
  bound: "IPS patches cannot affect bytes beyond offset 16842750
  (0x100fffe = 0xffffff + 0xffff)."
- Flips refuses to create patches for targets > 16,777,216 bytes
  (`libips.cpp:202`, verified locally).

Truncation marker: after `EOF`, per the Archiveteam wiki, "the
end-of-file marker may be followed by a three-byte length to which the
resulting file should be truncated. Not every patching program will
implement this extension, however." Flips does implement it: on create,
`write24(targetlen)` when `sourcelen > targetlen` (`libips.cpp:344`,
verified locally); on parse, reads truncation only when exactly 3 bytes
remain (`patchat+3 == patchend`, `libips.cpp:77`, verified locally).
slap should honor it for StandardIPS. The wiki is silent on IPS32
truncation (§1.2).

Note: SNESTool v1.2's DOC describes a concept called "IPS 2" for
cutting files, distinct from standard IPS. Whether this is the same
mechanism as the 3-byte truncation trailer or something else is
unknown — see `spec.md` "Truncate extension" for the full discussion.

### 1.2 IPS32 (`IPS32` / `EEOF`)

Authority: `leoetlino/sips` (`sips.cpp`) — the only implementation with
any claim to being a spec. A handful of downstream tools (incl. slap)
match it. byuu/Alcaro declined to extend IPS and wrote UPS/BPS instead.

```
  magic        5 bytes  "IPS32"
  records      *        offset is 4 bytes big-endian; size/RLE identical to IPS
  trailer      4 bytes  "EEOF"                     (0x45 45 4F 46)
```

Hard limits:
- Max patchable offset is `0xFFFFFFFF` − epsilon.
- Max record payload is still `0xFFFF` (the size field did not widen).

Truncation marker for IPS32 is **not specified by any authoritative
source.** sips.cpp does not emit one. slap currently parses and emits
a 4-byte truncation marker. **Judgment call to flag: keep, drop, or
retain as slap-only extension with a warning.**

### 1.3 EBP (EarthBound Patch)

Authority: `Lyrositor/EBPatcher` (`EBPPatch.py`). No romhacking.net spec
page. Single-implementation de-facto.

```
  magic        5 bytes  "PATCH"                    (structurally IPS)
  records      *        standard IPS records
  trailer      3 bytes  "EOF"
  metadata     *        raw UTF-8 JSON object, consumed to end of file
```

Canonical JSON fields per the reference:

```json
  { "patcher": "EBPatcher",
    "title":       "...",
    "author":      "...",
    "description": "..." }
```

The reference impl emits exactly these four keys. `patcher` is the
discriminator — EBPatcher rejects patches whose `patcher` field isn't
`"EBPatcher"`.

slap's EBP JSON detection is deliberately crude. It checks two
things: "does the trailer begin with `{`" (shape-gated entry), and
— in Describe — "if any of the four known fields (`patcher`,
`title`, `author`, `description`) are present, what's in them?"
That is the entire machinery.

Going further gets into "an entire JSON parser" territory. The EBP
spec, read literally, permits any Unicode encoding for the metadata
blob — which is absurd, but spec-compliant — and the original
author probably didn't realize the implication. In practice every
EBP patch we've seen uses the JSON blob to store four strings in
UTF-8 and nothing else. Building a robust parser to handle the
insane-spec case correctly is end-of-project polish at best; not a
near-term task and possibly never.

On "truncation-marker-then-JSON" inside an EBP trailer: no canonical
EBP tool emits that shape. slap doesn't either. We apply a spec and
we do it correctly — "topologically two lines would let us emit X"
is irrelevant.

### 1.4 EOF sentinel collision (0x454F46 / 0x45454F46)

Authority: Archiveteam wiki (explicit, spec-adjacent); Flips
`libips.cpp` (canonical implementation); SnesLab / Lunar release notes
/ nesdev discussions.

The 3-byte record offset `0x454F46` reads identically to the `"EOF"`
trailer. Any applier that scans the record stream eagerly will stop at
the first record whose offset happens to equal `0x454F46`, treating its
real payload as post-EOF garbage. The Archiveteam wiki calls this out
as one of the format's named pitfalls:

> Programs generating IPS files should avoid generating hunks with
> offset 0x454f46, as the byte encoding of this offset may be
> misinterpreted as the end-of-file marker. (One way to do it is to
> generate them with offset 0x454f45 and include the preceding byte.)

Encoder convention (matches the wiki exactly): shift the record offset
back by one and extend the payload forward by one, prepending the
source byte at `offset − 1`. Flips `libips.cpp` implements it as
(`libips.cpp:247-251`, verified locally):

```c
  if (offset == 0x454F46) { offset--; thislen++; }   // avoid premature EOF
```

On the parse side, Flips loops `while (offset != 0x454F46)` — an
unconditional hard stop with no lookahead (`libips.cpp:53`, verified
locally). A record at this offset is unparseable in Flips.

The shift-and-prepend fix needs `source[offset-1]`. With source
(normal create, source-attached conversion), `avoidSentinel` reads
that byte and substitutes, and the issue is invisible downstream.
Without source (direct format→IPS conversion with no ROM), there's
no byte to prepend, so the fix is impossible.

What's still possible without source is *detection*, and it's
exact. The collision happens iff some record has offset equal to
`0x454F46` — no more, no less. The ambiguity lives only in the
offset field at a record boundary, not in payload bytes; records
that write *through* the sentinel byte but start elsewhere are
fine. The input record list names every offset we're about to
emit, so one equality check per record answers the question with
certainty.

So slap's rule, explicitly: reject when any record offset equals
the sentinel; otherwise proceed. This is a deliberate design call.
No probabilistic hedging. Every rejection catches a patch that
would actually break; every non-rejection is a patch that won't.
No false positives, no false negatives.

Rejection is the forced move because we can't repair without
source, not because we're guessing. "Anal" only in the sense that a
sufficiently careful parser could lookahead past a sentinel-shaped
offset field and disambiguate from what follows — and we could rely
on that, in principle. No IPS parser we've examined does this, slap
included. slap's own parser peeks the next 3 bytes at each record
boundary; when they equal `45 4F 46` it treats them as the trailer
and stops, exactly like Flips's `while (offset != 0x454F46)`.
Neither does lookahead-through-the-offset-into-the-size-field to
disambiguate.

Failure modes across the ecosystem for a record at the sentinel:
strict parsers (slap, Flips) error on re-parse; permissive tools
silently stop at the false EOF and apply only the records before
it, producing a partially-patched ROM with no error surfaced. The
second mode is the one that motivates rejection-at-encode — a
clean upfront error is much kinder to the user than silent partial
application hours later. A conceptually-smarter lookahead parser
could resolve most cases unambiguously, but it leaves a residual
class of genuinely-ambiguous inputs and produces output that only
the smarter parser can read. Slap-only IPS patches defeat the
point of emitting IPS in the first place.

IPS32 analog: `"EEOF"` = `0x45454F46`. slap's `avoidSentinel` already
generalizes over both widths via `ipsSentinel` / `ips32Sentinel` in
`Slap.Measure`. This part of the existing code is already correct
and should survive the rewrite.

### 1.5 Trailing bytes after the trailer

No spec covers this. Observed possibilities:
- `EOF` then nothing. (Canonical.)
- `EOF` then 3-byte truncation marker. (Flips extension.)
- `PATCH`..`EOF` then JSON object. (EBP.)
- `EOF` then a ROM appended by accident or by a concatenating tool.

slap's parser currently treats trailing bytes as: `{` → EBP JSON; else
try 3-byte truncation then optional JSON; else nothing. The "else try
truncation" branch is permissive — any non-`{` trailing bytes become a
truncation value silently, even garbage. **Flag for the rewrite:** add a
sanity bound on the truncation value (e.g. reject if truncate <
maxRecordEnd, or warn).

-------------------------------------------------------------------------------

## 2. BPS/UPS practices the IPS rewrite must apply

Derived from reading `src/Slap/BPS/*.hs` and `src/Slap/UPS/*.hs` in full.

### 2.1 Load-bearing (structural, not taste)

**Parse returns `Either SlapError <Patch>`; apply returns `Either
SlapError TargetFileContents`.** No `IO Int`, no throw, no partial
functions. Existing IPS violates this on both ends
(`applyIPS :: IPSPatch -> FilePath -> IO Int`,
`applyIPSMemory :: IPSPatch -> SourceFileContents -> TargetFileContents`).
The rewrite must unify on the BPS/UPS shape.

**Parse-time vs apply-time separation.** Parsing validates structure
(magic, trailer, per-field byte widths, RLE count non-zero) and
returns a fully-decoded patch. Apply validates semantics (bounds,
cursor, target-size fit) and performs the write. Each layer has one
job. BPS/UPS even split `parseBPSBody`/`parseUPSBody` as pure `Get`
actions inside `runGet`, with the CRC framing done outside. IPS should
adopt the same split even though it has no CRC.

> **STILL DESIRED.** The IPS rewrite did not adopt this split —
> `parseRecordsAndCaptureTrailer` returns an ad-hoc
> `([IPSRecord], ByteString)` tuple and `buildResultPatch`
> finalizes from it. Endorsed in later review as "cute symmetry
> we want if we can have it." Tracked in `slap-vs-spec.md`.

**Body type vs. parsed-patch type split.** `BPSBody`/`UPSBody` are what
the `Get` action returns; `BPSPatch`/`UPSPatch` are what the parser
finalizes (after CRC validation, after any list→vector materialization).
The existing IPS module has `IPSPatch` only. Introducing `IPSBody` gives
the parser a natural unit of work and lets `parseIPS` own the
trailer-and-variant discrimination without threading it into `Get`.

**Named constants for framing.** `bpsMagicLength`, `bpsCRC32Length`,
`bpsFooterLength`, `bpsOverheadLength`, `upsTerminatorByteLength`. These
are not magic numbers inside the parser; they are named `Length` values
exported from `Types.hs`. IPS should do the same: `ipsMagicLength`,
`ips32MagicLength`, `ipsEOFLength`, `ips32EEOFLength`,
`ipsTruncateMarkerLength variant`, etc.

**Role newtypes on error constructors.** When an error variant would
otherwise have two `Int` fields of the same base type, the two get
distinct newtype wrappers so the constructor can't be called with the
fields transposed. `Slap.Measure` already exports
`RequestedLength`/`RemainingLength`/`ReadOffset`/`WritePosition`/etc.
IPS errors should pull from that vocabulary rather than raw `Int`.

**Vector for record streams.** Both BPS and UPS parse into a `[Action]`
list inside `Get`, then materialize once at the boundary:
```haskell
  , bpsActions = Vector.fromList (bpsBodyActions body)
```
The apply loop indexes by `ActionIndex`. This is not taste — BPS's
docstring at `Types.hs:38–45` explicitly attributes ~90 MB of GC
overhead on stadium2-scale patches to the choice. IPS at stadium2
scale (27 MB `.ips32`) is in the same territory and should match.

**Semantic action stream driven by a tail-recursive apply loop with an
IORef escape hatch.** BPS's `applyActionStream` is a five-argument
tail-recursive `IO` function: `actionIndex`, `outputPosition`, plus
any cursor state. Failures call `abort :: ApplyError -> IO ()` which
writes into a pre-allocated `IORef` and returns. After the loop,
`readIORef` yields `Maybe ApplyError`. This avoids `ExceptT` in `IO`
and avoids exceptions. IPS apply should match this shape.

**`unsafePerformIO` around `create`.** Both `applyBPS` and `applyUPS`
allocate a strictly-sized output buffer via `Data.ByteString.Internal.create`
inside `unsafePerformIO`, passing a `runApply`-style IO callback that
writes into the buffer. The `unsafePerformIO` is safe because the
callback is pure in effect (writes only to the freshly-allocated buffer
and an error IORef that never escapes).

**Bounds validation via `fitsWithin` / `remainingFromOffset` /
`subtractLength` / `minLength`.** `Slap.Measure` exposes a small
algebra of `Offset`/`Length`/`FileSize` operations that keep the
apply loop free of raw-`Int` arithmetic. IPS apply should use the
same algebra instead of `offsetToInt` + `+` + `min` + `max`.

**Helper-function hygiene for tight loops.** UPS's `copySourceSlice`
and `xorSourceSlice` hoist the base pointer outside the inner loop and
split in-bounds / past-source-end into two passes. The IPS rewrite will
have a similar pattern for "copy record payload into target region"
and should follow this style.

### 2.2 Taste (stylistic, but pervasive)

- **Long, descriptive local names.** `outputPosition`, `readStart`,
  `safeSourceStart`, `sourceRelative`, `nextTargetRelative`. Never
  `off`, `pos`, `i`, `acc`, `n`.
- **Record-of-fields patches with explicit bang patterns** on the
  strict-tracked cursors.
- **Pattern match inside `applyActionStream` with short-name helpers**
  (`handleSourceRead`, `handleSourceCopy`, `handleTargetCopy`) rather
  than inline case bodies.
- **Commentary explains tradeoffs at the point of the decision.** E.g.
  BPS's `Vector` rationale, UPS's two-phase loop rationale, the
  `unsafeCreate` vs `create` rationale in UPS create. IPS has almost no
  commentary of this kind.
- **Describe/Explain threads a region-walker accumulator** (`mapAccumL`
  in BPS, `foldl'` with `buildRegion` in UPS). The existing IPS
  `makeIPSRegion` is a single-argument function with no accumulator
  because IPS doesn't need a running cursor — but if we decide to surface
  zero-fill-past-source regions or truncation as an explicit final
  region, we will need an accumulator.

### 2.3 What BPS/UPS do that IPS *doesn't* need

- **No CRC.** IPS has no checksum anywhere in the format. There is no
  `ipsPatchCRC` / `ipsSourceCRC` / `ipsTargetCRC`. `parseIPS` has one
  fewer validation pass than `parseBPS`.
- **No TargetUnderfilled / ApplyCursorUnderflow / ApplyTargetReadUnwritten.**
  IPS records are random-access writes, not a sequential cursor.
  `ApplyError` variants that BPS apply uses for cursor walks don't
  apply. IPS apply gets its own shorter list of semantic failures
  (see §4).
- **No `displace`/`examineSignedOffset`/`SignedOffset`.** IPS has no
  signed relative-offset deltas; all offsets are absolute 24-bit or
  32-bit unsigneds.

-------------------------------------------------------------------------------

## 3. The strict apply pattern, in my own words

BPS/UPS apply follows one shape. In prose:

1. **Short-circuit at the parsed type.** If `targetSize < 0` or
   `targetSize == 0`, return immediately with the trivial result. The
   buffer allocation path only runs when there is genuine work.

2. **Allocate a strictly-sized output buffer.** The `create
   (unFileSize targetSize)` call gives us a `ByteString` of exactly the
   declared target size. Every subsequent write is addressed by a
   validated `Offset` into that buffer — never past it, never short.

3. **Carry the error out of IO via a pre-allocated IORef.** Inside the
   `create` callback (which is `IO`), we run the apply loop. Any
   semantic failure writes an `ApplyError` into an `IORef (Maybe
   ApplyError)` and returns — the loop aborts by early return, not by
   exception. After `create` completes, the outer layer reads the
   `IORef`: `Nothing` means the buffer is a valid target;
   `Just applyErr` means we must wrap it as `ApplyFailed label err`
   and discard the buffer.

4. **Walk the action stream with a tail-recursive IO function.** The
   function takes `ActionIndex` plus whatever cursor state the format
   needs (output position, signed source relative, signed target
   relative for BPS; just output position for UPS). Each recursive
   call is `recurse newOutput …` — there is no accumulator list, no
   mutable loop counter. The action stream lives in a `Vector` and is
   indexed by `unActionIndex`.

5. **Validate each action's bounds BEFORE it writes.** Every handler
   starts with "does this action's claimed write region fit within
   the remaining target capacity, and is its source region inside the
   source file (or whatever the format reads from)?" If not, `abort`
   and return. A handler never partially writes and then discovers
   the action was invalid: the check is the first thing it does.

6. **At end-of-stream, cross-check that the buffer was fully filled.**
   For BPS: if `outputPosition /= targetSize`, abort with
   `ApplyTargetUnderfilled`. No corresponding over-write check is
   needed because `ApplyWritesPastTarget` per-action prevents it
   upstream — that proof is noted in the BPS source comment. UPS
   handles the under-fill case differently: the end-of-stream branch
   does a tail copy from source, so the buffer is always exactly
   filled and there is no `ApplyTargetUnderfilled` there.

The IPS rewrite should honor steps 1–5 exactly. Step 6 is the
asymmetry: IPS has no declared target size. Options for the IPS
equivalent are in §5.

-------------------------------------------------------------------------------

## 4. Existing IPS module — bugs, keepers, dead code

### 4.1 Diagnosis, per concern

> **ADDRESSED** by commit `18ad06b` (2026-04-11, *IPS.Apply:
> from-scratch rewrite to BPS/UPS standards*). `applyIPS` is now
> `SourceFileContents -> IPSPatch -> Either SlapError TargetFileContents`,
> built on the `unsafePerformIO` + `create` + `IORef` pattern with
> per-record bounds checks via `fitsWithin`. `applyIPSMemory` no
> longer exists. Diagnosis retained below as design record of why.

<!--
**`Slap.IPS.Apply` is permissive where BPS/UPS are strict.**
- `applyIPS :: IPSPatch -> FilePath -> IO Int` opens the target file
  read-write, seeks and writes each record in place, optionally calls
  `hSetFileSize`, and returns a record count. No bounds checking. No
  error type. If a record's offset or payload extends past the current
  file end, `ByteString.hPut` will extend the file — which may or may
  not be the intent, but it happens silently.
- `applyIPSMemory :: IPSPatch -> SourceFileContents -> TargetFileContents`
  is total (non-`Either`): computes `outputLength = maybe maxRecordEnd
  unFileSize truncation`, allocates, copies source, zero-fills past
  source, then writes each record. A malformed patch cannot cause a
  failure here — it can only produce a nonsense target.
- Both paths allow overlapping records, out-of-order records,
  records-past-source, and records-past-truncation silently.
-->

> **ADDRESSED** by commits `49f691d` (2026-04-11, *IPS.Parse:
> from-scratch rewrite to BPS/UPS standards* — whose message
> explicitly notes "Replaces the speculative trailing-bytes
> handling") and `921d220` (which replaced the free-form trailer
> `ParseError` strings with a structured `UnrecognizedTrailer`).
> `buildResultPatch` now accepts only empty / exact-marker-length /
> `{`-prefixed trailers for StandardIPS, and only empty for IPS32;
> anything else is `UnrecognizedTrailer`. Diagnosis retained below.

<!--
**`Slap.IPS.Parse` has speculative trailing-bytes handling.**
- Line 92–98: after `EOF`, if the next byte is `{` → EBP JSON. Else
  try to read 3 (or 4) bytes as a truncation marker, then check the
  byte *after* that for `{` → truncation + EBP JSON. There is no sanity
  bound on the truncation value and no explicit spec authority for
  "truncation-then-JSON" order (see §1.3). Per the research in §1.3,
  the writing side of that shape is a slap-only extension.
- Trailing garbage becomes silent "truncation marker" with no warning.
  A `.ips` file with a few random bytes glued on will parse as
  `ipsTruncate = Just someNonsense`.
-->


**`Slap.IPS.Types` makes several illegal states representable.**
- **ADDRESSED** by commit `9d2171b` (2026-04-11, *IPS.Types:
  from-scratch rewrite to BPS/UPS standards*). `IPSPatch` no longer
  carries `ipsEBPMeta`; EBP metadata lives on an `EBPPatch` wrapper
  holding an `IPSPatch` plus an `EBPMetadata` newtype around
  `ByteString`. The "IPS32 with EBP metadata" nonsense state is
  unrepresentable. Original diagnosis retained below.
  <!--
  `IPSPatch` carries `ipsVariant :: IPSVariant` (`StandardIPS | IPS32`)
  AND `ipsEBPMeta :: Maybe ByteString`. The combination "IPS32 with
  EBP metadata" is representable but nonsense: EBP is defined only
  as an extension of standard IPS. `SomePatch.hs:261-264` pattern
  matches exactly on `(StandardIPS, Just _) -> LabelEBP` and treats
  `(IPS32, _)` as plain IPS32 — i.e. the type permits a state the
  business logic then silently ignores. A sum
  ```haskell
    data IPSContainer
      = IPSContainerPlain
      | IPSContainerEBP !ByteString
      | IPSContainerIPS32
  ```
  (or a GADT over variant) would make the nonsense state unrepresentable.
  This is exactly the newtype-maximalist feedback.
  -->
- **ADDRESSED** by commits `9d2171b` (`IPSRecord` collapsed to a
  single sum with a constructor-agnostic `ipsRecordOffset` accessor)
  and `921d220` (*IPS: hoist `recordPayloadLength` to Types; structure
  trailer error* — hoisted the length accessor alongside it).
  Describe no longer needs a local `ipsOffset` helper. Original
  diagnosis retained below.
  <!--
  `IPSRecord` and `IPSRecordRLE` have distinct field accessors
  (`ipsRecordOffset` vs `ipsRleOffset`), forcing `Describe.hs:77-78` to
  define its own `ipsOffset` helper. A single record sum with a
  shared `offset` field (either through a common prefix record or
  via pattern matching once in an `ipsRecordOffset :: IPSRecord ->
  Offset` function exported from `Types.hs`) removes the duplication.
  -->
- `ipsRecordOffset` is a raw `Offset` (derived from `Int`). A standard
  IPS record has offset in `0..0xFFFFFF` and payload size in
  `1..0xFFFF`; the type doesn't capture that. Options: variant-tagged
  newtype (`IPSOffset variant`), smart constructor returning
  `Either SlapError IPSRecord`, or refine at parse time only. BPS/UPS
  don't have this problem because their cursor is driven by deltas,
  not absolute offsets.
- **PARTIALLY ADDRESSED.** Commit `9d2171b` removed the
  `ipsCleanEOF :: Bool` field. The audit's proposal — that the
  "trailer was / wasn't there" state belongs in the type system as a
  named variant, not a Bool — still stands. It was not carried
  through during the rewrite. Instead, `2f23350` (*SomePatch:
  graceful fallback for truncated IPS bodies*) reconstructed the
  missing-trailer handling at the SomePatch layer by pattern-matching
  on `ParseError LabelIPS` and synthesizing a 0-record `IPSPatch`
  fallback with a `NoEOFMarker` warning.

  The typeful reshape remains open. nyuu: "I'm drawn to this sort
  of thing, so I'll probably want to do it later." Original
  diagnosis retained below.
  <!--
  `ipsCleanEOF :: Bool` — a two-state flag that's true when a proper
  trailer was found. The parser currently uses it to emit a
  `NoEOFMarker` warning at `SomePatch.hs:266`. `Bool` here is pretty
  clearly two values that both deserve names:
  `TrailerPresent | TrailerMissing`.
  -->

> **ADDRESSED** by commits `9d2171b` (introduced the `OffsetWidth =
> Offset24 | Offset32` sum and `offsetWidthByteCount :: OffsetWidth
> -> Length` alongside `IPSVariantSpec`) and `49f691d` (*IPS.Parse:
> from-scratch rewrite* — commit message explicitly notes "bare-Int
> width parameterisation"). The raw 3/4 integers never appear in
> parser signatures. Diagnosis retained below.

<!--
**`parseRecords` is parameterized by `offsetWidth :: Int`.** Line 39:
```haskell
  parseRecords :: IPSVariant -> Int -> Word32 -> Get IPSPatch
```
That `Int` is either 3 or 4. BPS/UPS don't have this kind of
width-parameterized parser because their format has no variants. IPS
does, so some polymorphism is unavoidable — but the parameterization
should not be "magic integers 3 and 4 flow through function
signatures." A sum type `IPSOffsetWidth = OffsetWidth24 | OffsetWidth32`
with a `widthBytes :: IPSOffsetWidth -> Length` accessor removes the
footgun.
-->

> **ADDRESSED** by commits `602eb9f` (*IPS.Create: from-scratch
> rewrite; lift DP optimizer to IPS.Optimize*) and `cbad82c` (*Measure:
> move splitHunks out of IPS.Create*). The eighteen-export surface is
> gone: the DP optimizer now lives in `Slap.IPS.Optimize`, the
> format-agnostic `splitHunks` moved to `Slap.Measure` next to the
> `Hunk` type, and the four near-duplicate top-level encoders
> collapsed into one parameterised `encodeIPSPatch` plus a thin
> `encodeEBPPatch` wrapper. The proposed module split was carried
> through with `IPS.Optimize` used in place of the proposed
> `IPS.Create.Optimal`. Diagnosis retained below.

<!--
**`Slap.IPS.Create` is doing several jobs at once and under-exports
seams.** From `src/Slap/IPS/Create.hs`:
```haskell
module Slap.IPS.Create
  ( encodeIPSRecord , encodeOffset , allSame , avoidSentinel
  , encodeIPS , encodeIPS32 , encodeEBP , encodeEBPRaw , encodeTruncation
  , ebpJson , optimalIPSRecords , diffRaw , mergeGaps , partitionOptimal
  , findByteRuns , ensureMaxGap , splitHunks ) where
```
Eighteen exports. Five of them are top-level encode entry points
(`encodeIPS`, `encodeIPS32`, `encodeEBP`, `encodeEBPRaw`, and a
lower-level `encodeIPSRecord`). The other thirteen are a mix of
wire-level primitives (`encodeOffset`, `allSame`, `encodeTruncation`,
`ebpJson`), the sentinel-avoidance post-pass (`avoidSentinel`), the
greedy hunk splitter (`splitHunks`, consumed by `Slap.Convert`), and
the DP optimal-record engine (`optimalIPSRecords` plus its six
internal helpers `diffRaw`, `mergeGaps`, `partitionOptimal`,
`findByteRuns`, `ensureMaxGap`, `computeRLEPredecessors`). For the
rewrite, these should split cleanly into:

- `Slap.IPS.Types` — framing constants, sum type for
  variant/container/offset-width, record types.
- `Slap.IPS.Parse` — parse-only, returns `Either SlapError IPSPatch`.
- `Slap.IPS.Apply` — apply-only, returns `Either SlapError TargetFileContents`.
- `Slap.IPS.Describe` — info, meta, explain.
- `Slap.IPS.Create` — the two public creators (`createIPS`,
  `createIPS32`, `createEBP`) plus `splitHunks` for convert, with the DP
  engine living in `Slap.IPS.Create.Optimal` (new submodule).
-->

> **SUPERSEDED** — this analysis describes an export surface that no
> longer exists. Commit `602eb9f` (*IPS.Create: from-scratch rewrite*)
> collapsed the four near-duplicate top-level encoders (`encodeIPS`,
> `encodeIPS32`, `encodeEBP`, `encodeEBPRaw`) into one parameterised
> `encodeIPSPatch :: IPSVariant -> SourceFileContents -> [EncodedHunk]
> -> Maybe FileSize -> PatchFileContents` plus a thin `encodeEBPPatch`
> wrapper, exactly as suggested in the final sentence of this
> subsection. The follow-up "could move splitHunks" note was acted on
> by `cbad82c` (now in `Slap.Measure`). The zero-dead-exports finding
> still holds for the current surface. Original analysis retained
> below.

<!--
**Encoder dead-code / near-dead-code (fact, not suspicion).**

`splitHunks` — pre-grepped usage:
```
src/Slap/Convert.hs:27:  import Slap.IPS.Create (splitHunks)
src/Slap/Convert.hs:443:  records <- narrow (splitHunks ipsMaxRecordData (contentsRecords contents))
src/Slap/Convert.hs:446:  records <- narrow (splitHunks ipsMaxRecordData (contentsRecords contents))
src/Slap/Convert.hs:449:  records <- narrow (splitHunks ipsMaxRecordData (contentsRecords contents))
src/Slap/Convert.hs:470:  let ppfResult = PPF.encodePPF3 (splitHunks 255 (contentsRecords contents)) description
```
Live. Consumed by `Slap.Convert` for IPS / IPS32 / EBP direct conversion
and by PPF3 for its 255-byte record cap. Must stay public after
the rewrite. Note: `splitHunks` is currently in `Slap.IPS.Create` but
it's a format-agnostic list chunker. It could move to `Slap.Binary`
or `Slap.Convert` as a follow-up; not blocking.

`encodeIPS`, `encodeIPS32`, `encodeEBP`, `encodeEBPRaw` — pre-grepped:
```
src/Slap/Convert.hs:444:  Right (CreateResult (IPS.encodeIPS source records …) [])
src/Slap/Convert.hs:447:  Right (CreateResult (IPS.encodeIPS32 source records …) [])
src/Slap/Convert.hs:465:  Just raw -> CreateResult (IPS.encodeEBPRaw source records … raw) []
src/Slap/Convert.hs:466:  Nothing  -> CreateResult (IPS.encodeEBP source records … …) []
```
All four are live, all called exclusively from `Slap.Convert` for
direct conversion. The `createFromMemory … CreateIPS` / `CreateIPS32`
/ `CreateEBP` path, used by all the round-trip property tests, also
flows through `Slap.Convert`. So these four live encode entry points
are not a dispatch over something a caller selects at runtime — they
are four exports that `Slap.Convert` picks by pattern match. The
rewrite can collapse them to one entry point taking a container sum,
but that's a refactor choice, not a dead-code cleanup.

`encodeIPSRecord`, `encodeOffset`, `encodeTruncation`, `avoidSentinel`,
`allSame`, `ebpJson`, `optimalIPSRecords` and the DP internals — all
called from within `Create.hs` itself or from test code (`RoundTrip.hs`
imports `avoidSentinel` and `optimalIPSRecords`;
`Encoding.hs` imports `ebpJson`; `Describe.hs` imports `jsonPairs`,
`jsonFieldCI`). No dead exports.

State as fact: **there is no dead code in `Slap.IPS.Create`. The four
encoder entry points are all live, and `splitHunks` is a cross-format
utility consumed by Convert.**
-->

> **ADDRESSED** — the "Keep" recommendation was reconsidered. Commit
> `06ce9e6` (*IPS.Describe: from-scratch rewrite to BPS/UPS standards*)
> dropped the ad-hoc scanner from Describe, and `59538ac` (*Slap.JSON:
> port jsonPairs/jsonFieldCI out of IPS.Describe*) gave the helpers a
> format-neutral home so Convert can import them without reaching
> into IPS internals. The parser is still tiny and aeson-free; it
> just lives in the right module now. Diagnosis retained below.

<!--
**`Slap.IPS.Describe` has its own mini-JSON parser (`jsonPairs`,
`jsonFieldCI`) that's used both by Describe and by `SomePatch.hs` to
extract EBP metadata.** This is fine as-is — there's no aeson
dependency, and the parser is tiny. Not worth touching in the rewrite
unless we find an actual bug. Keep.
-->

### 4.2 Apply.hs type signature — quoted

> **ADDRESSED** by commit `18ad06b` (*IPS.Apply: from-scratch rewrite
> to BPS/UPS standards*). The signature converged on
> `applyIPS :: SourceFileContents -> IPSPatch -> Either SlapError
> TargetFileContents`, and the file-handle round-trip tests were
> migrated in commit `2ec253f` (*SomePatch: rewrite IPS dispatch for
> new parse/apply API*). Quoted old signatures retained below as
> design record of why this section existed.

<!--
Asked in the prompt whether the existing `IPS/Apply.hs` returns
`Either SlapError ByteString` strictly. It does not.

```haskell
-- src/Slap/IPS/Apply.hs:22-23
applyIPS :: IPSPatch -> FilePath -> IO Int
applyIPS patch target = withBinaryFile target ReadWriteMode $ \handle -> do
```

```haskell
-- src/Slap/IPS/Apply.hs:44-45
applyIPSMemory :: IPSPatch -> SourceFileContents -> TargetFileContents
applyIPSMemory patch (SourceFileContents source) = TargetFileContents $ unsafeCreate outputLength $ \outputPointer -> do
```

Both are permissive. `applyIPS` mutates a file on disk and returns a
count. `applyIPSMemory` is infallible and returns a buffer. Compare
the BPS / UPS shape:

```haskell
-- src/Slap/BPS/Apply.hs:80
applyBPS :: BPSPatch -> SourceFileContents -> Either SlapError TargetFileContents

-- src/Slap/UPS/Apply.hs:37
applyUPS :: UPSPatch -> SourceFileContents -> Either SlapError TargetFileContents
```

The rewrite must converge on
`applyIPS :: IPSPatch -> SourceFileContents -> Either SlapError TargetFileContents`.

The file-handle path currently used by the round-trip tests
(`test/Props/RoundTrip.hs:158,168,236,246` and
`test/Props/Contracts.hs:179` all via `applyViaFile IPS.applyIPS`) will
need a new helper that materializes the source, calls pure `applyIPS`,
and writes the result back. That's a test-harness change, not a
behavioral one.
-->

### 4.3 Behaviors worth preserving

These are in the existing module and should survive the rewrite:

- **Magic discrimination:** check `PATCH` → StandardIPS, else `IPS32` →
  IPS32, else BadMagic. Simple and correct. No ambiguity.
- **RECONSIDERED: RLE-count-zero rejection.** The original audit
  lean was "keep rejection" (matching Flips). We later changed our
  minds — a zero-length RLE is a no-op, not corruption — and
  `slap-vs-spec.md` records the current plan as "accept and warn."
  Confidence is low; we may tighten back to rejection later.
  Original lean:
  <!--
  **RLE-count-zero rejection at parse time** (`Parse.hs:72-74`). Per
  spec, `rle_size` must be non-zero.
  -->

- **Big-endian sentinel constants in `Slap.Measure`:**
  ```haskell
    ipsSentinel   :: Word32   -- 0x454F46
    ips32Sentinel :: Word32   -- 0x45454F46
  ```
  Good. Keep.
- **`avoidSentinel`** as an encode-time post-pass that shifts a record
  back by one and prepends a source byte. Exactly the Flips
  convention. Keep.
- **Direct-conversion sentinel rejection** when no source is available
  (`prop_ipsSentinelDirect`, `prop_ips32SentinelDirect` in
  `test/Props/Contracts.hs:131-166`). Keep; this is the *feature*
  from CLAUDE.md's "source-less conversion is contract-checked and
  often says 'no' — that's the feature, not a coverage gap."
- **DP optimal record partition** (`optimalIPSRecords` and its
  internals). Produces smaller patches than the greedy splitter —
  verified by `prop_dpNotLarger` / `prop_dpIPS32NotLarger` in
  `test/Props/RoundTrip.hs:190-207`. Keep; move the internals to a
  submodule.
- **Permissive EBP JSON field extraction** (case-insensitive lookup,
  tolerant of missing fields). Keep; this lets slap read EBP patches
  from every known encoder.
- **`ipsCleanEOF` → `NoEOFMarker` warning path.** Keep the concept,
  but replace the `Bool` flag with a two-state sum.
- **Truncation marker for StandardIPS.** Well-documented via
  Flips/Lunar. Keep.

### 4.4 Behaviors to drop or flag

- **`applyIPS :: FilePath -> IO Int`** — drop. Replace with
  `SourceFileContents -> Either SlapError TargetFileContents`.
- **`applyIPSMemory` totality** — drop the infallible shape. Return
  `Either SlapError TargetFileContents`.
- **Encoding truncation marker inside EBP** (`encodeEBP`, `encodeEBPRaw`
  pipe through `encodeTruncation`). No reference implementation emits
  that shape. Recommend: drop from the writer; keep lenient parse
  for robustness.
- **Emitting truncation marker for IPS32** (`encodeIPS32` with
  non-`Nothing` truncation). No authoritative source. **Flag for
  discussion.** Options: (a) drop entirely, (b) keep as slap-only
  extension and emit a warning on parse.
- **Parsing trailing-non-`{` bytes as a truncation marker with no
  sanity bound.** Add a bound: reject if `truncate` value exceeds
  `maxRecordEnd + some-slack`, or at minimum emit a warning.

-------------------------------------------------------------------------------

## 5. Strict apply for IPS — the asymmetry with BPS/UPS

IPS has no declared target size. The writer never promises "the output
is exactly N bytes"; the patch just lists regions to modify. BPS/UPS
can check `outputPosition == targetSize` at end-of-stream because
`targetSize` came from the parsed body; IPS cannot.

Options for the target-size determination step (there's no right
answer here — pick and flag):

**Option A — derive output size.**
`outputLength = maybe (max sourceLen maxRecordEnd) unFileSize truncate`
Same as today's `applyIPSMemory`. Simple, matches Flips behavior for
expanding-file cases. Downside: a single malformed record with a
huge offset blows up the allocation before any validation runs.

**Option B — derive output size, but reject out-of-bounds records at
parse time.**
Parse validates every record against the variant's max offset
(`0xFFFFFF` for StandardIPS, `0xFFFFFFFF` for IPS32) and per-record
payload (`0xFFFF`). Apply trusts the parser and computes
`outputLength` as in Option A. This is the pattern BPS/UPS already
follow: parse rejects negative sizes (`Parse.hs:53-58` in BPS),
apply trusts. I think this is the right answer.

**Option C — explicit target-size field in the parsed patch.**
Synthesize a `FileSize` at parse time (= `maxRecordEnd` or the
truncation marker) and store it on `IPSPatch`. Apply allocates a
buffer of exactly that size and does a BPS-style
`outputPosition == targetSize` check. This makes the apply loop
symmetric with BPS/UPS at the cost of hiding the "computed vs declared
target size" distinction behind a field name.

Recommend B. It keeps the parsed `IPSPatch` honest about the format
(no computed fields that look like parsed ones), and it pushes the
"does this record fit" check to parse time where it belongs — apply
then only needs to validate source bounds, not target bounds.

Apply-time semantic checks that remain regardless of which option:

- record offset + payload length ≤ output size,
- if truncation present, truncation ≤ output size and ≥ max record end
  (or: reject on contradiction),
- nothing about source-read bounds: IPS doesn't read from source, it
  just copies source through.

The `ApplyError` constructors IPS needs: something like
`ApplyRecordOutOfBounds`, `ApplyTruncationContradiction`. BPS/UPS's
existing `ApplyWritesPastTarget` could be reused for the first one if
the types align.

-------------------------------------------------------------------------------

## 6. Test fixture inventory, per variant

All paths relative to repo root. Sizes in bytes. Provenance is from
the suite files (`test/suites/*.suite`) — "converted" means re-diffed
by an external tool, "real" means authentic historical patch,
"synthetic" is a suite-file-declared tier for heavy-diff patches that
are neither real-world nor round-tripped from another format.

### StandardIPS (`PATCH` / `EOF`)

| File | Size | Provenance | Notes |
|---|---|---|---|
| `test/data/dm4y/patch.ips` | 560,669 | converted (Flips) | 4 MB GBC base (`dm4y/base.gbc`). Primary small-ROM fixture; dm4y.suite exercises 14 formats including this. |
| `test/data/emerald/heavy-diff/patch.ips` | 6,702,378 | synthetic (Flips) | 16 MB GBA base (`emerald/base.gba`). Exercises ≤16 MB ceiling. Also the cross-validation fixture (`test/specs/crossval.txt:25`: `ips \| emerald-heavy \| ... \| flips`). |
| `test/data/fe6/fe6.ips` | 610,179 | real | Fire Emblem 6 English translation. Standalone real-world patch; `test/suites/fe6-ips.suite`. Only "real" StandardIPS fixture in the tree. |
| `test/data/paper-mario/deblur.ips` | 207 | real | N64 deblur, 207 bytes total, cross-validated against the paired APS. Exercises the tiny-patch path. |

### IPS32 (`IPS32` / `EEOF`)

| File | Size | Provenance | Notes |
|---|---|---|---|
| `test/data/dm4y/patch.ips32` | 760,118 | converted (sips) | Same source ROM as `patch.ips`. Exercises the IPS32 parser on a small input. |
| `test/data/stadium2/heavy-diff/patch.ips32` | 27,360,744 | synthetic (sips) | 64 MB N64 base. Only >16 MB fixture. This is the stress test for the stadium2-scale action-stream memory claims from the BPS docstring. |

### EBP (IPS with trailing JSON)

| File | Size | Provenance | Notes |
|---|---|---|---|
| `test/data/dm4y/patch.ebp` | 748,501 | converted (RomPatcher.js) | Searched with `grep -ao '"patcher"'`: zero matches. This fixture has **no JSON metadata** — it's an IPS (with "EOF" trailer) that happens to be named `.ebp`, from a converter that didn't emit a metadata block. Real-world edge case for the parser: `.ebp` extension, no JSON. |
| `test/data/emerald/heavy-diff/patch.ebp` | 6,702,461 | synthetic (RomPatcher.js) | Has JSON: trailer ends with `..."description":"Generated for slap testing"}`. Exercises the full EBP code path including metadata round-trip. Size difference vs. the sibling `.ips` is 83 bytes = exactly the JSON blob. |

### Sentinel-collision and truncation property fixtures

Not files on disk — generated inside the test suite:
- `test/Props/RoundTrip.hs:161-169` — `prop_ipsEofCollision`, QuickCheck-driven source/target pairs that exercise the `0x454F46` sentinel path end-to-end.
- `test/Props/RoundTrip.hs:171-187` — `prop_avoidSentinel` unit-style table of `avoidSentinel` cases.
- `test/Props/Contracts.hs:131-180` — `prop_ipsSentinelDirect`, `prop_ips32SentinelDirect`, `prop_ipsSentinelSplitDirect`, `prop_ips32SentinelSplitDirect`, `prop_ipsSentinelWithSource`. These are the contract-system tests for direct-conversion sentinel rejection.
- `test/Props/Truncation.hs:69-85` — `prop_ipsTrunc`, `prop_ips32Trunc`, `prop_ebpTrunc`: fuzz the parser against arbitrarily truncated inputs. Generic robustness, not variant-specific.

### Integration / CLI test shards

- `test/Integration/CLI.hs:38` — `dm4yIps = repo </> "test/data/dm4y/patch.ips"` threaded through the CLI round-trip suite.
- `test/Integration/CLI.hs:84-85, 270, 277, 284` — hand-crafted tiny IPS byte sequences (`PATCH\x01\x02` for truncated-input warnings; `PATCHEOF` for the empty-patch case).
- `test/Integration/FailureMode.hs:299-310` — `round-trip/IPS -> EBP -> IPS` and `round-trip/IPS -> PPF3 -> IPS` conversion chains.
- `test/Integration/FailureMode.hs:397, 409` — `create-round-trip/dm4y IPS` and `create-round-trip/stadium2 IPS32`.

Coverage gaps to be aware of:

- No real-world `.ips32` or `.ebp` fixture — both variants are
  tooling-converted or synthetic. The rewrite cannot regress
  against a known-real-world IPS32 pattern because none exists in
  the tree.
- No fixture exercises an IPS with Flips-emitted truncation marker.
  `prop_ipsTrunc` fuzzes truncation but doesn't specifically produce
  a shrink-direction patch. Worth adding a unit test with a known
  Flips-produced truncation patch if one is available externally.
- No fixture exercises an EBP with a truncation marker in the
  "slap-only extension" position (truncate-then-JSON). This makes
  dropping the writer side of that shape safe.

-------------------------------------------------------------------------------

## 7. Open questions

Things the rewrite can't decide from the spec and existing code alone.
I'll want answers before the writer is touched, and before apply's
error taxonomy is finalized.

**Q1. IPS32 truncation marker: keep or drop?**
Drop both sides. No authoritative source documents truncation for
IPS32, and we don't add slap-only extensions to a format we don't
own. createIPS32 never emits a marker; parseIPS rejects any trailing
bytes after EEOF as SlapError. Symmetric strictness.

**Q2. EBP writer emitting truncation-then-JSON: drop?**
Doesn't exist; not supported.

**Q3. Parse-time or apply-time bounds check on record offsets?**
Option B from §5 — parse rejects records whose `offset + payload`
exceeds `0xFFFFFF + 0xFFFF` (or `0xFFFFFFFF + 0xFFFF` for IPS32). This
is symmetric with how BPS rejects negative sizes at parse time. But
it makes a real-world patch that *happens to* write to the last 16
bytes of a 16 MB ROM fail to parse when today it would apply fine.
Check whether any real-world fixture actually lives at the top of
the offset range before committing.

**Q4. Overlapping records — detect, reject, or tolerate silently?**
Warn. Records apply in wire order; later writes clobber earlier
ones. Overlap is unusual, so we tell the user. Warnings are used
liberally in slap.

**Q5. Unsorted records — detect, reject, or tolerate silently?**
Warn. Records apply in wire order; unsorted records (wire order ≠
offset order) are unusual, so we tell the user. Same reasoning as
Q4.

**Q6. The `IPSContainer` sum shape.**
Resolved via a fourth option not sketched here: `EBPPatch` is a
wrapper type holding an `IPSPatch` plus `EBPMetadata`. Plain and
IPS32 patches are `IPSPatch` values directly; EBP is `EBPPatch
{ ebpBasePatch = IPSPatch { ipsVariant = StandardIPS }, ebpMetadata
= ... }`. Same end state as the `IPSContainer` sum — the nonsense
combination (IPS32 + EBP metadata) is unrepresentable — via a
different type shape.

**Q7. Record type — one sum with two constructors, or
`IPSRecord { offset, payload } | IPSRecordRLE { offset, count,
byte }` with a common `ipsRecordOffset :: IPSRecord -> Offset`
accessor?** The existing split has distinct field names which
Describe has to work around. Not load-bearing; pick a shape and stick
with it.

**Q8. `applyIPS` in the round-trip tests.**
The round-trip property tests all go through `applyViaFile
IPS.applyIPS`. Changing `applyIPS` to
`SourceFileContents -> Either SlapError TargetFileContents` means
those tests need a new helper or need to switch to
`applyViaMemory`-style. Small but real test-harness change.

**Q9. Is `splitHunks` in the right module?**
It's consumed by `Slap.Convert` (for IPS/IPS32/EBP and, separately,
PPF3 at a 255-byte cap). Its home in `Slap.IPS.Create` is historical;
it could live in `Slap.Binary` or `Slap.Convert`. Not a blocker for
the rewrite.

-------------------------------------------------------------------------------

## 8. Notes on sources

- `zerosoft.zophar.net/ips.php` — base-spec (PATCH/EOF, record format,
  RLE format). Now mirrored locally in `upstream/zerosoft-source.md`.
  Note: the preamble has garbled unit conversions ("2^24-1 bits (2047
  Mb)") but the record-format tables are sound. This is the origin of
  the "Any nonzero value" RLE_Size constraint that anosh.se repeats.
- `fileformats.archiveteam.org` wiki page on IPS — confirmation of
  base-spec, sentinel-avoidance pitfall (quoted in §1.4), truncation
  extension language (quoted in §1.1). Originally unreachable from
  this session (ECONNREFUSED); pasted in by the user after the first
  draft.
- Flips `libips.cpp` — available locally at `tools/flips/libips.cpp`.
  Key claims about Flips behavior in this document (sentinel avoidance
  at line 247, truncation emit at line 344, truncation parse at line
  77, RLE-zero rejection at line 59, EOF loop at line 53) were
  verified against the local source during a later accuracy review.
- IPS32, EBP: research-subagent pass against `leoetlino/sips`,
  `Lyrositor/EBPatcher`, SnesLab, and nesdev. None of these are
  spec-like; they are single-implementation de-facto.
- SNESTool DOC (`upstream/SNESTL12.DOC`, mirrored as
  `upstream/SNESTL12.md`): 1996 release notes. Historical source for
  provenance and the "IPS 2" cutting concept. Not a technical
  specification — it does not describe the wire format. Claims from
  this source should be attributed as "SNESTool's DOC says" rather
  than presented as format facts.

Judgment calls flagged in §7 stem from the gaps between these
sources: the Archiveteam wiki and ZeroSoft describe only StandardIPS,
so everything about IPS32/EBP is folklore-from-one-implementation and
deserves an explicit "we chose X" note in the rewrite.
