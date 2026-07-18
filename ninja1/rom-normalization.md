# NINJA1 ROM-type normalization — slap's calls

NINJA1's per-platform normalization matches NINJA2's; `docs/ninja2/rom-normalization.md` holds the dispositions common to both. What differs for v1:

**Only SNES, Genesis, and Game Boy normalize.** Those are the types the v1 spec (`ninja1-filespec10.txt`) defines procedures for; every other type applies as-is. NES in particular is a v2 concept, not a v1 one.

**Forward-only.** The reference readers return clean bytes and never re-emit a header, so nothing set aside during normalization comes back to the output — the applied result is the patched canonical form, the stripped header left off.

**One Genesis deinterleave.** A single 16 KiB-block swap serves both versions; the block size did not change between v1 and v2. The v1 listing sets out the scheme, but its loop, as written, does not advance, so there was never a differing v1 behavior to reconstruct.

**Normalize before hashing.** A large source is verified over a sample rather than the whole file; normalization runs first, so the sample is drawn from the canonical bytes.
