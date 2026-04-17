# IPS

IPS is the oldest and most ubiquitous ROM patch format in the
hobbyist romhacking world. It has no formal specification written
by its original author — the format's existence predates anyone's
attempt to document it rigorously, and what "IPS is" comes from a
handful of implementations that mostly agreed with each other, plus
later community writeups filling in the gaps.

## What it is (summary)

- **Magic**: `PATCH` (5 bytes, ASCII), no null terminator
- **Footer**: `EOF` (3 bytes, ASCII), no null terminator
- **Offsets**: 3 bytes big-endian (max addressable byte: ~16 MB)
- **Records**: either a plain "write these bytes at this offset"
  with a 2-byte big-endian length, or an RLE "write this byte N times"
  where the length field is zero and three additional bytes give the
  run length (2 bytes) and fill byte (1 byte)
- **No checksums**, no metadata, no file size field, no version field
- **Not reversible**: records contain new data, not old

For the full on-disk structure and the edge cases, see `spec.md`.

## Files

- `spec.md` — canonical IPS wire format: records, trailer, truncation
  extension, EOF ambiguity. Compiled from community sources (no
  authoritative original spec exists) and cross-referenced against
  Flips' `libips.cpp`.
- `proposal.md` — slap's resolved design decisions: IPS32 truncation
  symmetry, EBP trailing-JSON shape-check, variant ceiling rejection,
  initialFill vs. copy-on-gap performance note.
- `slap-vs-spec.md` — live discrepancy report between spec/proposal
  and what `src/Slap/IPS` actually does. The open-todo surface:
  currently tracks RLE-zero handling, createIPS truncation emission,
  sentinel-avoidance plumbing, and the createIPS target-size guard.
- `ips-audit.md` — design-context archaeology from the rewrite. The
  "why the code is shaped the way it is" reference: wire format
  reconstructions with Flips line-refs, BPS/UPS practice notes that
  IPS inherited, target-size-derivation options considered, test
  fixture inventory, open-question archive, source-confidence meta.
  Items addressed during the rewrite are annotated inline with the
  commits that resolved them.
- `quality-checked-by.md` — append-only log of Claude review snapshots
  at review time. Not a living doc; don't amend.
- `upstream/snestl12.zip` — the archive as downloaded from
  [romhacking.net utility #18](https://www.romhacking.net/utilities/18).
  SNESTool v1.2 by "THE MCA" of ELITE, dated February 12, 1996.
  DOS tool. Supports IPS creation and application among many other
  SNES ROM utilities.
- `upstream/SNESTL12.DOC` — the tool's documentation, extracted for
  convenience. Contains internal origin claims (see below). This is
  a historical source — release notes from one scene developer, not
  a technical specification.
- `upstream/SNESTL12.EXE` — the tool binary, extracted for
  convenience.
- `upstream/zerosoft-source.md` — the ZeroSoft IPS spec
  (`zerosoft.zophar.net/ips.php`), the closest thing to a base
  specification that exists. Cited by anosh.se. Has unit errors.
- `upstream/anosh-source.md` — the anosh.se writeup. Most detailed
  community source but contains internal contradictions (noted in
  the file's error appendix).
- `upstream/archiveteam-source.md` — the archiveteam wiki page.
  Brief, careful.
- `upstream/sneslab-source.md` — the sneslab wiki page. Brief,
  format-focused, documents the truncation extension.

## Origin claims

The format's origin is contested and not fully documented anywhere
authoritative. Several things are worth recording precisely because
this is where oral-tradition claims start to drift.

### The SNESTool self-claim

Inside `upstream/SNESTL12.DOC`, the "USE IPS" section says, verbatim:

> Apply the International Patch Standard on a file.
> This type was invented by DAX and ME, we got a lot
> of success with it, because we released the programs
> on ATARI/AMIGA AND PC format at the same time.

"ME" refers to the tool's author, THE MCA of ELITE. "DAX" is the
co-inventor claimed here. The expansion of the acronym in this
document is **International Patch Standard**.

This is the strongest contemporaneous origin claim I have access
to: it's a 1996 document inside a widely-distributed tool, written
by a person who was clearly present in the scene at the time,
naming a specific co-author. It is not a later community
reconstruction.

Caveats:
- The claim doesn't establish a date. SNESTool 1.2 is from February
  1996 but IPS support existed from the first version: the
  v1.0→v1.01 changelog already mentions "IPS Creating fucked up on
  big files" and fixes to "IPS 2 (cutting files)." The earliest
  SNESTool version isn't documented in the archive we have.
- "Invented" is ambiguous. It could mean "designed the format" or
  "wrote the first implementation." Formats that are "just a file
  structure" often get designed organically and attributed later.
- No independent source corroborates the DAX + MCA co-authorship.
  Later writeups generally say "origin unknown."

### Later community writeups

- [zerosoft.zophar.net/ips.php](https://zerosoft.zophar.net/ips.php)
  — the closest thing to a base spec. Describes the record format
  and RLE encoding. Has garbled unit conversions (see error notes
  in `upstream/zerosoft-source.md`). No origin claims.
- [fileformats.archiveteam.org](http://fileformats.archiveteam.org/wiki/IPS_(binary_patch_format))
  — general overview, origin not documented
- [anosh.se/ips](https://www.anosh.se/ips/) — most detailed
  community writeup, but a secondary synthesis source with
  internal contradictions (see `upstream/anosh-source.md`).
  Expands IPS as **"International Patching System"** (note:
  different from SNESTool's "International Patch Standard") and
  dates the format loosely to "the early 1990s" with exact
  origin "unknown"
- [sneslab.net/wiki/IPS_file_format](https://sneslab.net/wiki/IPS_file_format)
  — format-focused writeup, documents the truncation extension,
  no origin claims

The acronym expansion disagreement is real: SNESTool's 1996 doc
says "International Patch Standard", anosh.se says "International
Patching System". Neither source cites the other. I've used the
SNESTool expansion in `spec.md` because it's the older of the two
by about 25 years and is from someone who claims to have been
present at the format's creation, but I note both in the doc.

## Why IPS sticks around

The format is severely limited — 16 MB max, no checksums, no
reversibility, no metadata, can only overwrite bytes at given
offsets — but it's also trivially easy to implement. A parser
fits in twenty lines of any language. That simplicity plus its
early arrival made it the default for SNES ROM hacks in the 90s,
and the inertia has never fully dissipated even though BPS and
UPS are strictly better in every way that matters.

Writing past the end of the source file grows the target
implicitly (there is no mechanism to prevent it). A truncation
extension exists (see `spec.md` for details and provenance) for
shrinking the target. We have not specifically searched for
real-world IPS patches that use truncation; none has turned up in
our test corpus, but we haven't looked and IPS patches are
abundant, so this tells us nothing. SNESTool's 1996 DOC describes
"IPS 2" for cutting files (to "Kill them fucking advertisement
Intro's"), so truncating patches almost certainly existed in the
wild. ROM hacks, which make up the bulk of surviving IPS patches,
almost universally add content.

Modern SNES ROM hacks still frequently ship as IPS. Large game
engines (GBA, N64) that can't fit within 16 MB have mostly moved
on. The format remains in the wild mostly as a historical artifact
and as the "lowest common denominator" option.
