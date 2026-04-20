# BPS — design questions

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

**slap treats a source CRC mismatch as fatal; `--no-verify` downgrades it to a warning and allows the apply to proceed.** This is slap-wide policy for CRC-bearing formats; BPS inherits it. No BPS-specific decision is required.

"Never checked" is not in slap's vocabulary. If the patch carries a `source-checksum`, slap computes the CRC of the provided source and compares them.

### What happens when the computed target CRC doesn't match the patch's declared `target-checksum`?

**Same policy as for source: fatal by default, downgraded to a warning by `--no-verify`.** Uniform across CRC-bearing formats in slap; BPS inherits it.

What's semantically distinct about the target side: in the default path, source-CRC has already passed (we wouldn't have reached apply otherwise), and patch-CRC has already passed (we wouldn't have reached parse otherwise). So a target-CRC mismatch at this point means either slap's applier has a bug for this patch, or something impossible has happened. Both are loud-error-worthy; fatal is the right default. The `--no-verify` downgrade is consistent with the source-side behavior for users who have said "proceed through verification failures."

### What happens when the computed patch CRC doesn't match the patch's declared `patch-checksum`?

`patch-checksum` covers the entire patch file except the last four bytes. A mismatch means the patch bytes are corrupt, which makes every field we decoded (source-size, target-size, metadata, actions, source-checksum, target-checksum) come from potentially-garbled bytes. This is qualitatively different from source/target mismatch, where the patch itself is trusted and we're checking files against its declared values.

**slap treats a patch-CRC mismatch as fatal, unconditional. `--no-verify` does not downgrade it.**

The check runs during parse, before any body field is decoded. Mismatch produces a parse-level integrity error that propagates out ahead of any verification-layer policy, and therefore ahead of any `--no-verify` logic. This is deliberate: patch-CRC is the integrity gate over the decoding machinery itself, and `--no-verify`'s "proceed despite verification failures" stance is only meaningful when the field values being verified against are themselves trusted.

Note the deliberate asymmetry with source/target-CRC, which *do* downgrade under `--no-verify`. Future readers should not "uniformize" this.

Ecosystem is unanimous on "fatal, no override": flips `libbps.cpp:95`, beat `patch.hpp:212`, RomPatcher.js `:128-130`. slap's current behavior aligns.

### In what order do we compute and compare the three CRCs?

Three CRCs live in a BPS patch: `patch-checksum` over the patch itself, `source-checksum` over the author's source, `target-checksum` over the author's target. Each has a natural earliest-possible firing time: patch-CRC can be checked as soon as the patch bytes are available (before any parse work); source-CRC as soon as source is loaded and the patch is parsed (before apply); target-CRC only after apply has produced the target.

**slap fires the three CRC gates in the order "patch-CRC at parse, source-CRC before apply, target-CRC after apply." Each gate fails before the next runs.**

Three prior entries cover the per-gate policy (see "What happens when the computed X CRC doesn't match..." for source, target, and patch). The ordering principle is simply: each gate checks what it can check as soon as it can check it, and a failing gate stops the pipeline before downstream work runs.

slap currently computes each CRC over the whole relevant buffer once, not streamed. Verify-as-you-go (pipelining CRC computation into the parse/apply loops) would be an evolution for very large files; slap's whole-file in-memory architecture (see the project README's `[^INPLACE]` footnote) means streaming isn't on the table here. If that architectural premise changes, this ordering answer survives; the only thing that'd change is *how* each CRC is computed, not *when* each gate fires.

## Metadata

### Does slap validate metadata as XML during parse?

byuu's spec has the carveout in a single paragraph: metadata is "officially" XML 1.0 UTF-8, *and also* "contents are entirely domain-specific" and "a patch with arbitrary metadata contents is still considered valid." The convention and the bypass are granted together. Compliant XML 1.0 parsing (DTDs, entities, namespaces, processing instructions, CDATA, encoding declarations) is a whole language-parser's worth of machinery to handle a field that in practice no one uses for anything beyond a few strings.

**slap's parser captures metadata as opaque bytes: no validation, no warning.** The parse layer's job is integrity, and the spec answers the integrity question directly — any bytes are valid. Rejecting non-XML metadata would be stricter than the spec itself. Warning on non-XML metadata would be chatty about something the spec explicitly permits.

BPS patches carrying *any* metadata are genuinely rare, however — 0 of 1,495 patches in the `roms/curated/bps/` corpus (drawn from the romhacking.net archive, 2024-08-01) had a non-empty metadata field. When a patch does carry one, it is an unusual artifact and worth mentioning to the user — but the right place for that mention is the presentation layer (`info` / `explain`), not the parse layer, and the right severity is informational, not warning. That presentation-layer surfacing depends on slap gaining an info channel distinct from its current warning channel; see `notebook.md` for that proposal.

The "metadata as a typed document, not just bytes" ergonomics is also tempting. The clean placement is *also* the presentation layer: parse keeps `ByteString`, and a `MetadataDisplay` sum type (opaque bytes vs. attempted-XML-parse, etc.) blooms at the boundary where XML-awareness pays for itself. See `notebook.md`.

This decision propagates to the other three Metadata items: since parse doesn't commit to XML, create doesn't have to (slap can emit nothing, or emit opaque bytes from `--metadata FILE`), pass-through is straightforwardly opaque, and byte-preservation on round-trip falls out trivially.
### What does slap put in the metadata field when creating a patch?

**Nothing, by default — length-zero metadata, a single `0x80` byte for the `metadata-size` varint, no further bytes on the wire.** Users who want metadata opt in via `--metadata FILE`, which embeds the file's contents verbatim.

Rationale: 0 of 1,495 patches in the `roms/curated/bps/` corpus carry any metadata. "Nothing" is the convention; a patch with embedded metadata is the unusual case and should require the user to have asked for it.

Inline metadata (e.g. a hypothetical `--metadata-inline "..."`) isn't currently exposed. Structurally equivalent to the file form — no format obstacle; just a CLI decision that hasn't come up.

### How does slap surface parsed metadata to callers?

**Opaque bytes through the core; a meaningful-representation attempt at the display layer.** The parse layer stores metadata untyped; so do the operations that don't display it (apply, create, convert). Only at the display boundary (`info`, `explain`, `--extract-metadata`) does slap attempt to interpret the bytes as something a user can read.

The "last possible second" principle: no operation that doesn't need to look at metadata *should* look at metadata. Display is the only boundary where interpretation earns its keep, and it does so on demand. See `notebook.md`'s *typed metadata at the presentation layer* entry for the sketched shape.

For programmatic uses that want the bytes raw (notably `info --extract-metadata FILE`), slap keeps them raw; the file on disk matches the bytes the patch carried.

### Does slap preserve metadata bytes exactly on round-trip?

**Yes, for round-trip within BPS.** No canonicalization, no whitespace-normalization, no UTF-8 re-encoding, no BOM munging. If the original metadata was bytes X, re-emitted metadata is bytes X — which is necessary for `patch-checksum` to round-trip, and ratifies what slap already does (no canonicalization step exists in the parse → create → convert path today).

"Round-trip within BPS" covers: explicit `slap convert --to bps` on a BPS patch; any apply-and-recreate flow that reuses the original metadata; any future operation whose effect is "re-emit this BPS patch as BPS." In each case, exact byte preservation is the commitment.

Cross-format round-trip (e.g. `bps → ips → bps`) is out of scope. IPS has no metadata field; there is nothing to preserve. A user who wants metadata to survive that trip re-supplies it via `--metadata FILE` on the final leg.

The explicit decision: preservation is the invariant, not a discovered property. Any future code that would canonicalize metadata bytes needs to know it's breaking this invariant, and `patch-checksum` along with it.

## Size agreements

### What do we do when the source file on disk isn't exactly `source-size` bytes long? (see also: Checksums — scope of "the source file".)

Two sub-cases: source longer on disk than `source-size`, source shorter than `source-size`. byuu's prose doesn't address either — the general "no reading past end of file" rule handles too-short at action-execution time but prescribes no policy, and too-long is completely silent.

**On source-file length mismatch, slap's default is the CRC-gate-plus-diagnostic pattern it already uses across formats: whole-file source-CRC is the authoritative fatal gate, downgraded to a warning under `--no-verify`; file-size-advisory is a warning-only specific diagnostic layered over it.** Both too-short and too-long fall out of this pattern without BPS-specific handling.

Per case:

- **Too long on disk**: whole-file CRC won't match the authored `source-checksum` (which was computed over exactly `source-size` bytes). Fatal by default. Under `--no-verify`, the CRC downgrades to a warning, apply proceeds, and since the action stream only consumes source positions that live within `source-size`, a too-long source with the authored bytes as prefix (a ROM with trainer, copier header, or similar wart) still yields the correct target. A too-long source with wrong content yields a wrong target; the user was warned.

- **Too short on disk**: whole-file CRC won't match. Fatal by default. Under `--no-verify`, the CRC downgrades and apply proceeds; whichever action first references a source position past end-of-file fails with an out-of-bounds diagnostic. The user ends up with no output and a step-level error rather than a file-level one.

slap does not add a pre-flight stat-based size check ahead of the CRC. The CRC gate is authoritative for "is this the right file"; a second gate ahead of it would duplicate responsibility. Sharpening the "wrong size specifically" diagnostic is the file-size-advisory's job — see `notebook.md` on that advisory currently being dead code in the common paths.

Ecosystem is split on this:

- **flips**: whole-file CRC, length-mismatch fatal unless `--accept-wrong-input`. Same shape as slap's default.
- **beat**: CRCs only the first `source-size` bytes; length-mismatch fatal only when too-short. A correct-prefix too-long source applies cleanly in beat without any user intervention.
- **RomPatcher.js**: whole-file CRC, no size check, optional user-supplied header offset.

slap matches flips's semantics with slap's `--no-verify` escape hatch. The "headered ROM" case is supported, but only through `--no-verify`, which requires the user to affirmatively acknowledge "I know this file is not bit-for-bit the patch's authored source."

Target side: the analogous question for `target-checksum` does not arise in normal apply (target is produced from scratch, so its length is `target-size` by construction).

### What do we do when the action stream produces the wrong number of bytes for `target-size`?

Two distinct failure modes. **Overproduction**: an action's declared length would write past `target-size`. Detectable per-action, before the write runs. **Underproduction**: the action stream ends but we haven't reached `target-size` bytes yet. Detectable only at end-of-stream.

**Both fatal, with distinct diagnostics.** Overproduction is an eager per-action check (before each write). Underproduction is a terminal check (once the stream ends). The check order falls out of detectability — overproduction cannot be detected lazily, underproduction cannot be detected eagerly.

Related: this is the enforcement mechanism for the `target-size = 0` degenerate case (any action writes ≥ 1 byte, so the first action overproduces) and for the "zero-action patches with `target-size > 0`" case (end-of-stream underproduces). Candidate for condensation with those entries in the next section.

### What happens when SourceRead's per-byte reads walk past `source-size`?

SourceRead copies `source[outputOffset..outputOffset+length]` to target at the same offsets. If `outputOffset + length > source-size`, the read walks off the end of source. byuu's general "no reading past end of file" rule applies, but the spec doesn't call out SourceRead by name.

**Fatal, detected per-action during apply.** Before a SourceRead begins, slap checks `outputOffset + length ≤ source-size`; failure produces an out-of-bounds source-read error pinned to the offending action's index in the stream. Consequence of byuu's general rule, not a new policy — the same check applies uniformly across every action kind that reads source.

### What do we do when `metadata-size` declares more bytes than the patch actually contains?

`metadata-size` is a varint with no format-imposed upper bound. A broken or malicious patch could declare a `metadata-size` larger than the patch's remaining byte count (or even larger than the integer-width cap slap adopts — see the Malformed section entry).

**Fatal at parse time.** Once `metadata-size` is decoded, the parser compares it against the remaining patch-body bytes; if it exceeds, the patch is malformed and is rejected.

Candidate for condensation: this is a specialization of "Patch truncated mid-varint" / "Patch truncated mid-action" from the Malformed section — all three are "declared length exceeds what's actually there." May fold into a single "declared lengths vs actual bytes" entry when we audit.

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
