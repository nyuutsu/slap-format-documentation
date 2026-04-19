# BPS format findings

Source: romhacking.net archive (2024-08-01). 749 archives, ~1,507 `.bps` files.
Shortlisted 14 archives for hands-on testing. All 14 have matching ROMs sourced.

## Provenance

All structural data (sizes, CRCs, action counts, action types) from `slap info` and `slap explain`. Bulk classification used `slap info` on all 1,507 files (2 minutes, single-threaded). Action type breakdowns from `slap explain` (summary mode) on all files in staging.

## Structural summary

- **Parsed OK**: 1,495 (16 had path encoding issues on first scan, all resolved on rescan)
- **Metadata present**: 0 / 1,495. Nobody uses BPS metadata in the wild.
- **Same size**: 965
- **Grows**: 393
- **Shrinks**: 22 — rare and valuable
- **Action counts**: min 1, max 3,390,330, median 665
- **Source ROMs <=16 MB**: 1,193 (80%)

### Metadata

BPS supports an optional metadata field (varint-length-prefixed blob, spec says XML UTF-8). Across all 1,507 patches: zero use it. slap can both read and write metadata (`--metadata FILE` on create, `--extract-metadata FILE` on info), but no real-world tool populates this field. beat, Flips, and slap all write length 0.

Note for findings: slap's metadata support works correctly — verified by synthesizing a test patch with embedded XML metadata. `slap info` displays it, `slap create --metadata` embeds it.

### Action type distribution

- **Writes only**: 0 (every patch uses copies)
- **Copies only**: 9 (pure source reassembly, no TargetRead)
- **Mixed**: 1,305 (typical: 50–90% copies)

The 50% floor appears to be the natural baseline — writes and copies alternate as SourceRead spans unchanged regions and TargetRead inserts changed bytes.

### Shrink patches (22 total)

All 22 listed with action breakdown. Range: -8,192 bytes (NES) to -22,486,496 bytes (DS). Copy ratios span 50–100%.

## Shortlisted patches (14 archives)

| # | Archive | Key patch | Source → Target | Actions | Why |
|---|---------|-----------|-----------------|---------|-----|
| 1 | `[6695]Pac-Yuyu` | 82s123.bps | 32 B → 32 B | 2 | Minimum ROM size |
| 2 | `[4606]n64-ntsc` | Duke Nukem 64 (Beta).bps | 8M → 8M | 1 | No-op (source CRC == target CRC) |
| 3 | `[4861]Super Luigi Bros.` | Super Luigi Bros..bps | 49K → 41K | 50 | Shrink + all 4 action types |
| 4 | `[6524]Super Luigi Bros. v.1.3` | Super Luigi Bros. v1.3.bps | 41K → 41K | 51 | Same-size variant |
| 5 | `[2430]Super Wario Bros` | Super Wario Bros.bps | 49K → 41K | 344 | Shrink, NES |
| 6 | `[6862]DiddyKongPilot` | Diddy Kong Pilot.bps | 7.5M → 5M | 2 | Copies-only shrink |
| 7 | `[7897]doom64` | doom64.bps + doom64-stripped.bps | 15M/8M | 373K/1.2M | Shrink + growth variants |
| 8 | `[5964]SM64 Last Impact` | SM64 Last Impact V1.2.bps | 8M → 67M | 3.1M | Extreme growth (8x), stress test |
| 9 | `[4326]BOWSETTE2` | Bowsette2-Red/Yellow.bps | 262K → 262K | 2K | Same-size NES, TargetCopy |
| 10 | `[6129]SMT1-iOS-to-GBA` | SMT1_2_ios.bps | 8.8M → 8.4M | 17K | Shrink, cross-platform |
| 11 | `[6225]YOTO` | Myth of Jitra.bps | 67M → 67M | 3.3M | Highest actions, 100% rewrite |
| 12 | `[4964]MaternalBound Redux` | MaternalBound-Redux.bps | 3M → 4M | 154K | Triple-format (BPS+IPS+xdelta) |
| 13 | `[6762]wataru` | Mashin Hero Wataru.bps | 256K → 512K | 2K | Triple-format, 2x growth |
| 14 | `[65]smbtlle` | SMB꞉ Lost Levels Enhanced.bps | 512K → 4M | 86K | Unicode colon in filename, BPS+IPS |

## Notable structural specimens

### Duke Nukem 64 (Beta) — the no-op

1 action: `SourceRead 8,388,608 B`. Source CRC == target CRC. The patch literally copies the entire file unchanged. slap should probably warn about this.

### Diddy Kong Pilot — copies-only shrink

2 actions, both SourceCopy. Extracts two non-contiguous chunks from a 7.5 MB source and concatenates them into 5 MB output. No writes at all — pure reassembly.

### Super Luigi Bros — all 4 action types

50 actions including SourceRead, TargetRead, SourceCopy, and TargetCopy. The TargetCopy at action 9 references earlier output (self-referential). Also shrinks by 8K. Best single specimen for exercising the full BPS apply path.

### 82s123 — 32-byte ROM

Arcade PROM. 2 actions: 15 bytes TargetRead + 17 bytes SourceRead. Minimum-size test case.

## ROM sourcing

ROMs and patches are stored alongside this document in the research corpus.

### Matched from existing romset (6 ROMs)

| ROM | CRC | Size | Patch |
|-----|-----|------|-------|
| Super Mario 64 (USA).z64 | 3ce60709 | 8M | SM64 Last Impact |
| Doom 64 (USA) (Rev 1).z64 | 1d3a17b5 | 8M | doom64-stripped |
| Doom 64 (Europe).z64 | d985c356 | 8M | doom64 |
| EarthBound (USA).sfc | dc9bb451 | 3M | MaternalBound Redux |
| Super Mario World (USA).sfc | b19ed489 | 512K | SMB Lost Levels Enhanced |
| Shin Megami Tensei (Japan).gba | b857c3c5 | 8M | SMT1 font hack variant |

### Assembled from MAME chips (1 ROM)

**Super Mario Bros. (PC10).nes** — CRC `bad306ef`, 49,168 bytes.

This ROM does not exist in standard No-Intro NES sets. PlayChoice-10 arcade variants are catalogued separately. The patch targets a specific dump format: regular SMB1 PRG+CHR with the PC10 instruction/hint screen ROM appended, under an iNES header that doesn't set the PC10 flag.

**Source material**: MAME ROM chips downloaded as `pc_smb.zip` (the MAME PC10 SMB1 romset). Contains:
- `u1sm` — 32,768 bytes, PRG ROM
- `u2sm` — 8,192 bytes, CHR ROM  
- `u3sm` — 8,192 bytes, instruction screen ROM

**Assembly**: concatenate iNES header + PRG + CHR + inst ROM.

The correct iNES header (determined by brute-force CRC matching across flag combinations):
```
4E 45 53 1A  02 01 01 00  00 00 00 00  00 00 00 00
```
- Bytes 0–3: `NES\x1a` magic
- Byte 4: `02` — 2x 16K PRG banks (32K)
- Byte 5: `01` — 1x 8K CHR bank
- Byte 6: `01` — vertical mirroring, mapper 0
- Byte 7: `00` — no PC10 flag, no other flags
- Bytes 8–15: all zero

The inst ROM (8K) sits after the declared CHR region — it's "extra" data beyond what the iNES header accounts for. This is the standard way PC10 dumps were distributed in the GoodNES era: the header describes a normal NROM game, and the instruction ROM is silently appended. Tools that don't know about PC10 ignore the trailing 8K.

CRC verified: `bad306ef` ✓

This ROM is the source for Super Luigi Bros (.bps) and Super Wario Bros (.bps) — two of our most valuable specimens (shrink patches, all action types).

### Assembled by header downgrade: SMB2 (Rev A) and SMB (World)

Same iNES 2.0 → 1.0 fix as described above. In both cases: zero byte 7, zero bytes 8–15, same PRG/CHR data. See the dedicated section below for the systemic implications.

| ROM | Source | CRC |
|-----|--------|-----|
| Super Mario Bros. 2 (USA) (Rev A).nes | No-Intro `(Rev 1)` with header downgrade | e0ca425c |
| Super Mario Bros. (World) (iNES 1.0).nes | No-Intro `(World)` with header downgrade | 3337ec46 |

### Sourced from preservation/prototype collections

| ROM | CRC | Size | Source |
|-----|-----|------|--------|
| Duke Nukem 64 (Europe) (Beta).z64 | 4a82d036 | 8M | Pre-release dump, preservation scene |
| Diddy Kong Pilot 2001.elf | 8903cc5f | 7,509,167 | Unreleased GBA prototype. Note: `.elf` extension, odd size (not power-of-2) |
| 82s123.7f | 2fc650bd | 32 | MAME Pac-Man PROM chip |
| 82s126.4a | 3eb3a8e4 | 256 | MAME Pac-Man PROM chip |
| pacman.5e | 0c944964 | 4,096 | MAME Pac-Man ROM chip |
| pacman.5f | 958fedf9 | 4,096 | MAME Pac-Man ROM chip |
| pacman.6j | 817d94e3 | 4,096 | MAME Pac-Man ROM chip |

### Assembled by byte-order swap: OoT MQ Debug (V64)

**Legend of Zelda, The - Ocarina of Time - Master Quest (USA) (Debug) (GameCube).v64** — CRC `2ee3e247`, 67,108,864 bytes.

The patch targets the **V64 byte-order** (word-swapped) variant of the USA MQ Debug ROM. Our Z64 (big-endian) dump has CRC `62f92704`. Word-swapping every pair of bytes (the Z64→V64 conversion) produces `2ee3e247`.

This is another instance of the byte-representation problem: the game data is identical, the bytes are just interleaved differently. N64 ROMs exist in three byte orders:
- **Z64** (big-endian): starts with `80 37 12 40` — modern standard
- **V64** (word-swapped): starts with `37 80 40 12` — old Doctor V64 copier format
- **N64** (little-endian): starts with `40 12 37 80` — rare

A patch built against one byte order won't CRC-match another, even though they contain identical game data. This parallels the iNES header problem: the "envelope" changes but the content doesn't.

**Implication for slap**: N64 byte-order normalization is a potential feature. On apply, detect the ROM's byte order, convert to the patch's expected order, apply, then convert back. Unlike iNES headers (which are a fixed-size prefix), this is a whole-file transform — every byte pair gets swapped. More expensive but still O(n).

### Sourced from PCE/TG16 set: Wataru

**Mashin Eiyuuden Wataru (Japan).pce** — CRC `2f8935aa`, 262,144 bytes.

This is a **PC Engine HuCard** dump, not a Famicom game. The RHDN archive filed the patch under NES (`[6762]wataru`) and our NES set had a Famicom game with the same name (`Mashin Eiyuuden Wataru Gaiden`), which was a red herring — different platform, different game entirely. The BPS patch targets the PCE original.

The MAME `pce_tourvision` software list confirms this ROM is also used as a TourVision arcade bootleg cartridge, using the unmodified HuCard dump.

### All 14 patches now have matching ROMs.

### Assembled by header downgrade: SMB2 (Rev A)

**Super Mario Bros. 2 (USA) (Rev A).nes** — CRC `e0ca425c`, 262,160 bytes.

The No-Intro set has `Super Mario Bros. 2 (USA) (Rev 1).nes` with CRC `dea55f53`. The PRG and CHR ROM data are identical. The only difference is the iNES header:

| Byte | No-Intro (NES 2.0) | Patch expects (iNES 1.0) |
|------|-------|-------|
| 7 (flags) | `0x08` (NES 2.0 identifier) | `0x00` |
| 8–15 | NES 2.0 extended fields | all `0x00` |

Fix: zero byte 7 and bytes 8–15. This converts the NES 2.0 header back to iNES 1.0 format. The ROM data is untouched.

This reveals an important pattern: **No-Intro is actively migrating NES sets to NES 2.0 headers**. The entire back-catalogue of NES patches was built against iNES 1.0. As No-Intro updates headers, old patches become CRC-incompatible — not because the ROM data changed, but because header bytes did.

### The iNES header problem (systemic)

This is not a one-off. Every NES patch that checksums the full file (BPS, UPS) will eventually fail CRC validation against modern No-Intro dumps, because the header format is changing while the game data isn't.

What happens if you force-apply (--no-verify) with the wrong header:
- The patch's actions at offsets 0–15 will operate on the wrong input bytes
- The output header will be mangled (old header XORed/overwritten with patch's expectations)
- Everything from offset 16 onward (the actual game) will be correct
- Most emulators are tolerant of header weirdness, so it would probably run

This is the strongest argument for slap's header-offset feature. With `--header-offset 16`:
1. Strip 16-byte header from ROM
2. CRC-check the body against patch expectation (would match)
3. Apply patch to body
4. Re-attach the original header
5. Output is correct: modern NES 2.0 header + correctly patched game data

Without it, users must either find obsolete dumps or manually downgrade headers. Both are friction that slap could eliminate.

Note: this affects NES only. SNES copier headers are dead (nobody has them anymore), GB/GBA have no external headers, and N64 is headerless. But NES is a huge chunk of the romhacking corpus.

## Slap observations

- No bugs found (unlike UPS). BPS apply path appears correct.
- No-op patch (source==target CRC) could warrant a warning.
- Zero metadata in the wild — slap's metadata support is untestable against real patches.
- `slap explain` output is useful and fast (3.3M actions in 6 seconds).
- **iNES header migration is a systemic CRC-compatibility problem for NES patches**. Header-offset support would solve it cleanly.
