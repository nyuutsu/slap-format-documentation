# xdelta1

Joshua MacDonald's original binary delta tool (1997–2003). Completely
unrelated to xdelta3/VCDIFF despite sharing the author and name.

The format has no formal spec. The canonical source (`xdelta-1.1.4`,
LGPL/GPL) is the spec. slap is, to our knowledge, the only other
implementation — the romhacking ecosystem used the bundled canonical
binary (`xdelta.exe`) or GUI wrappers around it.

## Status in slap

- **Parse**: v1.0.4 and v1.1. Older versions rejected with
  `UnsupportedSubformat`.
- **Apply**: in-memory, single-pass. MD5 verification wired up for
  both source and target.
- **Info/Explain**: shows from/to names, MD5s, source list,
  instruction regions.
- **Create**: not yet implemented.

## Files

- `spec.md` — wire format specification (derived from canonical source)
- `upstream/` — canonical source distribution (`xdelta-1.1.4`)

## Known issues

### Real bugs (wrong output or spurious failure)

**Gzip transparency not implemented.** The canonical tool
auto-decompresses gzipped inputs (FLAG_FROM_COMPRESSED,
FLAG_TO_COMPRESSED). slap ignores these flags. A patch created from
gzipped inputs produces silently wrong output in slap. No corpus
patch exercises this, but it's a real spec gap.

*Design intent*: implement the feature. Match canonical behavior. Add
a `--pristine` analog for byte-exact compressed content preservation.

**FLAG_NO_VERIFY not respected.** Patches created with canonical's
`--noverify` have garbage MD5 fields and FLAG_NO_VERIFY set. slap
ignores the flag and runs MD5 verification, producing spurious
mismatch errors.

*Design intent*: when FLAG_NO_VERIFY is set, skip MD5 verification
automatically. Emit a note explaining why. `--no-verify` suppresses
the note. `slap info` should surface the flag.

### Validator gaps (parser too permissive)

**Source count and ordering.** The canonical tool accepts exactly
four source shapes: `[]`, `[data]`, `[file]`, `[data, file]`. slap's
parser accepts any source count and ordering. No observed patch
violates canonical's constraints, but slap should match them.

*Design intent*: validate at parse time with a dedicated checker
citing canonical's behavior. Type the wire field permissively (the
wire permits it); enforce semantic constraints in a validator.

### Missing features

**Create.** slap cannot produce xdelta1 patches. Lower priority than
other formats but wanted for round-trip capability.

**Unused file source detection.** When the delta found no shared
chunks, the entire output is in the data segment and the file source
is unreferenced. Canonical warns; slap doesn't detect this.

*Design intent*: emit a SlapWarning at parse time if no instruction
references a file source.

### Display concerns

**Source MD5 labeling in `info` output.** The summary block labels
sources by position (`source 1 MD5`, `source 2 MD5`) without
annotating which is the file source (user must verify) vs the data
segment (slap handles internally). The detail block has annotations
but the summary doesn't. Misleading when reading quickly.

*Design intent*: deferred pending broader audit of info output across
formats. See the xdelta1 findings document for a proposed redesign
framing.

**From/to names not in metadata layer.** `SomePatch.hs` sets
`patchMetadata = Nothing` for xdelta1. The from/to names are shown
in `info` but silently dropped on conversion. Should be promoted to
metadata.

### Unexercised code paths

- **Sequential offset mode**: zero corpus patches use it.
  `fixSequentialOffsets` is untested by real data.
- **v1.0.4**: all corpus patches are v1.1. v1.0.4 parsing is
  untested empirically.

## Corpus research (romhacking.net 2024-08-01)

7 `.xdelta` files from 4 archives (out of ~2,026 `.xdelta` files;
the rest were VCDIFF/xdelta3). All v1.1. 7/7 applied and verified
end-to-end, including a 3-step chained dependency (OoE-Redrawn →
Albus Recolored) that required cross-dump-variant application.

Detailed findings, including the full slap-vs-canonical comparison,
severity tiers, and design calls, are in `findings.md` in this
directory.
