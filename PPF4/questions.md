# PPF4 — design questions

PPF4 has no prose specification, but it is not undocumented: its applier carries a description of the layout in a comment block, and its maker's header comment explains most of the design decisions. Between them the two sources answer more than the silence suggests — and where they do not, there is code to read.

## Numbers

### Is the offset signed?

No, and this one is settled by a deliberate choice rather than by a default. `ppfmaker.cpp` defines both `s32` and `u32` in the same block of typedefs, and declares the field `u32`. Its applier reads the same field with `readU32`.

**slap treats the offset as unsigned, so it reaches the full four-byte range.**

### Why four bytes, when the format it is named after uses eight?

The author says, in the maker's header comment:

> Notably it includes a 32-bit offset instead of 64 … the former is a limitation of the Lua patcher (no native support for 64-bit types).

The applier's own comment gives the requirement it had to meet: "The patch requires a format that can handle files of around 150 MB". Four bytes covers that with room to spare.

### Which end of the offset comes first?

Neither source says. The maker writes the field with a single `fwrite` of a `u32`, which lays down whatever order the machine building the patch uses; the applier reads it with `readU32`, whose order is a property of the library behind it rather than of the format.

**slap reads and writes little-endian.**

Both tools were built and run on x86, so that is the order every patch in existence carries. Strictly the format says only "four bytes, read back as the same number", and the byte order belongs to the maker-and-applier pair rather than to the format — but there is only one such pair.

## Records

### What does a command byte other than 0 or 1 mean?

Nothing is defined for one. The applier's loop tests for each of the two commands and does nothing when neither matches — but it has already consumed the six header bytes by then, and does not consume the payload, so the next record is read from inside the previous one's data.

**slap refuses a patch carrying a command it does not recognise.**

Reading on from that point cannot produce anything meaningful, so there is no reading to fall back to; naming the byte is more use than continuing.

### Must replaces come before adds?

Yes, and this is one of the few rules the format actually enforces. The applier begins in replace mode, switches to append mode at the first add, and refuses a replace after that.

**slap holds patches to the same order and refuses one that breaks it.** The rule is not a convention here; it is checked, with its own error.

### May a replace reach past the end of the file?

No. The applier refuses a replace whose offset is past the end, and one whose offset plus count runs past it. Replace is bounded to the file as it was before patching.

**slap refuses both shapes.** Growth is what add is for, and letting replace do it as well would make the two commands mean the same thing.

### Must replaces ascend, and what do two covering one byte mean?

Not stated, and not checked.

**slap writes records in the order the patch lists them, so where two cover a byte the later one decides it.**

### How many bytes does a record need, and what happens to a tail too short for one?

Seven: one for the command, four for the offset, one for the count, and at least one of payload. The maker never emits a record without payload — every one of its record writes is guarded on there being at least one byte to write — so the smallest record it produces is seven bytes, and its applier enforces the mirror image: it needs seven bytes to begin a record and refuses the whole patch, `ERROR_BAD_PATCH_INST`, if fewer remain. A leftover of one to six bytes is a patch that ends mid-record, not one that ended cleanly.

**slap follows the reference: a record needs seven bytes to begin, and a trailing run of one to six is refused by name.** That covers both shapes the maker cannot make — a fragment shorter than a header, and a bare six-byte header with no payload — since the reference admits neither.

This is settled deliberately rather than by slap's general leniency about trailing bytes. Elsewhere a format that leaves its tail undefined earns a warning and an apply that proceeds; PPF4's tail is not undefined, it is defined as an error by the only applier the format has, so the faithful answer is to refuse it too.

## Sizes

### May a patch change the file's length?

It may make it longer, and that is the whole point. From the maker's header comment:

> adds support for adding data to the end of files. The latter is necessary for translations

**slap creates patches that grow a file and refuses to create one that would shrink it.**

Shrinking is not expressible: there is no truncate command and no output-size field, so nothing in the format can say a file got smaller.

### What happens if a patch is applied twice?

The file grows twice. Appending is unconditional — nothing in a patch or a file records that a patch has already been applied — and the maker's comment says so plainly:

> running an already expanded file through a second time will cause the file to be expanded again with duplicated data

**slap cannot detect it either**, and nothing in the format would let it. Worth knowing when applying a patch that adds data: the operation is not one you can repeat safely, and there is no check that will catch a second run.

## Text

### What encoding is the description in, and how is the field filled?

Filled with zero bytes, which the maker does explicitly. Neither source says anything about encoding.

**slap reads the field under `--metadata-encoding`, defaulting to UTF-8, and never writes anything into it.** The maker takes no description, so slap's create fills the 50 bytes with zeros to match, and a description reaches a reader only off a patch some other tool wrote.

The zero fill is worth care on the reading side: text that stops short of 50 bytes is followed by zeros rather than spaces, so a reader trimming only whitespace will carry them into whatever it displays.
