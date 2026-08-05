# PPF family — cross-cutting notes

Each PPF version has its own wire format and design questions, in `PPF1/`, `PPF2/`, `PPF3/`, and `PPF4/`. This file keeps only what those per-version docs cannot: the four set side by side, a census of the patches we have, and the one endianness question that runs the length of PPF1's history.

## The four versions side by side

| | PPF1 | PPF2 | PPF3 | PPF4 |
|---|---|---|---|---|
| Magic | `PPF10` | `PPF20` | `PPF30` | `PPF40` |
| Encoding byte | `0x00` | `0x01` | `0x02` | `0xFF` |
| Own spec document | yes (`ppf.txt`) | yes (`PPF2.txt`) | yes (`PPF3.txt`) | none (only an inline source comment) |
| Creator source we have | yes (pdx-ppf1) | no — only the DOS binaries, which unpack with `upx -d` | yes (ppf-master) | yes (gs2-bugfixes-master) |
| Header size | 56 | 1084 | 60 or 1084 | 60 |
| Offset width on disk | 32-bit | 32-bit | 64-bit | 32-bit |
| Reach | ≤ 2 GB: `ppf-doc.txt` says so, and `applyppf.c` reads the offset into a C `long` for `fseek` | ≤ 2 GB: undeclared in `PPF2.txt`, but the DOS tools are Turbo Pascal 6.0, whose only 32-bit integer is the signed `Longint` | 2^63 − 1, per `PPF3.txt` | ≤ 4 GB−1: undeclared, but `ppfmaker.cpp` declares the offset `u32` and `patcher.lua` reads it back with `readU32` |
| Endianness | undeclared in PPF1's own material; the tools are endian-native (see below) | little-endian, declared — but a big-endian patch exists in the wild (see below) | little-endian, declared | little-endian |
| RLE record mode | yes (in the doc) | doc silent | no | no |
| File-size field | — | u32 at offset 56 | — | — |
| Validation block | — | always | optional | — |
| Undo data | — | — | per-record, optional | — |
| FILE_ID.DIZ length field | — | u32 (4 bytes) | u16 (2 bytes) | — |
| Tail growth (ADD) | — | — | — | yes, append-only |

## Size across the versions

No version's own document requires a patch to keep the file the same length. The property comes from tool policy and the disc-image setting, not from the format — except in PPF4, where growth is the point.

| | PPF1 | PPF2 | PPF3 | PPF4 |
|---|---|---|---|---|
| Spec requires same size? | no | no | no | n/a |
| Size metadata in the patch | none | u32 at offset 56, "Used for Identification" | none — PPF2's field was dropped as "too inaccurate" | none |
| Maker-side enforcement | pdx-ppf1 refuses inputs of different sizes | unknown (no source); the user docs ask for same-size | a size check is present in the source but compares a value against itself, so it always passes | none — the maker grows files by design |
| Apply-side check | none | advisory: prompts y/n on a size mismatch | none | REPLACE is bounded to the original's length; ADD appends |
| Format-level shrink | not expressible | not expressible | not expressible | not expressible (no truncate) |

A PPF2 patch carries a size field, but it identifies the input — it is not a statement about the output.

## What the patches we have turned up

A walk over the PPF patches we have, looking for the shapes the formats permit but the documents never promise. What surfaced:

- **Records do overlap.** Three PPF2 patches labelled "Cracked By Bad/Pdx" — Paradox's own, the format's authors — carry runs of consecutive records that overlap by a byte: a long stretch of changes split into 255-byte records at a 254-byte stride, so each record rewrites the last byte of the one before. Overlap is rare, but it is not hypothetical, and it came out of the authors' own tool. Where records overlap the later one wins, so their order carries meaning — slap must never sort them, for apply or for display.
- **Offsets run backward in some patches.** A handful of PPF2 and PPF3 patches — the Chrono Cross translation among them — list records whose offsets are not in ascending order. Not malformed; nothing requires ascending order. Another reason order is load-bearing, and it happens in both formats, not only PPF3.
- **A big-endian PPF2 patch exists.** `Asuka120P_LO.ppf` — see the endianness note below. This is the surprise that mattered most.
- **Zero-count records: none seen.** For PPF2/3/4 a count of zero would mean "write nothing here"; for PPF1 a count of zero is instead the RLE marker, so the format has no way to say "write nothing" at all.
- **One empty PPF4 patch**, `gs1names.ppf`: a valid 60-byte header and no records — a patch that changes nothing.

## Endianness and the Amiga question

PPF1's own documents never mention byte order, and its original tools do not impose one: `makeppf.c` and `applyppf.c` read and write the offset straight into a C `long` with no swap, so a patch carries whatever order the machine that built it used. The archive ships a PC build and an Amiga (m68k) build from that same source, so little-endian and big-endian patches can both exist, and nothing in a patch says which it is.

Little-endian on disk is nonetheless the settled reading, and PPF2 is where it is said outright: `PPF2.txt` declares the offset little-endian and tells anyone building an Amiga patcher to swap it. That declaration predates PPF3 and settles the convention for the whole family. PPF1's own worked examples already show the little-endian form (`D0 F9 15 00` for `0x0015F9D0`) — a demonstration rather than a rule, but it points the same way.

The 339 PPF1 patches we have are all PC-origin, so all little-endian on disk. They appear to come from a single scene-mirror archive, though, so read that as "all 339 we looked at" rather than a claim about every PPF1 there is. The Amiga tool shipped in Icarus's own distribution and is still findable on period PSX-scene mirrors, so big-endian Amiga-origin patches plausibly exist somewhere even without a specimen in hand.

**slap defaults to little-endian, carries a separate dialect for the big-endian Amiga case, and warns when that override is used.** A big-endian patch is not malformed — it came from an Amiga, and the dialect is how you say so.

PPF2 was thought settled: `PPF2.txt` declares the offset little-endian and tells anyone building an Amiga patcher to swap it, so the question that hangs over PPF1 looked answered here. It is not. `Asuka120P_LO.ppf`, a PPF2 patch in the set, reads as nonsense little-endian — its offsets climb past 2 GB and run backward — and reads cleanly big-endian: all 2,785 of its offsets ascend and sit inside 62 MB. Someone built it on the far side of PPF2.txt's own warning. So the big-endian case is not a PPF1 curiosity; a real PPF2 patch is in the other order too.

slap reads PPF2 little-endian by default and carries the same big-endian dialect as PPF1: `--is-amiga-patch` reads a PPF2 patch's integers — its source-size field, its offsets, and any FILE_ID.DIZ length — big-endian. On `Asuka120P_LO.ppf` that turns `slap info`'s garbage (a negative offset range, a 1.6 GB source size) into the real picture: a 538 MB source and offsets climbing from `0x09dec1`. Without the flag, slap reads it little-endian and shows the garbage — the wire carries no marker, so the reader has to be told, and the flag is how you tell it.
