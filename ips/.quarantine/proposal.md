# slap's IPS implementation notes

## Strictness and symmetry

slap's parse and create paths are symmetric: the set of wire shapes
createIPS can emit and the set parseIPS accepts without error are
(modulo category-1 warnings for conformant-unusual input) the same
set. Spec violations are rejected with SlapError at whichever layer
sees them first; SlapWarning is reserved for unusual-but-conformant
input, loss during format conversion, and opinionated architectural
decisions on the user's behalf.

## Resolved design questions

### IPS32 truncation markers
slap rejects trailing bytes after "EEOF" with SlapError. The
truncation marker is a Flips-era StandardIPS community extension
with no IPS32 analogue. createIPS does not emit them for IPS32
and parseIPS does not accept them for IPS32 — symmetric rejection.

### EBP trailing JSON
slap's EBP trailer check is shape-only: bytes after the StandardIPS
"EOF" trailer are accepted as EBP metadata when they begin with `{`,
and captured verbatim without JSON parsing or schema validation.
Slap.IPS.Describe's field extractor pulls title/author/description
leniently if they're present (case-insensitive, missing-field
tolerant) but requires none of them. Trailing bytes that are neither
a spec-conformant Flips truncation marker nor `{`-prefixed are
SlapError. createIPS's EBP path writes the metadata blob verbatim;
the blobs buildEBPMetadataJSON produces carry exactly the four
canonical fields (patcher, title, author, description), so no
non-conformant shape escapes.

### Variant ceiling rejection
slap rejects records at parse time whose end position exceeds the
variant's spec ceiling: ipsVariantMaxAddressableOffset +
ipsMaxRecordPayload. The formula is the sum of the two independent
limits, not either one alone. For StandardIPS that's 0xFFFFFF +
0xFFFF = 0x100FFFE. The Fire Emblem 6 translation patch
(test/data/fe6/fe6.ips) exercises this ceiling — its last record
starts at 0xFFFFFF and extends to 0x1000000, within the arithmetic
ceiling. A naive "offset ≤ 0xFFFFFF" check would falsely reject
fe6. Apply trusts this precondition and does not re-check.

### Sentinel collision (0x454F46 / 0x45454F46)

slap never produces an IPS file containing a record at the sentinel
offset.

With source (normal create, or conversion with source provided):
avoidSentinel applies the Archiveteam wiki's shift-and-prepend fix
— emit the record at `offset-1` with the preceding source byte
prepended. Output has no collision.

Without source (direct format→IPS conversion): reject iff any input
record has offset exactly equal to the sentinel. The check is exact
equality; every rejection is a patch that would actually break,
every non-rejection is safe. No probabilistic hedging.

See ips-audit.md §1.4 for the full reasoning, including why the
lookahead-parser alternative is rejected.

### Record application order
Records apply in wire order — the exact order they appear in the
patch file, period. Overlap is permitted; later writes clobber
earlier ones. Both overlap and unsorted records are unusual and
warn. Warnings are used liberally in slap. (The warnings themselves
are not yet emitted by Slap.IPS.Apply; tracked in slap-vs-spec.md.)

## Performance notes

Slap.IPS.Apply uses initialFill: allocate a buffer of the declared
or derived target size, copy the source bytes into it (bounded by
min(sourceLen, targetLen)), zero-fill any tail past source, then
walk the record vector in wire order and overlay each record's
write on top. Records apply in the exact order they appear in the
patch file; overlap causes later writes to clobber earlier ones;
no reordering happens anywhere.

This is mildly inefficient when the patch is small relative to the
source. fe6 is the pattern: an 8 MiB GBA ROM whose patch extends
the target to just under 16 MiB; wire patch is ~610 KB. initialFill
writes the full ~16 MiB of source-plus-zero-fill, and the record
walk then writes another ~610 KB on top — so every output byte a
record will overlay gets written twice. Steady-state waste is
roughly the total size of all record payloads (a few percent of
total buffer writes for fe6-shaped inputs).

There are no urgent or definite plans to change this. The current
approach is safe (every buffer byte has a defined value before any
record runs), reliable (behavior doesn't depend on record
distribution), and easy to reason about (apply order is exactly
wire order, period). A faster scheme carries real risk of
introducing reordering subtleties for little gain on typical inputs.

As an aside: if efficiency ever does become pressing, there is a
path that would preserve wire-order semantics. Compute a coverage
mask in a first pass — which output bytes are touched by at least
one record — then copy source passthrough only into the uncovered
regions, and apply records in wire order on top as before. The
mask is O(targetSize) bits; the record application is unchanged.
No sorting, no reordering, no change to clobber semantics. Listed
here as documentation, not as a plan.
