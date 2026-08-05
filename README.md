# slap-format-documentation

Format documentation for [slap](https://github.com/nyuutsu/slap). This submodule contains two kinds of things: primary source material, and *interpretation*.

Formats (and/or format families) get a folder. These folders can have a subfolder, `upstream`, for primary source stuff, such as spec documents, tools, and source code.

Everything else is *interpretation*. `vcdiff/` (especially!), `IPS/`, `UPS/` and `BPS/` each have scads of *interpretation* that I consider reliable.

## Why

Some of this stuff is at risk of disappearing via linkrot[^linkrot]. It deserves a stable home.

Other material here isn't at that kind of risk. byuu's BPS spec, for instance, is archived in plenty of places. It's here because we wanted it tracked alongside the rest. So the true inclusion criteria is "I felt like it". 

## Credit

The MIT license in `LICENSE` applies to our own writing. Upstream material in `upstream/` directories retains whatever license the original author set. See `CREDITS.md` for attribution[^attribution].

## Format index

| Folder | Format | Notes |
|--------|--------|-------|
| `aps-gba/` | APS (GBA) | "Alternate Patching System" by HackMew, 2010. |
| `aps-n64/` | APS (N64) | "Advanced Patching System" by Silo/Fractal of Blackbag, 1998. |
| `bps/` | BPS | "Binary Patching System" by byuu/Near. |
| `bsdiff/` | bsdiff | Colin Percival's algorithm and tool |
| `dps/` | DPS | By Marc de Falco ("deufeufeu" on GBAtemp). |
| `gdiff/` | GDIFF | "Generic Diff" by Arthur van Hoff and Jonathan Payne, 1997 |
| `ips/` | IPS | "International Patch Standard"[^yesreally], The oldest[^oldest] and most ubiquitous ROM patch format. |
| `headers/` | - | Information on ROM headers |
| `ninja1/` | NINJA 1.x | Derrick Sobodash's NINJA format, v1.x. |
| `ninja2/` | NINJA 2.x | Derrick Sobodash's NINJA format, v2.x. Different from ninja1. |
| `PPF1/` | PPF1 | PlayStation patch format by Icarus of Paradox. |
| `PPF2/` | PPF2 | PlayStation patch format by Icarus of Paradox. |
| `PPF3/` | PPF3 | PlayStation patch format by Icarus of Paradox. |
| `PPF4/` | PPF4 | Pyriel's Playstation patch format. |
| `ppf/` | PPF\* | Additional PPF material. |
| `vcdiff/` | VCDIFF | Per RFC 3284 (Korn, MacDonald, Mogul, Vo, 2002). |
| `vcdiff/` | xdelta3 | Joshua MacDonald's vcdiff implementation; adds some things and removes others |
| `ups/` | UPS | "Universal Patch System" by byuu/Near, 2008. |
| `xdelta1/` | xdelta1 | by Joshua MacDonald, 1997–2003. Unrelated to xdelta3 despite shared author and name. |
| `xdelta2/` | xdelta2 | by Joshua MacDonald, a cancelled(?) potential successor to xdelta1 |

[^linkrot]: The authoritative implementation of APS N64, for instance, exists on one mirror with an expired TLS cert.
[^oldest]: Sometime between 1992 and 1996. I went deep enough into the weeds on this one; if you can pin it down more precisely, please, let me know.
[^yesreally]: according to the snestool (1996) documentation
[^attribution]: might be imperfect. a handful of these are from "idk I found it on an open directory"