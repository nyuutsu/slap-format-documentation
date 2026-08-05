# PMSR — wire format

A PMSR patch is a list of byte runs to overwrite: each record names a position in the file and carries the bytes that belong there. Everything the patch does not name is left as the source had it.

There is no specification document. The format is what Star Rod writes, and this describes what that program does, read out of its source: `src/main/java/patcher/Patcher.java` in Star Rod Classic, the loop following `putInt(MOD_PACKAGE_IDENTIFIER)`. Where this document and that loop disagree, the loop wins.

## Identity

- **Magic**: `PMSR` (4 bytes, ASCII) — written as the integer `0x504D5352`, which is why the letters do not appear as a string anywhere in the program that emits them
- **Endianness**: big-endian, throughout
- **Numbers**: every one is a Java `int` — signed, 32 bits. The top bit of any of them is a sign, not magnitude
- **Extension**: `.mod`
- **Reference tool**: Star Rod (Paper Mario 64 modding tool, Java)

## Overall structure

```
┌────────┬──────────────┬───────────────┐
│  PMSR  │ record count │  records ...  │
│  (4)   │     (4)      │      (*)      │
└────────┴──────────────┴───────────────┘
```

The header is 8 bytes and the first record begins at `0x08`. The count says how many records follow; nothing marks the end of the stream but the count running out.

## Records

```
┌────────────┬────────────┬─────────────┐
│   offset   │   length   │   payload   │
│    (4)     │    (4)     │  (length)   │
└────────────┴────────────┴─────────────┘
```

The offset is an absolute byte position in the file. The payload is the bytes that belong there, verbatim — no run-length form, no back-reference, no compression within a record.

A whole patch is therefore `8 + 8n + Σ lengths` bytes for `n` records.

## Applying

The output is as long as the further of two things: the source, and the end of the record that reaches furthest. It starts as a copy of the source, extended with zeros if some record reaches past the source's end, and each record's payload is then written at its offset.

Records are written in the order they appear.

## What the encoder produces

Not rules of the format, but the shape of every patch anyone has: Star Rod walks the two files together and collects the runs where they differ. Two runs less than 8 bytes apart are merged into one, so records are chunkier than a strict difference would give and a handful of unchanged bytes often ride inside a record. When the target is longer than the source, a final record covers the whole of the extension.

Records therefore arrive in ascending order, never overlapping, and every byte past the source's end is covered by one.

## Compression

Star Rod can Yay0-compress the finished patch as a whole, under its `CompressModPackage` option, in which case the file begins `Yay0` and the bytes above are what comes out of decompressing it. The compression is an envelope around a complete patch, not a part of the format's own structure.

## What the format doesn't specify

There is no document to be silent, so nearly everything not listed above is unstated: whether records must ascend, what two records naming one position mean, what bytes past the last record are for, and whether a reader should insist the source is the file the patch was built from. Those are answered in `questions.md`.
