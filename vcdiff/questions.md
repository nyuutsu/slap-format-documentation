# VCDIFF family — design questions (cross-cutting)

Questions that span the whole family rather than one arc: the high-altitude "what should slap's VCDIFF be," the band where RFC 3284 and xdelta3 genuinely disagree, and the operational matters the RFC leaves silent. Per-arc questions live in `core/`, `rfc-vcdiff/`, `xdelta3/` `questions.md`.

Every entry is anchored to the passage (or the silence, or the disagreement) that raised it — `rfc3284.txt` by section, xd3 by source file/line. If an entry can't be anchored, it doesn't belong here yet. Each entry's agreed disposition follows in bold.

Flavor tags:
- `(open-design)` — genuinely up to us; pick the most principled option.
- `(rfc-vs-xd3)` — the standard and the dominant tool disagree; we choose, possibly per-arc. Unique to this family.
- `(rfc-silent)` — rigid standard, operational gap.
- `(one-right-answer-unknown)` — almost certainly one tolerable answer; we don't know it yet.
- `(is-this-the-right-question)` — suspect a fork here, unsure of framing.
- `(uses-frostmourne-to-butter-its-toast)` — the spec grants power far beyond practice; how far do we chase it.

## What slap, as an encoder, should be

### What is slap's encoder posture, and what does a slap-made VCDIFF patch look like? `(open-design)`

RFC §1-2 designs explicitly for this latitude: *"Algorithm genericity: the decoding algorithm is independent from string matching and windowing algorithms. This allows competition among implementations of the encoder while keeping the same decoder."* The decoder is universal; the encoder is free. So slap-as-encoder gets to decide what a slap patch *is*: how it windows (one big window vs many), whether it leans on source-copies or target self-references, whether it emits per-window checksums, whether it secondary-compresses at all. This isn't about internal data structures (the code settles those) — it's the observable character of our output. What posture do we want, and what's the default `slap create` shape? (Strongly interacts with the xdelta3-arc compression and adler emission questions.)

**A slap-made patch is whatever the user asked for by name, with the most checkable, most ordinary defaults that thing supports.** There is no "VCDIFF-family" default to pick: `--format` names a specific format, and xdelta3 and RFC-VCDIFF are two entries in that menu, each free to use its own features and no other's.

For xdelta3, the defaults follow the canonical tool's own emission shape, plus the practice of its users: Adler32 on (`--omit-verification` opts out), secondary compression on using LZMA, the canonical tool's own default, kept only where it actually pays (`--no-compress` opts out), windows of 8 MiB. `--compress-with` redirects to DJW; FGK's name resolves too, but slap declines to encode with it. For RFC-VCDIFF there is no canonical tool to mirror, so the default is the parameterless shape: one window spanning the whole target. That is also the size-favoring shape — a window's copies reach only its own segment and its own output, so each split shrinks the encoder's reach and adds framing — and splitting's real benefits are memory bounds slap, holding whole files in memory, doesn't need.

Window size is exposed as a creation option on both — encoder freedom is the format's own invitation — with the knob's documentation noting that the xdelta3 tool refuses to decode a window larger than its compiled `XD3_HARDMAXWINSIZE`: 16 MiB in the widespread 3.0.11 builds, 64 MiB in later sources. So the 8 MiB default is not just mirroring — staying at or under it is what keeps a slap patch decodable by every xdelta3 build. The appheader's create-side story lives in its own question; the version byte and reserved bits are settled in theirs.

### Given VCDIFF carries no source checksum, what is slap's stance on "did the user supply the right source"? `(open-design)`

VCDIFF has no source-integrity field anywhere — unlike BPS/UPS, there is nothing in the patch to check the source against before applying. The *only* integrity mechanism is xdelta3's per-window target Adler32 (xdelta3-arc, present in ~97% of the patches we have but absent on every RFC-arc and CoreOnly patch). So for a large class of valid patches, slap has no way to know the source is correct until (maybe) a target checksum fails after the fact — or never. What is the verification posture when there is structurally nothing to verify the input against? Do we lean entirely on the target-side adler when present, warn when it's absent, something else?

**slap is honest about the limit: with no source checksum in the format, "right source" mostly cannot be checked — so slap uses what each patch actually carries, and says so when that's nothing.** When a patch carries the per-window Adler32, a mismatch after decoding is the standard fatal verification error, downgradeable to a warning with `--no-verify`. When an xdelta3-arc patch arrives without it, the creator turned a default off, and slap notes that it cannot attest the result — the same notice xdelta1's no-verify flag earns. RFC-arc and CoreOnly patches stay silent: the format gives them no slot for integrity data, and that absence is a fact about the format, not about the patch.

## Where RFC 3284 and xdelta3 disagree

### Version byte: honor the RFC's "reserved for the future," or adopt xdelta3's hard reject — and does the answer differ by arc? `(rfc-vs-xd3)`

RFC §4.1: the version byte *"is currently set to zero. In the future, it might be used to indicate the version of Vcdiff."* — i.e. reserved, forward-looking, no rejection prescribed. xdelta3 hardcodes `0x00` and rejects anything else outright ("VCDIFF input version > 0 is not supported", `xdelta3-decode.h:858`). Two authorities, opposite postures. On the xdelta3 arc, mirroring xd3 (reject) is obvious. On the RFC arc, do we keep the RFC's openness (accept-and-flag? attempt?) or also reject? This is the cleanest instance of the family's signature tension.

**Reject any nonzero version, both arcs, for one reason: we implement v0 and don't know how to interpret another.** The two arcs land in the same spot — a non-v0 xd3 and a non-v0 RFC-VCDIFF are equally unreadable to us — so the "tension" dissolves: this isn't the RFC forbidding v>0 (it reserves the byte for future versioning and prescribes nothing), it's a decoder declining a version it can't read, which it's entitled to do. Same place xd3 lands ("version > 0 is not supported"), reached on our own terms. We don't mint a private version either, so slap only ever emits v0 (asserting "we are v1" isn't ours to do — that namespace is the format's, not an implementer's). The refusal is flavor-aware: it classifies the patch (xd3-produced or not) and says which, so the message is "an xd3 patch / a VCDIFF patch declaring unsupported version N," not a bare complaint.

### Both source bits set: the RFC forbids it, xdelta3 silently degrades it — which do we do? `(rfc-vs-xd3)`

RFC §4.2 is a hard MUST NOT: a Win_Indicator *"MUST NOT have more than one of the bits set."* xdelta3 doesn't reject it — its `SRCORTGT` macro (`xdelta3-decode.h:22`) maps a both-set indicator to "neither," silently dropping the source window (and then misreading the segment varints the encoder wrote). So the standard says illegal; the tool says degrade-quietly. slap's instinct is reject-loud (and the patches we have show zero both-set, so it costs nothing) — but is "reject" the right call on the xdelta3 arc, where matching xd3's leniency might apply some odd patch we haven't seen? Per-arc, or uniform?

**Reject, uniformly — a Win_Indicator with both copy-source bits set is malformed.** RFC §4.2 makes it a MUST NOT, and it's a contradiction in the window header (a window cannot name two copy sources at once), so we decline to apply, on both arcs. Costs nothing in practice: zero both-set patches in hand, and it isn't a shape any encoder produces.

### Is "RFC-VCDIFF with a secondary compressor declared" even a coherent thing to accept? `(rfc-vs-xd3)` `(is-this-the-right-question)`

The RFC defines the *framing* for secondary compression (VCD_DECOMPRESS, the compressor-id byte, the Delta_Indicator bits — §4.1, §4.3) but defines **no actual compressor**: §6 decodes "assuming that any such compressed data has been decompressed," and §1-2 frames secondary encoders as an open extension point with no registry. So a patch that is otherwise pure RFC but declares a compressor id is naming a codec the RFC never specified — the id can only mean an xdelta3 (or other-tool) catalog entry. Does that make any compressor-declaring patch *ipso facto* xdelta3-arc (our current classification leans this way)? Can an RFC-arc patch legitimately carry VCD_DECOMPRESS at all, or is declaring compression the thing that ejects a patch out of the RFC arc?

**An RFC-arc patch can declare secondary compression, but the RFC never defined any actual compressor — so slap understands the declaration and cannot honor it.** The framing (the indicator bits, the compressor-id byte) is the RFC's own and parses normally, in both arcs. What was never defined is what any id *means*: the RFC arc's compressor catalog is empty, and slap doesn't fill it by guessing that the ids mean what xdelta3's catalog says. So a patch with RFC-exclusive features that declares and uses compression is refused at the catalog lookup — "declares secondary compressor N, and no compressor is defined by that number here." The same refusal covers an id no catalog anywhere knows. A patch with no RFC-exclusive features declaring an id from xdelta3's catalog is an xdelta3 patch, decoded normally. Create and convert-to-RFC never offer secondary compression: there is nothing defined to offer.

## What the RFC leaves silent (operational)

### How does a decoder know the patch is over, and what do we do with bytes after the last window? `(rfc-silent)` `(open-design)`

RFC §4 lays out the file as a header followed by windows, with **no window count, no total-target-size field, and no footer** — nothing bounds where the window stream ends. A decoder reads windows until input runs out. This surfaced concretely in the patches we have: the LODModS suite ships valid single-window patches with ~452 trailing `FF FF FF FF 00…` bytes; `xdelta3 -d` writes the correct output and *then* errors trying to parse the trailing bytes as a malformed window, while `xdelta3 printhdr` ignores them (exit 0). So real working patches carry trailing junk, and even the canonical tool is of two minds. What is slap's termination model, and is a trailing remnant fatal, ignored, or warned-and-tolerated? (This is the one where the patches in hand forced a real choice rather than a settled fact.)

**Permit exactly one shape, with a note — settled with some unease, and narrowed from "permit a remnant" to "permit *this* remnant".** A trailing remnant is tolerated only when it is precisely the shape the field data shows: the four bytes `FF FF FF FF` followed by any number (zero included) of `0x00` bytes, running to end of input, sitting where the next window would otherwise begin. That shape is consumed and surfaced as a note naming what was seen — the recognizable tail and its byte count. Anything else trailing is malformed exactly as before: a single non-zero byte after the padding, a tail that opens differently, a patch truncated mid-window — all keep their rejections. Tolerance extends precisely as far as the evidence that motivated it — this one toolchain's tail — and a second shape earns its place the way this one did, by existing in the wild, not by generalization. The remnant's bytes are not patch semantics and are carried nowhere: create never emits a tail, and convert drops it by construction, the same note being the record of the drop.

The unease is worth keeping on the record: the format defines no footer, count, or terminator, so a trailing remnant is *undefined* — "permit" is a posture the format neither blesses nor forbids, not a right it grants. And the choice leans on the canonical tool being of two minds (above): with no single reference behavior to mirror, "apply and say what we saw" reads as more honest than `-d`'s die-on-the-remnant or `printhdr`'s silent swallow.

This covers bytes after a *complete, valid* final window only. Bytes *inside* a malformed window are a broken structural claim, rejected by the same checks as any bad window — not by this rule.

### Is a patch with zero windows well-formed, and what does it produce? `(rfc-silent)`

RFC §4 describes a sequence of windows but doesn't say the sequence may be empty, nor forbid it. A header-only patch (magic, version, indicator, EOF) decodes to… an empty target? Is that a valid no-op patch or a malformation? (Parallels the BPS empty-patch question.)

**Well-formed — accept it; it produces an empty target, and we say so.** Zero windows is a sequence the RFC neither forbids nor requires to be non-empty, so we don't force a reject. It is neither a no-op nor a malformation: the target is the concatenation of zero windows — an **empty, 0-byte file**. It does not pass the source through; it produces nothing. We accept and apply, with an `EmptyPatch` (`EmptyWindows`) advisory whose message conveys the *empty-output* result rather than a bare "0 windows," since the empty output is the consequential part (unlike the sparse-edit formats, where zero units means output == input). xdelta3 declines a zero-window patch (`xdelta3 -d`: "nothing to output"); the spec leaves it open, and our usual stance is to accept an odd-but-valid patch and say something about it. Parallels BPS's empty patch, which likewise yields an empty file.

### What does "abort on a malformed patch" actually mean for slap? `(rfc-silent)`

The RFC specifies how to decode a well-formed patch and is silent on decoder behavior facing a malformed one — it never says "abort," let alone what abort entails (partial output? nothing written? exit code?). This is largely slap-wide policy, but VCDIFF's streaming, window-by-window nature raises a specific version: if window 3 of 5 is malformed, windows 0–2 already produced output — is that output discarded, or is partial output ever surfaced? (xdelta3 -d writes-then-errors, per the trailing case above; do we?)

**Abort means: write nothing, emit the structured error, exit nonzero — no partial output, ever.** The RFC is silent here, so this isn't spec-derived; it falls out of slap's apply architecture: apply builds the whole target in memory and writes it once, only on success, so a failure at parse or mid-decode never reaches the write. The window-streaming wrinkle dissolves — windows 0–2's bytes live in the in-memory buffer and are discarded with the failed apply, never surfaced.

### Does slap narrow VCDIFF's deliberate word-size independence, and is that acceptable? `(uses-frostmourne-to-butter-its-toast)`

RFC §1-2 lists as a core design goal: *"Data portability: the basic encoding format is free from machine byte order and word size issues."* The varint is unbounded by design — a patch encoded on a 128-bit machine should decode anywhere. slap's `Measure` types and Rust FFI are machine-word-sized, so we will impose a cap the format pointedly does not. Where is the cap, what's the error past it, and do we state plainly that we're narrowing a portability guarantee the RFC makes on purpose?

**Resolved in `core/questions.md` → "The decoded width cap."** Yes, acceptable, and stated plainly: we cap at the host signed `Int64`, with a dedicated apologetic error for the `[2⁶³, 2⁶⁴)` band xd3 admits and we decline, and a plain over-width rejection above `2⁶⁴`. The full reasoning lives there; this entry just points at it.

## Resource limits

### What bounds the memory a patch can make slap allocate, across the size fields that drive allocations? `(open-design)`

Several fields let a tiny patch dictate a large up-front allocation, before slap has the bytes to justify it: a secondary section's decompressed size (`dec_size`, reserved before decode — xdelta3 arc), a custom code table's near/same cache sizes (rfc arc), a window's declared target size (core), and the appheader length (xdelta3 arc). Each is attacker-controlled — a small crafted or corrupt patch names a huge value and forces the reservation (a denial-of-service shape). Rather than an ad-hoc cap at each of the four sites, which invites inconsistency, the question is whether **one patch-wide allocation budget** governs them all: what the ceiling is, how it's derived (a fixed cap, proportional to patch or input size, memory-aware), and how each site consults it. Consumers: secondary-compression framing (`xdelta3/`), custom code tables (`rfc-vcdiff/`), window sizing (`core/`), appheader (`xdelta3/`).

**There is no single allocation budget. Each size field is bounded by what it actually is.**

When a field declares that some number of bytes follow — the appheader, the code-table data — slap confirms the file really has that many bytes left before reading or allocating anything. The check happens before the allocation, and the arithmetic is overflow-safe even for enormous declared values.

The custom-table cache sizes need no attention here: the wire format only lets them declare about half a megabyte at most.

A window's target size and a section's decompressed size genuinely have no bound in the format, so slap doesn't impose one of its own — it honors what the patch declares, up to `Int64`. If the machine can't provide that much memory, slap reports that plainly, naming the size the patch asked for.

## Surfacing the flavor

### How does slap name a patch's flavor to the user, especially a CoreOnly one? `(open-design)`

The parser's verdict is `XDelta3 | RFCVanilla | CoreOnly`, and CoreOnly
(a patch using only shared-core features — 7 of 187 in the curated sample) is genuinely both-and-neither: valid under either flavor, apply-identical. What does `info`/`explain` *call* it? "VCDIFF (RFC-3284 core; no dialect-specific features)"? Does the label claim a flavor it can't actually determine, or honestly report the indeterminacy? Small, but it's user-facing and the honest answer isn't obvious.

**Named by what it is: the shared core.** The label never picks a side a determination wasn't made for — a CoreOnly patch is presented as VCDIFF using only the features the RFC and xdelta3 share, with `info` carrying a short form and `explain` room for the full sentence. The exact user-facing wordings — including what the RFC flavor itself ends up being called — are implementation, and may be revised together.
