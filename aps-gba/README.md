# APS (GBA)

The "Alternate Patching System" for Game Boy Advance ROMs. Created by
HackMew (Andrea Sartori) circa 2010, released on PokeCommunity's
romhacking forums. XOR-based with 64KB blocks and dual CRC16 checksums
per block, making it inherently reversible.

**Completely unrelated** to APS N64 despite the shared extension and
overlapping magic bytes. APS GBA uses `APS1` as its magic; APS N64 uses
`APS10`. The formats share nothing else — different era, different
scene, different author, different wire format, different capabilities.

## Files

- `spec.md` — the on-disk structure, written up cleanly for future
  reference
- `proposal.md` — slap's design decisions for APS GBA: edge case
  handling, warning/error lines, implementation notes
- `upstream/A-Ptch.exe` — HackMew's original reference implementation,
  a UPX-packed VB6 executable. This is the canonical tool: it reads
  and writes APS GBA patches, and its behavior is the de facto spec
  for edge cases the written spec doesn't cover.

## Provenance

A-Ptch.exe was distributed through PokeCommunity and HackMew's now-
defunct personal website. The binary preserved here was obtained
directly and has not been modified. An unofficial fork with source
code exists at
[github.com/Gamer2020/Unofficial-A-ptch](https://github.com/Gamer2020/Unofficial-A-ptch),
but it was modified (to add GB/GBC support) and shouldn't be treated
as a faithful copy of the original — the VB6 native compile is
substantially different from the original binary.

## Known format issues

The proposal document enumerates edge cases, but two are worth calling
out here:

1. **The original tool has a truncation bug.** For size-changing
   patches (where source and target sizes differ), A-Ptch.exe
   truncates the output to the wrong declared size. Confirmed by
   disassembly of the original binary. This was never observed in
   practice because GBA ROMs are power-of-two sizes and don't change.
   slap implements the logically correct behavior (matching
   UniPatcher, not A-Ptch).

2. **Direction detection is CRC16-based and has a collision risk.**
   The format stores both source and target CRC16s per block.
   Direction is determined by which CRC the input block matches.
   With 16-bit hashes, collisions are ~1/65536 per block. Two
   different blocks with identical CRC16s in a single patch can
   make direction detection ambiguous. Unavoidable given the format.
