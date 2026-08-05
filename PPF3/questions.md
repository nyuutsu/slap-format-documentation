# PPF3 — design questions

PPF3 is unusually well served by its own material: a written specification that names its byte order, its ceiling, and the sign of its FILE_ID.DIZ length, plus the source of both tools in two build variants. Most of what follows is about the places where the document stops and the code carries on, and about one thing the format deliberately stopped doing.

## Sizes

### May a patch change the image's length?

PPF3.txt states no rule, and the header carries nothing that could imply one — there is no field anywhere in it for the source image's size.

That absence is deliberate, and the archive's readme says so:

> Image filesize checking is GONE. Over the years it became clear that this feature was simply too inaccurate and confused most of the users with warnings which were mostly incorrect.

**slap refuses to create a PPF3 patch that would change the length, and applies one that does, noting the growth.**

The format has no way to describe a length change: records overwrite bytes at offsets, and there is no append and no truncate. So a patch that grows an image does so as a side effect of an offset landing past its end, which is a thing that can happen rather than a thing the format offers. Refusing on create keeps slap from producing that shape on purpose; accepting on apply keeps it reading patches that exist.

One thing worth knowing if you go looking in the creator for a size rule: it contains a comparison labelled "Check if files have same size", and both of the values it compares are measured from the same file handle, so the two are always equal and the check never fires. That is consistent with the readme's account — a feature retired, with a fragment of it left in place.

### How far does an offset reach?

PPF3.txt gives the offset as a 64-bit integer and puts the ceiling in figures: 9,223,372,036,854,775,807 bytes. That number is the largest a signed 64-bit integer holds, so the two statements agree with each other.

**slap carries PPF3 offsets at that full width.**

## Records

### Where does the record stream end?

The format supplies neither a terminator nor a record count, so a reader has to work it out from the file's length: the body is what remains after the header, less the FILE_ID.DIZ area if one is present.

The reference does exactly that, keeping a running budget and subtracting each record's size until it reaches zero.

**slap reads records while at least a record header's worth of bytes remains, and stops otherwise.**

Both approaches read the same well-formed patch identically. They differ on a damaged one: the reference's loop runs `while(count != 0)`, so a record that steps the budget past zero rather than onto it leaves the loop running against a nonzero count. Reading against what is left avoids having to be right about landing on zero exactly.

### Must records ascend, and what do two records covering one byte mean?

Not stated. The creator emits them in ascending order because it scans the two images once, forward.

**slap writes records in the order the patch lists them, so where two cover a byte the later one decides it.** Neither ordering nor overlap is refused.

### What does a count of zero mean?

PPF3 has no run-length form — a record's count is simply how many bytes follow. A count of zero is a record that writes nothing.

**slap accepts such a record and applies it, which does nothing.**

## Undo

### What does the undo byte promise?

That every record carries, after its patch bytes, the same number of bytes that were at that offset beforehand. The creator writes them from the original image in the same loop that writes the patch bytes from the modified one, so the two runs are the same length by construction.

**slap holds every record in an undo patch to that shape, and will not write a patch where some records carry undo bytes and others do not.**

PPF3.txt does not put the requirement in those words; it follows from one count governing both runs. It is an invariant slap keeps rather than one it found written down.

### Can a patch without undo data be reversed?

No. The bytes that were replaced are not in the file, and no amount of care recovers them — reversing needs either an undo patch or the original image. The reference says as much by refusing: asked to undo a patch whose undo byte is clear, it stops with "no undo data available".

## Text

### What encoding is the description in, and how is the field filled?

PPF3.txt gives the field's position and width and stops there. The creator copies bytes from its command line into the field and the applier prints them back out, neither one interpreting them, so what they mean depends on the machine that made the patch — which the patch does not record.

**slap pads with spaces, reads the field under `--metadata-encoding` defaulting to UTF-8, and writes UTF-8.**

There is no correct answer available for the encoding, so slap offers the reader the choice and defaults to the reading most likely to be right for a patch made recently.

### What happens to text too long for the field?

**slap clips at the last whole character that fits**, rather than at the fiftieth byte, which can fall inside a character.

### What if a FILE_ID.DIZ is longer than 3072 bytes?

PPF3.txt states the cap in prose, and the two-byte length field could carry far more than that.

**slap reads an over-cap area and reports it, and will not write one.** Reading honours the field; writing honours the document.

The reference resolves the same tension differently on the writing side: handed a longer file, it takes the first 3072 bytes and writes those. slap would rather say the file does not fit than quietly store part of it — and `--diz` is how a shorter one is supplied in its place, which keeps the patch and loses only what the format has no room for.
