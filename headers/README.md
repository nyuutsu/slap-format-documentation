# headers/

Cross-format reference material on ROM file headers and byte orders — the envelopes around ROM bodies that patch interop cares about. A patch made against one framing of a ROM will not verify (and may silently corrupt) against another; the documents here record what those framings actually are, byte by byte.

- `spec.md` — the header layouts and byte-order transforms themselves. What bytes mean what. No slap opinions.
- `findings.md` — field research: cases from the patch-collection work where a framing difference was the whole problem, with verified checksums.
- `upstream/` — vendored primary sources (see `../CREDITS.md`).

`../header-awareness.md` is this topic's ancestor: the original cross-format problem statement and an early `--header-offset` proposal. It predates this directory and the design work in progress; expect it to be absorbed or superseded.

Status: draft, assembled 2026-07-08; not yet finalized.
