# GDIFF — wire format

A GDIFF patch is a stream of one-byte commands that build the new file from two sources: bytes carried in the patch, and runs copied out of the old file. It is read start to finish; there is no index, no header beyond five bytes, and no checksum.

This is a clean writeup of the specification vendored beside it, `Generic Diff Format Specification.html` — the W3C's NOTE-GDIFF-19970901, submitted by Marimba. Where this document and that one disagree, that one wins.

## Identity

- **Magic**: `0xD1 0xFF 0xD1 0xFF` (4 bytes)
- **Version**: one byte, value `4`
- **Endianness**: big-endian throughout, in the spec's words "most significant byte first"
- **MIME type**: `application/gdiff`
- **Reference implementation**: javaxdelta (`com.nothome.delta.GDiffPatcher` and `GDiffWriter`)

## Overall structure

```
┌─────────────┬─────────┬────────────────┬─────┐
│ D1 FF D1 FF │ version │  commands ...  │ 00  │
│     (4)     │   (1)   │      (*)       │ (1) │
└─────────────┴─────────┴────────────────┴─────┘
```

The stream ends at the first command byte of `0`, which is the end-of-file command and carries no arguments.

## Types

The spec names five, and the distinction between them is the whole of what a reader has to get right:

| name     | width   | signed | 
|----------|---------|--------|
| `byte`   | 8 bits  | yes    |
| `ubyte`  | 8 bits  | no     |
| `ushort` | 16 bits | no     |
| `int`    | 32 bits | **yes** |
| `long`   | 64 bits | yes    |

`int` being signed is the trap. Three of the COPY commands take a position of that type and four take a length of it, so a patch reaching past `0x7FFFFFFF` in any of those places is naming a negative number rather than a larger one. The spec says as much where it tells encoders what to do about it: *"If a number larger than 2^31-1 bytes is needed for a command that takes only `int` arguments, the command must be split into multiple commands."*

## Commands

Each command is one byte, followed by the arguments its opcode names.

| opcode | name | followed by | action |
|-------:|------|-------------|--------|
| 0 | EOF | — | end of stream |
| 1–246 | DATA | that many bytes | append them |
| 247 | DATA | `ushort`, then that many bytes | append them |
| 248 | DATA | `int`, then that many bytes | append them |
| 249 | COPY | `ushort` position, `ubyte` length | copy from the old file |
| 250 | COPY | `ushort` position, `ushort` length | " |
| 251 | COPY | `ushort` position, `int` length | " |
| 252 | COPY | `int` position, `ubyte` length | " |
| 253 | COPY | `int` position, `ushort` length | " |
| 254 | COPY | `int` position, `int` length | " |
| 255 | COPY | `long` position, `int` length | " |

The seven COPY forms are one command in seven encodings, chosen by how wide the two numbers need to be. An encoder picks the narrowest pair that fits; a reader must accept all of them.

Opcodes 1 to 246 are the compact DATA form, where the opcode is itself the byte count, so a short run of literal bytes costs one byte of overhead.

## Applying

The output starts empty and only ever grows at its end. DATA appends the bytes that follow it. COPY appends `length` bytes read from the old file starting at `position` — an absolute position, not relative to anything the walk has done, so the commands may draw from the old file in any order and revisit it freely.

Nothing writes to a position; nothing reads from the output. A patch is therefore applied in one forward pass with the old file open for random reads.

## Example

The specification's own, reproduced here because it is short and settles several questions at once. Old file `ABCDEFG`, new file `ABXYCDBCDE`:

```
  d1 ff d1 ff        magic
  04                 version
  f9 00 00 02        COPY 249: position 0, length 2      -> "AB"
  02 58 59           DATA 2: two literal bytes           -> "XY"
  f9 00 02 02        COPY 249: position 2, length 2      -> "CD"
  f9 00 01 04        COPY 249: position 1, length 4      -> "BCDE"
  00                 EOF
```

Note the third and fourth commands: the old file is read at position 2 and then at position 1, going backwards. That is ordinary.

## What the format doesn't specify

The spec gives widths and signs for every field and an action for every opcode, which is more than most formats in this collection manage. What it does not say is what a reader should do when a field carries a value the type admits but the action cannot use — a negative length, or a position past the end of the old file. Those are in `questions.md`.
