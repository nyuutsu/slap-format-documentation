# PPF1 — wire format

A PPF1 patch is a list of byte runs to overwrite in a disc image. Each record names an absolute position and carries the bytes that belong there. `ppf.txt` calls the encoding "Similar to IPS", which is a fair description: an offset, a count, some bytes, repeated to the end of the file.

Its own documents are `upstream/pdx-ppf1`'s `ppf.txt` and `ppf-doc.txt`, and the archive ships the C source of both tools alongside them. Everything below is from those four files. Where this document and they disagree, they win.

## Identity

- **Magic**: `PPF10` (5 bytes, ASCII)
- **Encoding method**: byte `0x00`, "Simple Encoding", the only one defined
- **Ceiling**: 2 GB — `ppf-doc.txt` lists it among the format's features, "PPF facilitate patching of iso-files up to 2Gb"
- **Author**: Paradox
- **Reference tools**: `sources/makeppf.c` and `sources/applyppf.c`, shipped with PC and Amiga binaries

## Header

```
┌─────────┬──────────┬───────────────┐
│  PPF10  │ encoding │  description  │
│   (5)   │   (1)    │     (50)      │
└─────────┴──────────┴───────────────┘
```

56 bytes. The description is free text, which `ppf.txt` says is space-padded. The patch begins at byte 56.

## Records

```
┌────────────┬───────┬─────────────┐
│   offset   │ count │   payload   │
│    (4)     │  (1)  │   (count)   │
└────────────┴───────┴─────────────┘
```

The offset is an absolute byte position in the image. The count is one byte, so a single record writes at most 255 bytes; the creator reads its inputs in 255-byte chunks, so a longer run of differences comes out as a series of records rather than one large one.

A count of zero means the two bytes that follow are a value and a repeat count, rather than a payload:

```
┌────────────┬───────┬───────┬────────┐
│   offset   │  00   │ value │ repeat │
│    (4)     │  (1)  │  (1)  │  (1)   │
└────────────┴───────┴───────┴────────┘
```

`ppf.txt`: "Byte zero (0) will be the data and byte one (1) will be the number of repetitions." Value first, then count.

The record stream runs to the end of the file. There is no terminator and no record count; the applier stops when its read of the next offset comes back empty.

## Examples

Both are `ppf.txt`'s own.

```
  D0 F9 15 00  03  01 02 03      at offset 0x0015F9D0, write 01 02 03
  D0 F9 15 00  00  FF 10         at offset 0x0015F9D0, write 0xFF sixteen times
```

Note what the offsets show: `D0 F9 15 00` is `0x0015F9D0` with its least significant byte first. The documents never use the word "endian", but their worked examples are little-endian.

## Applying

The output begins as a copy of the source, and each record is written at its offset. Nothing is checked first — the format carries no checksum, no size, and nothing else to check against.

## What PPF1 does not have

No file-size field, no validation block, no undo data, no FILE_ID.DIZ trailer. A PPF1 patch says only where bytes go.

## What the format doesn't specify

Byte order, in prose. Whether records must ascend, or what two records covering one byte mean. What encoding the description is in. Whether a patch may change the image's length. Those are in `questions.md`.
