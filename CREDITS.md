# Credits

The MIT license in `LICENSE` applies to content written for this repository
(README files, spec.md and proposal.md documents, implementation notes).
Third-party material under `upstream/` directories retains whatever
licensing the original author set — which for many of these files is
unclear or unstated. We preserve them here because the hosting is
fragile or gone, not because we have rights beyond personal archival use.

If you are a rights holder for any material in this repository and want
credit adjusted or content removed, please open an issue.

## Upstream material

### aps-gba/upstream/

| File | Provenance | Author | Notes |
|------|------------|--------|-------|
| `A-Ptch.exe` | Direct download, copy-preserved | HackMew (Andrea Sartori) | VB6 executable, UPX-packed. ~2010. The reference implementation of APS GBA. Unofficial fork by Gamer2020 exists at [github.com/Gamer2020/Unofficial-A-ptch](https://github.com/Gamer2020/Unofficial-A-ptch). |

### bps/upstream/

byuu's BPS format specification, authored as a public-domain
document. Preserved here because bsnes/byuu.org/near.sh are offline.

| File | Author | Notes |
|------|--------|-------|
| `bps_spec.md` | byuu (Near) | BPS format specification in Markdown. Public domain. |
| `bps_spec.html` | byuu (Near) | Same specification as originally distributed in HTML. Public domain. |
| `bps_spec.zip` | byuu (Near) | Archive of the spec files as retrieved. |

### aps-n64/upstream/

All files are from `bb-aps12.zip`, distributed circa December 1998 on
dextrose.com. Retrieved from the icequake.net mirror at
`http://n64.icequake.net/mirror/www.dextrose.com/files/n64/ips_tools/bb-aps12.zip`
(which has an expired TLS certificate and may not be long for this world).

| File | Author | Notes |
|------|--------|-------|
| `bb-aps12.zip` | Silo and Fractal, Blackbag | The complete archive as received |
| `aps.txt` | Silo and Fractal, Blackbag | Format specification (has byte-range typos; defer to the C source) |
| `readme.txt` | Silo and Fractal, Blackbag | Includes spec verbatim plus dedication "to CZN" |
| `n64aps.c` | Silo / Blackbag | Apply tool source. Authoritative reference. |
| `n64caps.c` | Silo / Blackbag | Create tool source. Authoritative reference. |
| `n64aps.exe` | Silo / Blackbag | Compiled apply binary (DOS/Win32) |
| `n64caps.exe` | Silo / Blackbag | Compiled create binary (DOS/Win32) |
| `makefile` | Silo / Blackbag | Build rules |
| `file_id.diz` | Silo / Blackbag | BBS-era archive description |
| `BLACKBAG.NFO` | Blackbag | Group nfo with ASCII art |

The preliminary draft of `aps.txt` includes this notice:
> "Full source code to the patcher and creator will be made available
> to any coder who wants it before the closure date and publically available
> after."

The closure date was July 31, 1998. We read this as a clear intent that
the source was to be made publicly available, which it was via the
archive. We preserve it here under that intent.

### ips/upstream/

| File | Author | Notes |
|------|--------|-------|
| `snestl12.zip` | THE MCA (ELITE) | SNESTool v1.2, released February 12, 1996. Downloaded from [romhacking.net utility #18](https://www.romhacking.net/utilities/18). DOS tool. The tool's own documentation credits "DAX and ME" (i.e. THE MCA) as inventors of the IPS format, though no independent source corroborates this and the format's true origin is considered unknown. |
| `SNESTL12.DOC` | THE MCA (ELITE) | Tool documentation, extracted from the zip for convenience. Contains the earliest internal reference we have to the format's origin. |
| `SNESTL12.EXE` | THE MCA (ELITE) | Tool executable, extracted from the zip for convenience. |

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

### ups/upstream/

| File | Author | Notes |
|------|--------|-------|
| `ups-spec.pdf` | byuu (Near) | UPS format specification, authored 2008-04-18. Licensed CC BY-NC-ND 3.0. Originally distributed from byuu.org / near.sh, both now offline. Copy preserved here was obtained from romhacking.net document #392. |

## Research and writing

The `proposal.md` and `spec.md` files for each format were written for
the slap project. For APS N64, the writing is grounded in the recovered
C source (`n64aps.c` / `n64caps.c`) as the authoritative reference. For
APS GBA, the writing draws from binary analysis of A-Ptch.exe, the
spec on the UniPatcher wiki, the unofficial Gamer2020 fork source, and
cross-reference with RomPatcher.js and UniPatcher implementations.

## Tools referenced

Not redistributed here, but cited in spec/proposal docs:

- [RomPatcher.js](https://github.com/marcrobledo/RomPatcher.js) by Marc Robledo — modern web-based patcher supporting many formats
- [UniPatcher](https://github.com/btimofeev/UniPatcher) by Boris Timofeev — Android patcher; wiki hosts several format specs we reference
- [Unofficial-A-ptch](https://github.com/Gamer2020/Unofficial-A-ptch) by Gamer2020 — VB6 fork of HackMew's A-Ptch with GB/GBC support added
