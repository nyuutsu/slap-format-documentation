# slap-format-documentation

Format documentation for [slap](https://github.com/nyuutsu/slap). This submodule contains two kinds of things: primary source material, and *interpretation*.

Formats and format families (as is so for PPF (though, arguably PPF4 is not "part of the family"...) & VCDIFF) get a folder. These folders can have a subfolder, `upstream`, for primary source stuff, such as spec documents, tools, and source code.

Everything else is *interpretation*. **A lot of which is out of date or wrong**. Early sketches rather than a representation of what we actually built. Some of what is here is good, though. The more structured it is (that is: split into `spec.md`, `questions.md`, `notebook.md`, and `todos.md`), the more likely it is to be up to date.

## Why

A lot of this stuff is at serious risk of disappearing via linkrot. The authoritative implementation of APS N64, for instance, exists on one mirror with an expired TLS cert. It deserves a stable home.

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
| `dps/` | DPS | By Marc de Falco ("deufeufeu" on GBAtemp). NDS-oriented. |
| `ips/` | IPS | "International Patch Standard"[^yesreally], The oldest and most ubiquitous ROM patch format. |
| `ninja1/` | NINJA 1.x | Derrick Sobodash's NINJA format, v1.x. |
| `ninja2/` | NINJA 2.x | Derrick Sobodash's NINJA format, v2.x. Different from ninja1. |
| `ppf/` | PPF1/2/3 | PlayStation patch format by Icarus of Paradox. Three successive versions. |
| `ppf/` | PPF4 | Pyriel's thing. Uses PPF3's header shape; is able to grow the file |
| `vcdiff/` | VCDIFF | Per RFC 3284 (Korn, MacDonald, Mogul, Vo, 2002). |
| `vcdiff/` | xdelta3 | Joshua MacDonald's take on vcdiff; adds some things and removes others |
| `ups/` | UPS | "Universal Patch System" by byuu/Near, 2008. |
| `xdelta1/` | xdelta1 | by Joshua MacDonald, 1997–2003. Unrelated to xdelta3 despite shared author and name. |
| `xdelta2/` | xdelta2 | by Joshua MacDonald, a cancelled(?) potential successor to xdelta1 |

[^yesreally]: according to the snestool (1996) documentation
[^attribution]: might be imperfect. a handful of these are from "idk I found it on an open directory"