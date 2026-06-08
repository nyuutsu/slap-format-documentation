# VCDIFF family — the core / RFC / xdelta3 partition

This is a **map, not a spec.** It records the boundary decision that the three spec docs are built on: for every element of the wire format, which of three coequal peers owns it —

- **Core** — executed *identically* by RFC-3284 VCDIFF and xdelta3. Owned by neither flavor. Defined as the **intersection**: an element is core only if both flavors do the same thing with it. → `core/spec.md`
- **RFC arc** — what RFC 3284 defines that xdelta3 refuses. → `rfc-vcdiff/spec.md`
- **xdelta3 arc** — what xdelta3 adds that RFC 3284 never defined. → `xdelta3/spec.md`

A consequence of "core = intersection": there are **no divergences *inside* core.** When the two flavors do a nominally-shared thing differently, that element is *ejected* from core into both arcs, each carrying its own policy. The ejected elements are listed last; they are the interesting ones.

Citations: RFC sections refer to `rfc3284.txt`; `xdelta3-*.h/.c` line refs are from the cloned `tools/xdelta` (decode path, the authority on what xdelta3 actually accepts). slap's current (pre-rewrite) behavior is noted where it bears on the rewrite.

---

## Core — identical in both, verified against source

| Element | Detail | Verified |
|---|---|---|
| Magic | `D6 C3 C4` (bytes 0–2) | RFC §4.1; `xdelta3.c:361-363` |
| Version byte *position* | byte 3 exists and is the version field | (policy is ejected — see below) |
| Varint | big-endian base-128, MSB = continue | RFC §2; `xdelta3` `read_size` |
| Window frame skeleton | Win_Indicator, then (if a copy-source bit) source-segment length+position, then delta-encoding length, then { target-window-size, Delta_Indicator, A, I, C, sections } | RFC §4.2–4.3; `xdelta3-decode.h` DEC_* states |
| Superstring model | COPY addresses index `U = S + T`; `addr < len(S)` → source segment, `addr ≥ len(S)` → already-written target | RFC §4.3; `xdelta3-decode.h:316-323` |
| Instruction set | NOOP=0, ADD=1, RUN=2, COPY=3; size 0 in table ⇒ size varint follows in inst stream | RFC §5.4; `xdelta3-decode.h` |
| Secondary-compression *framing* | Hdr_Indicator bit 0 (VCD_DECOMPRESS) ⇒ 1-byte compressor id; Delta_Indicator bits VCD_DATACOMP/INSTCOMP/ADDRCOMP (1/2/4) ⇒ that section is compressed | RFC §4.1, §4.3; parsed identically by `xdelta3-decode.h` (the *catalog* is xdelta3-arc) |
| Hdr/Win/Delta indicator *defined bits* | the RFC-defined bit meanings (Hdr 0/1, Win 0/1, Delta 0/1/2) | RFC §4.1–4.3 (which bits are *reserved/added* + reject policy is ejected) |
| Default code table | the fixed 256-entry RFC §5.6 table | RFC §5.6; xdelta3 uses `xd3_rfc3284_code_table()` `xdelta3-decode.h:936` |
| Address cache | near = circular `s_near` slots; same = `s_same*256` slots, slot `addr % (s_same*256)`; updated after **every** COPY; modes: 0 SELF (varint), 1 HERE (`here - varint`), 2..s_near+1 near (`near[m-2] + varint`), rest same (one byte) | RFC §5.1–5.3; `xdelta3.c:1259-1388` |
| Self-referential / overlapping COPY | `addr ≥ len(S)` into target-so-far, byte-by-byte forward, overlap legal (RLE-style) | RFC §4.3; shared |

### Core invariants (inherent to the shared semantics; both must enforce)

These are not features — they are the rules without which the shared core is meaningless. slap currently violates several (the silent `pure 0` apply path); the rewrite must enforce them as typed errors.

| Invariant | Source |
|---|---|
| A COPY may not reference target output not yet written (`addr < here`) | RFC §4.3; `xdelta3-decode.h:310` "address too large" |
| A COPY whose source is the segment may not run past the segment (`addr < cpylen ∧ addr+size > cpylen` ⇒ error) | `xdelta3-decode.h:316-323` "size too large" |
| A window's instructions must produce **exactly** the declared target-window size — no underfill, no overfill, no implicit source fill | `xdelta3-decode.h:771` `avail_out != dec_tgtlen` ⇒ error |

---

## RFC arc — RFC 3284 defines, xdelta3 refuses

| Element | RFC | xdelta3 behavior | slap now |
|---|---|---|---|
| `VCD_TARGET` window (Win_Indicator bit 1): copy-source segment drawn from earlier *target* file | RFC §4.2 (a first-class feature; described in prose — the §4.2 MUST is the both-bits prohibition, not VCD_TARGET support) | `XD3_UNIMPLEMENTED` "VCD_TARGET not implemented" (`xdelta3-decode.h:102`); no encoder | has an apply branch, untested |
| `VCD_CODETABLE` custom code tables (Hdr_Indicator bit 1): table encoded as a default-table-relative inner VCDIFF delta | RFC §7 | `XD3_UNIMPLEMENTED` "VCD_CODETABLE support was removed" (`xdelta3-decode.h:928`) | implements it (`deserializeCodeTable`/`decodeCustomTable`) — ahead of xdelta3 here |

In the wild: **zero** of the 187 patches we've looked at use either. These two are the conformance tail, not interop necessity.

---

## xdelta3 arc — xdelta3 adds, RFC never defined

| Element | Detail | RFC | slap now |
|---|---|---|---|
| Per-window Adler32 | signalled by **Win_Indicator bit 2 (`VCD_ADLER32`)** (`xdelta3.c:312`); 4 bytes big-endian after the A/I/C lengths, before the sections (`xdelta3-decode.h:1097-1114`) | absent (no checksum anywhere in RFC) | detects by gap-arithmetic, not the bit (bug); but enforces via `WindowCheck` |
| `VCD_APPHEADER` (Hdr_Indicator bit 2) | varint length + that many opaque app bytes; decoder skips | absent | skips correctly |
| Secondary-compressor *catalog* | the algorithms behind a declared compressor id: DJW=1, LZMA=2, FGK=16 (non-IANA, `xdelta3.c:324-326`), and their bitstream decoders. The *framing* is core; only the catalog is xdelta3's (RFC §6 leaves the registry undefined) | absent (RFC defines no compressor) | rejects compressed windows (unimplemented) |

In the wild: a compressor may be *declared* in the header yet used by no window (the SPOT_COLISEUM / Z2 "declared-but-unused" case) — still classifies as xdelta3, because declaring is itself xdelta3-exclusive.

## Not in any arc — out of scope

Two features are **neither RFC 3284's nor xdelta3's**, appear zero times in the patches we have, and slap does not target them:

- **Interleaved layout** — the three sections merged into one inline stream. The string "interleav" appears nowhere in the xdelta3 source, and xd3's decoder reads three separate sections (`xd3_decode_sections`, `xdelta3-decode.h:633`); the RFC decodes three separate arrays (§6).
- **The `0x53` ('S') version byte** — xdelta3 rejects any nonzero version (`xdelta3-decode.h:858`); the RFC reserves it.

They belong to some other VCDIFF lineage; whichever it is, it is out of scope.

---

## Ejected — shared structure, divergent policy (lives in BOTH arcs)

These are where "core = intersection" does its work: the *shape* is core, but the two flavors assign different meaning or policy, so the policy is recorded per-arc, never in core.

| Element | Core says | RFC arc policy | xdelta3 arc policy |
|---|---|---|---|
| Version byte (byte 3) | "this byte is the version" | "currently zero; reserved for future versioning" (RFC §4.1) | must be `0x00`; any nonzero ⇒ reject ("version > 0 not supported", `xdelta3-decode.h:858`) |
| Hdr_Indicator reserved bits | "a bitmask byte" | only bits 0–1 defined (DECOMPRESS, CODETABLE); 2–7 reserved | bits 0–2 valid (adds APPHEADER); 3–7 must be 0 (`VCD_INVHDR=~0x7`) |
| Win_Indicator reserved bits | "a bitmask byte; bits 0–1 select copy-source" | bits 0–1 only (SOURCE, TARGET); 2–7 reserved | bit 2 valid (ADLER32); 3–7 must be 0 (`VCD_INVWIN=~0x7`) |
| Delta_Indicator reserved bits | "a bitmask byte" | no bits defined (no secondary compression in RFC) | bits 0–2 valid (the COMP bits); 3–7 must be 0 (`VCD_INVDEL=~0x7`) |
| Win_Indicator both-bits (SOURCE+TARGET) | — | MUST NOT (RFC §4.2) → reject | `SRCORTGT` masks both-set to "neither" (looser); we reject |

---

## Open questions this partition surfaces (seed for the per-arc `questions.md`)

1. **Version byte policy** — do we adopt xdelta3's "reject nonzero" for the xdelta3 arc and "tolerate per RFC future-versioning" for the RFC arc? (slap currently accepts `0x53` as a phantom "xdelta3 marker" — wrong on both arcs; drop it.)
2. **`CoreOnly` labeling** — a patch using only core features is valid as either flavor and applies identically. Confirmed: 7 of 187 sample patches. It is its own parser verdict, not a tiebreak. How does `describe`/`info` name it to the user?
3. **Adler32 detection** — bit-authoritative (read `VCD_ADLER32`) vs the old gap-arithmetic. Bit wins; is the gap kept as a corruption cross-check?
4. **Reserved-bit strictness** — do we reject set-reserved-bits (xdelta3 does) or tolerate? Differs per arc because the valid-bit mask differs per arc.
5. **Both-source-bits** — reject (RFC MUST NOT, slap's instinct) vs xdelta3's silent-mask-to-neither.
6. **The conformance tail** — VCD_TARGET and custom code tables: we commit to implementing both fully (the stated goal), despite never appearing in a real patch. Logged so the cost is visible.
