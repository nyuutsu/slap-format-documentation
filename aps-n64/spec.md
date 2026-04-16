# APS (N64) — Format Specification

This is a clean writeup of the on-disk structure of an APS N64 patch,
derived from the authoritative reference: the C source in
`upstream/n64aps.c` (apply) and `upstream/n64caps.c` (create). Where
the source code and `upstream/aps.txt` (the spec text) disagree, the
source code is correct. The spec text has known errata in byte
ranges.

The `proposal.md` sibling file covers slap's design decisions for
edge cases. This document describes what the format is.

## Identity

- **Magic**: `APS10` (5 bytes, ASCII)
- **Endianness**: little-endian for integers; N64 metadata fields
  stored in canonical Z64 (big-endian) byte order regardless of
  source
- **Authors**: Silo and Fractal of Blackbag
- **Reference tool**: bb-aps 1.2 (December 1998)
- **Scene**: N64 backup copier / homebrew (Doctor V64, CD64)

## Overall structure

```
[ standard header (57 bytes) ]
[ type-specific header (4 or 21 bytes) ]
[ record 0 (variable size) ]
[ record 1 ]
...
[ record N-1 ]
```

No EOF marker; records run until end of file.

## Standard header (57 bytes, always)

| Offset | Size | Field            | Notes |
|--------|------|------------------|-------|
| 0x00   | 5    | Magic            | ASCII `APS10` |
| 0x05   | 1    | Patch type       | 0 = Simple, 1 = N64-specific |
| 0x06   | 1    | Encoding method  | 0 = Simple (only defined value) |
| 0x07   | 50   | Description      | Free text, space-padded, not null-terminated, no declared encoding |

## Type 0 (Simple) header (4 bytes)

When patch type is 0, the standard header is followed by just:

| Offset | Size | Field            | Notes |
|--------|------|------------------|-------|
| 0x39   | 4    | Destination size | uint32 LE |

Then records begin at byte 0x3D.

## Type 1 (N64-specific) header (21 bytes)

When patch type is 1, the standard header is followed by:

| Offset | Size | Field            | Notes |
|--------|------|------------------|-------|
| 0x39   | 1    | Image format     | 0 = Doctor V64, 1 = CD64/Z64/Wc/SP |
| 0x3A   | 2    | Cart ID          | From source ROM, stored in Z64 byte order |
| 0x3C   | 1    | Country code     | From source ROM |
| 0x3D   | 8    | CRC              | From source ROM, stored in Z64 byte order |
| 0x45   | 5    | Padding          | Reserved, always zero on create |
| 0x4A   | 4    | Destination size | uint32 LE |

Then records begin at byte 0x4E.

## Records

Each record begins with a 5-byte prefix:

| Size | Field       | Notes |
|------|-------------|-------|
| 4    | Offset      | uint32 LE, byte offset in the target file |
| 1    | Data length | Dispatch byte: 0 for RLE, 1-255 for literal |

### Literal record (data length 1-255)

If data length is 1-255, the next `data length` bytes are the
replacement bytes to write at the given offset.

```
[ offset: 4 ] [ length: 1 ] [ data: length ]
```

Total size: `5 + length` bytes (6-260 bytes per record).

### RLE record (data length 0)

If data length is 0, the record continues with:

| Size | Field         | Notes |
|------|---------------|-------|
| 1    | Fill value    | The byte to repeat |
| 1    | Repeat count  | Number of times to write the fill byte (1-255) |

```
[ offset: 4 ] [ 0x00 ] [ fill: 1 ] [ count: 1 ]
```

Total size: 7 bytes per RLE record.

RLE records can represent runs of 1-255 identical bytes. Longer runs
require splitting into multiple records.

## N64 byte order handling

N64 ROMs exist in three physical byte orders:

- **Z64**: native big-endian. First 4 bytes `80 37 12 40`.
- **V64**: 16-bit byte-swapped (Doctor V64 format). First 4 bytes
  `37 80 40 12`.
- **N64**: 32-bit byte-swapped, little-endian. First 4 bytes
  `40 12 37 80`. (bb-aps does not recognize this format.)

For type 1 patches, the image format field declares which physical
byte order the records target. The cart ID, country code, and CRC
fields in the patch header are always stored in canonical Z64
byte order regardless of the source format.

At apply time, the reference tool reads the N64 header fields from
the source at their Z64 physical offsets:

- Cart ID: bytes 60-61 (byte-swapped in the buffer if source is V64)
- Country: byte 62 for Z64, byte 63 for V64 (physical positions)
- CRC: bytes 16-23 (byte-pair-swapped if source is V64)

Records target physical byte positions. A patch created from a V64
source is not interoperable with a Z64 source (or vice versa) without
conversion — the physical byte layouts differ.

## Destination size semantics

The destination size field is **binding**, not advisory. The reference
tool resizes the source file to exactly the declared destination size
before applying any records:

- If the source is larger: truncate to destination size
- If the source is smaller: zero-fill to destination size

Records are then applied against the resized file. A record whose
offset plus length would exceed the destination size is not valid
under these semantics (the reference tool would silently extend the
file, but the resulting output violates the contract).

## Reversibility

**None.** APS N64 is byte replacement — records contain new data,
not old. Once applied, the original bytes are gone. Applying a patch
to an already-patched file does not recover the original.

Same fundamental limitation as IPS.

## Checksums

- **No patch-file checksum.** Corruption of the patch itself is
  undetectable.
- **N64 cart header fields** (cart ID, country, CRC) are used for
  source verification, not for the patch file itself. These verify
  that the source ROM matches what the patch expects.

## Source verification (type 1 only)

On apply, the reference tool verifies the source ROM against the
patch header fields:

| Field | Mismatch behavior | Bypassable? |
|-------|-------------------|-------------|
| Image format (Z64 vs V64) | Hard error | No |
| Cart ID | Hard error | No |
| Country | Warning | Yes, with `-f` flag |
| CRC | Warning | Yes, with `-f` flag |

The cart ID is the load-bearing identifier — it identifies the
specific game. Country and CRC mismatches are common across ROM
revisions and can be overridden.

## Limitations

- **Per-record data length is 1 byte**: max 255 bytes of literal per
  record. Longer runs of changes must be split across multiple
  records.
- **Per-record RLE count is 1 byte**: max 255 bytes per RLE record.
- **Offset is 4 bytes**: 4GB theoretical address space (the spec
  claims "up to 2GB").
- **Only encoding method 0 is defined.** The byte exists "for future
  expansion" but no other encoding was ever implemented.
- **Only patch types 0 and 1 are defined.**
- **Only image format values 0 (V64) and 1 (Z64) are defined.** The
  .n64 byte order has no reserved value.
- **Description encoding is unspecified.** The format stores 50 raw
  bytes with no encoding declaration.
- **No metadata beyond description and the N64 cart fields.**

## Spec text errata

`upstream/aps.txt` contains the following byte-range typos:

- Type 0 header: claims "BYTE 57-61: Size of destination image" (5
  bytes). Actual: 4 bytes at 57-60.
- Type 1 header padding: claims "BYTE 69-74: Pad" (6 bytes). Actual:
  5 bytes at 69-73.
- Type 1 header size: claims "BYTE 75-79: Size of destination image"
  (5 bytes). Actual: 4 bytes at 74-77.

The C source in `n64aps.c` and `n64caps.c` is authoritative. This
spec document reflects the code, not the prose errata.
