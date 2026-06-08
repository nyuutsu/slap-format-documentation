# VCDIFF core — the shared wire format

This document describes the part of the VCDIFF wire format that RFC-3284 VCDIFF and xdelta3 execute **identically**. It is the intersection of the two flavors — the substrate both build on — and it is owned by neither.

Core is deliberately defined as the intersection. Where the two flavors do a nominally-shared thing differently (the version byte's meaning, the set of legal indicator bits), that element is **not** described here; it is *ejected* to the two flavor specs, each carrying its own policy. This document points to those ejections but does not resolve them. See `../rfc-vcdiff/spec.md` (the RFC arc), `../xdelta3/spec.md` (the xdelta3 arc), and `../partition.md` (the map that fixes the boundary).

Core is not itself a complete format. You do not deliberately author a "core-only" patch. But a patch that happens to use only the features described here is valid as either flavor and decodes identically under both — slap calls that the `CoreOnly` parse verdict, and it is the honest answer when a patch carries no flavor-distinguishing feature.

Where this document and RFC 3284 disagree, RFC 3284 wins; report the discrepancy. Every multi-byte integer described as a "varint" uses the encoding in the Numbers section. All other multi-byte quantities are big-endian.

## Glossary

- **source** — the original (input) file the patch is applied against.
- **target** — the modified (output) file the patch produces.
- **window** — one self-contained chunk of the target. A patch is a sequence of windows, each producing a run of target bytes; the target is their concatenation in order.
- **varint** — a variable-length unsigned integer (see Numbers).
- **source segment** — for a window that copies from external data, the contiguous region of the source (or of earlier target output) that this window's COPY instructions address against.
- **superstring** — the concatenation `U = S + T` of a window's source segment `S` and the target bytes `T` it has produced so far. COPY addresses index into `U`.
- **instruction** — one of ADD, RUN, COPY (or the no-op NOOP), read from the instruction stream through the code table.
- **code table** — a 256-entry table mapping each instruction-stream byte to one or two instructions. Core uses the default table below.
- **address cache** — two small caches (near, same) that let COPY addresses be encoded compactly relative to recently-used addresses.

## Identity

- **Magic**: the three bytes `D6 C3 C4` (ASCII `V`, `C`, `D` with the high bit set), at the very start of the patch.
- **Version byte**: byte 3. Core fixes only that this byte *exists* and is the version field. Its accepted values and the policy on a nonzero value are **flavor-specific** (ejected — see each arc). Every patch in practice carries `0x00`.

## Numbers

VCDIFF encodes every integer field — header sizes, window lengths, instruction sizes, COPY addresses — as an unsigned **varint**: base-128, **big-endian**, with the high bit of each byte as a "more bytes follow" flag.

```
Each byte:
  ┌───┬───────────────────────┐
  │ m │      data (7 bits)    │
  └───┴───────────────────────┘
    │
    more-bytes flag
    1 = another byte follows
    0 = last byte of this number
```

The seven payload bits are assembled **most-significant group first**: each subsequent byte's payload is shifted into the low seven bits and the running value's existing bits move up by seven. A decoder reads bytes until it sees one with the high bit clear.

This is **not** the byuu varint used by UPS/BPS (which is little-endian, with a subtract-one canonicality trick). The two are incompatible and encode the same integer differently. RFC 3284's worked example: `123456789` encodes as `BA EF 9A 15`.

## Overall structure

```
Header
    Magic                    3 bytes: D6 C3 C4
    Version                  1 byte
    Hdr_Indicator            1 byte (bitmask)
    [Secondary compressor id] 1 byte   (iff Hdr_Indicator bit 0)
    [Code table length]      varint    (iff Hdr_Indicator bit 1)
    [Code table data]        N bytes   (iff Hdr_Indicator bit 1)
    [ ...any flavor-added header field (e.g. application data)... ]
Window 0
Window 1
...
Window N-1
```

There is no footer, no file-level checksum, and no up-front window count. Windows are decoded sequentially until the input is exhausted.

**Hdr_Indicator** is a bitmask byte introducing optional header initialization data, in bit order. Core defines the two RFC bits, which both flavors parse identically:

- **bit 0 (VCD_DECOMPRESS)** — a secondary compressor is in use; a one-byte compressor id follows here.
- **bit 1 (VCD_CODETABLE)** — a custom code table is supplied; its length (varint) and that many bytes follow here.

What a decoder then *does* with these is flavor-specific: the compressor behind the id (RFC leaves the registry undefined; xdelta3 supplies one — xdelta3-arc) and the custom-table decode (RFC §7 — RFC-arc; xdelta3 parses then rejects). Bit 2 (application data) is an xdelta3 addition not in RFC. Reserved-bit policy is ejected. See the arcs.

## Window structure

```
Win_Indicator                  1 byte (bitmask)
[Source segment length]        varint   ┐ present iff a copy-source
[Source segment position]      varint   ┘ bit is set in Win_Indicator
Length of the delta encoding   varint
  Target window size           varint
  Delta_Indicator              1 byte (bitmask)
  Length of data section (A)   varint
  Length of inst section (I)   varint
  Length of addr section (C)   varint
  [ ...any per-window init, gated on indicator bits... ]
  Data section                 A bytes
  Instructions section         I bytes
  Addresses section            C bytes
```

**Win_Indicator** is a bitmask. Core fixes the meaning of its two low bits, which select the COPY source:

- **bit 0 (VCD_SOURCE)** — COPY instructions may address a segment of the *source* file.
- **bit 1 (VCD_TARGET)** — COPY instructions may address a segment of the already-produced *target* file.

At most one of these two bits may be set; setting both is forbidden (RFC §4.2, MUST NOT). If neither is set, the window is self-contained (no external source segment) and the two source-segment varints are absent. If one is set, the source-segment length and position varints follow, naming the region of source (or earlier target) the window addresses against.

(VCD_TARGET windows are an RFC-arc feature in practice — xdelta3 refuses them. Higher Win_Indicator bits, and the policy on reserved bits, are flavor-specific; see each arc.)

**Length of the delta encoding** is the byte count of everything that follows it in this window — target-window-size varint, Delta_Indicator, the three section-length varints, any per-window init data, and the three sections. It lets a decoder find the window boundary up front.

**Target window size** is the exact number of bytes this window produces. The decoder allocates this much and must fill it exactly.

**Delta_Indicator** is a bitmask governing per-section secondary compression. Core defines its three RFC bits — VCD_DATACOMP (bit 0), VCD_INSTCOMP (bit 1), VCD_ADDRCOMP (bit 2) — each meaning "the corresponding section (data / inst / addr) was secondary-compressed and must be decompressed before decoding." Both flavors parse these identically. The compressor that does the decompressing is *not* in core: RFC defines no compressor; xdelta3 supplies the catalog (xdelta3-arc). Reserved-bit policy is ejected.

**The three sections** carry, in order: the literal bytes consumed by ADD and RUN instructions (data, length A); the instruction-stream bytes and any out-of-line sizes (inst, length I); and the encoded COPY addresses (addr, length C).

## The superstring and addressing

A window builds its target `T` by executing instructions. COPY instructions address into the **superstring** `U = S + T`, where `S` is the window's source segment (length `len(S)`, possibly zero) and `T` is the portion of the target this window has produced so far.

- An address `a < len(S)` reads from the source segment at offset `a`.
- An address `a ≥ len(S)` reads from already-produced target output at offset `a − len(S)`.

A COPY may overlap its own destination (a self-referential copy whose read position trails the write position). This is executed byte by byte, forward, so each freshly written byte becomes available to the same copy — the standard run-length/periodic expansion. This is legal and intended.

## Instructions

Four instruction types, identified by a numeric code:

| Code | Type | Effect |
|-----:|------|--------|
| 0 | NOOP | nothing (the empty half of a single-instruction code-table entry) |
| 1 | ADD  | consume `size` bytes from the data section, append to target |
| 2 | RUN  | consume one byte from the data section, append it `size` times |
| 3 | COPY | append `size` bytes read from the superstring `U` at a decoded address |

Instructions are not encoded directly. Each byte of the instruction section indexes the code table; the looked-up entry supplies one or two instructions, each with a type, a size, and (for COPY) an address mode. If an instruction's size in the table is zero, the actual size is read as a varint from the instruction section, immediately after the code byte. COPY addresses are decoded from the address section using the mode (see Address cache). RUN always consumes exactly one data byte regardless of size.

## The default code table

Core uses the fixed 256-entry default code table defined by RFC §5.6. It is not stored in the patch. Each entry holds a pair `(type1, size1, mode1, type2, size2, mode2)`; a `NOOP` second triple means the entry encodes a single instruction. A zero size means the size is coded separately as a varint; a zero mode applies to non-COPY instructions.

| Index range | Entry |
|------------|-------|
| 0          | RUN, size coded separately |
| 1–18       | ADD, size 0 (coded separately) then sizes 1–17 |
| 19–162     | COPY, for each mode 0–8: size 0 then sizes 4–18 |
| 163–234    | ADD(1–4) + COPY(4–6), modes 0–5 |
| 235–246    | ADD(1–4) + COPY(4), modes 6–8 |
| 247–255    | COPY(4) + ADD(1), modes 0–8 |

In the combined-instruction rows the ADD size is the outer loop and the COPY size the inner: index 163 is ADD(1)+COPY(4), 164 is ADD(1)+COPY(5), 165 is ADD(1)+COPY(6), 166 is ADD(2)+COPY(4), and so on.

(A flavor may replace the default table for a patch via a custom code table; that is an RFC-arc feature. Absent it, this table is in force.)

## Address cache

COPY addresses are encoded relative to recently-used addresses, through two caches the decoder maintains and updates in lockstep with the encoder. Both are reset to zero at the **start of each window**.

- **Near cache** — a circular buffer of `s_near` slots (default 4). After each COPY's address is decoded, that address is written to the next slot, advancing round-robin.
- **Same cache** — `s_same × 256` slots (default `s_same = 3`, so 768). After each COPY, the address is written to slot `address mod (s_same × 256)`.

Both caches update after **every** COPY, keeping decoder and encoder in step. (`s_near` and `s_same` take their defaults from the default code table; a custom code table may change them — RFC-arc.)

The address mode (from the code-table entry) selects the decoding:

| Mode | Name | Decoding |
|-----:|------|----------|
| 0 | SELF | `address = read varint` |
| 1 | HERE | `address = here − (read varint)` |
| 2 … s_near+1 | near | `address = near[mode−2] + (read varint)` |
| s_near+2 … s_near+s_same+1 | same | `address = same[(mode−(s_near+2))×256 + (read one byte)]` |

`here` is the current position in `U` (i.e. `len(S)` plus the target bytes produced so far in this window). With the defaults there are nine modes (0–8). Same-mode addresses are encoded as a single byte, not a varint, because the same cache guarantees the result is in range.

## Decoding a window

After locating the three sections (and decompressing any that a flavor marked compressed — xdelta3-arc), the decoder repeatedly:

1. reads one byte from the instruction section as a code-table index;
2. for each of the (up to two) instructions in that entry, reads an out-of-line size varint if the table size was zero, then executes: ADD/RUN consume from the data section, COPY decodes an address (and updates both caches) and reads from `U`;

until the instruction section is exhausted. The produced bytes are the window's target output.

## Core invariants

These are not optional features; they are the rules the shared semantics require. Both flavors enforce them, and a decoder that does not is wrong. A patch that violates any of them is malformed and the decoder must fail (loudly — not by substituting zero bytes or inventing fill).

1. **No reading unwritten target.** A COPY address must point strictly before the current write position: `address < here`. You cannot copy target output that does not exist yet. (RFC §4.3; xdelta3 rejects with "address too large".)
2. **Copies stay within their segment.** A COPY that begins inside the source segment must not run past the segment's end: `address < len(S)` and `address + size > len(S)` is malformed. (xdelta3 rejects with "size too large".)
3. **Windows fill exactly.** A window's instructions must produce exactly the declared target-window size — no more, no fewer. There is **no** implicit fill of leftover bytes from the source; a short instruction stream is malformed, not completed silently. (xdelta3 rejects when produced length ≠ declared length.)

## What core does not decide

Core fixes shape and shared semantics only. The following are flavor-specific and resolved in the arc specs, not here:

- the accepted **version-byte** values and the policy on a nonzero value;
- **reserved-bit policy** for the three indicator bytes — which bits beyond the RFC ones are claimed (xdelta3 adds Hdr bit 2 and Win bit 2), and whether a set reserved bit is rejected;
- the **handling** of features whose grammar core parses but whose behavior diverges: decoding a **custom code table** and applying a **VCD_TARGET** window (RFC honors both; xdelta3 rejects both);
- the **secondary compressor catalog** — the algorithm behind a declared compressor id (RFC defines none; xdelta3 supplies DJW/LZMA/FGK);
- the wire features xdelta3 adds outright: **application header** data and a per-window **Adler32** checksum;
- how a `CoreOnly` patch (one using none of the divergent features) is named to the user by `info` / `explain`.

These, and the genuinely-unspecified corners (abort semantics, trailing bytes after the last window, an empty patch with zero windows), are carried in each arc's `questions.md`.
