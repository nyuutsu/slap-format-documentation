# PPF2 — design questions

PPF2 declares more than its layout: it names its byte order and says what its identity fields are for. What it does not say is how far its numbers reach, and no source was released to look it up in — so that answer came out of the programs themselves.

## Numbers

### How far does an offset reach?

PPF2.txt gives the field as "4 byte file offset" and says nothing about its sign, which leaves it looking like it should reach 4 GB.

**slap will not write an offset, a source size, or a FILE_ID.DIZ length above `0x7FFFFFFF`.**

The programs settle it. All four DOS executables in the archive are UPX-compressed, unpack cleanly, and carry the Turbo Pascal 6.0 runtime — the string "Portions Copyright (c) 1983,90 Borland" sits in each. Turbo Pascal 6 has exactly one 32-bit integer type, the signed `Longint`, and nothing unsigned at that width. A four-byte number in these programs is therefore signed whether or not anyone chose it, and a value with its top bit set is a negative one rather than a larger one.

Each of the format's three four-byte numbers was checked against this separately rather than assumed from the others.

## The two identity checks

### What happens when the image does not match what the patch expects?

The header carries two things to recognise an image by: its source length, which PPF2.txt says is "Used for Identification", and the 1024-byte block. `ApplyPPF.txt` says what its own program does when either fails to match, and it is the same thing in both cases — it asks whether to go on. It says so more gently about the size:

> "The filesize of the binfile does not match … CONTINUE? (y/n)" … Different CDRWin versions may add some more bytes to the binfile so this warning is not to be taken toooo seriously!

and much more sharply about the block:

> "The binfile does not to be the same one this PPF-Patch was made of! CONTINUE? (Suggestion: NO!) (y/n)" … you have a serious problem! … It's very very likely that you will burn a nice coaster if you continue!

but neither one stops the patch on its own. The difference between them is how loudly the user is warned, not whether they are allowed to proceed.

**slap treats both as advisory: it warns and applies, with `--no-verify` governing whether checks stop an apply at all.** Both mismatches carry the same weight in slap, which is warn-and-proceed.

That follows the reference, where both are questions rather than refusals. slap does not carry the reference's difference in tone between the two — a mismatch of either kind is one warning, and the user decides.

Worth knowing what the block does not tell you, since its warning is the alarming one. It is a sample from one position, so it says which release an image is; it says nothing about whether the rest of that image is intact.

## Records

### Is there a form other than the literal one?

PPF2.txt describes one record shape and gives one example, both literal. Nothing in it mentions a count of zero meaning anything in particular.

**slap writes only the literal form, and reads a count of zero as a record that writes no bytes.**

### Must records ascend, and what do two records covering one byte mean?

Not stated.

**slap writes records in the order the patch lists them, so where two cover a byte the later one decides it.** Neither ordering nor overlap is refused.

## Sizes

### May a patch change the image's length?

PPF2.txt states no rule. `MakePPF.txt` asks the person running it — "Please be sure that the filesize of the Original and Patched iso files are identical!" — which is an instruction to a user rather than a property of the format, and with no source released there is no way to tell whether the program enforced it.

**slap refuses to create a patch whose target is shorter than its source, and allows one that is longer.**

Shrinking has no representation: records overwrite bytes and there is no way to say a file got smaller. Growth is a different matter — the only size signal the format carries is the header field its own author called soft, so a growing patch is inside what the format tolerates, and slap says so rather than refusing.

## Text

### What encoding is the description in, and how is the field filled?

PPF2.txt gives the field's position and width and stops there. It says nothing about padding, and nothing about encoding — and with no source released, there is nothing to read the answer out of.

**slap pads with spaces, reads the field under `--metadata-encoding` defaulting to UTF-8, and writes UTF-8.**

The space padding is the weakest-founded thing slap does with this format: it is what the patches we have carry, but PPF2's own material never says it.

### What if a FILE_ID.DIZ is longer than 3072 bytes?

PPF2.txt states the cap in prose while the length field is four bytes wide, so the wire admits far more than the document allows. Patches carrying an over-cap area exist.

**slap reads one and reports it, and will not write one.** Reading honours the field; writing honours the document.

A patch whose FILE_ID.DIZ is too long to re-emit is not thereby stuck: its content can be extracted and a shorter one supplied with `--diz`, which keeps the patch and loses only the part the format has no room for.
