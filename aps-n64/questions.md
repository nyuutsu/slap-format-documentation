# APS-N64 — design questions

APS-N64 comes with a specification and with its authors' own applier and creator in C. That is more than most formats here have, and it means the questions below are mostly about the two disagreeing, rather than about neither saying anything.

## Where the document and the code differ

### The header byte ranges do not add up

`aps.txt` gives type 0's destination size as "BYTE 57-61", which is five bytes for a field both programs read and write as a four-byte C `long`. It gives type 1's pad as "BYTE 69-74" and the size that follows as "BYTE 75-79", which is six bytes and five.

**slap follows the code: four bytes for the size, five for the pad.** `n64caps.c` writes the pad as exactly five `fputc(0)` calls, and both programs read and write the size with a single `long`.

The ranges look like an off-by-one in how they were written down rather than a description of anything, and the fields either side of them are consistent with the code throughout. Worth recording because the document reads authoritatively and a reader following it would be one byte out from the first record onward.

### Type 0 is specified, and no program the authors shipped will read it

`n64aps.c` refuses any patch whose type byte is not `1`, printing "Unable to Process Patch File" and stopping. `n64caps.c` only ever writes `1`.

**slap reads both, and writes type 0.**

Type 0 is what the format offers for a file that is not an N64 image, and refusing to read it would leave the simpler of the two forms unusable for no reason we can point at. Writing it is the right choice for a general tool: slap has no business claiming a source is an N64 image and copying identity fields out of it unless it is one.

The consequence is worth stating plainly: a type 0 patch slap writes is not one the authors' applier will take. A tool that only reads type 1 was written for a job where every patch was for one console.

## Signed fields

### How far does the format reach?

`aps.txt` says the format "should facilitate patching of files up to 2Gb", and both programs hold the record offset and the destination size in a C `long`, handing the offset to `fseek`, whose offset parameter is signed.

**slap will not write an offset or a size above `0x7FFFFFFF`.** The document's stated ceiling and the type its authors chose agree, and a value past it is a negative position rather than a further one.

A reimplementation reads these fields unsigned and so reaches twice as far. Where it and the authors' own programs differ about the format, the authors' programs and the document that came with them are what slap follows.

## Records

### Must records ascend, and what do overlapping ones mean?

Unstated. `n64caps.c` emits them in ascending order because it walks the two files once.

**slap writes records in the order the patch lists them, so where two cover a byte the later one decides it.** Neither ordering nor overlap is refused, in line with every other absolute-offset format here.

### What does a run-length record with a count of zero mean?

Unstated. It writes nothing, and nothing in the format forbids it.

**slap accepts it and applies it, which writes nothing.** Refusing a record that does nothing would reject a patch that is otherwise entirely well formed.

It passes without remark. IPS says something about the same shape, and saying something here too would be reasonable; it has simply not been done.

### What do bytes after the last whole record mean?

The stream runs to end of file, so a patch whose length leaves fewer bytes than a record needs has a fragment at the end.

**slap warns and ignores it.** The records that did parse apply normally, consistent with how slap answers unrecognized trailing bytes across formats.

## Verification

### Is a destination size mismatch fatal?

The header states what the output should be, and the authors' applier resizes the file to match it rather than checking anything. If the truncation fails it prints a complaint and carries on.

**slap treats the size as a description and applies regardless**, resizing the output to it.

### What do the identity fields prove?

Cart ID, country and CRC are copies of bytes in the ROM's own header, not values computed over its contents, so they identify which game an image is and not whether it is intact.

**slap checks them where a type 1 patch carries them, and treats a mismatch the way it treats any failed check** — fatal by default, downgraded by `--no-verify`.

Worth knowing what that check is worth: a ROM with one byte changed in its body still matches all three.

### Which country codes are recognized?

`docs/aps-n64/country-codes.md` carries the table slap reads them with. The common codes are well attested; the rarer ones are collected from N64 header lore rather than from anything in this format's own material, and are marked there as unconfirmed.

They affect a displayed name and nothing else.
