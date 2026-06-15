# xdelta3 — secondary compression

The RFC defines only the signaling: Hdr_Indicator bit 0 declares a compressor id, and each window's Delta_Indicator bits mark which of its three sections are compressed (§4.1, §4.3). Everything below is xdelta3's, recovered from its source and from real patches.

## Per-section framing

A compressed section is `[decompressed-size varint][compressor-native stream]`. The decoder reads the size, decompresses, and must consume the whole stream *and* emit exactly that many bytes; either shortfall is malformed (`xdelta3-second.h`, `xd3_decode_secondary`). A section flagged compressed but too short to hold a size varint, or one declaring a size of zero, is rejected — compressing nothing yields framing bytes, never zero.

## Catalog

| id | name | algorithm |
|---:|------|-----------|
| 1  | DJW  | xdelta3's own static multi-table Huffman (David J. Wheeler); no external library |
| 2  | LZMA | xz/LZMA2 via liblzma (`lzma_stream_encoder`, check=none) — *not* raw LZMA1 |
| 16 | FGK  | adaptive Huffman (Faller, Gallager, Knuth); marked "demonstration purposes only" in its own header, and never seen in the wild |

The ids are not IANA-registered (`xdelta3.c`); an unknown one is rejected ("unknown secondary compressor ID"). A patch may declare a compressor yet have no window use it (all Delta_Indicator bits clear) — still valid.

## Stream shapes

What the compressor-native stream *is* differs per compressor.

**LZMA** is one continuous stream per section kind, spread across the windows: the xz header appears only in the kind's first compressed section, later sections are bare continuation slices, and the stream is never finished — the encoder drives liblzma with `LZMA_SYNC_FLUSH` and never `LZMA_FINISH`, so no end-of-stream marker, index, or footer is ever written (`xdelta3-lzma.h`). The three kinds — data, instructions, addresses — are independent streams.

**DJW** is fresh per section: `xd3_decode_secondary` holds every section to its own consume-all and exact-output checks, the stream state is `struct _djw_stream { int unused; }`, and `xd3_decode_huff` starts a fresh bit reader on every call. Each section carries its own table headers and bit stream (`xdelta3-djw.h` — the source is the format's only definition). The wire grammar is closed: a 3-bit group count caps the Huffman tables at eight by field width, sector sizes run 5–160 in fives, and every code-length field bounds itself.

**FGK** sends no tables ahead of time. The decoder and encoder grow the same Huffman tree as they go, one byte at a time, and that growing tree is all the state there is. All of a patch's data sections share a single tree: it's built once, when the first of them is read, and carried on to the next and the next — each section picking up from the tree the ones before it grew, never starting empty (`xdelta3-second.h`, `xdelta3-fgk.h`). The instruction sections and the address sections each carry their own tree the same way. The bytes of any one section still stand on their own, padded out to a whole byte; only the tree carries across.
