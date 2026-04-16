# BPS

BPS ("Binary Patching System") was designed by byuu (now Near) for the
bsnes project and became the dominant patch format for ROM hacking in
the 2010s. Designed to be a strict improvement over IPS: larger
address space, integrity checks, optional metadata, and a flexible
diff engine that can encode very small patches for large files.

## Files

- `slap-diff-algorithm.md` — notes on how slap's BPS diff engine
  works. Slap-specific implementation documentation, not a format
  spec. The algorithm is based on Alcaro's Flips
  (`libbps-suf.cpp`, GPL v3); we studied the Flips source and wrote
  our own implementation in Rust. The document records the algorithm
  for future reference.

- `upstream/bps_spec.md` — byuu's original BPS format specification,
  in Markdown. This is the authoritative reference for the format.
- `upstream/bps_spec.html` — the same specification in HTML, as
  originally distributed.
- `upstream/bps_spec.zip` — the archive as retrieved, containing the
  original spec files.

## Corpus research (romhacking.net 2024-08-01)

1,507 `.bps` files scanned with `slap info`. 14 shortlisted for
hands-on testing with matched ROMs. Detailed findings in
`findings.md` in this directory.

Key results:
- **Metadata**: 0 of 1,495 parsed patches use the optional metadata
  field. Nobody populates it in the wild.
- **All 4 action types** observed in practice (SourceRead, TargetRead,
  SourceCopy, TargetCopy). Super Luigi Bros exercises all four
  including the self-referential TargetCopy.
- **22 shrink patches** found (rare, valuable for testing).
- **No-op patches**: Duke Nukem 64 has a 1-action patch that copies
  the entire source unchanged (output CRC matches input CRC). slap
  should warn about this.
- **iNES header migration**: NES patches break when No-Intro changes
  header versions. See `docs/header-awareness.md`.
