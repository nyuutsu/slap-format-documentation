# UPS — design questions

## Dispatch

### How does slap dispatch forward vs reverse application?

The spec describes bi-directional patching as a capability without prescribing a dispatch mechanism. byuu's beat and Flips auto-detect direction by checking the user-supplied file's size against both declared sizes.

**slap splits direction into two explicit commands: `slap apply` runs forward (input → output) and `slap undo` runs backward (output → input).** Both approaches are spec-faithful.

`applyUPS` walks the block stream against the source into an `output_size`-sized output buffer; `undoUPS` walks the same block stream against the target into a `source_size`-sized output buffer. Both go through the shared internal walker `runUPSXorWalk`. The XOR is self-inverse, so the block stream is unchanged between directions; only the output buffer size differs, which is what makes size-changing patches round-trip cleanly.

## Verification

### What's slap's policy on CRC verification?

The spec is SHOULD-level: "Values should be verified when applying the UPS patch."

**slap verifies CRCs by default; `--no-verify` downgrades source/target mismatches to warnings.** Patch-CRC mismatch is fatal and unconditional — `--no-verify` does NOT downgrade it. Same shape as BPS; no UPS-specific decision.

### What does slap do when the supplied file's size doesn't match the declared size?

The spec doesn't address size verification explicitly. The sizes are described as "exact file sizes" but no procedure is given for what to do if the user's file size differs.

**slap populates `verifyFileSizeAdvisory` from the declared input size for the forward-apply path; surfaces a warn-level size-mismatch diagnostic before the CRC hard-error fires.** This gives the user a more specific message than "CRC mismatch" when the underlying problem is that they handed slap the wrong file, without inventing a rejection requirement the spec doesn't state.

## Create-side decisions

### What does slap do when source is longer than target with non-zero tail bytes?

The spec permits shrinkage where the extra source bytes are zero so they XOR cleanly with the nothing-follows region.

**slap rejects with `UPSUnencodeablePair UPSSourceTailNonZero` when the tail isn't all zero.** Those bytes have nowhere to be encoded in the bi-directional XOR stream (the block stream only covers `[0, target_size)`); accepting the pair would silently break the spec's bi-directional guarantee on undo.

### What does slap do with diff runs that extend to the end of output?

The block terminator requires `input[p] == output[p]` at the position after the last differing byte. When the run reaches `output_size`, no such position exists within `[0, output_size)` — the terminator's "phantom" position lands at `output_size`, past the last written byte.

**slap's `createUPS` emits blocks in this shape**, matching what beat, NUPS, and Tsukuyomi produce in practice. The apply path clips the terminator write against the output buffer (`applyUPS`'s OOB branch); `detectOOBBlocks` surfaces a parse-time warning. Forward apply and reverse apply both reconstruct cleanly.

## Malformed patches — structural

### What does slap do when xorData contains 0x00?

The spec implicitly forbids this — `0x00` is indistinguishable from the block terminator, so a well-formed patch cannot contain one inside xorData. A hand-crafted or corrupted patch could.

**slap's parser splits the run on the first `0x00` via `getUntilByte`**, producing two adjacent blocks where the second has `skip = 0`. The `UPSBlock` type doc notes this is a property of the encoded form, not an in-memory invariant.

### What does slap do when a block's total span exceeds output_size?

Common creation-tool artifact (NUPS, Tsukuyomi): the final block's terminator lands at `output_size` rather than `output_size − 1`.

**`applyUPS` clips out-of-bounds portions to remaining target space; `detectOOBBlocks` emits a summary warning at parse time.**

### What does slap do when a block lacks a terminator before end of body?

**Rejects at parse: `getUntilByte` fails with "terminator not found at offset …".**

### What does slap do on patch-CRC32 mismatch?

**Fatal at parse, before any body decoding. `--no-verify` does NOT downgrade.** Patch-CRC protects the decoding machinery itself; the flag's "proceed despite verification failures" stance is only meaningful when the field values being verified against are themselves trusted. Same shape as BPS.

### What does slap do on input/output CRC32 mismatch against the user-supplied file?

**Fatal by default at apply time, downgraded to warning under `--no-verify`.** Caught by `Main.verifySource` via `checkCRC`. Same shape as BPS source/target CRC.
