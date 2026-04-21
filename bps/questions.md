# BPS — design questions

Questions tagged **(uses-frostmourne-to-butter-its-toast)** mark places where the spec grants expressive power wildly beyond what convention actually uses. A compliant implementation would have to handle a possible-space radically larger than the practical-space. The archetype is EBP's trailing-JSON field: convention is a UTF-8 JSON object holding four specific strings, but EBP has no spec — the only actual rule is "it has a JSON at the end of it, therefore it is a valid EBP." The lack of rules creates the anarchy: a UTF-32-encoded JSON of any shape passes the same check, and a strictly-compliant parser has to accept it. These questions carry an extra dimension — not just "what do we do," but "how far do we chase the hypothetical."

## Checksums

### Which CRC-32 variant does BPS actually use?

"CRC32" admits IEEE 802.3, CRC-32C, CRC-32/BZIP2, and others. byuu names none. The prose pins nothing down; his reference implementation in `beat/nall/crc32.hpp` does: init `0xFFFFFFFF`, input and output reflected, final XOR `0xFFFFFFFF`, reversed-poly table constant `0xEDB88320` (normal poly `0x04C11DB7`). That's the variant `reveng` catalogs as **CRC-32/ISO-HDLC** — also known as just "CRC-32", the zlib/PNG/PKZIP/gzip flavor. Check value (CRC of ASCII `"123456789"`) is `0xCBF43926`. Flips and RomPatcher.js use the same variant.

**slap uses CRC-32/ISO-HDLC.** This is the exact variant everyone actually means when they write "CRC32" in this corner of the ecosystem.

### What byte order do the footer checksums use?

The three footer checksums are the only fixed-width integers in the whole wire format; every other number uses the varint encoding, which is byte-order-free by construction. byuu writes them as `uint32` without naming a byte order.

**slap writes and reads the footer little-endian.** Least-significant byte first; bytes at offsets `size − 12`, `size − 8`, `size − 4` hold the low bytes of `source-checksum`, `target-checksum`, `patch-checksum` respectively.

beat, Flips, and RomPatcher.js are unanimous on this.

### What counts as "the source file" (and target file) when computing their checksums? (see also: Size agreements — source file length vs `source-size`.)

byuu's prose says "the CRC32 of the source file" without clarifying whether "source file" means "the bytes on disk the user handed us" or "exactly `source-size` bytes of it." The options differ observably when file length doesn't equal `source-size`.

**slap CRCs the whole source file as handed in.** Same shape for target whenever a target-CRC is computed against an existing target (not applicable in normal apply, where target is produced from scratch).

This is the flips approach. beat instead CRCs exactly `source-size` bytes, which observably differs when the file is longer than `source-size` (beat accepts with matching prefix; slap does not, absent `--no-verify`). RomPatcher.js also does whole-file.

Source-CRC catches both content mismatch and size mismatch. A too-long source with a matching prefix fails the default path; the user opts in with `--no-verify` if they know better. The full fault-handling is covered in the size-agreement entry.

Target side: `target-checksum` in normal apply is a CRC of exactly the bytes slap just produced, and those bytes are always `target-size` bytes long. No scope ambiguity on the target side in the apply flow.

### What happens when the computed source CRC doesn't match the patch's declared `source-checksum`?

byuu's spec frames `source-checksum` as "verifies that the input file is correct" — a purpose, not a policy. The spec doesn't say whether a mismatch is fatal, advisory, or ignored.

**slap treats a source CRC mismatch as fatal; `--no-verify` downgrades it to a warning and allows the apply to proceed.** This is slap-wide policy for CRC-bearing formats; BPS inherits it. No BPS-specific decision is required.

### What happens when the computed target CRC doesn't match the patch's declared `target-checksum`?

**Same policy as for source: fatal by default, downgraded to a warning by `--no-verify`.** Uniform across CRC-bearing formats in slap; BPS inherits it.

What's semantically distinct about the target side: in the default path, source-CRC has already passed (we wouldn't have reached apply otherwise), and patch-CRC has already passed (we wouldn't have reached parse otherwise). So a target-CRC mismatch at this point means either slap's applier has a bug for this patch, or something impossible has happened. Both are loud-error-worthy; fatal is the right default. The `--no-verify` downgrade is consistent with the source-side behavior for users who have said "proceed through verification failures."

### What happens when the computed patch CRC doesn't match the patch's declared `patch-checksum`?

`patch-checksum` covers the entire patch file except the last four bytes. A mismatch means the patch bytes are corrupt, which makes every field we decoded (source-size, target-size, metadata, actions, source-checksum, target-checksum) come from potentially-garbled bytes. This is qualitatively different from source/target mismatch, where the patch itself is trusted and we're checking files against its declared values.

**slap treats a patch-CRC mismatch as fatal, unconditional. `--no-verify` does not downgrade it.**

The check runs during parse, before any body field is decoded. Mismatch produces a parse-level integrity error that propagates out ahead of any verification-layer policy, and therefore ahead of any `--no-verify` logic. This is deliberate: patch-CRC protects the decoding machinery itself, and `--no-verify`'s "proceed despite verification failures" stance is only meaningful when the field values being verified against are themselves trusted.

Note the deliberate asymmetry with source/target-CRC, which *do* downgrade under `--no-verify`. Future readers should not "uniformize" this.

Ecosystem is unanimous on "fatal, no override".

### In what order do we compute and compare the three CRCs?

Three CRCs live in a BPS patch: `patch-checksum` over the patch itself, `source-checksum` over the author's source, `target-checksum` over the author's target. Each can be checked at a different moment: patch-CRC as soon as the patch bytes are available (before any parse work); source-CRC as soon as source is loaded and the patch is parsed (before apply); target-CRC only after apply has produced the target.

**slap does them in that order: patch-CRC at parse, source-CRC before apply, target-CRC after apply. A failed check stops the next from running.**

Three prior entries cover what happens for each mismatch (see "What happens when the computed X CRC doesn't match..." for source, target, and patch).

## Metadata

### Does slap validate metadata as XML during parse?

byuu's spec has the carveout in a single paragraph: metadata is "officially" XML 1.0 UTF-8, *and also* "contents are entirely domain-specific" and "a patch with arbitrary metadata contents is still considered valid." The convention and the bypass are granted together.

**slap captures metadata as opaque bytes: no validation, no warning.** Rejecting non-XML metadata would be stricter than the spec itself; warning on it would be chatty about something the spec explicitly permits.

BPS patches carrying metadata are genuinely rare; we've thus far seen none. When a patch does carry some, it's an unusual artifact worth mentioning to the user — but as information, not a warning, and at display time rather than at parse. That needs slap to gain an info channel distinct from its warning channel; see `notebook.md`.

The "metadata as a typed document, not opaque bytes" ergonomics is also tempting. The clean placement is also at display time; see `notebook.md`.

### What does slap put in the metadata field when creating a patch?

**Nothing, by default — length-zero metadata, a single `0x80` byte for the `metadata-size` varint, no further bytes on the wire.** Users who want metadata opt in via `--metadata FILE`, which embeds the file's contents verbatim.

A patch with metadata is the unusual case; we've thus far seen none in the wild. Requiring the user to ask for metadata is the right default.

### How does slap surface parsed metadata to callers?

**Opaque bytes, until display time.** Parsing, applying, creating, and converting all leave metadata untouched as a stream of bytes. Only when slap needs to show metadata to a user (`info`, `explain`, `--extract-metadata`) does it try to interpret them as something readable.

The "last possible second" principle: no operation that doesn't need to look at metadata *should* look at metadata. Only display earns the interpretation work, and it does so on demand. See `notebook.md`.

For `info --extract-metadata FILE`, slap writes the bytes exactly as the patch carried them.

### What happens to metadata when converting a bps patch to bps?

Converting a patch from bps to bps should result in no metadata changes. This is desirable in general. In the case of bps it is essential, as the `patch-checksum` is checksumming the metadata as well as the regular data.

**Convert paths are allowed to change data so as to conform to the target format. Self to self should be safe from this.**

## Size agreements

### What do we do when the source file on disk isn't exactly `source-size` bytes long?

byuu's prose doesn't address this directly; his general "no reading past end of file" rule covers too-short at apply time, and too-long is silent.

**slap treats length mismatch as a source-CRC mismatch: fatal by default, downgraded to a warning under `--no-verify`. A separate size-mismatch warning fires alongside.**

The common real-world cause is a headered ROM. slap's planned handling is a cross-format `--header-offset N` flag (with platform sugar like `--header-offset nes`) that splits the file at offset N, CRCs and applies against the body, and re-prepends the header to the output. See `docs/header-awareness.md`.

Ecosystem: flips rejects any length mismatch; beat tolerates too-long; RomPatcher.js does whole-file CRC with an optional user-supplied header offset.

### What do we do when the action stream produces the wrong number of bytes for `target-size`?

The action stream can produce too few bytes or too many. Too few: the stream ends before reaching `target-size`. Too many: an action somewhere in the stream would write past `target-size`.

Both are fatal. They show up differently. If the stream ends short, slap reports the under-fill and names the shortfall. If an action overshoots, slap reports at that specific action, naming its length and how much room was left. The too-many case can be caught just before the write runs; the too-few case can only be caught once there are no more actions left.

### What happens when SourceRead's per-byte reads walk past `source-size`?

SourceRead copies `source[outputOffset..outputOffset+length]` to target at the same offsets. If `outputOffset + length > source-size`, the read would walk off the end of the source. byuu's general "no reading past end of file" rule applies, but the spec doesn't call out SourceRead by name.

slap treats it as fatal, like every other source-read overrun. The error names the specific action that tried it. Nothing special about SourceRead here — the same rule applies to SourceCopy and any other action that touches source.

### What do we do when `metadata-size` declares more bytes than the patch actually contains?

`metadata-size` is a varint with no format-imposed upper bound. A broken or malicious patch could declare a value larger than the remaining patch-body bytes.

slap rejects the patch at parse time. The same shape of problem appears in "patch truncated mid-varint" and "patch truncated mid-action" below — all three are "declared length exceeds what's actually there."

## Malformed patches — structural

Shared theme: what does slap do when the parser hits trouble, and how is it categorized.

### What do we do when the patch file is shorter than BPS's structural minimum?

The floor is 19 bytes for a metadata-free patch (4 magic + three ≥1-byte varints + 12 footer); a patch with declared `metadata-size > 0` has a correspondingly higher floor.

slap rejects at parse entry. Nothing below the structural minimum can be decoded coherently.

### What do we do when a varint decode hits EOF without finding a terminator?

The varint encoding requires a byte with the high bit set to terminate the number. A decoder that runs off the end without seeing one has a structurally broken patch on its hands.

slap rejects as a parse error. Nothing further can be trusted — if a varint is truncated, the bytes we thought we were decoding weren't a varint in the first place.

### What do we do when an action's packed prefix decodes but its body runs off the end?

The packed prefix declares the action's code and length; some action codes then require further bytes (SourceCopy/TargetCopy's signed delta, TargetRead's inline payload). A patch that decodes a prefix promising more bytes than remain on the wire is structurally broken.

slap rejects as a parse error. Same family as truncated-mid-varint and `metadata-size` sanity — declared length exceeds what's available.

### How wide does slap let BPS varints and cursor arithmetic go? (uses-frostmourne-to-butter-its-toast)

byuu's spec endorses arbitrary-width integers. slap's shared `Measure` types and Rust FFI boundary are built around machine-word-sized integers.

**slap caps BPS varints and cursor state at `Int` (63-bit signed on 64-bit systems). Values exceeding that produce a specific parse error naming the offending byte and the range.**

For a real BPS patch to need values beyond the cap, one of three things has to hold: (a) a file larger than 9.2 exabytes, (b) a patch that exercises the spec's arbitrary-width carveout by encoding values beyond `Int`, or (c) an authoring-tool bug producing nonsense varints. (b) is the spec-allowed case slap would prefer to support; we just don't, currently. See `notebook.md` for the scope sketch.

### What do we do when the decoder finishes past `size − 12`, or mid-action at the boundary?

byuu's stopping condition is `offset() >= size() − 12`. `>=` rather than `==` lets two end-states qualify as "stopped": the clean case (last action finished with `offset == size − 12`) and the overshoot case (last action's body consumed bytes that belong to the footer — TargetRead's inline payload, SourceCopy/TargetCopy's signed-offset varint, etc. ate into the 12 footer bytes).

slap rejects overshoot as a structural parse error. A well-formed BPS patch tiles its actions exactly up to `size − 12`; the footer bytes are untouched by action decoding. byuu's `>=` silently tolerates overshoot under a well-formed-patch assumption; slap doesn't make that assumption and treats the two end-states as distinct. Overshoot indicates a corrupt patch or a buggy authoring tool; either way the right response is to fail loudly rather than silently reinterpret footer bytes as action payload.

### What do we do about the two encodings for zero-delta (`0x80` vs `0x81`)?

`0x80` decodes to unsigned varint 0, which under the sign/magnitude scheme is sign-0 magnitude-0 — plain zero. `0x81` decodes to unsigned varint 1, which is sign-1 magnitude-0 — "negative zero." Both mean "add zero" semantically; they are indistinguishable in effect but distinct on the wire. Every other signed value has exactly one encoding.

slap emits `0x80` canonically for zero-delta on create, and accepts `0x81` on parse with a warning (or info, once an info channel exists — see `notebook.md`). Same shape as IPS's zero-count RLE: accept, flag, never emit. The warning flags a patch as unusual.

## Degenerate but structurally valid patches

### What about patches that are wire-valid but semantically-degenerate (zero-action, `source-size = 0`, `target-size = 0`, first-action TargetCopy)?

None of these are forbidden by the spec. They break down:

- **Zero-action + `target-size = 0`**: works. Produces empty output. Legal, pointless.
- **Zero-action + `target-size > 0`**: incoherent. Promises bytes, delivers none.
- **`target-size = 0` + any actions**: incoherent. Any action writes ≥ 1 byte into a zero-size target.
- **`source-size = 0`**: works. Produces target from inline bytes only. Equivalent to shipping the ROM.
- **First-action TargetCopy**: incoherent. Reads from an empty already-written region.

slap accepts all of these at parse time (they are wire-format-valid) and rejects the incoherent ones at apply time via normal bounds-checking.

## Trailing bytes

### What happens if a BPS patch has bytes appended after the footer?

byuu's spec says the footer is the last twelve bytes of the patch, and `patch-checksum` covers every byte before those twelve. Appending bytes means the footer is no longer at the end, and the spec is no longer being followed — the file is malformed.

slap rejects it. The parser reads the last twelve bytes as the footer, computes `patch-checksum` over the rest, and the check fails. No special detection for "trailing bytes"; no heuristic for stripping. BPS leaves no room for an EBP-style "trailing something" convention because the spec closes that door.

## Creation strategy

### Does slap create BPS patches in linear or delta mode?

Right now slap makes delta patches. Linear creation is an option permitted by the spec; it has not been thoroughly explored.
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
