# APS-GBA — design questions

`reference-harness.md` describes how to put a question to the original tool directly. Several entries here were settled that way, and the ones still open can be settled the same way.

## Checksums

### Which CRC-16 does the format use?

"CRC-16" names a large family. The patcher builds its lookup table at startup instead of storing it, so the table cannot be read out of the file directly — but the generator loop is plain enough: an unreflected `0x1021` polynomial, MSB-first, and the accumulator starts at `0xFFFF`.

**slap uses CRC-16/CCITT-FALSE.** Polynomial `0x1021` unreflected, init `0xFFFF`, no final XOR, no reflection of input or output. Check value `0x29B1`.

Confirmed twice over: from the generator and accumulator init in the binary, and by reproducing all 24 stored checksums in the one field-sourced APS-GBA patch available to us.

### What bytes does a record's checksum cover when its block runs past end of file?

The only question in this format with real consequences, and the one that most invites a wrong guess. A file whose length is not a multiple of 64 KiB has a final block that is partly real bytes and partly nothing. A checksum could reasonably cover the real bytes alone, or the real bytes zero-extended to a full block.

**slap checksums the whole 65536-byte block, zero-extended past end of file.**

This was measured, not inferred. Driving the original patcher against a 114979-byte pair produces a final record whose stored checksums match the zero-extended block exactly and match no other candidate. Its XOR payload tail past end of file is all zeros over a region where the two files genuinely differ elsewhere, which independently rules out the alternatives. The disassembly agrees and is the stronger statement of the two: the checksum routine takes no length parameter and loops against a hardcoded 65535 bound, so a short checksum is not something a caller can ask it for.

Worth recording because two careful static readings of the same binary predicted otherwise — the buffers are allocated once and never re-cleared, which suggests a stale tail — and both were wrong. The language runtime's own read call clears the unread remainder. Only running it settled this.

### Some third-party appliers compute that checksum differently. What follows for us?

At least one widely used reimplementation checksums only the bytes that exist, and treats a mismatch as fatal rather than advisory. Against a source whose length is not a multiple of 64 KiB, its result and ours will differ on the final record, in both directions: it will reject patches we write, and we will reject patches it writes.

**slap matches the original patcher and does not warn about it.**

## Records and blocks

### Must a record's offset be 64 KiB-aligned?

Every offset the original patcher emits is a multiple of 65536, because it only ever walks aligned blocks. Nothing in the format prevents an offset that is not.

**slap accepts any offset on read and emits only aligned ones.** Alignment is tested during detection, to tell a genuine patch from a file that merely opens with four plausible bytes, but the parser does not re-impose it.

### What happens when two records name the same offset, or overlapping ones?

Since only aligned offsets are ever emitted, two records covering the same block can only arise from a patch built by something other than the original tool. The format states no rule, and the patcher's own apply loop was not examined on this point.

**slap XORs each record's payload into the output in wire order, so overlapping records compose.** A byte covered twice receives `source ^ first ^ second`.

This is the position slap holds today and it rests on nothing but internal consistency — it is what falls out of applying records as XOR against one accumulating buffer. An applier that instead read each record's operands from the pristine source and wrote the result would give the last record the byte outright. Both are defensible from the format alone.

This one is open and settleable. The reference patcher's apply path is a known entry point, and the harness can put a two-record patch to it directly. Until that is done, treat the composing behavior as slap's convention rather than the format's rule.

### What does a trailing fragment shorter than a whole record mean?

The record stream runs to end of file with no count and no terminator, so a file whose length is not `12 + 65544n` has bytes left over that cannot form a record.

**slap warns and discards the fragment.** The records that did parse apply normally.

Consistent with how slap treats unrecognized trailing bytes across formats. Refusing outright would discard a patch whose every complete record is intact and applies cleanly.

## Sizes and verification

### Are the header's two sizes a requirement, or a description?

They are written from the lengths of the two files the patch was built from. Whether a reader should insist the file in hand matches is unstated.

**slap treats a source size mismatch as advisory, not fatal.**

Uniform with slap's stance elsewhere: size and checksum disagreements inform, and `--no-verify` governs whether they stop the apply. A tool that refuses on size alone rejects a file that the per-record checksums would have judged more precisely anyway.

### What happens when a record's source checksum doesn't match the file in hand?

**Fatal by default; `--no-verify` downgrades it to a warning and proceeds.** Slap-wide policy for checksum-bearing formats, inherited here without an APS-GBA-specific decision.

The per-record checksums are a sharper instrument than the header sizes: they say which block disagrees, not merely that something does.

## Identity

### The magic is four bytes and another format's is those four plus one. How is that resolved?

APS-GBA opens `APS1`. APS-N64 opens `APS10`. Every APS-N64 patch therefore opens with a valid APS-GBA magic, and a detector that tests the shorter one first claims all of them.

**slap tests the longer magic first, and re-routes on record shape when the longer test passes but the structure does not fit.**

Longest-magic-wins is the general rule; the structural re-route exists because prefix collision makes magic alone insufficient here. This is the only pair in slap where that is true today.

Worth noting for anyone auditing coverage: the shared `.aps` extension makes the two easy to conflate by eye. Of the five files opening `APS1` in slap's own test data, one is APS-GBA and four are APS-N64.
