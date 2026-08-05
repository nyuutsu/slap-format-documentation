# APS-N64 — wire format

An APS-N64 patch is a list of byte runs to overwrite, in the manner of IPS but with a four-byte offset and a header that can carry enough of an N64 image's identity to check it before patching.

The format has a specification, `upstream/bb-aps12.zip`'s `aps.txt`, and the authors shipped their own applier and creator as C source in the same archive. The document and the code disagree in a few places about where fields sit; where they do, this follows the code, and `questions.md` says which places those are.

## Identity

- **Magic**: `APS10` (5 bytes, ASCII)
- **Endianness**: little-endian for offsets and sizes; big-endian for the identity fields copied out of the ROM, which `aps.txt` calls "Motorola (human readable) endian"
- **Addressable range**: 2 GB, which `aps.txt` states in prose and both authors' programs implement by holding the offset in a C `long`
- **Reference tools**: `n64aps.c` (apply) and `n64caps.c` (create), by Black Bag

Note that APS-GBA, an unrelated format, opens with the first four of those five bytes. A reader testing `APS1` before `APS10` will claim every APS-N64 patch as one of those.

## Header

Every patch opens with the same 57 bytes, and then carries one of two tails depending on the patch type.

```
┌─────────┬──────┬──────────┬───────────────┬──────────────┐
│  APS10  │ type │ encoding │  description  │  type tail   │
│   (5)   │ (1)  │   (1)    │     (50)      │     (*)      │
└─────────┴──────┴──────────┴───────────────┴──────────────┘
```

The type byte is `0` for a plain patch and `1` for one carrying N64 identity. The encoding byte is `0`, the only method defined. The description is free text, space-padded.

**Type 0** adds only the size the output should be:

```
┌───────────────────┐
│ destination size  │   header is 61 bytes
│       (4)         │
└───────────────────┘
```

**Type 1** adds the fields an applier can check a ROM against, taken verbatim out of the image's own header:

```
┌────────┬─────────┬─────────┬───────┬───────┬───────────────────┐
│ image  │ cart ID │ country │  CRC  │  pad  │ destination size  │
│ format │   (2)   │   (1)   │  (8)  │  (5)  │       (4)         │
│  (1)   │         │         │       │       │                   │
└────────┴─────────┴─────────┴───────┴───────┴───────────────────┘
```

Header is 78 bytes. The image-format byte is `0` for Doctor V64 and `1` for CD64/Z64/Wc/SP, naming the byte order the image was in. The pad is five zero bytes, reserved.

## Records

```
┌────────────┬──────┬─────────────┐
│   offset   │ size │   payload   │
│    (4)     │ (1)  │   (size)    │
└────────────┴──────┴─────────────┘
```

The offset is an absolute byte position. The size is a single byte, so one record writes at most 255 bytes and a longer run is split across several.

A size of zero marks the run-length form, where two bytes follow instead of a payload:

```
┌────────────┬──────┬───────┬───────┐
│   offset   │  00  │ value │ count │
│    (4)     │ (1)  │  (1)  │  (1)  │
└────────────┴──────┴───────┴───────┘
```

which writes `value` repeated `count` times, starting at the offset. Value first, then count.

The record stream runs to the end of the file; nothing marks its end.

## Applying

The output begins as a copy of the source, resized to the header's destination size — truncated if the source is longer, extended if shorter — and each record is then written at its offset.

For a type 1 patch, an applier can first check the source's cart ID, country and CRC against the header's, all three being copies of bytes in the ROM's own header rather than anything computed.

## What the format doesn't specify

`aps.txt` covers the layout and little else. It does not say whether records must ascend or what two records at one position mean, what bytes past the last whole record are for, whether a size mismatch should stop an apply, or what an image-format byte other than `0` or `1` would mean. Those, and the places where its byte ranges do not survive arithmetic, are in `questions.md`.
