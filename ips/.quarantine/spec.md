# IPS — Format Specification

IPS has no formal specification by its original author. What "the
format" is comes entirely from implementation consensus: tools that
create and apply IPS patches agree on the wire layout, and community
writeups (ZeroSoft, anosh.se, sneslab.net, the archiveteam wiki)
document what the implementations do. None of these sources are
authoritative in the way a formal spec would be; they are attempts
to describe observed behavior, each with their own errors and gaps.

This document attempts to distill that consensus, cross-referenced
against SNESTool's 1996 documentation (a historical source, not a
technical reference) and the source code of Flips (Alcaro, available
locally in `tools/flips/libips.cpp`). Where sources agree, we state
the consensus. Where they disagree or go silent, we note the gap.
Where a claim originates from a single source, we name that source.
The goal is to pin down what an implementation needs to get right to
correctly apply and create IPS patches, and to be honest about what
remains uncertain.

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

The ZeroSoft spec says RLE_Size must be "Any nonzero value";
anosh.se repeats this (likely sourced from ZeroSoft). No other
source (sneslab, archiveteam) addresses the question. A run
length of zero would mean "write this byte zero times" — a no-op.

Implementations disagree on what to do with it:

- **Flips** rejects the entire patch (`libips.cpp:59`). Alcaro's
  comment: "don't know what this means (65536 bytes? something
  else?), better reject it until I find out." The concern is
  that zero might be a wraparound encoding for 65536, but no
  source documents such behavior and nothing else in the format
  uses uint16 wraparound.
- **RomPatcher.js** does not validate; on apply, the write loop
  runs zero iterations — silent no-op.
- **lua-ips** (`thenumbernine/lua-ips`) does not validate;
  `value:rep(0)` produces an empty string — silent no-op.

No encoder produces zero-length RLE records (Flips has a minimum
threshold; RomPatcher.js requires `length > 2`). In practice the
question only arises for hand-crafted or corrupted patches.

## The EOF ambiguity

This is the most famous footgun in the format. The footer is the
literal ASCII string `EOF` (bytes `0x45 0x4F 0x46`). A parser reads
records until it sees this three-byte sequence.

**Problem**: `0x45 0x4F 0x46` is also a valid 3-byte offset —
decimal `4,542,278`, about 4.3 MiB into a file. If a record
needs to start writing at exactly that offset, the record's first
three bytes are indistinguishable from the footer.

**What happens in practice**: a parser reads 3 bytes at a time
and interprets them as an offset. If those 3 bytes are
`45 4F 46`, the parser concludes the patch is over — it has no
way to look further and determine whether this was really the
footer or a record offset that happened to collide. Flips's
parser (`libips.cpp:53`) loops `while (offset != 0x454F46)`: an
unconditional hard stop, no lookahead. RomPatcher.js does the
same (`if(offset===0x454f46)`). This behavior has not been
surveyed across all parsers, but the archiveteam wiki warns that
parsers "may" misinterpret this offset as EOF, which implies it
is a known widespread issue.

**Scope of the problem**: the collision only matters when a
record's *starting offset* is exactly `0x454F46`. Records that
start at an earlier offset and write *through* byte 4,542,278
are unaffected — the 3-byte ambiguity is only in the offset
field at the start of each record, not in the payload. Any file
whose changed regions do not include a record boundary at exactly
that byte is unaffected, regardless of file size.

**How encoders avoid it**: the archiveteam wiki recommends
shifting the record back by one byte and including the preceding
byte in the payload. Flips implements exactly this
(`libips.cpp:247`):
```c
if (offset == 0x454F46) { offset--; thislen++; }
```
This requires the encoder to have the source bytes available to
look up the preceding byte. Without a source (e.g. direct
format-to-format conversion with no ROM), the collision is
genuinely unresolvable.

There is no clean parser-side solution within the format.

## Addressable range

This is arithmetic on the field widths, not a value that appears
anywhere in the format.

The offset field is 3 bytes: `0x000000`–`0xFFFFFF` (0–16,777,215).
The length field is 2 bytes: 1–65,535 for a literal record, same
for RLE. A record at the maximum offset with maximum payload writes
its last byte at position `0xFFFFFF + 0xFFFF - 1` = `0x100FFFD`
(16,842,749).

Flips refuses to create IPS patches for targets larger than
16,777,216 bytes (`libips.cpp:202`). This is a practical ceiling
imposed by the most widely-used creator, consistent with the
field widths but not itself a format-level constraint.

## Truncate extension

Some parsers recognize an optional 3-byte big-endian value after
the `EOF` footer that specifies a target file size. If present,
the target is truncated to that many bytes after records are
applied.

**Sources.** The sneslab wiki describes this extension and
attributes it to "a later version of SNESTool and some other IPS
patchers (For example: Lunar IPS, NINJA)." The archiveteam wiki
mentions it without attribution: "the end-of-file marker may be
followed by a three-byte length to which the resulting file should
be truncated. Not every patching program will implement this
extension, however." The anosh.se writeup does not mention it. The
ZeroSoft spec does not mention it.

**Flips** (verified in `tools/flips/libips.cpp`): on create, emits
`write24(targetlen)` when `sourcelen > targetlen` (line 344). On
parse, reads the trailer only when exactly 3 bytes remain after
`EOF` (line 77: `if (patchat+3 == patchend)`); any other amount
of trailing data is rejected as invalid (line 87).

**SNESTool and "IPS 2."** SNESTool v1.2's documentation (1996)
describes a concept it calls "IPS 2" for cutting files. The
v1.0-to-v1.01 changelog says: "IPS 2 ( cutting files ) Create and
Use of a IPS 2 file work ok now." The "Use IPS" section says: "If
IPS2 is detected, it will write a temporary file called TEMP.SMC
then renames it back to the original. IPS2 files are ment to 'Cut'
a file." Whether SNESTool's "IPS 2" is the same mechanism as the
3-byte truncation trailer described above, or something else (a
separate file variant? a different wire encoding?), is unknown —
the DOC does not describe the wire format of either IPS or "IPS 2."
The phrasing "If IPS2 is detected" implies the tool distinguishes
the two at parse time, but by what means is not documented. Flips
has no concept of "IPS 2."

The existence of "IPS 2" as a distinct concept for cutting suggests
that standard IPS, in its original author's view, did not support
shrinking — consistent with the truncation trailer being a later
community extension rather than part of the original design. But
this is inference from a 1996 release-notes file, not established
fact.

Not all parsers recognize the truncation trailer. Treat as
optional.

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
bytes is long enough to be smaller as RLE than as literal. As a
standalone record, RLE breaks even with literal at a run of 3
bytes (both 8 bytes on the wire). In practice, replacing a run
inside a larger literal requires splitting it, adding an extra
5-byte record header per split point — the effective break-even
is ~8 bytes at a region boundary or ~13 bytes mid-region.

## Format-legal but structurally malformed

The format lacks structural constraints that a well-formed patch
would honor but that a hand-crafted or corrupted patch could
violate:

- **Records whose offset is `0x454F46`**: legal in principle,
  unparseable in practice. See the EOF ambiguity section.
- **Records whose offset + length exceeds `0x100FFFE`**: a record
  at a legal offset whose payload extends past the format's
  arithmetic ceiling (`0xFFFFFF + 0xFFFF`). No implementation
  can produce this, but a hand-crafted file could contain it.
  Behavior is implementation-defined.
- **Overlapping records**: the format doesn't forbid two records
  writing to the same byte. Last-write-wins in practice.
- **RLE records with run length 0**: semantically a no-op. ZeroSoft
  says "Any nonzero value"; Flips rejects the entire patch; other
  implementations silently no-op. See the RLE record section.
- **Missing EOF footer**: the format has no length field, so a
  truncated IPS file is indistinguishable from one missing its
  footer. Parsers either error or terminate at end-of-file.
- **Data after the EOF footer that isn't a truncate trailer**:
  unspecified. Some parsers error; some ignore.

## Limitations baked into the format

- **~16 MB addressable range** (3-byte offset; see "Addressable
  range" above)
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
- **Can only overwrite** at given offsets — no insertion or
  deletion of bytes. Writing past the source EOF grows the target
  implicitly. A truncation extension exists for shrinking (see
  "Truncate extension" above)

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
