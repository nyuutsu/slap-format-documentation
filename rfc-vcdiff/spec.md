# VCDIFF — Format Specification

This is a clean writeup of the on-disk structure of a VCDIFF patch,
derived from RFC 3284 (June 2002). Where this document and the RFC
disagree, the RFC wins. xdelta3 extensions are documented inline and
marked as such.

This document covers both "vanilla" RFC 3284 VCDIFF and the xdelta3
dialect. They share a wire format; xdelta3 adds optional features
that vanilla VCDIFF decoders can either handle or reject.

## Identity

- **Magic**: `D6 C3 C4` (3 bytes — ASCII `V`, `C`, `D` with MSB set)
- **Version byte**: `0x00` or `0x53`. See "Version byte" below.
- **Endianness**: big-endian for varints and all multi-byte integers
- **Authors**: D. Korn, J. MacDonald, J. Mogul, K. Vo
- **Published**: June 2002, RFC 3284
- **xdelta3 extensions**: Joshua MacDonald (same MacDonald as the RFC)
- **Reference tools**: open-vcdiff (Google), xdelta3
- **License**: RFC (IETF standard); format itself is unencumbered

## Varints

VCDIFF encodes unsigned integers in base-128, big-endian, with the
MSB of each byte indicating "more bytes follow." The final byte has
MSB clear. Example from the RFC: `123456789` encodes as `BA EF 9A 15`.

This is NOT the byuu varint used by UPS/BPS. The byuu varint is
little-endian, MSB-means-stop, with a subtract-one trick for
canonicality. The two encodings are incompatible and produce
different byte sequences for the same integer.

Cross-reference: `Slap.Get.vcdiffVarint` (this format),
`Slap.Binary.getByuuVarint` (UPS/BPS).

## Version byte

Byte 4 of the header. The RFC says it is "currently set to zero"
and "in the future, it might be used to indicate the version."

Two values are known:

- `0x00`: standard. Used by all known encoders, including xdelta3
  in its default mode. Does NOT reliably indicate "vanilla VCDIFF" —
  xdelta3 writes `0x00` even when using its own extensions
  (checksums, app data, secondary compression).
- `0x53` (ASCII `S`): xdelta3 marker. Indicates the file MAY use
  interleaved window layout and/or per-window Adler32 checksums.
  [Source: xdelta3 source code and open-vcdiff documentation.]

**Corpus observation** (original research): across ~4,300 VCDIFF-family
patches from romhacking.net (2024-08-01), zero use `0x53`. Every file
is `0x00`, including those with xdelta3 extensions active. The `0x53`
value exists in xdelta3's source but is not emitted in default mode.

**This byte is not a reliable format discriminator.** Feature
detection must come from the actual header indicator bits and
per-window content, not from this byte.

slap accepts `0x00` and `0x53` without diagnostic. Any other value
is rejected.

## Overall structure

```
Header
    Magic                               3 bytes: D6 C3 C4
    Version                             1 byte
    Hdr_Indicator                       1 byte (bitmask)
    [Secondary compressor ID]           1 byte (if bit 0 set)
    [Code table length]                 varint (if bit 1 set)
    [Code table data]                   N bytes (if bit 1 set)
    [Application data length]           varint (if bit 2 set)
    [Application data]                  N bytes (if bit 2 set)
Window 0
Window 1
...
Window N-1
```

Items in brackets are conditional on the header indicator bits.
There is no footer and no file-level checksum.

## Header indicator (Hdr_Indicator)

A bitmask byte. Three bits are defined:

| Bit | Name             | Meaning |
|----:|------------------|---------|
|   0 | VCD_DECOMPRESS   | Secondary compressor ID byte follows |
|   1 | VCD_CODETABLE    | Custom code table data follows |
|   2 | VCD_APPHEADER    | Application-defined data follows |

Bits 3–7 are reserved and must be zero per the RFC.

**Bit 0 — VCD_DECOMPRESS.** If set, the next byte is a compressor
ID. This declares that individual windows MAY use secondary
compression on their data sections. The presence of this bit does
NOT mean all windows are compressed — each window independently
opts in via its own delta indicator byte.

Known compressor IDs [source: xdelta3 source code; not registered
with IANA despite the RFC deferring to IANA for this registry]:

| ID   | Algorithm | Description |
|-----:|-----------|-------------|
|    1 | DJW       | Static Huffman (David J. Wheeler) |
|    2 | LZMA      | Lempel-Ziv-Markov chain |
|   16 | FGK       | Adaptive Huffman (Faller, Gallager, Knuth) |

Corpus observation (original research): DJW (1) and LZMA (2) are
both common. FGK (16) was not observed. Some patches declare a
compressor in the header but have windows that don't use it —
this is valid.

**Bit 1 — VCD_CODETABLE.** If set, a custom code table follows.
It is encoded as a varint length and then that many bytes of
compressed table data. The table data is itself a VCDIFF delta
applied against the serialized default code table (see §7 of the
RFC). Nested custom code tables are forbidden — the inner delta
must use the default table.

Corpus observation: zero patches use custom code tables.

**Bit 2 — VCD_APPHEADER.** Not in RFC 3284. xdelta3 extension. If
set, a varint length and that many bytes of application-defined
data follow. Standard decoders must skip this data. xdelta3 uses
this for internal metadata.

Corpus observation: the majority of xdelta3-produced patches set
this bit. No vanilla VCDIFF patches do.

## Window structure

Each window encodes one chunk of the target file.

```
Win_Indicator                       1 byte (bitmask)
[Source segment length]             varint (if Win_Indicator nonzero)
[Source segment position]           varint (if Win_Indicator nonzero)
Delta encoding of the target window (see next section)
```

**Win_Indicator** has two relevant bits:

| Bit | Name       | Meaning |
|----:|------------|---------|
|   0 | VCD_SOURCE | COPY instructions reference the source file |
|   1 | VCD_TARGET | COPY instructions reference earlier target output |

- If neither bit is set: the window is self-contained (ADD/RUN only,
  no COPY from external data).
- If one bit is set: source segment length and position follow,
  identifying the region of source (or earlier target) that COPY
  addresses resolve against.
- Both bits set: **forbidden** by the RFC (MUST NOT). The address
  space for COPY instructions becomes ambiguous — the decoder cannot
  determine whether the source segment comes from the source file or
  from already-decoded target data. slap rejects this at parse time.

Corpus observation: zero patches set both bits.

## Delta encoding (per window)

The delta encoding is the body of each window. Its total length is
declared up front, and the content is:

```
Length of the delta encoding        varint (total bytes that follow)
  Target window size                varint
  Delta_Indicator                   1 byte (bitmask)
  Length of data for ADDs and RUNs  varint (call it A)
  Length of instructions and sizes  varint (call it I)
  Length of addresses for COPYs     varint (call it C)
  [Adler32 checksum]               4 bytes, big-endian (xdelta3 only)
  Data section for ADDs and RUNs    A bytes
  Instructions and sizes section    I bytes
  Addresses section for COPYs       C bytes
```

**Target window size** is the number of bytes this window produces
when decoded. The decoder allocates this much space.

### Delta_Indicator (per-window secondary compression)

A bitmask controlling which of the three data sections are
secondary-compressed:

| Bit | Name         | Meaning |
|----:|--------------|---------|
|   0 | VCD_DATACOMP | ADD/RUN data section is compressed |
|   1 | VCD_INSTCOMP | Instructions section is compressed |
|   2 | VCD_ADDRCOMP | Addresses section is compressed |

If any bit is set, VCD_DECOMPRESS must have been set in the file
header (declaring a compressor ID). Compressed sections must be
decompressed before the delta instructions can be decoded.

Corpus observation (original research): most compressed patches
compress all three sections. Some compress only a subset (e.g.
instructions + addresses but not data). Some declare a compressor
in the header but set no bits here — valid, just means this
particular window isn't compressed.

### Adler32 checksum (xdelta3 extension)

Not in RFC 3284. xdelta3 inserts a 4-byte big-endian Adler32 of
the target window data between the section lengths and the data
sections. No header bit or version byte signals its presence.

**Detection.** The delta encoding length tells us where the window
ends. The three section lengths tell us how big the data is. The
varints at the top take some bytes. Working backward from the
known end (subtracting A + I + C) gives us where the data sections
start. Whatever bytes remain between the end of the length fields
and the start of the data sections is the gap. If the gap is
exactly 4 bytes, they are an Adler32. If 0, no checksum. Any other
value is a structural error.

This back-to-front arithmetic turns a content heuristic ("do these
bytes look like a checksum?") into a measurement ("there are exactly
N unaccounted bytes"). The measurement is reliable — across ~4,300
patches the gap is always exactly 0 or exactly 4.

**Enforcement.** On xdelta3 patches (those with checksums present):
enforce. Compute the Adler32 of the decoded target window and
compare. Mismatch means the output is wrong. On vanilla VCDIFF that
unexpectedly has a 4-byte gap: also enforce, but diagnostic messages
should note that checksum detection is heuristic-based, since the
RFC does not define this space.

Corpus observation (original research): 188 of 239 rfc-vcdiff-bucket
patches have the checksum despite having no xdelta3 header flags.
These were produced by xdelta3 with secondary compression disabled.
49 patches have gap=0 (truly plain). 199 of 200 sampled xdelta3-
bucket patches have gap=4. The checksum is nearly universal in
xdelta3 output but not absolutely guaranteed.

## Delta instructions

Three instruction types:

| Type | Code | Arguments | Meaning |
|------|-----:|-----------|---------|
| ADD  |    1 | size, data bytes | Write `size` literal bytes to target |
| RUN  |    2 | size, byte | Write `byte` repeated `size` times to target |
| COPY |    3 | size, address | Copy `size` bytes from source or earlier target |
| NOOP |    0 | (none) | No operation (used for padding in code table pairs) |

**ADD** consumes `size` bytes from the data section (or inline in
interleaved mode) and writes them to the target window.

**RUN** consumes 1 byte from the data section and writes it `size`
times to the target window.

**COPY** copies `size` bytes starting at `address` in the
superstring `U = S + T`, where S is the source segment and T is the
target window being built. If `address < len(S)`, the copy is from
source. If `address >= len(S)`, the copy is from earlier target
output (self-referential). Self-referential copies may overlap
their destination — this is valid and enables encoding of periodic
sequences (e.g. `COPY 12, 24` where the source region and target
region overlap).

## Instruction code table

Instructions are not encoded directly. Instead, each byte in the
instructions section is an index into a 256-entry code table. Each
entry defines either one or two instructions (a pair):

```
entry = (inst1, size1, mode1, inst2, size2, mode2)
```

- `inst`: NOOP(0), ADD(1), RUN(2), or COPY(3)
- `size`: if nonzero, the instruction's size is this value. If zero,
  the size is encoded as a varint immediately following the code byte
  in the instructions stream.
- `mode`: address encoding mode for COPY (see below). Zero for
  ADD/RUN/NOOP.

The second triple (inst2, size2, mode2) allows two common small
instructions to be encoded in a single byte. If inst2 is NOOP, the
entry encodes only one instruction.

The **default code table** is a fixed 256-entry table defined by the
RFC (§5.6). It is not stored in the file. Key structure:

| Indices   | Instruction(s) |
|-----------|----------------|
| 0         | RUN (size separate) |
| 1–18      | ADD (size 0 then 1–17) |
| 19–162    | COPY (size 0 then 4–18, modes 0–8) |
| 163–246   | ADD (size 1–4) + COPY (size 4–6, modes 0–8) |
| 247–255   | COPY (size 4, modes 0–8) + ADD (size 1) |

Applications may define a custom code table via the VCD_CODETABLE
header flag. The custom table is encoded as a VCDIFF delta applied
to the serialized default table. Corpus observation: zero patches
use custom code tables.

Cross-reference: `Slap.VCDIFF.Types.defaultCodeTable`,
`Slap.VCDIFF.Types.decodeCustomTable`.

## Address cache modes

COPY addresses are encoded relative to cached addresses to reduce
their size. The decoder maintains two caches, initialized to zero
at the start of each window:

**Near cache**: circular buffer of `s_near` slots (default 4).
After each COPY, the address is written to the next slot.

**Same cache**: hash table of `s_same * 256` slots (default 768).
After each COPY, the address is written to slot `addr % (s_same * 256)`.

The address mode (stored in the code table entry's `mode` field)
determines how to decode the address from the addresses section:

| Mode | Name | Decoding |
|-----:|------|----------|
| 0 | VCD_SELF | `addr = read_varint()` |
| 1 | VCD_HERE | `addr = here - read_varint()` |
| 2 to s_near+1 | Near | `addr = near[mode-2] + read_varint()` |
| s_near+2 to s_near+s_same+1 | Same | `addr = same[(mode-(s_near+2))*256 + read_byte()]` |

With default cache sizes (s_near=4, s_same=3), there are 9 modes
(0–8). Same-mode addresses are encoded as a single byte (not a
varint), because the same cache guarantees the result is in [0,255].

Both caches are updated after every COPY instruction's address is
decoded, keeping encoder and decoder synchronized.

Cross-reference: `Slap.VCDIFF.Apply` (address decoding and cache
management).

## Decoding procedure (sectioned layout)

For each window, after decompressing any secondary-compressed
sections, the decoder has three byte arrays:

- `data`: bytes for ADD and RUN instructions
- `inst`: instruction codes and out-of-line sizes
- `addr`: encoded COPY addresses

Processing reads one byte from `inst` as a code table index, looks
up the entry, and for each of the (up to two) instructions in that
entry:

1. If size is 0 in the table entry, read a varint from `inst`.
2. **ADD**: consume `size` bytes from `data`, write to target.
3. **RUN**: consume 1 byte from `data`, write it `size` times.
4. **COPY**: decode address from `addr` using the entry's mode,
   copy `size` bytes from the superstring U, update caches.

Repeat until the instruction stream is exhausted.

## Interleaved layout (xdelta3 extension)

Not in RFC 3284. xdelta3 extension. In interleaved mode, the three
data streams (data, instructions, addresses) are merged into a
single stream. The section lengths for ADD/RUN data and COPY
addresses are set to 0, and the instructions section length covers
the entire interleaved body. [Source: open-vcdiff documentation and
xdelta3 source code.]

Detection: if the ADD/RUN data length and COPY addresses length are
both 0 while the instructions length is nonzero, the window uses
interleaved layout.

In interleaved mode, instruction operands immediately follow each
instruction in the single stream:

- **ADD**: code byte, [varint size if table size==0], then `size`
  literal data bytes inline.
- **RUN**: code byte, [varint size if table size==0], then 1 data
  byte inline.
- **COPY**: code byte, [varint size if table size==0], then encoded
  address inline (varint or single byte depending on mode).

The decoding procedure is the same — only the source of bytes
changes (all from one stream instead of three).

**slap status**: sectioned layout is implemented. Interleaved layout
is not yet implemented. No files in the corpus use interleaved
layout. Detection is by content (A=0 and C=0), not by version byte.

## Secondary compression

When VCD_DECOMPRESS is set in the header, individual windows may
compress their data sections using the declared compressor. The
per-window Delta_Indicator bits (VCD_DATACOMP, VCD_INSTCOMP,
VCD_ADDRCOMP) indicate which of the three sections are compressed.
Compressed sections must be decompressed before the delta
instructions can be decoded.

Known compressor algorithms [source: xdelta3 source code]:

| ID | Name | Type |
|---:|------|------|
|  1 | DJW  | Static Huffman coding (David J. Wheeler) |
|  2 | LZMA | Lempel-Ziv-Markov chain algorithm |
| 16 | FGK  | Adaptive Huffman coding (Faller, Gallager, Knuth) |

Corpus observation (original research): DJW and LZMA are both
common. FGK was not observed. Most compressed patches compress all
three sections; some compress a subset. Some declare a compressor
in the header but compress zero windows — valid per the spec.

**slap status**: secondary compression is not yet implemented. slap
rejects any window that has compression bits set in its delta
indicator.

## Limitations

- **No file-level integrity.** There is no footer, no file CRC, no
  patch-over-self checksum. The only integrity mechanism is the
  optional per-window Adler32, which is an xdelta3 extension. A
  truncated file may parse partially without error.
- **No metadata.** The RFC defines no metadata fields. xdelta3's
  VCD_APPHEADER is opaque and application-defined; it has no
  standard schema.
- **No explicit window count.** Windows are parsed sequentially
  until EOF. There is no up-front declaration of how many windows
  the file contains.
- **No bidirectional patching.** Unlike UPS, VCDIFF patches are
  one-way. Undoing requires the inverse delta or the original file.

## Cross-reference to slap modules

| Concern | Module |
|:--------|:-------|
| Parse (patch bytes → VCDIFFPatch) | `Slap.VCDIFF.Parse` |
| Apply (VCDIFFPatch + source → target) | `Slap.VCDIFF.Apply` |
| Describe (VCDIFFPatch → ExplainData) | `Slap.VCDIFF.Describe` |
| Types (VCDIFFPatch, code table, caches) | `Slap.VCDIFF.Types` |
| Varint codec | `Slap.Get.vcdiffVarint` |

## Known implementation gaps

1. **Interleaved layout**: not implemented. No corpus files use it.
2. **Secondary compression (DJW, LZMA, FGK)**: not implemented.
   Blocks ~98% of xdelta3-produced patches.
3. **Adler32 validation**: parsed but not computed or enforced.
4. **Win_Indicator mutual exclusion**: VCD_SOURCE + VCD_TARGET both
   set is not rejected at parse time (bug — RFC says MUST NOT).
5. **Create mode**: not implemented.
6. **Silent error recovery**: out-of-bounds reads in Apply.hs return
   0 rather than propagating errors.
