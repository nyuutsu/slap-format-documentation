# RFC-3284 VCDIFF — the RFC arc

RFC-3284 VCDIFF is the published IETF standard (D. Korn, J. MacDonald, J. Mogul, K. Vo; June 2002). As a slap format it is **core plus this arc**: everything in `../core/spec.md`, plus the features the RFC defines that xdelta3 refuses, plus the RFC's policy on the elements core left to the flavors.

This document does **not** restate core. It covers only what is RFC-arc. Where it and `rfc3284.txt` disagree, the RFC wins; report it.

**No patch we've seen uses the two features below** (VCD_TARGET windows, custom code tables). They are the conformance tail: implemented for completeness, not because the community emits them. A patch that uses neither — and none of the xdelta3-arc features either — is a `CoreOnly` patch and is equally valid as xdelta3.

## Version byte

The RFC says byte 3 "is currently set to zero" and "in the future, it might be used to indicate the version of Vcdiff." So under the RFC arc, `0x00` is the only defined value; a nonzero value would denote a future version whose format the RFC does not describe. The RFC prescribes no behavior for that case. slap's position on a nonzero version byte is in `questions.md`.

## Indicator bits

Core defines the meanings of the RFC bits (Hdr 0–1, Win 0–1, Delta 0–2). The RFC arc adds the *reserved* picture:

- Only those bits are defined. The RFC describes the Hdr_Indicator as taking nonzero values for "either, both, or neither of the two bits VCD_DECOMPRESS and VCD_CODETABLE" — i.e. bits 2–7 are reserved. Win reserves 2–7; Delta reserves 3–7.
- The RFC does not state that a set reserved bit must be rejected. slap's position is in `questions.md`.

**Both source bits set.** The RFC is explicit: the Win_Indicator "MUST NOT have more than one of the bits set." A window with both VCD_SOURCE and VCD_TARGET is malformed. The response (reject) is the RFC-arc position; see `questions.md`.

## VCD_TARGET windows

Core fixes the grammar: Win_Indicator bit 1 set means the source-segment length and position name a region of the *target* file rather than the source file. The RFC arc fixes the handling.

When VCD_TARGET is set, the window's source segment `S` is the `source-segment-length` bytes of already-produced **target output** beginning at `source-segment-position` (an offset into the target the patch has built so far). COPY addresses then resolve against `U = S + T` exactly as in core — `addr < len(S)` reads from this target-drawn segment, `addr ≥ len(S)` from the current window's output. This lets a window reuse material from an earlier part of the target that lies outside its own window, which a within-window self-referential COPY cannot reach.

The source-segment-position for VCD_TARGET must name bytes already produced (it cannot point into target output this window or a later window has not yet written); the core invariant "no reading unwritten target" applies to the segment as a whole.

xdelta3 refuses VCD_TARGET outright (it has no encoder for it and its decoder returns an error). slap implements it; that divergence is noted in `../xdelta3/spec.md`.

## Custom code tables (VCD_CODETABLE)

Core fixes the grammar: Hdr_Indicator bit 1 set means a code-table length (varint) and that many bytes follow in the header. The RFC arc (§7) fixes what those bytes are and how the table is built.

The code-table data is:

1. one byte `s_near` (the near-cache size for this table),
2. one byte `s_same` (the same-cache size),
3. a VCDIFF delta encoding.

The delta in (3) is applied — using the **default** code table and the default cache sizes — to the 1536-byte serialized form of the default code table (the six 256-byte arrays: type1, type2, size1, size2, mode1, mode2). The result is the serialized custom table, which is deserialized into the 256 entries this patch's windows decode against, with `s_near`/`s_same` as the cache sizes.

**Nested custom tables are forbidden.** The inner delta in (3) must itself use the default code table — it may not declare its own VCD_CODETABLE. A decoder applies the inner delta with the default table unconditionally.

A custom table replaces the default table (core) and the default cache sizes for the whole patch. xdelta3 removed support for custom code tables and rejects them; see `../xdelta3/spec.md`.

## What the RFC doesn't specify

The RFC is silent or informal on several practical points; these are carried in `questions.md`:

- behavior on a nonzero version byte (a "future version");
- whether a set reserved indicator bit is rejected or tolerated;
- the response to the forbidden both-source-bits window;
- what "the source file" means when its length differs from the bytes a window addresses (over-long / short source);
- abort semantics — what a decoder does on a malformed patch beyond "fail";
- trailing bytes after the last window (there is no window count and no footer to bound the file);
- an empty patch (header, zero windows): well-formed or not.
