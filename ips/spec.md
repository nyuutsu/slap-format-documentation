# IPS — Format Specification

IPS has no formal specification by its original author. This document
is compiled from multiple community sources (anosh.se, sneslab.net,
the archive team file formats wiki) cross-referenced against
SNESTool's internal documentation and the behavior of long-standing
implementations (Lunar IPS, Flips, NINJA).

Where sources disagree, we note the disagreement. Where there is no
authoritative answer, we describe the de facto behavior followed by
the majority of patchers.

## Identity

- **Acronym**: IPS
- **Expansion**: "International Patch Standard" per SNESTool v1.2's
  documentation (1996). "International Patching System" per later
  community writeups (anosh.se). Both are in use.
- **Earliest known tool**: SNESTool by THE MCA of ELITE. Version 1.2
  is from February 1996; earlier versions existed. SNESTool 1.2's
  own documentation claims the format was "invented by DAX and ME"
  (MCA), but no independent corroboration exists.
- **Era**: early 1990s, exact origin undocumented
- **Design context**: SNES ROM hacking scene, backup copier tools

## Overall structure

```
[ "PATCH"     ] 5 bytes, ASCII, no null terminator
[ record 0   ]
[ record 1   ]
[ ...        ]
[ record N-1 ]
[ "EOF"       ] 3 bytes, ASCII, no null terminator
[ truncate? ] optional 3-byte truncation offset (extension)
```

## Records

Each record begins with a 5-byte prefix:

| Size | Field        | Notes                                         |
|------|--------------|-----------------------------------------------|
| 3    | Offset       | big-endian, byte offset in the target file    |
| 2    | Length       | big-endian. 0 = RLE record. non-zero = literal |

### Literal record (length 1-65535)

```
[ offset: 3 ] [ length: 2, non-zero ] [ data: length ]
```

The next `length` bytes are the replacement bytes to write at the
given offset. Total record size: `5 + length` bytes.

### RLE record (length == 0)

```
[ offset: 3 ] [ 0x00 0x00 ] [ run length: 2 ] [ fill byte: 1 ]
```

A length of zero marks the record as RLE. The next 2 bytes give the
big-endian run length, and the byte after that is the fill value.
Writes `fill_byte` `run_length` times starting at `offset`. Total
record size: 8 bytes.

Run length should be non-zero; behavior with run length 0 is not
specified and varies by implementation.

## The EOF ambiguity

This is the most famous footgun in the format. The footer is the
literal ASCII string `EOF` (bytes `0x45 0x4F 0x46`). A parser reads
records until it sees this three-byte sequence.

**Problem**: `0x45 0x4F 0x46` is also a valid 3-byte offset —
decimal `4,542,278`. If a record legitimately needs to write bytes
starting at offset `0x454F46`, the first three bytes of that record
look like the footer, and many parsers terminate early.

**Consequence**: the effective maximum addressable offset is
`2^24 - 2` (one byte below the full 24-bit range), not `2^24 - 1`,
because parsers must distinguish "real EOF" from "offset that
happens to spell EOF".

**How implementations handle it**: most parsers just treat
`0x454F46` as the EOF and refuse to parse records at that offset.
Strict-mode patchers may check whether more data follows and
continue if so. There is no clean solution within the format.

## File size limitation

The 3-byte offset field caps the addressable file space at
**16,777,216 bytes (16 MB)** minus the EOF collision byte, giving a
practical limit of **16,777,215 bytes**. Records whose offsets fall
within this range can write up to 65,535 bytes (maximum literal
length) or `2^16 - 1` bytes (maximum RLE run length).

IPS cannot address or modify data beyond byte 16,777,215. Large
ROMs (N64, GBA Pokemon Emerald, GameCube, etc.) cannot be patched
with IPS.

## Truncate extension

Some parsers recognize an optional 3-byte trailer after `EOF` that
specifies a truncation offset. If present, the target file is
truncated to that many bytes after records are applied. This is a
late extension (SNESTool in a later version, plus Lunar IPS,
Flips, NINJA). Not all parsers recognize it; parsers that don't
will silently ignore the trailing bytes or error on them.

The sneslab.net writeup describes this extension. The anosh.se
writeup does not. Treat as optional.

## No checksums, no metadata, no verification

The format has no provisions for:

- Patch integrity checking (no CRC, hash, or length field)
- Source ROM identification (no cart ID, no source CRC)
- Target ROM verification (no output CRC)
- Description, author, or any other metadata
- File size in the header
- Version or extension mechanism

Applying an IPS patch to the wrong source ROM silently produces
garbage. This is by design, or at least by original simplicity.

## Reversibility

**None.** IPS is byte replacement. Records contain the new bytes
only; the original bytes are gone after apply. Applying an IPS
patch to an already-patched file produces neither the original nor
the target — just corrupt data.

## Diff semantics

There are none formally. An IPS patch is just "a list of byte
writes at specific offsets, plus a footer." Implementations are
free to compute these however they like. The format doesn't care
how the records were generated.

In practice, creators walk the source and target file in parallel,
emit a literal record when they diverge, and close it when they
reconverge. RLE records may be emitted when a run of identical
bytes is long enough to be smaller as RLE than as literal. The
break-even point for RLE is a run of ~8 bytes (8-byte RLE record
vs. ~5+N literal overhead).

## Format-legal but structurally malformed

The format lacks structural constraints that a well-formed patch
would honor but that a hand-crafted or corrupted patch could
violate:

- **Records whose offset is `0x454F46`**: legal in principle,
  unparseable by most implementations. See the EOF ambiguity
  section.
- **Records whose offset + length exceeds 16 MB**: format can
  represent the offset but not the full range; behavior on apply
  is implementation-defined (typically silent write-past-EOF).
- **Overlapping records**: the format doesn't forbid two records
  writing to the same byte. Last-write-wins in practice.
- **RLE records with run length 0**: behavior unspecified. Most
  parsers treat as a no-op.
- **Missing EOF footer**: the format has no length field, so a
  truncated IPS file is indistinguishable from one missing its
  footer. Parsers either error or terminate at end-of-file.
- **Data after the EOF footer that isn't a truncate trailer**:
  unspecified. Some parsers error; some ignore.

## Limitations baked into the format

- **16 MB addressable space** (3-byte big-endian offset)
- **65,535 bytes per literal record** (2-byte length)
- **65,535 bytes per RLE record**
- **No checksums**, no integrity check
- **No source verification**, no target verification
- **No reversibility**
- **No metadata** (description, author, version, date, etc.)
- **No file size field**
- **No extension mechanism**
- **EOF ambiguity** at offset `0x454F46` is unavoidable without
  breaking the format
- **Can only overwrite**, cannot insert, delete, or resize the
  target (except via the truncate extension)

## Cross-reference to implementations

Tools known to handle IPS:

- **SNESTool** (THE MCA / ELITE, 1996) — earliest surviving tool we
  have a copy of. `upstream/SNESTL12.EXE` in this folder.
- **Lunar IPS (LIPS)** (FuSoYa) — widely-used Windows IPS tool,
  supports truncate extension
- **Flips** (Alcaro) — modern IPS/BPS patcher with good error
  handling
- **NINJA** (Derrick Sobodash) — supports IPS alongside NINJA 1.x
  and 2.x
- **Romhacking.net patcher**, **beat**, **nups**, **sips**, many
  others — IPS is the most widely-supported format in the scene

slap implements IPS in `src/Slap/IPS/`.
