# IPS — wire format

An IPS patch is a sequence of records that write bytes at given offsets in
a target file. The format carries no checksums, no metadata, no file-size
field, and no source identification.

## Layout

An IPS patch consists of:

1. The five ASCII bytes `PATCH`.
2. Zero or more records.
3. The three ASCII bytes `EOF`.

```
┌─────────┬────────────────┬───────┐
│  PATCH  │   records ...  │  EOF  │
│   (5)   │      (*)       │  (3)  │
└─────────┴────────────────┴───────┘
```

## Records

Each record begins with a 3-byte big-endian offset and a 2-byte big-endian
size. The body that follows depends on the size.

```
┌──────────┬────────┬──────────────┐
│  offset  │  size  │   body ...   │
│   (3)    │  (2)   │              │
└──────────┴────────┴──────────────┘
```

If the size is nonzero, the record's payload is the next `size` bytes.
Applying the record writes those bytes to the target starting at the given
offset.

```
┌──────────┬────────┬────────────┐
│  offset  │  size  │  payload   │
│   (3)    │ (2, ≥1)│   (size)   │
└──────────┴────────┴────────────┘
```

If the size is zero, the record is run-length-encoded. The next 2 bytes
are a big-endian run count, and the following byte is a fill value.
Applying the record writes the fill byte, `count` times, to the target
starting at the given offset.

```
┌──────────┬───────┬─────────┬──────┐
│  offset  │ 0x00  │  count  │ fill │
│   (3)    │  (2)  │   (2)   │ (1)  │
└──────────┴───────┴─────────┴──────┘
```

Offsets are 24 bits wide, big-endian. A hypothetical offset of `0x123456`
encodes as three bytes on the wire:

```
┌──────┬──────┬──────┐
│ 0x12 │ 0x34 │ 0x56 │
└──────┴──────┴──────┘
  high          low
```

Sizes, RLE counts, and payload lengths are 16 bits wide, also big-endian.
The maximum byte a single record can touch is therefore
`0xFFFFFF + 0xFFFF - 1 = 0x100FFFD`.

## Variants

**IPS32.** Magic becomes `IPS32` (five bytes). Trailer becomes `EEOF`
(four bytes, `0x45 0x45 0x4F 0x46`). Record offsets widen from 3 to 4
bytes. Record size and RLE encoding are unchanged.

```
┌─────────┬────────────────┬────────┐
│  IPS32  │   records ...  │  EEOF  │
│   (5)   │      (*)       │  (4)   │
└─────────┴────────────────┴────────┘

┌────────────┬────────┬──────────────┐
│   offset   │  size  │   body ...   │
│    (4)     │  (2)   │              │
└────────────┴────────┴──────────────┘
```

**EBP.** Structurally IPS: a `PATCH` patch with an `EOF` trailer,
followed by a JSON metadata blob.

```
┌─────────┬────────────────┬───────┬────────────────────┐
│  PATCH  │   records ...  │  EOF  │  JSON metadata ... │
│   (5)   │      (*)       │  (3)  │         (*)        │
└─────────┴────────────────┴───────┴────────────────────┘
```

## Example

A minimal patch that writes the three bytes `0xAA 0xBB 0xCC` at offset
`0x000100` of the target:

```
  50 41 54 43 48     "PATCH"
  00 01 00           offset 0x000100
  00 03              size 3
  AA BB CC           payload
  45 4F 46           "EOF"
```

Total: 16 bytes on the wire.

A minimal RLE patch that writes the byte `0xFF` fifty times starting at
offset `0x002000`:

```
  50 41 54 43 48     "PATCH"
  00 20 00           offset 0x002000
  00 00              size 0 (RLE marker)
  00 32              count 50
  FF                 fill byte
  45 4F 46           "EOF"
```

Total: 16 bytes on the wire.

## What the format doesn't specify

The wire layout above is the spec. Everything else — how to handle a
record whose offset encodes to the trailer bytes, whether a zero-count
RLE record is valid, whether the target can be truncated after record
application, whether overlapping records are permitted, in what order
records apply — is either implementation convention or left open. Those
questions are answered separately.
