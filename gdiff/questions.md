# GDIFF — design questions

GDIFF is the best-specified format in this collection: every field has a stated width and sign, and every opcode a stated action. The questions that remain are all of one kind — what a reader should do when a field carries a value its type admits but its use cannot accept.

## Signed fields

### What does a negative length mean?

The spec types the length of DATA 248 and of COPY 251, 254 and 255 as `int`, which it defines as signed. Nothing in it says what a negative one means, because nothing in it contemplates one: the same section tells encoders to split a command rather than let a number grow past `0x7FFFFFFF`.

**slap refuses the patch, naming the record and the length.** A run of bytes has no negative extent, so there is no reading of the command that does anything.

The reference implementation instead treats it as nothing to do: its copy loop tests the length before its first iteration and falls straight out, so the command contributes no bytes and the walk moves on. That is a reasonable thing for a loop to do and it is not what the format asks for, so slap answers rather than continues.

### What does a negative position mean?

Same shape, and the same answer, one layer later: **slap refuses it when the patch is applied**, where the source it would read from is known. A position behind the start of a file names nothing.

The distinction from a length is worth keeping: a negative length is wrong on its face and can be answered while reading the patch, where a position needs the source in hand before anything can be said about it.

### Is a length that fits its field but overruns the old file a different question?

Yes, and it is the ordinary one every format has. **slap refuses a COPY whose run would read past the end of the source**, naming where the read ended and where the source does.

## Where the two implementations part

### DATA 248 with a negative length

Worth its own entry because the disagreement is not about the output but about where the *next command starts*.

Reading the length as the signed number the spec calls for, the reference's append returns having consumed none of the bytes that follow, so its next read lands on what it takes to be an opcode. Reading it as an unsigned count instead asks for four gigabytes of payload and fails.

**slap refuses the patch outright**, which is neither, and is the only one of the three that does not go on to describe a file built from a stream it has lost its place in.

## Encoding

### Which COPY form should slap emit?

The seven forms differ only in how wide their two numbers are, and a reader must take all of them, so the choice is purely one of size.

**slap emits the narrowest form whose fields hold the position and the length**, and splits a run too long for `0x7FFFFFFF` across several commands, as the spec directs.

### Is the compact DATA form worth using?

Opcodes 1 to 246 carry their own byte count, so a short literal run costs one byte where the `ushort` form would cost three.

**slap uses it for runs of 246 bytes or fewer**, which is what it is for.
