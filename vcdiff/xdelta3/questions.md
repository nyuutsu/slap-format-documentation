# xdelta3 — design questions

Questions specific to the xdelta3 arc (see `spec.md`): the wire features xdelta3 adds that RFC 3284 never defined. Cross-arc disagreements (version byte, both-source-bits, the compressor-declared-⇒-xdelta3 classification) live in the family `../questions.md`.

## Secondary compression — the framing

### How does slap validate a secondary-compressed section?

A secondary-compressed section is a `dec_size` — its decompressed length — followed by the compressed bytes. xd3 requires the decompressor to use all of it and produce exactly `dec_size` bytes; the RFC leaves this open, so it's xd3 convention, and slap follows it to read real patches.

**slap enforces both, as separate errors:** unconsumed input and wrong-length output are different faults, and slap says which it hit. An empty section — no bytes, or a `dec_size` of 0 — is rejected; nothing honest decodes to nothing, and none of the 9,046 compressed sections in the patches we have hit either case (smallest real `dec_size`: 10).

### Are the three sections independent, and does a compressed section stand on its own?

A window's three sections — data, instructions, addresses — each carry their own compression bit, so a window may compress any subset. What the grammar doesn't settle is whether a compressed section decodes on its own or continues one from an earlier window.

**The three sit in separate secondary streams and never affect one another; whether a single section decodes alone depends on the compressor.** DJW sections stand alone. LZMA and FGK carry state across a stream's sections, so slap decodes a stream's sections together, in window order, through one decoder rather than one at a time. Each section still has its own `dec_size` framing; only the decoder's state crosses between them. The per-compressor mechanics are their own entries.

## Secondary compression — the compressors themselves

### How does slap decode LZMA-compressed sections?

xd3 emits LZMA as an xz/LZMA2 stream with check=none, not raw LZMA1. slap takes no C-library dependencies, so wrapping liblzma is out.

**slap decodes LZMA with `lzma-rs`, a pure-Rust library.** Every LZMA stream in the patches we have — 1,240 across 440 patches — decodes to its declared size.

Two things keep this from being turn-key, and slap handles both. A stream isn't a standalone `.xz`: the `.xz` header sits only on its first section and there's no footer, so slap gathers a stream's sections, strips that header, decodes the run, and splits the result back by `dec_size`. And xd3 flushes the stream rather than finalizing it, so the end-of-stream marker is never written; a decoder that reads to the end would fail at end-of-input, so slap appends one before decoding (a real marker, if ever present, stops the read there, leaving any trailing bytes as unconsumed input for the framing checks).

### How does slap decode DJW-compressed sections?

DJW is xdelta3's own multi-table Huffman coder, with no library and no written spec — only xd3's source defines it. It's the most common compressor, in 2,265 of the patches we have.

**slap decodes DJW with its own pure-Rust decoder, output byte-identical to xd3's.** The source is the specification, not a template: the one requirement is that the output matches, and within that slap's decoder is written to read well in Rust rather than transcribe the C. Each section carries its own tables and bitstream and decodes to its `dec_size` alone. slap applies every DJW patch we have and compares each finished ROM against xd3's.

### How does slap decode FGK-compressed sections?

FGK is xdelta3's adaptive Huffman coder, marked "for demonstration purposes only" in its own source and used by no real encoder — it appears in none of the patches we have. slap implements it anyway, for completeness.

**slap decodes FGK with its own pure-Rust decoder, output byte-identical to xd3's — the same stance as DJW.** Unlike DJW, FGK carries state: it grows one adaptive tree across a stream's sections rather than starting fresh each time, so slap decodes a stream's sections in order through one tree. With no real patches to check against, slap validates by synthesis: xdelta3 emits FGK sections under `-S fgk`, and they decode to the same bytes as the patch's uncompressed `-S none` form.

## Per-window Adler32

### How does slap check the per-window Adler32?

Each xdelta3 window can carry an Adler-32 of its own decoded output — the standard variant, mod 65521.

**slap verifies it: each window's output against its stored checksum.** slap's own Adler-32 is that same standard variant, so the stored checksums check directly, and the check can run as each window decodes or once over the finished output — same answer either way. A mismatch is fatal by default — it's the only integrity check VCDIFF has.

## Application header

### Is the application header opaque, or a metadata channel?

VCD_APPHEADER carries application-defined bytes, present in about 43% of the patches we have. The format fixes no meaning for them: slap can't know the bytes' structure or encoding, only that they're present. But in practice the field is a real metadata channel: LODModS stores source and target file paths there, and xd3 stores its own.

slap treats it as a metadata channel, the same as BPS's metadata blob. The bytes are carried byte-exact: surfaced in `info`/`explain`, extractable, preserved across xdelta3→xdelta3 convert, and settable or droppable by the usual metadata flags. BPS↔xdelta3 conversions carry the blob across — both formats define an anything-goes metadata area. Where the bytes read as text — file paths, prose, sometimes in cp1252 — slap shows them, honoring `--metadata-encoding`.

Create writes no appheader by default. Some appliers, given no output argument, read this field as a filename-and-compressor record rather than free metadata — so when a user sets content of some other shape, create warns that those tools may misbehave on a patch handed to them without an output argument. The field can also direct the applier to recompress its output (none of the patches we have do); slap's output is always what the instructions produce, and it notes any such request it didn't honor.
