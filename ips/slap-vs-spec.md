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

### What to fix

Both problems are one fix. Currently sentinel handling is split:
`narrowHunks` rejects collisions in the no-source path,
`avoidSentinel` fixes them in the with-source path, and the
with-source path strips the sentinel from `EncodingLimits` to
stop `narrowHunks` from rejecting what `avoidSentinel` is about
to fix. Two mechanisms, selective enabling per path,
comment-enforced safety.

Replace with one mechanism:

1. **Remove `sentinelOffset` from `EncodingLimits`.** Let
   `narrowHunks` be purely about offset ranges — a general
   question for a general function. The sentinel is IPS-specific
   and doesn't belong in a structure shared by every format.

2. **Replace `avoidSentinel` with a unified, fallible sentinel
   resolution function.** Takes the sentinel offset, source bytes
   (possibly empty), and the record list. For each record at the
   sentinel offset:
   - Source byte at `offset - 1` available: shift back, prepend.
     Return the fixed record.
   - Source byte unavailable: return an error explaining the
     problem and what would fix it ("record at offset 0x454F46
     collides with the IPS EOF marker; conversion requires source
     bytes to resolve this").

   Signature: something like
   `resolveSentinelCollisions :: Offset -> ByteString -> [EncodedHunk] -> Either SlapError [EncodedHunk]`.

   Both paths use this. The with-source path gets fixes. The
   without-source path gets clear errors. No choreography, no
   stripping fields, no comment-contracts.

3. **The silent pass-through is eliminated.** The current
   `avoidSentinel` has an `| otherwise = record` branch: if it
   encounters a collision it can't fix, it silently produces a
   corrupt patch. The safety of that branch depends on a comment
   saying it can't fire. In the unified function, that branch
   becomes an `Either` — the compiler forces callers to handle
   the failure case. There is no "encounter a collision and do
   nothing" path.

4. **The error message improves for free.** Currently the
   no-source rejection comes from `narrowHunk` and reads as a
   generic encoding-limits violation. The new function explains
   *why* and *what would fix it*.

Priority: low. Converting *to* IPS is a downgrade that basically
shouldn't happen. The collision is ~1 in 2,000 for large patches
and only matters in the conversion path. But the structural
cleanup (separating IPS baggage from `EncodingLimits`) is worth
doing regardless of how often the sentinel fires.

## Needs change: createIPS has no target-size guard

`createIPS` is infallible:
`SourceFileContents -> TargetFileContents -> PatchFileContents`.
If you pass it files larger than the variant can address, it
silently produces a corrupt patch. The optimizer
(`scanDiffRegions` in `Optimize.hs:132`) walks the entire target
with no offset bound. Records at offsets past 0xFFFFFF get
truncated to 3 bytes by `encodeOffset Offset24`
(`Create.hs:298-301`) — a record at 0x1200000 becomes 0x200000.
Silent corruption.

Flips has this guard explicitly:
`if (targetlen > 16777216) return ips_16MB` (`libips.cpp:202`).
slap has no equivalent.

Fix: guard at the top of `createIPS`. If `max(sourceLen,
targetLen)` exceeds the variant's addressable range, return an
error. This means `createIPS` becomes fallible — `Either SlapError
PatchFileContents` — which brings it in line with `createIPS32`
and `createEBP` which already return `Either`.

Same risk exists in principle for `createIPS32` with files > 4 GiB
but is not realistic.
