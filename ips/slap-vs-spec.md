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

## Notable: sentinel avoidance silently passes through when source unavailable

`avoidSentinel` is a no-op if the source is too short to read the
byte preceding the sentinel offset. In direct conversion (no ROM),
a record at the sentinel offset will be left in place — the
resulting patch will be misread by parsers that stop at `0x454F46`.
The audit says "the encoder must reject" but the code doesn't
reject, it silently passes through. Contract tests
(`prop_ipsSentinelDirect` etc.) catch this at test time.

When touching `Create.hs` or the sentinel avoidance path, check
whether encode-time rejection is warranted for the
source-unavailable case, or whether the test-time contract check
is sufficient.
