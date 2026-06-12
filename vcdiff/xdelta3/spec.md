# xdelta3 — the xdelta3 arc

xdelta3 (Joshua MacDonald) is the VCDIFF dialect that essentially all `.xdelta` patches in circulation use. As a slap format it is **core plus this arc**: everything in `../core/spec.md`, plus the features xdelta3 adds that RFC 3284 never defined, minus the two RFC features xdelta3 refuses, plus xdelta3's policy on the elements core left to the flavors.

xdelta3 is **not** a superset of RFC-3284 VCDIFF. It adds wire features the RFC lacks *and* rejects two features the RFC mandates. The two formats overlap in core; neither contains the other.

This document does not restate core. Citations are to the cloned `tools/xdelta` decode path, the authority on what xdelta3 accepts.

## Version byte

xdelta3 requires byte 3 to be `0x00`. Any nonzero value is rejected: the decoder bails with "VCDIFF input version > 0 is not supported" (`xdelta3-decode.h:858`), and `VCDIFF_VERSION` is hardcoded `0x00` on the encode side (`xdelta3.c:364`). There is no `0x53`/ASCII-`S` marker in xdelta3 — that value is not xdelta3's and is neither produced nor accepted here.

## Refusals — RFC features xdelta3 rejects

Core parses the grammar of both of these; xdelta3 parses then refuses.

- **VCD_TARGET windows** (Win_Indicator bit 1). xdelta3 has no encoder for them and its decoder returns `XD3_UNIMPLEMENTED`, "VCD_TARGET not implemented" (`xdelta3-decode.h:102`). (Their handling is the RFC arc; see `../rfc-vcdiff/spec.md`.) Note this is distinct from within-window self-referential COPY, which xdelta3 fully supports — that is core.
- **Custom code tables** (Hdr_Indicator bit 1). xdelta3 once supported them and removed it; the decoder reads the table fields then returns `XD3_UNIMPLEMENTED`, "VCD_CODETABLE support was removed" (`xdelta3-decode.h:928`). xdelta3 always decodes against the default table (core).

## Reserved bits

xdelta3 claims one more bit than the RFC in two indicators, and enforces that the rest are zero:

- **Hdr_Indicator**: bits 0–2 valid (bit 2 = VCD_APPHEADER, below); bits 3–7 must be zero, else "unrecognized header indicator bits set" (`VCD_INVHDR = ~0x7`, `xdelta3.c:307`).
- **Win_Indicator**: bits 0–2 valid (bit 2 = VCD_ADLER32, below); bits 3–7 must be zero (`VCD_INVWIN = ~0x7`).
- **Delta_Indicator**: bits 0–2 valid (the section-compression bits, core); bits 3–7 must be zero (`VCD_INVDEL = ~0x7`).

**Both source bits set.** Where the RFC says reject, xdelta3 is looser: its `SRCORTGT` macro masks a both-set Win_Indicator down to "neither" (no source segment) rather than erroring. slap's position is in `questions.md`.

## Additions — wire features not in RFC 3284

### Application header (VCD_APPHEADER, Hdr_Indicator bit 2)

If set, a varint length and that many bytes of application-defined data follow in the header (after the secondary-compressor id and code-table fields). The data is opaque and has no standard schema; a decoder skips it. xdelta3 uses it for its own metadata. The majority of xdelta3-produced patches set this bit.

### Per-window Adler32 (VCD_ADLER32, Win_Indicator bit 2)

If set, a 4-byte **big-endian Adler32** of the window's decoded target output is present, positioned immediately after the three section-length varints (A, I, C) and before the data section (`xdelta3-decode.h`, state `DEC_CKSUM`).

**Presence is signalled by the bit, full stop.** Read Win_Indicator bit 2; if set, the four bytes are there, otherwise they are not. (A gap-arithmetic fallback — measuring the unaccounted bytes between the length fields and the sections, which is 4 when present and 0 when not — is a sound *corruption cross-check*, but it is not how presence is decided.) On a present checksum, compute the Adler32 of the decoded window and compare; a mismatch means the output is wrong. The checksum is near-universal in xdelta3 output but not guaranteed (a window may omit it).

### Secondary compression

The *signaling* is core — Hdr_Indicator bit 0 declares a compressor id, and each window's Delta_Indicator bits say which of its three sections are compressed. xdelta3 supplies the rest, all of which the RFC leaves undefined: the per-section framing, the catalog of compressor ids, and each compressor's stream shape. That layer has its own page, `secondary-compression.md`.

Because the RFC registered no compressors, **a declared compressor id is in practice an xdelta3 signal**: naming a compressor means using xdelta3's catalog, so the patch is xdelta3 even if no window exercises it.

### Interleaved layout is *not* an xdelta3 feature

Interleaved layout — the three sections merged into one stream with each instruction's operands inline — was mis-attributed to xdelta3 in an earlier draft. It is not xdelta3's: the string "interleav" appears **nowhere** in the xdelta3 source, and the decoder reads three separate, contiguously-sized sections (`xd3_decode_sections`, `xdelta3-decode.h:633`, `698-705`) with no inline-operand path — an `A=0, C=0` window would starve those reads. It is also not in RFC 3284 (§6 decodes three separate arrays). Interleaved is neither RFC's nor xdelta3's, appears zero times in the patches we have, and is out of scope.

## Classification

A patch belongs to the xdelta3 arc if it uses any xdelta3-arc feature: VCD_APPHEADER, VCD_ADLER32, or a declared secondary compressor. A patch using none of these (and none of the RFC-arc features) is `CoreOnly`. The feature sets are disjoint in the patches we have (187/187 bucket cleanly), so this classification is sound; how a `CoreOnly` patch is named to the user is in `questions.md`.

## What xdelta3 doesn't pin down

Carried in `questions.md`:

- the response to the forbidden both-source-bits window (xdelta3 masks; do we mimic or reject?);
- Adler32 enforcement policy (fatal vs advisory) and what to do on a 4-byte gap with the bit clear (or vice versa);
- whether to mirror xdelta3's reserved-bit rejection or tolerate;
- the opaque VCD_APPHEADER contents (xdelta3's internal metadata format is undocumented here);
- abort semantics, trailing bytes after the last window, empty patches — shared with the RFC arc.
