# UPS

The Universal Patch System. Created by byuu (now Near) and specified
on 2008-04-18 as a public-domain replacement for IPS. Designed to be
as simple to implement as IPS while removing IPS's 16 MB file size
limit and adding CRC32 integrity checking.

The format's defining feature is **bi-directional patching**: a single
UPS patch can be applied to the source to produce the target, *and*
applied to the target to produce the source. This falls out of using
XOR as the diff operation (XOR is self-inverse) and storing both
source and target metadata (sizes and CRC32s) in the patch header.

## Files

- `spec.md` — the on-disk structure, written up for future reference
  alongside slap's implementation modules
- `upstream/ups-spec.pdf` — byuu's original specification document,
  verbatim. This is the canonical and authoritative source; if this
  document and `spec.md` ever disagree, the PDF wins.

## Provenance

The PDF was authored by byuu on 2008-04-18, released under Creative
Commons BY-NC-ND 3.0. It was originally distributed from byuu.org /
near.sh, both of which are now offline. The copy preserved here was
obtained from Romhacking.net, document #392.

## Relationship to NINJA

The spec's introduction explicitly positions UPS as a *complement* to
NINJA, not a replacement:

> "NINJA3 will use UPS internally as the raw patch data, and will
> handle detection and support for each individual system, as needed.
> NINJA3 behaves as a container, much like the relationship between
> OGG and Vorbis."

NINJA3 was never shipped. UPS survives on its own as a standalone
format, which is how slap treats it.

## Known issues

### OOB blocks

**Status: fixed.** Apply clips out-of-bounds blocks to the declared
target size and emits a warning. `detectOOBBlocks` in
`Slap.UPS.Apply` walks the block stream at parse time and populates
`patchWarnings` so the diagnostic appears before apply runs.

Background: real-world UPS patches from NUPS and Tsukuyomi commonly
have a final block whose terminator byte lands 1 past the declared
output size. This is a creation-tool artifact — the output is
complete before the OOB block fires. Previously slap hard-errored
with `ApplyWritesPastTarget`, rejecting CRC-verified patches like
`crystalleaf.ups` and `FE1+2_GBA.UPS`. Every other UPS tool (beat,
NUPS, Flips) was lenient here; slap was the outlier.

### No shrink patches in the wild

The UPS spec supports `output_size < input_size` but zero patches in
the romhacking.net corpus (192 files) exercise this. The shrink code
path is tested only by synthesized data.

## Corpus research

Detailed empirical findings in `findings.md` in this directory.
Key results: 8/8 CRC-matched patches apply successfully (4 clean,
4 with OOB clipping warning). All curated patches verified.
