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
slap accepts bytes after the StandardIPS "EOF" trailer only when
they match the recognized EBP metadata shape (JSON object with the
discriminator field). Bytes that are neither a spec-conformant
Flips truncation marker nor conformant EBP metadata are SlapError.
createIPS's EBP path does not emit non-conformant trailing-JSON
shapes.

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

## Performance notes

Slap.IPS.Apply's current initialFill approach unconditionally
copies the source ByteString into the output buffer before the
record walk runs, then overlays record writes. For patches that
rewrite most of the target, this is near-optimal. For large-source
/ small-patch cases (fe6 is an 8 MiB GBA ROM whose patch extends
the target to just under 16 MiB; wire patch is ~610 KB), the
record payloads sum to roughly the wire size, so initialFill
writes ~15 MiB of source-passthrough and zero-fill that no record
ever overlays.

The asymptotically-better approach is copy-on-gap: walk the sorted
record stream, copy source passthrough in gaps between records,
write records over the gaps they fill, copy final tail. Each output
byte is written exactly once.

Copy-on-gap needs to iterate records in offset order to walk gaps
between them. slap stores records in wire order, so the sort would
live inside Slap.IPS.Apply as a local view over the record vector —
an implementation choice inside apply, not a parse-time commitment
to sorted storage. The optimization is planned as a follow-up commit
once test coverage exists to validate the copy-on-gap path against
the reference initialFill path for the same inputs.
