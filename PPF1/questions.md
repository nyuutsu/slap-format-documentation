# PPF1 — design questions

PPF1's documents describe a layout and very little else, so most of what follows is settled from the two C files shipped beside them — and one thing is settled only by what the format's own examples happen to show.

## Byte order

### Which end of the offset comes first?

Neither document uses the word "endian". Both tools read and write the field with no swapping — the applier does `fread(&Offset, sizeof(Offset), 1, ...)` into a `long`, and the creator writes one out the same way — so a patch carries whatever order the machine that made it used. The archive ships PC and Amiga binaries built from that same source, which means patches in both orders can exist and nothing in one says which it is.

**slap reads and writes little-endian, and carries a separate dialect for the other order.**

Little-endian is what the documents' own worked examples show: `D0 F9 15 00` for offset `0x0015F9D0`, twice. That is a demonstration rather than a rule, but it is the only statement either document makes on the subject.

A patch in the other order is not malformed. It came from an Amiga, and the dialect is how you say so.

## Records

### Both documents describe a run-length record. Does anything write one?

The applier implements it. The creator never emits one: its record path writes an offset, a count, and that many literal bytes, and there is no branch anywhere in it that produces a zero count.

**slap reads it and does not write it.** A form both documents describe and the original applier handles is one to accept; a form the original creator never produced is not one to start producing now.

### Must records ascend, and what do two records covering one byte mean?

Neither document says. The creator emits them in ascending order because it walks the two images once, forward.

**slap writes records in the order the patch lists them, so where two cover a byte the later one decides it.** Neither ordering nor overlap is refused.

### How does a reader know the records have ended?

The applier stops when the read of the next offset returns nothing — end of file, with no count and no terminator.

**slap does the same.** There is nothing else available: the format carries no record count, and no byte value is reserved to mark the end.

## Sizes

### May a patch change the image's length?

Neither document states a rule. The creator refuses outright — it measures both files first and stops with "Images doesnt match (length)!!" if they differ. The applier neither checks nor cares; it seeks to each offset and writes, and a record reaching past the end would extend the file.

**slap refuses to create a PPF1 patch that would change the length, and applies one that does.**

Refusing on create is what the original creator does, and costs nothing: PPF1 was made for disc images, where the length is fixed by the medium. Accepting on apply is what the original applier does, and refusing there would reject patches that already exist and work.

### How far does the format reach?

`ppf-doc.txt` puts 2 GB in its feature list, and the applier holds the offset in a `long` and hands it to `fseek`, whose offset parameter is signed.

**slap will not write an offset above `0x7FFFFFFF`.** The stated ceiling and the type the authors chose agree. The field is four bytes wide, so a value above that is not a further position but a negative one.

## Text

### What encoding is the description in?

Nothing says, and nothing could: the creator copies bytes from its command line into the field and the applier prints them back out, neither one interpreting them. What they mean depends on the machine that made the patch, which the patch does not record.

**slap reads the field under `--metadata-encoding`, defaulting to UTF-8, and writes UTF-8.** There is no correct answer available, so slap offers the choice and defaults to the reading most likely to be right for a patch made recently.

### How is the field filled, and what happens to text too long for it?

`ppf.txt` says space-padded, and the creator fills it that way.

**slap pads with spaces and clips text at the last whole character that fits**, rather than at the fiftieth byte, which can fall inside a character.
