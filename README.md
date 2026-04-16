# slap-format-documentation

Format documentation for [slap](https://github.com/nyuutsu/slap). This repo
is used as a git submodule; that way a plain clone of slap doesn't drag in
archived binaries, scanned specs, and other preservation material. To set
this up from the slap repo:

```
git submodule update --init
```

## What's here

One folder per patch format. Each folder contains some combination of:

- `README.md` — brief intro and provenance notes
- `spec.md` — our writeup of the format itself (bytes, semantics). Useful to
  anyone implementing the format, not specific to slap.
- `proposal.md` — slap's design decisions for the format (what we warn on,
  what we reject, how edge cases are resolved). Slap-specific.
- `upstream/` — unmodified third-party material: original specs, reference
  implementations, tool binaries, archives as received.

Not every format has all of these. Some folders are only preservation
material; some have our writing but no recovered upstream; some are notes
on slap's own implementation rather than the format.

## Why

Patch formats in the ROM hacking world are often poorly-documented,
proprietary-ish, or built by small groups whose hosting lapses. Primary
sources disappear. The authoritative implementation of APS N64, for
instance, exists on one mirror with an expired TLS cert. The authoritative
binary for APS GBA is a UPX-packed VB6 executable whose author's homepage
is gone. These things deserve a stable home.

We preserve what we can, credit the authors, and write our own clean
specs where the originals are lossy or wrong. The MIT license in
`LICENSE` applies to our own writing. Upstream material in `upstream/`
directories retains whatever license the original author set (often
unclear, in which case we preserve for archival purposes with credit).

See `CREDITS.md` for attribution.

## Format index

| Folder | Format | Notes |
|--------|--------|-------|
| `aps-gba/` | APS (GBA) | "Alternate Patching System" by HackMew, 2010. XOR-based, 64KB blocks, reversible via CRC16. |
| `aps-n64/` | APS (N64) | "Advanced Patching System" by Silo/Fractal of Blackbag, 1998. Byte replacement with optional RLE. |
| `bps/` | BPS | "Binary Patching System" by byuu/Near. Authoritative spec preserved under `upstream/`; slap's diff algorithm notes alongside. |
| `ips/` | IPS | "International Patch Standard" per SNESTool's 1996 documentation. The oldest and most ubiquitous ROM patch format. No formal spec by its (disputed) author. |
| `ninja1/` | NINJA 1.x | Derrick Sobodash's NINJA format, v1.x. |
| `ninja2/` | NINJA 2.x | Derrick Sobodash's NINJA format, v2.x. Different from ninja1. |
| `ups/` | UPS | "Universal Patch System" by byuu/Near, 2008. XOR-based, inherently bi-directional, public-domain spec. |

Folders may grow as more formats are documented.
