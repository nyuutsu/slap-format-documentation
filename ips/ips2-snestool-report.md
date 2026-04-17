# IPS2: evidence report

**Subject:** The "IPS2" patch format, its origin in SNESTool, and the claim that
IPS itself originated in the early-1990s Atari ST cracking scene.

**Primary artifact examined:** `snestl12.zip` (SNESTool v1.2), containing
`SNESTL12.EXE` (MS-DOS MZ executable, 23,978 bytes, dated 1996-02-12) and
`SNESTL12.DOC` (11,185 bytes, dated 1996-01-01). The zip is mirrored on
scene.org at `files.scene.org/browse/resources/tools/snes/snestl12.zip`.

**Executive summary:** "IPS2" is not an apocryphal format. It is a real,
documented extension to the IPS patch format, introduced by SNESTool (authored
by MCA of Elite/Elitendo) no later than v1.01, for the purpose of truncating
the patched file. Its semantics are preserved today as the IPS "truncation
extension." The SNESTool readme also contains a first-person claim that the
IPS format itself was co-invented by its author; this predates the
documentation on archiveteam.org and sneslab.net, both of which list the
original author as unknown.

---

## 1. The readme evidence

`SNESTL12.DOC`, authored by "The M.C.A. / ELITE" and co-credited to "Sledge /
Hotline," contains the following unambiguous references to IPS2 (transcribed
from the v1.0-to-v1.01 changelog and the "Use IPS" function description
respectively):

> "IPS 2 ( cutting files ) Create and Use of a IPS 2 file work ok now."

> "If IPS2 is detected, it will write a temporary file called TEMP.SMC then
> renames it back to the original. IPS2 files are ment to 'Cut' a file. Kill
> them fucking advertisement Intro's..."

The second quote fixes both the purpose (file truncation) and the motivating
use case (removing cracktro intros that had been appended to the end of
pirated SNES ROMs by rival crack groups — Anthrox, Sonic, Swat, Napalm, etc.,
all of whom appear in the readme's greetz list).

The "Use IPS" section also contains the following attribution for the IPS
format as a whole, quoted verbatim:

> "This type was invented by DAX and ME, we got a lot of success with it,
> because we released the programs on ATARI/AMIGA AND PC format at the same
> time."

"ME" here is MCA (The M.C.A., a.k.a. The Magician). DAX is unidentified at
time of writing.

## 2. Binary verification

The readme describes a behaviour. The EXE is the ground truth. Disassembly
of `SNESTL12.EXE` (16-bit real-mode x86, MASM-compiled per the doc)
confirms the readme and pins down the exact on-disk format.

### 2.1 Relevant string table entries

Found by direct byte search in `SNESTL12.EXE`:

| File offset | String |
|-|-|
| 0x3e8b | `PATCHEOF` (stored as a single literal, split at write time) |
| 0x4149 | `IPS patched ok.` |
| 0x410c | `This is no correct IPS file !` |
| 0x412a | `No File Cut, IPS2 size error !` |
| 0x4386 | `IPS file created.` |
| 0x4416 | `TEMP.SMC` |

DS at startup is `0x36F` (set at image offset `0x0000` by `MOV DX, 0x36F;
MOV DS, DX`), so the DS-relative offset of each string is
`file_offset - 0x200 - 0x36F0`. Cross-references in the code use DS-relative
`LEA SI` instructions against those values, which is how the routines below
were located.

### 2.2 Use IPS routine (apply path)

Located at image offset `0xA50` through `0xBE9`. Reconstructed behaviour:

1. **Header check** (`0xAD2`, `0xADB`): compares the first 4 bytes of the
   patch file against `'PA'` then `'TC'`, as two 16-bit CMPs. The `'H'` of
   `PATCH` is **not verified**. A patch beginning `PATC?` with any fifth byte
   will be accepted. This is a minor quirk of this particular implementation;
   it does not appear in later IPS tools.

2. **Record loop** (`0xAE5`–`0xB45`): reads a 3-byte big-endian offset; if it
   equals ASCII `EOF` (`45 4F 46`), branches to the EOF handler. Otherwise
   reads a 2-byte big-endian length. Length 0 ⇒ RLE hunk (2-byte BE run size,
   1-byte value). Length > 0 ⇒ literal hunk. Identical to the canonical IPS
   format documented at
   [fileformats.archiveteam.org/wiki/IPS_(binary_patch_format)](http://fileformats.archiveteam.org/wiki/IPS_(binary_patch_format)).

3. **EOF branch, IPS2 detection** (`0xB47`): attempts to read 3 more bytes
   past `EOF` into the record buffer (`CALL 0xC5C` with `CX=3`). If fewer than
   3 bytes are returned, the patch is plain IPS and processing ends. If
   exactly 3 are returned, the patch is IPS2 and the 3 bytes are interpreted
   as a big-endian 24-bit unsigned integer (`LODSB; MOV DL, AL; XOR DH, DH;
   LODSW; XCHG AL, AH; MOV CX, AX` gives `DX:CX` = 24-bit BE value in
   `DL:CH:CL`).

4. **Sanity check** (`0xB6F`–`0xB77`): `AND CX, 0xFFF; CMP CX, 0x200; JZ
   continue`. The low 12 bits of the size must equal `0x200`. This is a
   hard-coded SMC-header assumption: every valid headered SMC ROM has size
   `(2^N KB) + 0x200` for some N, so `size & 0xFFF == 0x200` always holds for
   legitimate SMC files. If the check fails, the error string "No File Cut,
   IPS2 size error !" is printed and the patch is rejected.

5. **Truncation** (`0xB83`–`0xBE9`):
   - `SUB AX, 0x200` strips the copier header from the size
   - `DIV 0x2000` computes the number of 8 KB blocks to keep
   - Opens the (already-patched) ROM and creates `TEMP.SMC`
   - Copies the 0x200-byte SMC header
   - Copies N × 0x2000 bytes in an 8 KB block loop
   - Closes both files, deletes the original (INT 21h AH=41), renames
     `TEMP.SMC` to the original name (INT 21h AH=56)

### 2.3 Create IPS routine (write path)

Located at image offset `0xD30` through `0xE40`. Reconstructed behaviour:

1. Opens both input files, writes `PATCH` header (`0xD56`: `LEA DX, [0x59B];
   MOV CX, 5; CALL write`, where `0x59B` is the DS-relative offset of
   `PATCHEOF`).

2. Compares files in 0x8000-byte blocks and emits standard IPS records.
   A flag at `[0x247C]` tracks whether the modified file was shorter than
   the original.

3. Writes `EOF` footer (`0xDE5`: `CALL 0xC8E` which points into the same
   `PATCHEOF` literal).

4. **Trailer emission** (`0xDEB`–`0xE3F`): if the "modified was shorter"
   flag is nonzero, assembles a 3-byte buffer at DS-relative offset `0x5A0`
   with stores `MOV [BX+2], AL; MOV [BX+1], AH; MOV [BX], DL` (unambiguously
   big-endian), then writes those 3 bytes to the patch file with `CX=3`.

This confirms the format is symmetric: SNESTool can both read and write IPS2,
and the writer only emits the trailer when truncation is actually needed.

## 3. IPS2 format specification

As implemented by SNESTool v1.2 (and, per the changelog, v1.01 onward):

```
+------------------+
| 'PATCH'  (5 B)   |    Header. First 4 bytes are checked; 5th is not.
+------------------+
| record 1         |    Standard IPS records. Either:
| record 2         |      - literal: 3-byte BE offset, 2-byte BE length, N bytes
| ...              |      - RLE:     3-byte BE offset, 2-byte BE length = 0,
| record K         |                 2-byte BE run-size, 1-byte value
+------------------+
| 'EOF'    (3 B)   |    Sentinel. If followed by 3 more bytes, those are:
+------------------+
| size[0..2]  (3 B)|    Final file size, 24-bit big-endian, *including*
+------------------+    the 0x200-byte SMC copier header.
                        Constraint: (size & 0xFFF) == 0x200.
                        Absence of these 3 bytes = plain IPS, no truncation.
```

The modern "IPS truncation extension" described by
[sneslab.net/wiki/IPS_file_format](https://sneslab.net/wiki/IPS_file_format)
is the same trailer without the SMC-header assumption or the 8KB-block
constraint. Round-trip between SNESTool IPS2 and generic truncated-IPS
therefore works only when the target file happens to satisfy
`(size & 0xFFF) == 0x200`.

**Parser implications:** a reader that supports the modern truncation
extension correctly reads SNESTool IPS2 without modification. A writer
that wants to produce patches SNESTool itself will accept must validate
the size constraint before emitting the trailer.

## 4. Provenance of the format

### 4.1 SNESTool's position in the SNES cracking scene

"The M.C.A." is The Magician, a.k.a. MCA, lead cracker of the Dutch/Atari ST
crew Elite and its SNES sub-crew Elitendo.

- Demozoo entry for Elite:
  [demozoo.org/groups/15355](https://demozoo.org/groups/15355/) — "founded
  in 1990 as a cracking group on Atari ST/E... M.C.A. did most of the
  professional cracks like Calamus (DMC), and Cubase."

- Demozoo entry for Elitendo:
  [demozoo.org/groups/18735](https://demozoo.org/groups/18735/) — "SNES
  cracking and training group coming from the Atari ST/E cracking scene.
  It was the Nintendo-spin-off from Elite."

- Demozoo entry for SNESTool v1.2:
  [demozoo.org/productions/151956](https://demozoo.org/productions/151956/)
  — credits "Hotline + Menacing Cracking Alliance / Elite".

- Scene history essay "Elitendo" at hotline9009.com — states that an SNES
  Assembler and Snes-Tool were released on the Atari ST, and that Snes-Tool
  was later ported/rewritten for PC; the PC version "has seen 5 more
  updates," matching the v1.00 → v1.01 → v1.02 → v1.03 → v1.04 → v1.10 →
  v1.11 → v1.2 chain visible in the readme changelog.

### 4.2 The IPS attribution claim

The sentence "This type was invented by DAX and ME" in the readme is a
first-person claim by MCA dated to the mid-1990s at latest. It predates:

- The ArchiveTeam file format wiki's entry on IPS, which states the original
  author is unknown:
  [fileformats.archiveteam.org/wiki/IPS_(binary_patch_format)](http://fileformats.archiveteam.org/wiki/IPS_(binary_patch_format))
- The SnesLab wiki's entry, which simply states IPS patches are widely used
  without naming an origin: [sneslab.net/wiki/IPS_patch](https://sneslab.net/wiki/IPS_patch)
- The SnesLab IPS *file format* wiki, which only attributes the truncation
  extension to "a later version of SNESTool and some other IPS patchers"
  without identifying which version or who wrote it:
  [sneslab.net/wiki/IPS_file_format](https://sneslab.net/wiki/IPS_file_format)

The "we released the programs on ATARI/AMIGA AND PC format at the same time"
remark is consistent with MCA's known biography: ST-based cracking
→ Elite's cross-platform distribution pipeline → PC port of SNESTool.

This is a primary-source claim from a credible and identifiable author.
It is not proof — cracking-scene self-credit is not automatically reliable —
but it is the best-attested origin claim currently extant, and no
contradicting claim of authorship has been found in any secondary source.

## 5. Downstream lineage

FuSoYa's Lunar IPS page at
[fusoya.eludevisibility.org/lips/](https://fusoya.eludevisibility.org/lips/)
describes Lunar IPS explicitly as "intended as an easy to use, lightweight
IPS patch utility for windows to replace the SNESTool DOS program." The DOS
program in question is this one. The canonical lineage is therefore:

```
    SNESTool (MCA/Elite, 1994–1996, DOS)
         ↓
    Lunar IPS / LIPS (FuSoYa, 2000s, Windows)
         ↓
    Floating IPS / Flips (Alcaro, 2010s, cross-platform)
         ↓
    Web-based patchers (RomPatcher.js, RomPatcher.app, ipstool, etc.)
```

The SMC header conventions that continue to complicate SNES ROM hacking to
this day — the headered-vs-headerless split, the 0x200-byte copier header —
are baked into IPS2's size-check heuristic because they were baked into the
1990s copier hardware SNESTool was built to talk to.

## 6. Mirrors and primary sources

For independent verification of the tool and readme:

- scene.org file archive:
  [files.scene.org/browse/resources/tools/snes/](https://files.scene.org/browse/resources/tools/snes/)
  — contains `snestl12.zip`, `sntl_101.lzh`, `sntl_102.lzh`, and earlier
  versions.

- Internet Archive: [archive.org/details/SNESTool](https://archive.org/details/SNESTool)
  and [archive.org/details/snestoolv110](https://archive.org/details/snestoolv110).

- snes-projects.de mirror of the v1.2 doc:
  [snes-projects.de/filebase/index.php?file/166-snestool-v1-20/](https://snes-projects.de/filebase/index.php?file/166-snestool-v1-20/)

- Romhacking.net's utilities page for SNESTool:
  [romhacking.net/utilities/18/](https://www.romhacking.net/utilities/18/)

## 7. Summary

| Claim | Status |
|-|-|
| "IPS2" is a real format | Confirmed. Readme describes it; disassembly verifies exact byte layout in both reader and writer paths. |
| IPS2 is a truncation extension to IPS | Confirmed. Same `PATCH…EOF` base format, followed by optional 3-byte big-endian size. |
| Modern IPS truncation extension derives from SNESTool's IPS2 | Strongly supported. SnesLab credits "a later version of SNESTool"; SNESTL12.EXE implements it; Lunar IPS and descendants replaced SNESTool. |
| SNESTool was a significant scene tool | Confirmed. Documented on Demozoo, scene.org, Internet Archive, Romhacking.net. Authored by MCA of Elite/Elitendo. |
| IPS itself was co-invented by MCA and "DAX" on Atari ST | First-person claim in the readme. Not independently corroborated, but not contradicted either. Currently the best-attested origin statement. |
