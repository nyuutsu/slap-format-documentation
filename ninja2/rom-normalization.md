# NINJA2 ROM-type normalization — slap's calls

The procedures follow `ninja2-convroms.txt` and the reference program `ninja2.php` (inside `ninja-2.0-win-beta20060726.zip`). This file records the decisions that are slap's own rather than the reference's, and the two places the implementation was followed over the document.

## Dispositions

**slap never refuses on ROM type alone.** Where the reference tool exits on an unrecognized image (no SEGA signature, unknown N64 magic, no SNES internal header it can validate), slap takes the image as-is and warns. Normalization is a guess about the input's shape; the patch's source checksum confirms or refutes the guess on its own, and a missing checksum is the creator's omission, not a reason for slap to be stricter than the format. A patch whose type has a procedure but whose checksum slots are empty gets a warning saying the normalized input cannot be confirmed.

**Ordering.** Normalization runs before the source hash is taken, so the hash covers the bytes the patch's checksums were computed over. Restoration runs after target verification, because the stored target MD5 also describes the clean form — the reference computes both MD5s over normalized files and re-prepends headers afterwards.

**What restores.** The NES headers (iNES, FFE), the SNES NSRT header, and the Lynx header return to the output after apply; UNIF data reinserts into its original container. Plain SNES copier headers and the GB SmartCard, PC-Engine Magic Super Griffin, and Sega SMD headers are dropped without restore, matching what the reference applier captures and what it lets go.

**UNIF rebuild needs an exact size match.** The reinsert walks the original chunk table and replaces each PRG/CHR payload with the next slice of patched data; a patch that resized the merged data leaves that walk nothing well-defined to do (the reference writes short or drops the tail silently). slap emits the merged form and warns, naming both byte counts.

**Structural impossibilities warn and pass through.** An SMD body that is not whole 16 KiB blocks, an interleaved SNES body that does not split into two equal halves of 32 KiB banks, a byteswapped N64 image with an odd byte count, a UNIF chunk declaring bytes past the end of the file: each is taken as-is with a warning naming the contradiction, where the reference would read short or die.

**The GB logo probe is not run.** The reference's Game Boy read checks the four logo bytes at 0x104 and refuses on a mismatch, its own comment doubting the check ("Does this cause unlicensed games to fail?"). The probe gates no transform — the SmartCard strip is decided by size alone — so slap skips it.

**Create normalizes both files.** The diff, the stored MD5s, and the size fields all describe the canonical forms, which is what makes the patch portable across differently-headered dumps. Convert with `--with` does the same on its re-create side.

**`--rom-type` on convert only disambiguates siblings.** A source patch that carries a ROM type keeps it; the flag may rename within the SMS/Game Gear pair (whose procedures are identical, and which NINJA2 stores in one combined slot) and nothing else. A cross-platform retag would tell appliers to normalize the input differently from the form the records were built against, so it is refused outright — `slap create` from the ROM files is the way to target another platform.

## Implementation over document

- The UNIF chunk table starts at 0x20 (the 32-byte UNIF file header), where the reference's NES read seeks; convroms says to start reading at $40. The implementation wins.
- convroms describes the SMD deinterleave as "16 KBYTE blocks (0x8000)"; the loops in `ninja2.php` read 16 KiB (0x4000) blocks with an 8 KiB half-stride, and slap follows the loops.

## Reference quirks noted, not reproduced

- The reference's SNES read reads a `GAME DOCTOR SF 3` signature it never consults; slap doesn't read it.
- The reference's SNES "possibly a beta cart" case (ROM-state odd, checksum failing at both probe positions) passes through as-is; slap does the same, quietly.
