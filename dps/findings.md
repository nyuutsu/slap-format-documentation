DPS format findings from romhacking.net archive (2024-08-01)
=============================================================

Source: 1 archive, 1 .dps file. The only DPS in the entire RHDN dump.

PROVENANCE
----------
Structural analysis from: slap info/explain, manual disassembly of
dpspatcher.exe (reference implementation), and direct byte inspection.

FORMAT ORIGIN
-------------
DPS was created by Marc de Falco
("deufeufeu" on GBAtemp) for NDS ROM translations. NDS ROMs have
an internal filesystem; modifying files reshuffles the entire image,
making byte-offset formats like IPS useless. DPS solves this by
describing how to rebuild the output from source ROM pieces + new
data. Patches are large because they carry most of the new filesystem.

Known DPS translations:
  - Jump Ultimate Stars (the specimen we have)
  - Ys DS
  - Daigasso! Band Brothers
All by deufeufeu. The format's website (deufeufeu.free.fr) is dead.

Published spec: UniPatcher wiki, reverse-engineered from dps.c source.
Note: the wiki has record types 0 and 1 SWAPPED relative to the
actual dpspatcher.exe behavior (slap's comment on Parse.hs:114
acknowledges this).

REFERENCE IMPLEMENTATION (dpspatcher.exe)
-----------------------------------------
27 KB, PE32, compiled with MinGW GCC 3.4.5. Source files: dpspatcher.c
and dps.c. Disassembly reveals the apply logic:

  Header (198 bytes):
    3 × 64-byte null-padded text fields (name, author, version)
    1 byte: stability flag
    1 byte: format version
    4 bytes: LE uint32 original ROM size

  Records (until EOF):
    1 byte: mode
      mode 0 → CopyFromROM: 4B output offset, 4B source offset, 4B length
      mode 1 → EnclosedData: 4B output offset, 4B length, [length] bytes
    No other modes defined. Spec does not address unknown modes.

  Validation: size check only (ftell == orig_size). No checksums, no
  CRCs, no hashes. Feed it the wrong ROM of the right size → silent
  corruption.

  Post-processing: NONE. dpspatcher.exe does not recalculate NDS
  header fields. The header is written by a record like any other.
  (Earlier hypothesis about NDS fixup was disproved by Wine testing.)

SLAP BUGS FOUND
---------------
1. Wildcard record type (Parse.hs:120):
   slap uses `_ ->` for the EnclosedData case, silently treating any
   non-zero mode byte as enclosed data. The spec defines exactly two
   modes (0 and 1). An unknown mode should be a parse error.

   Current:  0 -> CopyFromROM; _ -> EnclosedData
   Correct:  0 -> CopyFromROM; 1 -> EnclosedData; _ -> error

2. Output initialization (Apply.hs:24):
   slap starts with `let base = source` — a copy of the entire source
   ROM. dpspatcher.exe starts from a fresh/zeroed output file.
   CONFIRMED: 85,757 bytes differ between slap and dpspatcher output.
   Every single diff: slap has the original ROM byte, dpspatcher has
   0x00. Zero other differences. These are the 393 gap regions (totaling
   116,239 bytes; 85,757 non-zero original bytes within them).

   Fix: start from a zeroed buffer, not a source copy. Size the buffer
   to the furthest record write offset.

3. Output size:
   slap produces output == input size (67,108,864). dpspatcher.exe
   produces output == furthest record write (65,942,936). The output
   should be truncated/sized to the actual written extent, not padded
   to source size.

APPLY TEST RESULTS
------------------
  Patch: jus-eng-dev03_05_08.dps (401 records, 33 data + 368 copies)
  ROM:   Jump Ultimate Stars (Japan).nds (67,108,864 bytes)

  slap apply:       67,108,864 bytes  CRC=723EB113
  dpspatcher (Wine): 65,942,936 bytes  CRC=4CE9FB94  ← AUTHORITATIVE
  GGn reference:    65,943,448 bytes  CRC=8640B4D3  ← different patch version

  slap vs dpspatcher: 85,757 differing bytes.
    ALL diffs: slap has original ROM byte, dpspatcher has 0x00.
    Zero other differences. Cause: slap pre-fills with source, should
    use zeroed buffer.

  slap vs dpspatcher header (0x000-0x1FF): ZERO diffs.
    dpspatcher does NOT do NDS header fixup. The patch's header record
    (record 400) contains a stale ROM-size field (884,552) and that's
    what both tools write. Earlier hypothesis about NDS post-processing
    was wrong.

  GGn reference: NOT produced by this patch. Contains NDS filesystem
  data in gap regions that exists in neither the original nor patch.
  Likely from a newer translation version (patch says v0.8, GGn says
  v1.0).

STALE HEADER MYSTERY (SOLVED)
-----------------------------
The patch's header record (record 400) writes ROM-size = 884,552 at
NDS header offset 0x80. This is the end offset of record 398
(0xD7F48). The patch creation tool likely snapshotted the header
partway through building the output, capturing an intermediate write
cursor as the ROM-size. A bug in the creation tool, not in the format.

Confirmed on hardware: all three patched ROMs (dpspatcher output,
GGn reference, mediafire "fixed") boot on a DSPico flash cart despite
having different (mostly wrong) ROM-size fields. The NDS firmware
does not check this field for booting. The dpspatcher.exe output
does NOT do NDS header fixup — it applies the patch's stale header
as-is.

ADDITIONAL PATCHED ROMS
-----------------------
  jus-from-dpspatcher.nds   65,942,936 bytes  header ROM-size: 884,552     (WRONG)
  jus-from-gazellegames.nds 65,943,448 bytes  header ROM-size: 65,943,448  (correct)
  jus-from-mediafire.nds    67,108,864 bytes  header ROM-size: 66,701,016  (WRONG)

  Three different outputs from three different sources. Only the
  dpspatcher output has known provenance (this patch + this tool).
  The GGn version is from a different/later patch (v1.0 vs v0.8).
  The mediafire version has v030508 in its NDS title field (same date
  as the patch filename) but differs from both others.

  dpspatcher.exe is apply-only. No creation logic in the binary.
  The patch was created by a separate tool that deufeufeu did not
  distribute.

OPEN QUESTIONS
--------------
- The format stores orig_size as an input validation field (expected
  source ROM size). The output size is implicit — determined by the
  furthest record write. slap should size output to furthest write,
  matching dpspatcher.exe behavior.

FORMAT PROPERTIES
-----------------
  Record types:    2 (copy from ROM, enclosed data)
  Integrity:       original ROM size only, no checksums
  Metadata:        name, author, version (64 bytes each), stability flag
  Addressing:      32-bit LE offsets (4 GB max)
  Compression:     none
  Endianness:      little-endian
  Bidirectional:   no
  Platform-aware:  de facto NDS-only (but format is generic; no NDS fixup)
