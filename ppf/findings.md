# PPF format findings

Source: 250 archives from romhacking.net (587 PPF files) + LibCrypt PPF Patches collection (339 files).

## PPF version distribution

### From RHDN corpus (250 archives, 587 PPF files)

| Version | Count | Notes |
|---------|------:|-------|
| PPF3 | 440 | The bulk |
| PPF4 | 131 | Mostly from one Grandia 2 bugfix archive (124 files) — about 8 archives total use PPF4 |
| PPF2 | 16 | Vietnamese translations, Chinese RE3, Spanish Vagrant Story, FF6 PSX Spanish (mojibake-named) |
| PPF1 | 0 | None in RHDN |
| Extract fail | 0 | (2 mojibake-named files recovered via direct unzip — both PPF2) |

### From LibCrypt PPF Patches collection (archive.org)

339 files, **all PPF1**. These are PSX copy protection bypass patches
from the late 90s scene. A single byte or two changed per disc — PPF1
is all they needed.

Source: https://archive.org/details/lib-crypt-ppf-patches
- Uploaded 2018-12-24 by user "brill", re-uploaded 2022-05-30
- Original PPFs collated from a thread on psxplanet.ru (Russian PSX
  scene forum): http://psxplanet.ru/forum/showthread.php?p=242014
- Description: "(Almost?) all PPF patches for LibCrypt protection on
  PS1 games." Uploader added FF8, FF9, Galerians patchers from
  elsewhere since the original thread was missing those.
- Intended use: apply with PPF-O-Matic 3 to a single merged .bin disc
  image (use CDMage Beta to combine multi-bin rips first).

The collection name is slightly misleading — it contains both LibCrypt
bypasses AND modchip-replacement cracks. Breakdown by the embedded
50-byte description in each PPF1 header:

**By region** (n=339):
| Region | Count | % |
|--------|------:|--:|
| PAL (Europe) | 192 | 56.6% |
| NTSC-J (Japan) | 105 | 31.0% |
| NTSC-U (US) | 42 | 12.4% |

**By crack type**:
| Type | Count | Notes |
|---|---:|---|
| LibCrypt | 172 | All PAL (LibCrypt was a European scheme) |
| ModChipCrack | 163 | Crosses all regions — bypasses boot-sector region lock |
| MeconCrack | 3 | Rare, PAL+US |

**Cross-tab** (region × type):
- NTSC-J: exclusively ModChipCrack (105/105). Japan used boot-sector
  region lock, no LibCrypt.
- PAL: mostly LibCrypt (172/192 ≈ 90%), some ModChipCrack (18),
  one MeconCrack.
- NTSC-U: all ModChipCrack (40) or MeconCrack (2). No LibCrypt in
  US games.

### PPF1 header format (empirical)

From inspecting the LibCrypt PPF1s:

```
Offset  Size  Content
0       5     Magic "PPF10"
5       1     Encoding byte (0x00 in these files)
6       50    ASCII/Latin-1 description, space-padded
56+     ...   Patch records: 4-byte offset (LE) + 1-byte count + bytes
```

The 50-byte description slot is used for game title + region +
crack type tag (e.g. `MediEvil (Europe) [LibCryptCrack]` or
`I.Q Final (Japan) [PAPX-90063] [ModChipCrack]`).

## Coverage achieved

We now have real-world specimens of all four PPF versions:
- PPF1: 339 files (LibCrypt collection)
- PPF2: 16 files (RHDN)
- PPF3: 440 files (RHDN)
- PPF4: 131 files (RHDN)

PPF1 was previously unconfirmed in the wild for our purposes. The
LibCrypt collection confirms PPF1 is a real, used format with
hundreds of distributed examples.

## Format notes (from research)

- **Creator**: Icarus of Paradox (PSX scene group)
- **PPF1**: original format. Just offsets and replacement bytes.
  No file size field, no validation block.
- **PPF2**: adds file size tracking (needed for >3GB DVD images).
- **PPF3**: adds undo data (store original bytes) and validation
  blocks (1024-byte sample for verifying target).
- **PPF4**: Pyriel's distinct format that happens to use the .ppf
  extension. Magic is `PPF40` but otherwise unrelated to PPF1-3.

## Heaviest archives

- `[2777]ToVPatcher-1.0.zip` — N/A (xdelta3, not PPF)
- `[4398]gs2-bugfixes-master.zip` — 124 PPF4 files (Grandia 2 European bugfix)

## By section/console (RHDN)

| Section/Console | Archives |
|-----------------|---------:|
| hacks/psx | 101 |
| translations/psx | 62 |
| hacks/n64 | 23 |
| hacks/snes | 15 |
| translations/ps2 | 7 |
| hacks/ps2 | 7 |
| hacks/ds | 6 |
| hacks/psp | 6 |
| translations/psp | 5 |
| translations/tgcd | 5 |
| (smaller) | rest |

Heavily PlayStation, as expected for the format's origin.

## Investigation: FF8 All PAL bundle

One bundle from the LibCrypt rar, "FF8 All PAL", contains no .ppf
file at all. Just:
- `UNIvFf8.exe` (41,984 bytes) — a standalone Windows patcher
- `Chencrack.nfo` — scene release info from "Chenchotronk", dated
  1999-11-03. The nfo says "use PPF utility from Icarius and apply
  the ppf to your Cdrwin BIN image file" — but there IS no .ppf file
  in the bundle. Either the scene author wrote standard boilerplate
  that didn't match what they shipped, or the PPF is embedded.
- `file_id.diz` — BBS description

### Is there a PPF hiding inside UNIvFf8.exe?

The exe is packed with **PE-PACK** (by Elmar Koeppen, common in
1999). We can confirm this from the PE section named `PEPACK!!`.
Packed consequences:
- Entropy 7.56 bits/byte (consistent with compressed data)
- No PPF magic bytes visible in the raw exe
- No readable strings
- Even the .rsrc section's APPBMP and dialog resources are
  compressed

### What we tried

1. **Scan raw exe for PPF magic** — zero hits (expected: packed)
2. **unipacker** — ran it through the emulation-based unpacker.
   Produced a 172KB dump but it was incomplete — still had
   PE-PACK error strings, no decoded payload strings, no PPF
   magic. Emulation bailed at unimplemented Windows APIs
   (GetEnvironmentStrings, GetStartupInfoA).
3. **Wine execution + memory scan** — launched under wine, planned
   to read /proc/PID/mem looking for PPF magic in the unpacked
   image. Didn't work because wine-staging needs a display server
   to create the exe's window, and this system is Wayland-only
   with no Xvfb available. The exe errored out immediately on
   window creation.

### What would work

- **Xvfb** — a virtual X framebuffer that makes wine happy without
  a real display. If installed, wine would launch the exe, the
  window would be created (invisibly), and we could memory-scan
  the running process. Caveat: only catches PPF data loaded at
  startup, not data pulled in after user interaction. But a 42KB
  scene patcher almost certainly loads its payload at startup.
- **Manual PE-PACK reversing** — algorithm is documented in old
  reversing writeups. Hours of work.
- **Run it on an actual display** — user opens it on their desktop
  environment, observes behavior, and we settle it.

### Classification

FF8 All PAL is from the LibCrypt rar, which by provenance rule
means it's part of the PPF1 collection. But it has no .ppf file
and we can't prove it contains PPF data. Currently parked at
`ppf/tools/FF8 All PAL/` — acknowledging it's a PPF-scene artifact
but not confirmed as any PPF format version.
