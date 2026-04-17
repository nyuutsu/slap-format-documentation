# IPS — wire format

An IPS patch is a sequence of records that write bytes at given offsets in
a target file. The format carries no checksums, no metadata, no file-size
field, and no source identification.

## Layout

An IPS patch consists of:

1. The five ASCII bytes `PATCH`.
2. Zero or more records.
3. The three ASCII bytes `EOF`.

None of these bytes are null-terminated.

## Records

Each record begins with a 3-byte big-endian offset and a 2-byte big-endian
size.

If the size is nonzero, the record's payload is the next `size` bytes.
Applying the record writes those bytes to the target starting at the given
offset.

If the size is zero, the record is run-length-encoded. The next 2 bytes
are a big-endian run count, and the following byte is a fill value.
Applying the record writes the fill byte, `count` times, to the target
starting at the given offset.

Offsets are 24 bits wide. Sizes, RLE counts, and payload lengths are 16
bits wide. The maximum byte a single record can touch is therefore
`0xFFFFFF + 0xFFFF - 1 = 0x100FFFD`.

## Variants

**IPS32.** Magic becomes `IPS32` (five bytes). Trailer becomes `EEOF`
(four bytes, `0x45 0x45 0x4F 0x46`). Record offsets widen from 3 to 4
bytes. Record size and RLE encoding are unchanged.

**EBP.** Structurally IPS: a `PATCH` patch with an `EOF` trailer,
followed by a UTF-8 JSON metadata blob.

## What the format doesn't specify

The wire layout above is the spec. Everything else — how to handle a
record whose offset encodes to the trailer bytes, whether a zero-count
RLE record is valid, whether the target can be truncated after record
application, whether overlapping records are permitted, in what order
records apply — is either implementation convention or left open. slap's
answers to those questions are documented separately.
