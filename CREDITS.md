# Credits

## Upstream material

### aps-gba/upstream/

| File | Provenance | Author | Notes |
|------|------------|--------|-------|
| `A-Ptch.exe` | Direct download from I-forget-where | HackMew (Andrea Sartori) | VB6 executable, UPX-packed. ~2010. The reference implementation of APS GBA. Rare. |

### bps/upstream/

byuu's BPS format specification, authored as a public-domain document. Preserved here because bsnes/byuu.org/near.sh are offline.

| File | Author | Notes |
|------|--------|-------|
| `bps_spec.md` | byuu (Near) | BPS format specification in Markdown. Public domain. |
| `bps_spec.html` | byuu (Near) | Same specification as originally distributed in HTML. Public domain. |
| `bps_spec.zip` | byuu (Near) | Archive of the spec files as retrieved. |

### aps-n64/upstream/

| File | Author | Notes |
|------|--------|-------|
| `bb-aps12.zip` | Silo and Fractal, Blackbag | Distributed circa December 1998 on dextrose.com. Retrieved from the [icequake.net mirror](http://n64.icequake.net/mirror/www.dextrose.com/files/n64/ips_tools/bb-aps12.zip) (which has an expired TLS certificate). |

### bsdiff/upstream/

| File | Author | Notes |
|------|--------|-------|
| `bsdiff-android-22535836b12c.tar.gz` | Colin Percival (original bsdiff); Android/Chromium maintainers (this packaging) | Android's bsdiff fork, which vendors Colin Percival's bsdiff 4.3, which *used* to be available at: [daemonology.net/bsdiff](http://www.daemonology.net/bsdiff/). O.G. is © 2003–2005 Colin Percival. |

### headers/upstream/

| File | Author | Notes |
|------|--------|-------|
| `libretro-handy-cart.h` | K. Wilkins (Handy); libretro-handy maintainers | The LYNX_HEADER struct that defines the .lnx header format. zlib license (notice embedded in the file). Retrieved 2026-07-08 from [github.com/libretro/libretro-handy](https://github.com/libretro/libretro-handy/blob/master/lynx/cart.h). |

### ips/upstream/

These tools descend from the same author: THE MCA — "The Magician" — of ELiTE, credited (with DAX) as the inventor of the IPS format.

| File(s) | Author | Notes |
|---------|--------|-------|
| `snes-famicom-utility-v2.0_doc.txt` | The Magician/MCA (with Sledge & Lowlife of Hotline) | Documentation from "SNES-FAMICOM UTILITY V2.0", an Atari ST tool (from `SNES_V20.MSA`), at or after late 1992. It calls IPS the "International Patch Standard". |
| `SNES_V18.ZIP` | The Magician/MCA | SNES utility v1.8 for Atari ST, as loose files. v1.8's headline addition is Action Replay support; its readme credits DAX with the idea. |
| `SNES_V20.ZIP` | The Magician/MCA (with Sledge & Lowlife of Hotline) | Contains `SNES_V20.MSA`, cited above. |
| `SNES_V21.ZIP` | M.C.A. of ELITENDO (testing by Sledge) | The readme signs off "Updated By: M.C.A. of ELITENDO". |
| `snes_tools_v1.00-elite-pc.zip`, `snes_tools_v1.00-elite-pc.diz` | MCA/ELiTE | SnesTool v1.0 for PC (`SNESTOOL.EXE` dated 1995), the DOS successor to the Atari tool; does "IPS Patch+Create". `.diz` is the scene release descriptor. |
| `snes_tools_v1.01-elite-pc.zip`, `snes_tools_v1.01-elite-pc.diz` | MCA/ELiTE | SnesTool v1.01 — per the `.diz`, "IPS bugs have been fixed". |
| `snes_tools_v1.03-elite-pc.zip`, `snes_tools_v1.03-elite-pc.diz` | MCA/ELiTE | PAL/Slowrom search fixes. |
| `snes_tools_v1.20-elite-pc.zip`, `snes_tools_v1.20-elite-pc.diz` | MCA & Sledge | For PC; adds Game Doctor 3 file-format conversion. |
| `snestl12.zip` | THE MCA (ELiTE) | SNESTool v1.2, released 1996-02-12. Downloaded from [romhacking.net utility #18](https://www.romhacking.net/utilities/18). DOS tool. Its documentation credits "DAX and ME" (i.e. THE MCA) as inventors of the IPS format. |

The `*-source.md` files are text-ifications of what you'll find if you *today* look up "IPS format specification".

* `zerosoft-source.md` is Z.e.r.o's 2002 write-up on [zerosoft.zophar.net](zerosoft.zophar.net) and is cited by the later authors.
* `anosh-source.md` is anosh.se's 2023 synthesis (which cites ZeroSoft)
* `archiveteam-source.md` is the ArchiveTeam file-formats wiki entry
* `sneslab-source.md` is the SnesLab wiki entry.

### ninja1/upstream/

| File | Author | Notes |
|------|--------|-------|
| `ninja-1.01php.tar.gz` | Derrick Sobodash (Cinnamon Pirate) | NINJA 1.01 PHP reference implementation |
| `ninja1-filespec10.txt` | Derrick Sobodash | NINJA 1.0 file format specification |

### ninja2/upstream/

| File | Author | Notes |
|------|--------|-------|
| `ninja2-cliusage.txt` | Derrick Sobodash | NINJA 2.0 CLI usage documentation |
| `ninja2-convroms.txt` | Derrick Sobodash | NINJA 2.0 conversion notes for ROM variants |
| `ninja2-filespec20.txt` | Derrick Sobodash | NINJA 2.0 file format specification |
| `ninja2-filespec20-rhdn.txt` | Derrick Sobodash / romhacking.net | NINJA 2.0 file spec, romhacking.net-distributed version |

### PPF1/upstream/

PPF1 (Playstation Patch File). Icarus/Paradox, 1999, built on the APS format by Silo and Fractal (Blackbag).

| File | Author | Notes |
|------|--------|-------|
| `pdx-ppf1.zip` | Icarus/Paradox | The Paradox PPF1 release: PC + Amiga `ApplyPPF`/`MakePPF` binaries, C sources, and the `ppf.txt` format description (magic `PPF10`, encoding method 0). |
| `AmiPPF.lha` | AmiPPF (`AmiPPF@yahoo.de`, "JBI") | Independent AmigaOS (m68k) PPF applier for PSX images; applies v1.0/2.0 patches. |
| `AmiPPF.readme` | AmiPPF | Amiga `.readme` (install / ToolTypes doc) accompanying `AmiPPF.lha`. |

### PPF2/upstream/

PPF2, Icarus/Paradox (≈October 1999 by the archive's internal file dates).

| File | Author | Notes |
|------|--------|-------|
| `pdx-ppf2.zip` | Icarus/Paradox | The PPF2 distribution: DOS tools (`MakePPF`, `ApplyPPF`, `PPFInfo`, `PPFDiz`), PPF-O-Matic 2.0 (Windows), and the `PPF2.txt` developer file-structure spec. |

*Regrettably*, I do not have the source code to this or any other comprehensive PPF2 implementation.

### PPF3/upstream/

| File | Author | Notes |
|------|--------|-------|
| `ppf-master.zip` | Icarus/Paradox | The PPF3 developer kit: `PPF3.txt` (spec document), `applyppf3`/`makeppf3` C source. A GitHub mirror snapshot (2018 file dates). |
| `PPF-O-Matic.zip` | Icarus/Paradox | PPF-O-Matic 3.0 (`pdx-pom3`), the Windows GUI applier, 2001. |
| `PPF Studio.zip` | Starbee | PPF-Studio 1.01beta (2003), a Windows GUI PPF3 patch creator |

### PPF4/upstream/

| File | Author | Notes |
|------|--------|-------|
| `gs2-bugfixes-master.zip` | Pyriell | Snapshot of [github.com/pyriell/gs2-bugfixes](https://github.com/pyriell/gs2-bugfixes), release 2.02.078 (dated 2019-01-03 in `version.txt`). |

### ups/upstream/

| File | Author | Notes |
|------|--------|-------|
| `ups-spec.pdf` | byuu (Near) | UPS format specification, authored 2008-04-18. Licensed CC BY-NC-ND 3.0. Originally distributed from byuu.org / near.sh, both now offline. Copy preserved here was obtained from romhacking.net document #392. |

### xdelta1/upstream/

The first five items on this list were found on SourceForge.

| File | Author | Notes |
|------|--------|-------|
| `xdelta-0.13.tar.gz` | Joshua MacDonald | Early 0.13 source release. |
| `xdelta-1.0.0.tar.gz` | Joshua MacDonald | 1.0.0 source release. |
| `xdelta-1.1.1.tar.gz` | Joshua MacDonald | 1.1.1 source release. |
| `xdelta-1.1.2.tar.gz` | Joshua MacDonald | 1.1.2 source release. |
| `xdelta-1.1.3.tar.gz` | Joshua MacDonald | 1.1.3 source release. |
| `xdelta.1` | Joshua MacDonald | - |
| `xdelta_1.1.3-10.8.debian.tar.xz` | Debian xdelta maintainers (Frédéric Lepied, then LaMont Jones) | Debian packaging overlay for 1.1.3 |

### xdelta2/upstream/

The xdelta 2.0 beta series, also by Joshua MacDonald. GPL. slap does not use this format in any way — it is kept purely as a curio and as a record of the 2.x line.

| File | Author | Notes |
|------|--------|-------|
| `xdelta-2.0-beta1.tar.gz` | Joshua MacDonald | 2.0 beta 1 source. |
| `xdelta-2.0-beta3.tar.gz` | Joshua MacDonald | 2.0 beta 3 source. |
| `xdelta-2.0-beta4.tar.gz` | Joshua MacDonald | 2.0 beta 4 source. |
| `xdelta-2.0-beta9.tar.gz` | Joshua MacDonald | 2.0 beta 9 source. |
| `xdelta-2.0-beta10.tar.gz` | Joshua MacDonald | 2.0 beta 10 source. |

## Tools referenced

Not redistributed here, but cited or otherwise studied:

- [RomPatcher.js](https://github.com/marcrobledo/RomPatcher.js) by Marc Robledo — modern web-based patcher supporting many formats
- [UniPatcher](https://github.com/btimofeev/UniPatcher) by Boris Timofeev — Android patcher; wiki hosts several format specs we reference
- [Unofficial-A-ptch](https://github.com/Gamer2020/Unofficial-A-ptch) by Gamer2020 — VB6 fork of HackMew's A-Ptch with GB/GBC support added
- Atmosphere
- atmosphere-ips-apply
- beat
- EBPatcher
- flips
- go-ups
- javaxdelta
- lua-ips
- sips
- tsukuyomi
- xdelta3