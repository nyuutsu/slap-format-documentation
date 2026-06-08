# VCDIFF family — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere — the `questions.md` set holds the decided dispositions. Sketches are sketches.

### Module split mirrors the partition

The three coequal peers (`partition.md`) want to be three modules under `Slap.VCDIFF`:

- `Slap.VCDIFF.Core` — the shared codec: varint, window frame, the superstring walk, ADD/RUN/COPY, the default code table, the address cache. Owned by neither flavor; defined as the intersection.
- `Slap.VCDIFF.RFC` — VCD_TARGET handling, custom-code-table decode.
- `Slap.VCDIFF.XDelta3` — Adler32, appheader, secondary compression.

The standard family layout (`Types`/`Parse`/`Apply`/`Describe`/`Create`) presumably lives under each, with `Core` holding the bulk. Naming stays locally obvious: never a bare ambiguous `VCDIFF` for "the vanilla one" — that's `RFC`.

### Flavor as a three-way sum, with CoreOnly first-class

The parser's verdict wants to be a closed sum, not a `Bool` or a version byte:

```haskell
data VCDIFFFlavor
  = XDelta3   XDelta3Patch
  | RFCVanilla RFCVCDIFFPatch
  | CoreOnly  CorePatch   -- used only shared features; flavor genuinely indeterminate
```

`CoreOnly` is the honest name for the ~feature-free patch (7 of 187 in the sample): valid under either flavor, apply-identical. It is a named state, not a silent default toward one side. `info`/`explain` then has something true to say about it rather than picking a flavor it can't determine. (Cross-ref the family `questions.md` CoreOnly-naming entry.)

### Flavor-specific patch types make wrong states unrepresentable

The payoff of the split: each flavor's patch type can only hold what that flavor permits. An `RFCVCDIFFPatch` window has no field for an Adler32; an `XDelta3Patch` window has nowhere to put a VCD_TARGET segment or a custom table. So "an RFC patch with a checksum" or "an xdelta3 patch with a custom code table" can't be constructed — the chimera no real patch produces is unrepresentable, not merely unobserved. Secondary-compression *framing* is the exception: it lives in both arcs' models, the RFC arc's compressor catalog simply being empty (the family compressor-classification disposition). The shared window mechanics (instructions, sections) live in `Core` types both reuse.

### One decode core, shared by apply and explain

The current code carries the whole decode algorithm *twice* — `applyWindow` (IO, writes bytes) and `decodeWindowInstructions` (ST, accumulates for explain) — hand-synced and free to drift. The rewrite wants a single core walk that both consume: apply writes, explain records, but the instruction/address/cache logic exists once. This is the "one source of truth" the rest of slap holds to.

### Typed ApplyError, mirroring BPS — the silent-`pure 0` disease dies

Today VCDIFF apply swallows every fault into `pure 0` and returns `Left` only on a negative total size. The rewrite wants a structured `ApplyError` like BPS's, with a constructor per real failure: address points at unwritten target, copy exceeds its source segment, source read past the real source, section exhausted (one per section), window under- or over-fills its declared size, adler mismatch. Rendering at the boundary; no zero-substitution anywhere.

### Buffer allocation should not be `unsafeCreate`

Current apply uses `unsafeCreate` (uninitialised memory) and a fabricated "implicit source fill" to paper over windows that don't fill. The rewrite wants BPS's shape: a zero-or-tracked buffer plus an exact-fill check (`ApplyWindowUnderfilled`), so an unfilled byte is an error, not heap garbage. (Cross-ref core invariant #3 and the abort-semantics question.)

### Secondary decompression is a preprocessing pass, orthogonal to decode

A clean layering falls out of RFC §6 ("assuming that any such compressed data has been decompressed"): secondary decompression turns the on-wire (possibly-compressed) sections into the plain byte arrays the `Core` decode loop already consumes, before the instruction walk — the walk needn't know compression exists. The unit is the section-*kind*, not the section: a kind's compressed pieces across all windows form one continuous stream (compressor header in the first window only, decoder state carrying across — the xdelta3 framing dispositions). slap gathers a kind's pieces in window order, decodes once, and splits back by each section's declared size.

### Where the compressors live: rusty-slap

DJW / LZMA / FGK are byte-crunching → rusty-slap, per the language priority. DJW (the most common compressor in the patches we have) and FGK have no library — we write our own clean Rust decoders, bit-exact in output, from the xdelta3 source as specification. LZMA is xz/LZMA2; `lzma-rs` decodes all of it, verified against every LZMA stream in the patches we have. The decode side feeds the preprocessing pass above.

### Routing: detect coarse, resolve flavor at parse (the EBP pattern)

`Detect` maps the `D6 C3 C4` magic to a family routing target only — it honestly can't know the flavor without reading feature content. The
parser, which *does* read the content, resolves `XDelta3 | RFCVanilla |
CoreOnly` and sets `patchFormat`. This is exactly how `PATCH` magic already resolves to `LabelIPS` vs `LabelEBP` by content; no new routing machinery.

### Custom-table decode wants a core primitive neither Parse nor Apply owns

Decoding a custom table means *running a VCDIFF apply* (the inner delta against the serialized default table) during parse — a Types→Apply cycle the current code breaks by threading an `applyInnerDelta` function. A cleaner shape might be a small `Core` decode primitive that both the custom-table path and the main apply path call, so the dependency is "both use Core" rather than "Parse reaches into Apply." Worth weighing when the module graph is drawn.

### VCD_TARGET keeps the whole produced target live

A VCD_TARGET window may read any earlier window's output (full reach — the rfc-vcdiff reach disposition), so the produced target stays addressable across the whole patch. We're building that buffer anyway (the target is the output), so this costs nothing.

### Create leans on rusty-slap's existing diff machinery

RFC §1-2 blesses encoder freedom ("competition among implementations of the encoder"). So a slap VCDIFF encoder is free to reuse rusty-slap's BPS-diff / suffix-array machinery to find source matches, then emit them as VCDIFF COPY/ADD/RUN. The windowing and match strategy are ours to choose; the decoder anyone uses doesn't care. Far off, but the building blocks already exist in the tree.

### Malformation is caught by the chain, not by any single check

The format's lengths are definitional — there is no independent record of where a field "should" end — so a wrong length can't be detected at the field that declares it. An early length that over-claims passes its own bytes-exist check, eats bytes that belonged to later structures, and gets caught downstream, when the next structure fails to decode or a read runs out of file. The error surfaces later than the cause, sometimes blaming a later field.

What makes that acceptable is a chain property rather than any single check: the cursor only moves forward and every read is bounds-checked; every consumed region must satisfy its own structure (the table's exact 1536 bytes, the enc-len cross-check, exact section consumption); and the chain must end cleanly — at end-of-file, or at the tolerated, noted trailing remnant. Every byte of the file is accounted for by exactly one of: header field, window, or noted remnant. There is no region the parser passes over silently.

If nothing downstream ever fails, the boundaries are simply a valid reading of the file. The format carries no checksum over its own framing, so it has no second notion of where the fields were "meant" to be.

Maybe worth stating as an explicit invariant when the parser is designed; maybe it just falls out of the byte-parser discipline. Noted so the understanding survives.
