# NINJA2 — wire format

A NINJA2 patch rebuilds a target file from a source by XORing masks over the source's bytes. It opens with a 2048-byte preamble of free-text metadata — author, title, description, and more — then a stream of commands: one that names a file with its before-and-after sizes and MD5s, XOR records carrying the changed bytes, and an overflow block holding the bytes by which the file grew or shrank. Because XOR is self-inverse and the patch stores both the source and the target MD5, one patch can drive the change in either direction. A patch may carry several files, and it tags each with a ROM type that selects a per-platform normalization. The file extension is `.RUP`, shared with NINJA1; the version byte tells the two apart.

This writeup is drawn from the format's spec sheet (`upstream/ninja2-filespec20.txt`), the reference program `ninja2.php` (in the 2006 Windows build under `upstream/`), and the wider reader ecosystem (librup, RomPatcher.js). Where they disagree, the running code is what the patches in the wild were made by, so it is what this document follows — and each disagreement becomes an entry in `questions.md`. Points the spec sheet leaves open are deferred there too.

## Identity

Every NINJA2 patch opens with the six ASCII bytes `NINJA2` — the string `NINJA` followed by the version byte `2` (`0x32`). NINJA1 carries `1` in that position; the two versions share the `.RUP` extension but nothing of their structure.

The magic begins a fixed 2048-byte (`0x800`) preamble — the six magic bytes, then the encoding byte and info fields below. The command stream begins at offset `0x800` and runs to the end of the file.

## Preamble

```
┌────────┬───────────┬──────────────────────┐
│ NINJA2 │ PATCH_ENC │   INFO fields ...    │
│  (6)   │    (1)    │        (2041)        │
└────────┴───────────┴──────────────────────┘
0x000    0x006       0x007              0x800
```

`PATCH_ENC` at `0x006` is a one-byte hint for how the INFO text is encoded: `0` for the system codepage, `1` for UTF-8. The fields themselves are stored as raw bytes; the byte only tells a reader how to interpret them.

The INFO region holds eight fixed-width fields, each right-padded with `0x00` to its full width:

| Offset | Width | Field |
|-------:|------:|-------|
| `0x007` | 84 | author |
| `0x05B` | 11 | patch version |
| `0x066` | 256 | title |
| `0x166` | 48 | genre |
| `0x196` | 48 | language |
| `0x1C6` | 8 | date (`YYYYMMDD`) |
| `0x1CE` | 512 | website |
| `0x3CE` | 1074 | description |

The description ends exactly at `0x800`. A patch with no metadata fills all 2042 bytes after the magic with `0x00`.

## Command stream

From `0x800`, the patch is a sequence of commands, each introduced by a one-byte code, ending at a `0x00`:

| Code | Command |
|------|---------|
| `0x01` | open a new file |
| `0x02` | XOR record |
| `0x4D` (`M`) | overflow — source longer than target |
| `0x41` (`A`) | overflow — target longer than source |
| `0x00` | end |

Sizes and offsets inside the stream are **variable-length values** (VLV): a one-byte count, then that many little-endian bytes. A count of `0` denotes the value zero with no bytes following.

```
┌───────┬──────────────────┐
│ count │  value bytes ... │   little-endian; count 0 = value 0
│  (1)  │     (count)      │
└───────┴──────────────────┘
```

### `0x01` — open a new file

```
┌──────┬───────┬───────┬──────┬───────┬───────┬───────┬───────┬──────┬──────┐
│ 0x01 │ N_MUL │ N_LEN │ NAME │ TYPE  │ SSIZE │ MSIZE │ SMD5  │ MMD5 │ ...  │
│ (1)  │  (1)  │(N_MUL)│(N_LEN)│ (1)  │ (VLV) │ (VLV) │ (16)  │ (16) │      │
└──────┴───────┴───────┴──────┴───────┴───────┴───────┴───────┴──────┴──────┘
```

`N_MUL` is the width of the filename-length value. When it is `0`, the patch is single-file: no `N_LEN` and no `NAME` follow. Otherwise `N_LEN` is a VLV-body of `N_MUL` bytes giving the name length, and `NAME` is that many bytes.

`TYPE` is the one-byte ROM type (see below). `SSIZE` and `MSIZE` are the source and target file sizes as VLVs. `SMD5` and `MMD5` are the raw 16-byte MD5 digests of the source and target — of the *normalized* forms, when the ROM type carries a normalization.

An overflow block follows the two MD5s when, and only when, `SSIZE` and `MSIZE` differ.

### `0x4D` / `0x41` — overflow

```
┌──────┬──────────┬──────────┬──────────────┐
│ M/A  │ OVER_MUL │   OVER   │  OVERFLOW    │
│ (1)  │   (1)    │(OVER_MUL)│   (OVER)     │
└──────┴──────────┴──────────┴──────────────┘
```

`M` (`0x4D`) appears when the source is longer than the target — the file shrank; `A` (`0x41`) when the target is longer — it grew. `OVER_MUL` and `OVER` together form a VLV giving the overflow length, the difference between the two sizes. `OVERFLOW` is that many bytes: for `A`, the target's tail past the source's end; for `M`, the source's tail past the target's end. The overflow bytes are stored **XORed with `0xFF`**.

### `0x02` — XOR record

```
┌──────┬─────────┬─────────┬──────────────┐
│ 0x02 │   OFF   │   LEN   │  XOR_DATA    │
│ (1)  │  (VLV)  │  (VLV)  │    (LEN)     │
└──────┴─────────┴─────────┴──────────────┘
```

`OFF` is the offset into the file, `LEN` the record length, and `XOR_DATA` a mask of `LEN` bytes. Applying the record sets `output[OFF+i] = input[OFF+i] XOR XOR_DATA[i]` for each `i` in `[0, LEN)`. Because the mask is the XOR of the source and target bytes, the same record reproduces the target from the source or the source from the target.

### `0x00` — end

A single `0x00` ends the stream.

## Reconstruction runs in either direction

A NINJA2 patch describes the difference between two files symmetrically. The XOR records are self-inverse: applied against the source they yield the target, applied against the target they yield the source. The overflow block carries the bytes one file has that the other doesn't, so a size change reverses too — an `A` that appends on the way forward truncates on the way back, an `M` the reverse. The two MD5s let an applier tell which file it was handed, and so which direction to run: a target matching `SMD5` goes forward, one matching `MMD5` goes back.

## ROM types

The one-byte `TYPE` field is both a label and a selector for normalization — stripping copier headers, deinterleaving, so that one patch fits several dumps of a game, with the stripped header put back on the output.

| # | Type | # | Type |
|---|------|---|------|
| 0 | raw | 5 | Game Boy |
| 1 | NES | 6 | SMS / Game Gear |
| 2 | FDS | 7 | Genesis / Mega Drive |
| 3 | SNES | 8 | PC Engine |
| 4 | N64 | 9 | Lynx |

NINJA1 numbers its ROM types differently and defines more of them; a NINJA2 reader must not reuse the NINJA1 table. The per-platform procedures, and slap's dispositions on them, live in [`rom-normalization.md`](rom-normalization.md).

## What the format doesn't specify

The layout above is what the spec sheet and `ninja2.php` state alike. What they leave open — how a reader decodes the `system codepage` when the codepage isn't named, whether the overflow bytes' `0xFF` masking is part of the format, what an applier does with a patch that ends without its `0x00`, how far a VLV may grow, what bytes after the end mean, and how the several implementations diverge on multi-file patches and reverse application — is answered, together with the places where the spec sheet's own prose is wrong (the preamble it calls 1024 bytes is 2048), in [`questions.md`](questions.md).
