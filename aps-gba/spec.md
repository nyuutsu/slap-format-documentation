# APS-GBA — wire format

An APS-GBA patch rewrites a file in fixed 64 KiB blocks. Each block that differs between source and target is carried whole, as the XOR of the two, alongside a checksum of each side. Blocks that match are absent from the patch entirely.

No specification document for this format is known to us. Everything below was read out of the original patcher's own binary, or measured by driving that binary and reading what it wrote. `reference-harness.md` describes how to reproduce any of it. Where this document and the binary disagree, the binary wins.

## Identity

- **Magic**: `APS1` (4 bytes, ASCII `41 50 53 31`)
- **Endianness**: little-endian for every fixed-width field
- **Block size**: 65536 bytes, fixed
- **Checksum**: CRC-16/CCITT-FALSE — polynomial `0x1021` unreflected, init `0xFFFF`, no final XOR, MSB-first
- **Reference tool**: A-Ptch (`AdvancePtch`), a Visual Basic 6 application for Windows

The magic is a strict prefix of APS-N64's five-byte `APS10`. Any detector that tests `APS1` before `APS10` will claim every APS-N64 patch as an APS-GBA one. The two formats are otherwise unrelated and share nothing past those four bytes.

## Overall structure

```
┌─────────┬─────────────┬─────────────┬───────────────┐
│  APS1   │ source size │ target size │  records ...  │
│   (4)   │     (4)     │     (4)     │      (*)      │
└─────────┴─────────────┴─────────────┴───────────────┘
```

The header is 12 bytes. The first record begins at `0x0C`. There is no record count, no trailer, and no terminator: the record stream runs to end of file, and the record count is `(fileSize − 12) / 65544`.

Both sizes are the exact byte lengths of the two files the patch was built from, written from the lengths as the operating system reported them.

## Records

Every record is exactly 65544 bytes — an 8-byte header and a full 65536-byte payload. Records are never short, not even the one covering the tail of a file that ends mid-block.

```
┌────────────┬───────────┬───────────┬──────────────────┐
│   offset   │ source    │ target    │  xor payload     │
│    (4)     │ crc16 (2) │ crc16 (2) │    (65536)       │
└────────────┴───────────┴───────────┴──────────────────┘
```

The offset is the absolute byte position in the file where the block begins. It is always a multiple of 65536.

The payload is the bytewise XOR of the target block against the source block. Applying a record writes `source[p] ^ payload[p]` for each of the 65536 positions from `offset`.

## The block model

The file is treated as a sequence of aligned 64 KiB blocks. Block `i` covers `[i × 65536, (i + 1) × 65536)`.

Where a block runs past the end of a file — because the file does not end on a 64 KiB boundary, or because the other file is longer — the missing bytes read as `0x00`. Both files are, in effect, zero-extended to a whole number of blocks. This applies to the XOR payload and to the checksums alike; the consequences are drawn out under *Checksums* below.

A record is emitted for a block only when the source and target blocks differ. Identical blocks produce nothing, so a patch's records are a sparse, ascending, gap-permitting sequence.

The number of blocks considered is derived from the longer of the two files. The reference tool computes `max(sourceLength, targetLength) ÷ 65536` by integer division and then iterates from `0` through that value **inclusive**, so it always considers one block beyond what the division alone would suggest. This is self-cancelling rather than consequential: for a file that is an exact multiple of 64 KiB the extra block lies wholly past both ends, both sides read as all-zero, the two compare equal, and no record is emitted. For any other length the extra block is the partial one that genuinely needs covering.

## Checksums

Each record carries a CRC-16 of the source block and of the target block, computed over the **whole 65536-byte block**, including any zero-extension past end of file.

This is worth stating plainly because it is the format's one real trap. The checksum routine in the reference tool takes no length argument; it loops a fixed 65536 iterations over a fixed-size buffer. A short block is not something the routine can express. So for a file whose length is not a multiple of 64 KiB, the final record's checksums cover real bytes followed by zeros — and any reader that instead checksums only the bytes that exist will compute a different value and reject a well-formed patch.

The variant is CRC-16/CCITT-FALSE. Its check value — the CRC of the ASCII digits `123456789` — is `0x29B1`.

The reference tool builds its 256-entry lookup table at startup rather than storing it, and returns each result through a signed 16-bit integer, which is a storage detail of the language it was written in and not part of the wire format. The two bytes on the wire are the unsigned value, little-endian.

## Diff semantics

Per byte at position `p` inside a record's block:

```
target[p] = source[p] ^ payload[p]
```

with `source[p]` reading as `0x00` past the end of the source file. Positions in blocks that no record covers are unchanged, which for a growing file means they are whatever the source held there, or zero past its end.

Because XOR is self-inverse, the same record applied to the target reproduces the source. The format carries no direction flag; which side a patch is being applied to is the caller's knowledge, and the two per-record checksums are what distinguish them.

## Sizes

The header's two sizes bound what the format addresses. A record offset is four bytes, so the last addressable block begins at `0xFFFF0000` and the format reaches `0x100000000` bytes — 4 GiB — which is far past any cartridge image the format was built for.

Where the target is longer than the source, the growth is carried the same way as any other difference: the blocks past the source's end have an all-zero source side, so the payload is simply the target's own bytes.

## What the format doesn't specify

The layout above is as firm as the source material allows. Several things it does not settle:

- Whether record offsets must be 64 KiB-aligned, or merely always are because the only encoder emits them that way.
- Whether records must ascend, and what a reader should do with two records naming the same offset.
- What a trailing fragment shorter than a whole record means.
- Whether the two header sizes are a requirement on the files a patch may be applied to, or a description of the files it was built from.
- What a reader should do when a record's source checksum does not match the file in hand.

Those are answered in `questions.md`.
