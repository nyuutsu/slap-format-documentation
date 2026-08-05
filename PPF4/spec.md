# PPF4 — wire format

A PPF4 patch does two things: it overwrites byte runs inside a file, and it appends bytes to the end of one. The second is the reason the format exists — its author needed to make files longer, and says so.

There is no separate specification document. What there is instead is a maker in C++ and an applier in Lua, the applier carrying a description of the layout in its comment block. Everything below comes from `upstream/gs2-bugfixes-master`'s `tools/ppfmaker (custom)/src/ppfmaker.cpp` and `patcher.lua`. Where this document and those disagree, those win.

## Identity

- **Magic**: `PPF40` (5 bytes, ASCII)
- **Encoding version**: byte `0xFF`
- **Offsets**: 4 bytes, unsigned
- **Author**: Pyriel
- **Reference tools**: the two named above

The magic was chosen to *avoid* recognition. From `ppfmaker.cpp`'s header comment:

> The patch header contains PPF40 which is not used by other tools, and will hopefully prevent individual files from being seen as patches that tools like PPFomatic could apply.

## Header

```
┌─────────┬─────────┬───────────────┬───────┬────────────┬──────┬───────────┐
│  PPF40  │ version │  description  │ image │ validation │ undo │ expansion │
│   (5)   │   (1)   │     (50)      │  (1)  │    (1)     │ (1)  │    (1)    │
└─────────┴─────────┴───────────────┴───────┴────────────┴──────┴───────────┘
```

60 bytes. The description is 50 bytes of free text, filled with zeros rather than spaces.

The last four bytes are all zero, and the applier's comment says why each is: image type "Always 0 for binary file", validation "Always 0, validation not supported", undo "Always 0, undo not supported", expansion "Unused value". The applier reads all four as one 32-bit value and refuses the patch unless it is zero — so no byte order applies to that read, four zero bytes being four zero bytes either way.

## Records

```
┌─────────┬────────────┬───────┬─────────────┐
│ command │   offset   │ count │   payload   │
│   (1)   │    (4)     │  (1)  │   (count)   │
└─────────┴────────────┴───────┴─────────────┘
```

Two commands:

- **`0` — replace.** Write the payload at the given offset in the file.
- **`1` — add.** Append the payload to the end of the file. The offset field is unused; the maker writes zero there and the applier ignores it.

The count is one byte, so a record carries at most 255 bytes; the maker's buffer is that size, so a longer run becomes a series of records.

Every replace must come before every add. Once the applier has seen an add it is appending, and a replace after that point is refused.

A replace must land wholly inside the file as it was: the applier refuses one whose offset is past the end, and one whose offset plus count runs past it.

The record stream runs to the end of the file. A record needs seven bytes to exist — its six header bytes and at least one byte of payload — and the applier refuses a patch whose remainder is shorter than that.

## Applying

Replaces are written at their offsets, then adds are appended in order. Nothing is checked about the file first; the format carries no checksum, no size, and no sample of the original.

Because appending is unconditional, `ppfmaker.cpp` notes what happens if a patch is run twice:

> running an already expanded file through a second time will cause the file to be expanded again with duplicated data

## What PPF4 does not have

No undo data, no validation block, no FILE_ID.DIZ area, and no way to make a file shorter. The header has bytes reserved for the first two and its own applier says neither is supported.

## What the format doesn't specify

Which end of the offset comes first. What a command byte other than `0` or `1` means. Whether replaces must ascend, or what two of them covering one byte mean. What encoding the description is in. Those are in `questions.md`.
