# PPF3 — wire format

A PPF3 patch overwrites byte runs in a disc image. It can carry a sample of the image it was made from, so a patcher can recognise the right file, and it can carry the bytes it replaced, so the patch can be taken back off again.

Its format document is `upstream/ppf-master`'s `ppfdev/PPF3.txt`, and the same archive ships the C source of both tools in two build variants each. Everything below comes from those. Where this document and they disagree, they win.

## Identity

- **Magic**: `PPF30` (5 bytes, ASCII)
- **Encoding method**: byte at offset 5, `0x02` for this encoding
- **Byte order**: little-endian, stated outright — "Be careful! Endian format is Intel!"
- **Ceiling**: 9,223,372,036,854,775,807 bytes, which PPF3.txt gives as a figure
- **Author**: Icarus of Paradox
- **Reference tools**: `makeppf3` and `applyppf3`, both as source

## Header

```
┌─────────┬──────────┬───────────────┬───────┬───────┬──────┬───────┬────────────────────┐
│  PPF30  │ encoding │  description  │ image │ block │ undo │ dummy │  identity block    │
│   (5)   │   (1)    │     (50)      │  (1)  │  (1)  │ (1)  │  (1)  │  (1024, optional)  │
└─────────┴──────────┴───────────────┴───────┴───────┴──────┴───────┴────────────────────┘
```

60 bytes when the identity block is absent, 1084 when it is present. Three of the single bytes are switches:

- **image** — `0x00` for a BIN image, `0x01` for a GI one, as made by PrimoDVD. It decides where the identity block was sampled from.
- **block** — `0x01` if an identity block follows, `0x00` if not. PPF3.txt: "If disabled applyppf won't validate the patch also the 1024 byte block won't be available."
- **undo** — `0x01` if every record carries the bytes it replaced, `0x00` if not. PPF3.txt says what that buys: an applier "will be able to restore a previous patched bin to back to its original state. Patchsize increases of course."
- **dummy** — PPF3.txt says "Not used."

The identity block, where present, is 1024 bytes copied from the source image starting at `0x9320` for a BIN image or `0x80A0` for a GI one.

## Records

```
┌──────────────────┬───────┬─────────────┬──────────────────┐
│      offset      │ count │ patch bytes │   undo bytes     │
│       (8)        │  (1)  │   (count)   │ (count, if undo) │
└──────────────────┴───────┴─────────────┴──────────────────┘
```

PPF3.txt gives the offset as a 64-bit integer and the count as a `u_char`, so one record writes at most 255 bytes; the creator ends a run at 255 and opens another, so a longer stretch of differences becomes a series of records.

When the header's undo byte is set, each record carries its count twice over: first the bytes to write, then the bytes that were there before. PPF3.txt's own two examples show the difference, both at offset `0x15F9D0`:

```
  D0 F9 15 00 00 00 00 00  03  01 02 03              write 01 02 03
  D0 F9 15 00 00 00 00 00  03  01 02 03  0A 0A 0A    write 01 02 03, which replaced 0A 0A 0A
```

## FILE_ID.DIZ area

Optional, and sitting after the record stream:

```
┌────────────────────┬───────────┬──────────────────┬────────┐
│ @BEGIN_FILE_ID.DIZ │  content  │ @END_FILE_ID.DIZ │ length │
│        (18)        │  (length) │       (16)       │  (2)   │
└────────────────────┴───────────┴──────────────────┴────────┘
```

Both markers are literal ASCII, following what PPF3.txt calls the Amiga BBS standard. The trailing field is "an unsigned short (2 byte) with the length" — named unsigned in as many words. The content "cannot exceed 3072 byte".

Because the area sits at the very end, PPF3.txt tells anyone writing an applier where to look for it:

> If you want to do a PPF3.0 Applier be sure to check for an existing FILE_ID area, because it is located after the PATCH DATA!

## Applying

The output begins as a copy of the source and each record's patch bytes are written at its offset. Where an identity block is present, a patcher can first compare the source's bytes at the sampling position against it.

A patch whose undo byte is set can also be run the other way: the same walk, writing each record's undo bytes instead of its patch bytes, which returns a patched image to what it was.

## Where the records end

Nothing marks the end of the record stream and no count of records is stored, so a reader works it out from the file's length — the body is what remains after the header, less the FILE_ID.DIZ area if one is present.

## What the format doesn't specify

Whether records must ascend, or what two records covering one byte mean. What encoding the description is in, and how its field is filled. Whether a patch may change the image's length. Those are in `questions.md`.
