# NINJA1 ROM-type normalization — slap's calls

NINJA1 shares the per-platform procedures in `Slap.Normalize` with NINJA2 (see `docs/ninja2/rom-normalization.md` for the dispositions common to both). What differs for v1:

**Only SNES, Genesis, and Game Boy normalize.** Those are the types the v1 spec (`ninja1-filespec10.txt`) defines procedures for, and `ninja1RomTypeNeedsNormalization` is the gate. Every other type applies as-is; NES in particular is a v2 concept, not a v1 one.

**Forward-only.** `ninja.php`'s readers return the clean bytes and never re-emit a header, so nothing set aside during normalization returns to the output — the applied result is the patched canonical form. This is `ForwardOnly` in `Slap.Normalize`, and it holds even for an SNES NSRT header that NINJA2 would restore.

**The v2 loops serve v1.** `ninja.php`'s own `smd_deinterleave` reads its strides from variables it never sets, so its loop does not advance; the v2 `smd_deint` is the working statement of the same 16 KiB-block scheme, and slap uses it for both formats. (This also settles a claim sometimes made that the Genesis block size changed between versions — v1 never had a working loop to differ from.)

**Ordering with the hash sample.** NINJA1 hashes large sources over a sample (`ninja1HashInput`); normalization runs first, so the sample is drawn from the canonical bytes.
