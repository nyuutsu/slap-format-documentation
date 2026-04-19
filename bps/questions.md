# BPS — open questions

## Checksums

- **Which CRC32 polynomial.** "CRC32" admits IEEE 802.3, CRC-32C, CRC-32/BZIP2, and others. byuu names none.
- **Endianness of the three uint32s on the wire.** Unstated.
- **Scope of "the source file" for `source-checksum`** (see also: Size agreements — source file length vs `source-size`). Bytes on disk, or exactly `source-size` bytes? Same question for target.
- **Source-checksum mismatch policy.** Fatal, advisory, or never checked.
- **Target-checksum mismatch policy.** Same question, distinct answer possible.
- **Patch-checksum mismatch policy.** Same again, distinct again. Patch corruption compromises everything downstream.
- **Verification order.** Check patch-checksum before touching anything? Source before applying? Verify-as-you-go?

## Metadata

- **Schema validation on parse** (affordance). Officially XML 1.0 UTF-8; actually up to 2^64 bytes of arbitrary binary. slap's parser stance.
- **Metadata on create** (affordance). Empty, slap-identifier, XML-shaped, or something else.
- **Metadata pass-through** (affordance). Surfaced to callers as opaque bytes, parsed structure, or not at all.
- **Metadata byte-preservation on round-trip** (affordance). If slap canonicalizes anything — whitespace, UTF-8 normalization, BOM — `patch-checksum` breaks on re-emit. Worth an explicit decision, not a discovered invariant.

## Size agreements

- **Source file length vs `source-size`** (see also: Checksums — scope of "the source file"). Longer on disk: truncate/reject/warn. Shorter: fatal, but diagnosed when?
- **Target output length vs `target-size`.** Underproduction at end-of-stream, overproduction during application, both presumably fatal but check order matters.
- **SourceRead past source-size.** SourceRead reads `source[outputOffset]`; if `outputOffset ≥ source-size` the per-byte read is off the end. byuu's general "no reading past end of file" applies, but he doesn't address this case specifically for SourceRead.
- **metadata-size sanity.** The varint is unbounded; nothing prevents a declared `metadata-size` larger than the remaining patch bytes.

## Malformed patches — structural

Shared theme: what does slap do when the parser hits trouble, and how is it categorized.

- **Patch shorter than the minimum.** For a patch with `metadata-size = 0`, the floor is 19 bytes (4 magic + 3 × ≥1 varint + 12 footer); a patch with declared `metadata-size > 0` has a correspondingly higher floor. Reject-at-the-door.
- **Patch truncated mid-varint.** Decoder hits EOF without a terminator byte.
- **Patch truncated mid-action.** Packed prefix decoded, promised body runs off the end.
- **Integer-width cap** (affordance). byuu explicitly endorses arbitrary-width integers; slap has a finite integer type (Haskell's `Int` / `Word64`). This is one question with multiple surfaces: where the cap sits, what happens on varint decode overflow at that cap, and what happens on cursor arithmetic overflow where `sourceRelativeOffset += delta` could overflow slap's type even if both operands are individually representable. Cap-and-fail, cap-and-saturate, or proceed-until-else-breaks.
- **Termination overshoot.** byuu's condition is `>=`, not `==`. What to do when finished past `size − 12` or mid-action at the boundary.
- **Syntactic vs semantic invalidity.** byuu uses one word for both. slap needs two categories with distinct handling.
- **Negative-zero signed offsets.** `0x80` and `0x81` both encode a zero-magnitude adjustment. Canonical emit, parse-time rejection, warn-only, or silently accept.

## Degenerate but structurally valid patches

- **Zero-action patches.** Grammar admits them. Nonsensical unless `target-size = 0`.
- **`source-size = 0`.** "From nothing" patch — only TargetRead and TargetCopy usable without immediate bounds violation. Legitimate for initial-release distribution.
- **`target-size = 0`.** "Erase everything" — only a zero-action stream avoids overshoot (any action writes ≥ 1 byte).
- **First action is TargetCopy (`outputOffset = 0`).** `targetRelativeOffset` starts at 0, valid range `[0, outputOffset)` is empty, so the action cannot execute regardless of the signed delta. Generic error, or specific diagnosis. (Distinct from a TargetCopy later in the stream, which is fine.)

## Trailing bytes

- **Trailing bytes after the footer** (affordance). Footer is positionally defined ("last twelve bytes"), so any trailing concatenation consumes the real footer and produces a wrong-but-parseable one. Strict-reject, tolerate-and-strip (by what heuristic), or something else. The EBP-shaped question for BPS.

## Abort semantics

- **What "abort" means behaviorally.** Diagnostics, exit code, whether original source is touched.
- **Partial target disposition.** Delete, truncate, preserve, temp-file-and-atomic-rename, or memory-stream-commit-on-success.

## Creation strategy

- **Linear vs delta creation.** One mode, both, hybrid.
- **Match-finding approach (if delta).** Suffix array, rolling hash, naive scan, bsdiff-style, xdelta-style.
- **Patch-size minimization.** Merge adjacent actions of the same type? Prefer SourceRead or SourceCopy when both work?
- **Determinism.** Bit-identical output across invocations for a given `(source, target)`.

## Minor / pedantic

- **Cursor-at-bound pedantry.** byuu's prose and pseudocode disagree about whether cursor may transiently equal the upper bound.
- **Zero-delta Source/TargetCopy.** Legitimate, encodes as `0x80`. Unusual.
- **"BPS1" as magic vs version.** The `1` hints at versioning that byuu never used. If "BPS2" turned up, would slap try?
- **Reference-implementation-vs-prose authority.** Meta-policy: which wins where they disagree.

## Tooling

- **`info` output scope.** Action-type histogram, total bytes moved, largest copy, metadata dump, checksums, size deltas.
- **`explain` output scope (and `explain --records`).** What the human-readable summary shows versus what the records-level dump emits.
- **Metadata surfacing under `info` / `explain` when the payload isn't XML.** Concrete user-visible case of the "metadata pass-through" affordance.
