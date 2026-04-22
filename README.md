# slap-format-documentation

Format documentation for [slap](https://github.com/nyuutsu/slap). Eventually this will be wired up as a git submodule, so a plain clone of slap doesn't drag in archived binaries and other preservation material.

## What's here

Each top-level format folder (e.g. `bps/`, `ips/`) holds our understanding of that format. Each folder's `upstream/` subfolder holds primary source materials we thought worth preserving — "worth preserving" being a distinct axis from "how important or correct is it?" (the **Why** section below elaborates).

Per-folder, our own writing converges on some combination of `spec.md`, `questions.md`, `notebook.md`, and `todos.md`. A folder matching that shape reflects work we stand by. A folder that doesn't — that still carries older-style `README.md`, `proposal.md`, research notes, or similar — hasn't had its polish pass yet; contents are provisional.

## Why

A lot of this stuff is at serious risk of disappearing via linkrot. The authoritative implementation of APS N64, for instance, exists on one mirror with an expired TLS cert. The authoritative binary for APS GBA is a UPX-packed VB6 executable whose author's homepage is gone. These things deserve a stable home.

Other material here isn't at that kind of risk — byuu's BPS spec, for instance, is archived in plenty of places. It's here because we wanted it tracked alongside the rest. That's the real criterion for `upstream/`: we wanted it in one place. The linkrot-protection is sometimes a happy side effect, sometimes the point.

We credit everything preserved here. The MIT license in `LICENSE` applies to our own writing. Upstream material in `upstream/` directories retains whatever license the original author set. See `CREDITS.md` for attribution.

## Format index

| Folder | Format | Notes |
|--------|--------|-------|
| `aps-gba/` | APS (GBA) | "Alternate Patching System" by HackMew, 2010. |
| `aps-n64/` | APS (N64) | "Advanced Patching System" by Silo/Fractal of Blackbag, 1998. |
| `bps/` | BPS | "Binary Patching System" by byuu/Near. Successor to UPS. |
| `dps/` | DPS | By Marc de Falco ("deufeufeu" on GBAtemp). NDS-oriented. |
| `ips/` | IPS | "International Patch Standard", documented in SNESTool (1996). The oldest and most ubiquitous ROM patch format. |
| `ninja1/` | NINJA 1.x | Derrick Sobodash's NINJA format, v1.x. |
| `ninja2/` | NINJA 2.x | Derrick Sobodash's NINJA format, v2.x. Different from ninja1. |
| `ppf/` | PPF1/2/3 | PlayStation patch format by Icarus of Paradox. Three successive versions. |
| `ppf/` | PPF4 | By Pyriel, for the GS2 bugfixes project. Uses PPF3's header shape but is otherwise a distinct format. |
| `rfc-vcdiff/` | VCDIFF | Per RFC 3284 (Korn, MacDonald, Mogul, Vo, 2002). xdelta3 is this, plus extensions. |
| `ups/` | UPS | "Universal Patch System" by byuu/Near, 2008. |
| `xdelta1/` | xdelta1 | by Joshua MacDonald, 1997–2003. Unrelated to xdelta3 despite shared author and name. |
