# PMSR — design questions

PMSR has no specification document, and for a long time it had no readable reference either: the description everyone worked from was a forum message, and the file it pointed at is no longer online. The answers below were settled by reading Star Rod's own source once we found it, and a few of them changed when we did.

## Numbers

### Are the record count, offsets and lengths signed?

Every number in the format is written by `ByteBuffer.putInt` on a Java `int`, and Java has no unsigned one. The top bit of each is therefore a sign.

**slap treats all three as signed, so the largest position or length it will write is `0x7FFFFFFF`.** A value above that is not a larger number but a negative one, and it is neither a value Star Rod could have written nor one it would read back the same way.

This corrects a reading we held for a while, taken from a reimplementation that reads the fields unsigned. That reimplementation was for years the only description of the format available to anyone, and being one bit out on a field nobody could reach is a small price for having made the format legible at all.

### What does slap do with an offset in the signed half?

**Refused when the patch is read, naming the record and the position.** Not left for the apply, which writes each record through a raw pointer and has no answer for a position behind the start of its buffer.

A negative length needs no separate answer: it reaches the read that would fetch the payload, which refuses one already. Nor does a nonsensical count, which runs the record walk past the end of the file and is named there.

## Records

### Must records ascend, and what do two records at one position mean?

Star Rod emits them in ascending order and never overlapping, because it walks the two files once and collects differences as it goes. The format states nothing, so a patch built by something else could do either.

**slap writes records in the order the patch lists them, so where two cover a byte the later one decides it.** Neither ordering nor overlap is refused.

That is what falls out of writing records in sequence, and it matches how every other absolute-offset format in slap behaves. Refusing an unordered patch would reject a file the format admits on the strength of one encoder's habit.

### What do bytes after the last record mean?

The count says how many records there are, so anything past the last one is unaccounted for — and unlike the formats whose streams simply run out, the leftover here can be any size at all, up to and including whole further records the count did not admit to.

**slap warns and ignores them.** The records that were counted apply normally.

Consistent with how slap answers unrecognized trailing bytes across formats. Refusing outright would discard a patch whose counted records are all intact.

## Sizes

### How much may a patch grow the file, and must it supply what it adds?

The output is as long as the furthest record reaches, so a single record can make the output arbitrarily larger than the source, and the bytes between the source's end and that record are zeros nobody supplied. Star Rod never does this: its growth record covers the whole extension.

**slap allows it, and does not require the extension to be covered.**

Refusing would mean ruling on the strength of an encoder's habit, in a format with no document to appeal to — the same reason unaligned offsets go unrefused elsewhere. What does bound it is the signed ceiling above, which caps a patch's reach at 2 GB, and slap's own limit on how large an output it will materialise at all.

### Is the source checked before applying?

The format carries no checksum, no source size, and no identifying field of any kind. There is nothing in a PMSR patch to check a source against.

**slap applies it to whatever it is given.** The reimplementation mentioned above hardcodes a check against one particular ROM, which is a sensible thing for a tool aimed at one game and not something the format asks for.

## Envelope

### Is Yay0 compression part of the format?

Star Rod compresses the finished patch under an option, so both a compressed and an uncompressed patch are ordinary output from it.

**slap accepts either, detecting the envelope by its own magic and unwrapping before it reads anything else.** It writes uncompressed patches.
