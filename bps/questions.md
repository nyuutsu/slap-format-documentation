# BPS — open questions

Questions tagged **(uses-frostmourne-to-butter-its-toast)** mark places where the spec grants expressive power wildly beyond what convention actually uses. A compliant implementation would have to handle a possible-space radically larger than the practical-space. The archetype is EBP's trailing-JSON field: convention is a UTF-8 JSON object holding four specific strings, but EBP has no spec — the only actual rule is "it has a JSON at the end of it, therefore it is a valid EBP." The lack of rules creates the anarchy: a UTF-32-encoded JSON of any shape passes the same check, and a strictly-compliant parser has to accept it. These questions carry an extra dimension — not just "what do we do," but "how far do we chase the hypothetical."

## Checksums

### Which CRC-32 variant does BPS actually use?

"CRC32" admits IEEE 802.3, CRC-32C, CRC-32/BZIP2, and others. byuu names none. The prose pins nothing down; his reference implementation in `beat/nall/crc32.hpp` does: init `0xFFFFFFFF`, input and output reflected, final XOR `0xFFFFFFFF`, reversed-poly table constant `0xEDB88320` (normal poly `0x04C11DB7`). That's the variant `reveng` catalogs as **CRC-32/ISO-HDLC** — also known as just "CRC-32", the zlib/PNG/PKZIP/gzip flavor. Check value (CRC of ASCII `"123456789"`) is `0xCBF43926`. Flips and RomPatcher.js use the same variant.

**slap uses CRC-32/ISO-HDLC.** This is the exact variant everyone actually means when they write "CRC32" in this corner of the ecosystem.

As real sticklers, this deserves at least a code comment noting two things:

1. The variant in use is CRC-32/ISO-HDLC.
2. A case could be made that the Right Thing is to check other CRC-32 variants when the declared checksum doesn't match, since detection is cheap given source and target on hand. This is a parallel to the EBP JSON implementation issue.

That line of thinking is incorrect. EBP JSON flavor-support expansion doesn't muddy the water on "is my patch corrupt or isn't it?" — the metadata is annotation, independent of integrity. CRC-32 variant-support expansion very much would.

### What byte order do the footer checksums use?

The three footer checksums are the only fixed-width integers in the whole wire format; every other number uses the varint encoding, which is byte-order-free by construction. byuu writes them as `uint32` without naming a byte order.

**slap writes and reads the footer little-endian.** Least-significant byte first; bytes at offsets `size − 12`, `size − 8`, `size − 4` hold the low bytes of `source-checksum`, `target-checksum`, `patch-checksum` respectively.

The prose says nothing, but the reference implementations are unanimous:

- **beat** `nall/beat/base.hpp:44-48` and `multi.hpp:196-200`: `writeChecksum` emits `cksum >> 0`, `>> 8`, `>> 16`, `>> 24`. Reader mirrors.
- **flips** `libbps.cpp:13-21` (`read32`) and `231-238` (`write32`): same pattern both directions.
- **RomPatcher.js** `RomPatcher.format.bps.js:92` (read path) and `:200` (write path) both set `file.littleEndian = true` before the footer is touched.

Reference-wins-on-silences meta-policy; this one settles itself.

### What counts as "the source file" (and target file) when computing their checksums? (see also: Size agreements — source file length vs `source-size`.)

byuu's prose says "the CRC32 of the source file" without clarifying whether "source file" means "the bytes on disk the user handed us" or "exactly `source-size` bytes of it." The options differ observably when file length doesn't equal `source-size`.

**slap CRCs the whole source file as handed in.** Same shape for target whenever a target-CRC is computed against an existing target (not applicable in normal apply, where target is produced from scratch).

This is the flips approach. beat instead CRCs exactly `source-size` bytes, which observably differs when the file is longer than `source-size` (beat accepts with matching prefix; slap does not, absent `--no-verify`). RomPatcher.js also does whole-file.

Consequence: slap's source-CRC check is a single gate that detects both content mismatch and size mismatch. A too-long source with a matching prefix fails the default path; the user opts in with `--no-verify` if they know better. The full fault-handling is covered in the size-agreement entry.

Target side: `target-checksum` in normal apply is a CRC of exactly the bytes slap just produced, which are `target-size` by construction. No scope ambiguity on the target side in the apply flow.

### What happens when the computed source CRC doesn't match the patch's declared `source-checksum`?

byuu's spec frames `source-checksum` as "verifies that the input file is correct" — a purpose, not a policy. The spec doesn't say whether a mismatch is fatal, advisory, or ignored.

**slap treats a source CRC mismatch as fatal; `--no-verify` downgrades it to a warning and allows the apply to proceed.** This is the behavior `checkCRC` at `app/Main.hs:742-749` already implements uniformly across every format that populates `verifySourceCRC32`, BPS included. No BPS-specific decision is required; the answer falls out of slap's existing architecture.

"Never checked" is not in slap's vocabulary. If the patch carries a `source-checksum`, slap computes the CRC of the provided source and compares them.

### What happens when the computed target CRC doesn't match the patch's declared `target-checksum`?

Same policy as for source: fatal by default, downgraded to a warning by `--no-verify`. The same `checkCRC` in `app/Main.hs:742-749` handles both sides, and every format slap supports populates `verifyTargetCRC32` (when the format carries one) through the same `Verification` record.

What's semantically distinct about the target side: in the default path, source-CRC has already passed (we wouldn't have reached apply otherwise), and patch-CRC has already passed (we wouldn't have reached parse otherwise). So a target-CRC mismatch at this point means either slap's applier has a bug for this patch, or something impossible has happened. Both are loud-error-worthy; fatal is the right default. The `--no-verify` downgrade is consistent with the source-side behavior for users who have said "proceed through verification failures."

### What happens when the computed patch CRC doesn't match the patch's declared `patch-checksum`?

`patch-checksum` covers the entire patch file except the last four bytes. A mismatch means the patch bytes are corrupt, which makes every field we decoded (source-size, target-size, metadata, actions, source-checksum, target-checksum) come from potentially-garbled bytes. This is qualitatively different from source/target mismatch, where the patch itself is trusted and we're checking files against its declared values.

**slap treats a patch-CRC mismatch as fatal, unconditional. `--no-verify` does not downgrade it.**

The check lives in `Slap/BPS/Parse.hs:35-45` and runs before the body is decoded. On mismatch, `parseBPS` returns `Left (PatchCRCMismatch ...)`, which propagates out before any `Verification` record is built and before `verifySource` runs. `PatchCRCMismatch` is grouped under `-- Parse: integrity` in `Error.hs`, reinforcing the intent: patch-CRC is a parse-time integrity gate, not a verify-time policy check.

Rationale: `--no-verify` means "proceed despite verification failures," and is meaningful when the field values being verified against are trusted. For patch-CRC, the field values themselves aren't trusted; there is nothing coherent to proceed with.

Note the deliberate asymmetry with source/target-CRC, which *do* downgrade under `--no-verify`. Future readers should not "uniformize" this.

Ecosystem is unanimous on "fatal, no override": flips `libbps.cpp:95`, beat `patch.hpp:212`, RomPatcher.js `:128-130`. slap's current behavior aligns.

### In what order do we compute and compare the three CRCs?

Three CRCs live in a BPS patch: `patch-checksum` over the patch itself, `source-checksum` over the author's source, `target-checksum` over the author's target. Each has a natural earliest-possible firing time: patch-CRC can be checked as soon as the patch bytes are available (before any parse work); source-CRC as soon as source is loaded and the patch is parsed (before apply); target-CRC only after apply has produced the target.

**slap fires them in that order: patch-CRC during parse, source-CRC before apply, target-CRC after apply. Each gate fails before the next runs.**

Three prior entries cover the per-gate policy (see "What happens when the computed X CRC doesn't match..." for source, target, and patch). The order is simply each gate checking what it can check as soon as it can check it. Concretely:

1. `parseBPS` computes patch-CRC against the declared value and returns `Left (PatchCRCMismatch ...)` on mismatch, before decoding any body fields.
2. `verifySource` computes source-CRC against the declared value (after parse, before apply) and calls `die` on mismatch, or `warn` under `--no-verify`.
3. `applyBPS` produces the target.
4. `verifyTarget` computes target-CRC against the declared value and applies the same fatal/downgrade policy.

slap currently computes each CRC over the whole relevant buffer once, not streamed. Verify-as-you-go (pipelining CRC computation into the parse/apply loops) would be an evolution for very large files; slap's whole-file in-memory architecture (see the project README's `[^INPLACE]` footnote) means streaming isn't on the table here. If that architectural premise changes, this ordering answer survives; the only thing that'd change is *how* each CRC is computed, not *when* each gate fires.

## Metadata

- **Schema validation on parse** (uses-frostmourne-to-butter-its-toast). Officially XML 1.0 UTF-8; actually up to 2^64 bytes of arbitrary binary. slap's parser stance.
- **Metadata on create.** Empty, slap-identifier, XML-shaped, or something else.
- **Metadata pass-through.** Surfaced to callers as opaque bytes, parsed structure, or not at all.
- **Metadata byte-preservation on round-trip.** If slap canonicalizes anything — whitespace, UTF-8 normalization, BOM — `patch-checksum` breaks on re-emit. Worth an explicit decision, not a discovered invariant.

## Size agreements

### What do we do when the source file on disk isn't exactly `source-size` bytes long? (see also: Checksums — scope of "the source file".)

Two sub-cases: source longer on disk than `source-size`, source shorter than `source-size`. byuu's prose doesn't address either — the general "no reading past end of file" rule handles too-short at action-execution time but prescribes no policy, and too-long is completely silent.

**slap runs the existing `Verification` machinery at apply time: `verifySourceCRC32` is fatal (downgraded to warning by `--no-verify`), `verifyFileSizeAdvisory` is warning-only.** Both too-short and too-long fall out of this without special-case BPS code. CRC scope is the whole source file as handed in (see the companion entry above).

Per case:

- **Too long on disk**: whole-file CRC will not match the authored `source-checksum` (which was computed over exactly `source-size` bytes). Fatal. With `--no-verify`, CRC downgrades to a warning and `applyBPS` proceeds; since the apply loop only consumes source bytes up to the positions its actions name — all within `source-size` for a well-formed patch — a too-long source with the authored bytes as prefix (a ROM with trainer, copier header, or similar wart) still yields the correct target. A too-long source with wrong content yields a wrong target, and the user was warned.

- **Too short on disk**: whole-file CRC will not match. Fatal. With `--no-verify`, CRC downgrades and `applyBPS` begins executing the action stream; the first action whose read position exceeds the actual file length trips `ApplySourceReadOutOfBounds`. User ends up with no output, with diagnostics at step level rather than file level.

slap does not add a pre-flight stat-based size check ahead of the CRC. The CRC gate is authoritative for "is this the right file"; a second gate ahead of it would duplicate responsibility. Specific-sharpness diagnostics are the job of `verifyFileSizeAdvisory` — though see `notebook.md` on that advisory currently being dead code in the common paths.

Ecosystem is split on this:

- **flips**: whole-file CRC, length-mismatch fatal unless `--accept-wrong-input`. Same shape as slap's default.
- **beat**: CRCs only the first `source-size` bytes; length-mismatch fatal only when too-short. A correct-prefix too-long source applies cleanly in beat without any user intervention.
- **RomPatcher.js**: whole-file CRC, no size check, optional user-supplied header offset.

slap matches flips's semantics with slap's `--no-verify` escape hatch. The "headered ROM" case is supported, but only through `--no-verify`, which requires the user to affirmatively acknowledge "I know this file is not bit-for-bit the patch's authored source."

Target side: the analogous question for `target-checksum` does not arise in normal apply (target is produced from scratch, so its length is `target-size` by construction).

- **Target output length vs `target-size`.** Underproduction at end-of-stream, overproduction during application, both presumably fatal but check order matters.
- **SourceRead past source-size.** SourceRead reads `source[outputOffset]`; if `outputOffset ≥ source-size` the per-byte read is off the end. byuu's general "no reading past end of file" applies, but he doesn't address this case specifically for SourceRead.
- **metadata-size sanity.** The varint is unbounded; nothing prevents a declared `metadata-size` larger than the remaining patch bytes.

## Malformed patches — structural

Shared theme: what does slap do when the parser hits trouble, and how is it categorized.

- **Patch shorter than the minimum.** For a patch with `metadata-size = 0`, the floor is 19 bytes (4 magic + 3 × ≥1 varint + 12 footer); a patch with declared `metadata-size > 0` has a correspondingly higher floor. Reject-at-the-door.
- **Patch truncated mid-varint.** Decoder hits EOF without a terminator byte.
- **Patch truncated mid-action.** Packed prefix decoded, promised body runs off the end.
- **Integer-width cap** (uses-frostmourne-to-butter-its-toast). byuu explicitly endorses arbitrary-width integers; slap has a finite integer type (Haskell's `Int` / `Word64`). This is one question with multiple surfaces: where the cap sits, what happens on varint decode overflow at that cap, and what happens on cursor arithmetic overflow where `sourceRelativeOffset += delta` could overflow slap's type even if both operands are individually representable. Cap-and-fail, cap-and-saturate, or proceed-until-else-breaks.
- **Termination overshoot.** byuu's condition is `>=`, not `==`. What to do when finished past `size − 12` or mid-action at the boundary.
- **Syntactic vs semantic invalidity.** byuu uses one word for both. slap needs two categories with distinct handling.
- **Negative-zero signed offsets.** `0x80` and `0x81` both encode a zero-magnitude adjustment. Canonical emit, parse-time rejection, warn-only, or silently accept.

## Degenerate but structurally valid patches

- **Zero-action patches.** Grammar admits them. Nonsensical unless `target-size = 0`.
- **`source-size = 0`.** "From nothing" patch — only TargetRead and TargetCopy usable without immediate bounds violation. Legitimate for initial-release distribution.
- **`target-size = 0`.** "Erase everything" — only a zero-action stream avoids overshoot (any action writes ≥ 1 byte).
- **First action is TargetCopy (`outputOffset = 0`).** `targetRelativeOffset` starts at 0, valid range `[0, outputOffset)` is empty, so the action cannot execute regardless of the signed delta. Generic error, or specific diagnosis. (Distinct from a TargetCopy later in the stream, which is fine.)

## Trailing bytes

- **Trailing bytes after the footer** (uses-frostmourne-to-butter-its-toast). Footer is positionally defined ("last twelve bytes"), so any trailing concatenation consumes the real footer and produces a wrong-but-parseable one. Strict-reject, tolerate-and-strip (by what heuristic), or something else. The EBP-shaped question for BPS.

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
- **"BPS1" as magic vs version** (uses-frostmourne-to-butter-its-toast). The `1` hints at versioning that byuu never used. If "BPS2" turned up, would slap try?
- **Reference-implementation-vs-prose authority.** Meta-policy: which wins where they disagree.

## Tooling

- **`info` output scope.** Action-type histogram, total bytes moved, largest copy, metadata dump, checksums, size deltas.
- **`explain` output scope (and `explain --records`).** What the human-readable summary shows versus what the records-level dump emits.
- **Metadata surfacing under `info` / `explain` when the payload isn't XML.** Concrete user-visible case of the "metadata pass-through" affordance.
