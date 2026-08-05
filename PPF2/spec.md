# PPF2 — wire format

A PPF2 patch overwrites byte runs in a disc image, and carries enough of that image's identity for a patcher to tell, before it writes anything, whether it has been handed the right file.

Its format document is `upstream/pdx-ppf2`'s `ppftools/ppfdev/PPF2.txt`, and the archive ships four DOS programs and their user documents. No source was released. Everything below comes from those; where this document and they disagree, they win.

## Identity

- **Magic**: `PPF20` (5 bytes, ASCII)
- **Encoding method**: byte at offset 5, `$01` for this encoding
- **Byte order**: little-endian, which PPF2.txt shows by example and warns Amiga implementers to swap
- **Author**: Icarus of Paradox
- **Tools**: `MakePPF.exe`, `ApplyPPF.exe`, `PPFDiz.exe`, `PPFINFO.EXE`, plus a Windows front end

## Header

```
┌─────────┬──────────┬───────────────┬─────────────┬────────────────────┐
│  PPF20  │ encoding │  description  │ source size │  identity block    │
│   (5)   │   (1)    │     (50)      │     (4)     │       (1024)       │
└─────────┴──────────┴───────────────┴─────────────┴────────────────────┘
```

1084 bytes, always. There is no flag governing the identity block; every PPF2 patch carries one, and the record stream begins at offset 1084.

**Source size** is the length of the image the patch was built from. PPF2.txt says what it is for: "Used for Identification."

**Identity block** is 1024 bytes copied out of that image starting at position `$9320`, and PPF2.txt gives it the same purpose. The two together are what let a patcher recognise the image; how much weight each carries is in `questions.md`.

**Description** is 50 bytes of free text. `MakePPF.txt` tells its user they have "max. 50 Chars".

## Records

```
┌────────────┬───────┬─────────────┐
│   offset   │ count │   payload   │
│    (4)     │  (1)  │   (count)   │
└────────────┴───────┴─────────────┘
```

An absolute byte position, then a one-byte count, then that many bytes to write there. A run longer than 255 bytes is carried by more than one record.

This is the only record form PPF2.txt describes, and it gives one example:

```
  D0 F9 15 00  03  01 02 03      at offset 0x0015F9D0, write 01 02 03
```

The offset in that example reads least significant byte first, and the document warns anyone porting the tool to a big-endian machine to swap it:

> Be careful! watch the endian format!!! If you own an Amiga and want to do a PPF2-Patcher for Amiga don't forget to swap the endian-format of the OFFSET to avoid seek errors!

The record stream runs to the end of the file, or to the start of the trailer below. Nothing marks its end.

## FILE_ID.DIZ area

Optional, and sitting after the record stream:

```
┌────────────────────┬───────────┬──────────────────┬────────┐
│ @BEGIN_FILE_ID.DIZ │  content  │ @END_FILE_ID.DIZ │ length │
│        (18)        │  (length) │       (16)       │  (4)   │
└────────────────────┴───────────┴──────────────────┴────────┘
```

Both markers are literal ASCII, following what PPF2.txt calls the Amiga BBS standard. The trailing field is the content's length — "an Integer (4 byte long)" — and the content itself "cannot be greater than 3072 Bytes".

Because the area sits at the very end, a reader finds it by looking there rather than by walking forward to it. PPF2.txt puts this plainly to anyone writing an applier:

> If you do a PPF 2.0 Applier be sure to check for an existing FILE ID AREA, because it is located after the PATCH DATA!

The area is written by a separate tool after a patch is made, not by the patch creator.

## Applying

A patcher can first compare the image it has been given against the header's source size and identity block, then copy the image and write each record at its offset. A FILE_ID.DIZ, where present, is shown to the user while patching.

## What the format doesn't specify

How the description field is padded, or what encoding its bytes are in. Whether records must ascend, or what two records covering one byte mean. How far an offset may reach. Whether a patch may change the image's length. Those are in `questions.md`.
