# NINJA1 — design questions

The wire format in `spec.md` is the part the spec sheet and `ninja.php` state alike. This file collects the rest: the places one carries a detail the other doesn't, and the corners the spec sheet leaves open. Each entry states the question, then slap's disposition.

A few entries carry the tag **(butters-its-toast-with-frostmourne)**, marking where the format's grammar grants expressive power far past anything a real patch uses — so the question is not only "what do we do" but "how far do we chase the hypothetical."

### Which byte follows `T` in the textual subformat — `0x0A` or `0x0D`?

The spec sheet's subformat table gives the textual identifier as `0x540D`, annotated `T\n`. `ninja.php` writes `NINJA1T` followed by `chr(0x0A)`, and its applier matches `"T" . chr(0x0a)`. The byte in the patches themselves, then, is `0x0A`.

**slap follows the running code: the textual subformat is `T` then `0x0A`.**

### What word marks a skipped textual checksum — `unk` or `unk.`?

The spec sheet describes the skip value as `unk.`, with a trailing period. `ninja.php` writes and reads `unk`, without one.

**slap accepts both.** `unk` is the form in the patches themselves; `unk.` is the form a reader of the spec sheet would reach for. Honoring both costs nothing and closes the gap between them.

### How many fields does a textual record line have, and what if one carries more?

The spec sheet writes the line as `OFFSET PATCH_BYTES` and gives no third field a meaning. The reference applier splits it with `preg_split("/ /", ...)` and binds the result with `list($offset, $patch)`, which takes the first two fields; any beyond them go unused. Its own encoder writes exactly two, separated by one space, so no patch it produced has a third for that binding to meet.

**slap refuses a line carrying more than two fields.** The format gives a third field no place, so a line holding one is malformed rather than generous. We could instead keep the first payload field as the reference does, but that binding follows from how `list()` fills its variables rather than from anything the format states, and taking it up would mean dropping bytes the line does carry without telling anyone.

### May the offset and the payload be separated by more than one space?

The spec sheet shows a single space and says nothing about runs. The reference splits on one space exactly, so a line padded with two produces three fields whose middle one is empty — and `list()` binds that empty field as the payload. Running the reference's own expression over `100  aabb` returns `offset="100"`, `payload=""`: the record writes nothing at all.

**slap reads across the padding and takes the payload that follows, and warns that it has done so.** Reading the run as a separator is the interpretation under which the line means what it appears to mean, and the format says nothing about runs either way. But the same patch then writes different bytes depending on which tool applies it, and that is the user's business, so it warns rather than notes.

### What ROM type does a patch record for a platform the tool doesn't normalize?

The spec sheet asks that a patcher without a normalization for some ROM type write `RAW` in the header, so every NINJA reader treats the patch the same way. `ninja.php` does this for ordinary-sized sources: building a patch for a type it has no procedure for (`nes`, `gba`, `n64`, …) against a source of 30 MiB or less, it records `raw`, so those names do not reach the wire. Above 30 MiB the maker follows a separate large-file path, which carries the declared type through. So in practice a NINJA1 header's type is `raw`, `snes`, `mega`, or `gb` for essentially every patch, with an un-normalized name reaching the wire only on the oversized-source path.

**slap preserves whatever ROM type it reads, and does not force un-normalized types to `raw`.** On parse this is plainly right: reporting what the patch claims beats overwriting it, and a type slap doesn't recognize at all is kept, not discarded. On create, slap does not replicate the reference's force-`raw`-for-small-sources rewrite — a small `nes` patch slap emits would carry the `nes` type where `ninja.php` would stamp `raw`. Since no type outside the normalize trio changes what gets applied, the divergence is one of round-trip identity, not of applied output. Whether slap should match the rewrite is open. Flagged, not settled.

### Is the compression gzip or zlib?

The spec sheet and readme call the compression gzip. The bytes are zlib: `ninja.php` uses PHP's `gzcompress`, which — despite the name — produces an RFC 1950 zlib stream, a two-byte header and an Adler-32 trailer rather than the RFC 1952 gzip container. It is a natural name to reach for; a reader feeding these to a gzip decoder just wants zlib instead.

**slap reads and writes the compressed subformats as zlib**, following the bytes on disk.

### How wide may an `offset` or `length` grow, and how far does slap follow it? **(butters-its-toast-with-frostmourne)**

Each field is introduced by a single width byte, so a field may be up to 255 bytes wide — the grammar reaches values up to `2^2040`. The readme frames this as the format's reach: addressing meant to keep working "for at least another decade" and beyond, "around 600 zeros longer than … a googolplex." The 2004 tool reached 32 bits, the width a PHP value of the day could hold.

So there is no single width to name. The grammar reaches as far as you like; the 2004 tool reached 32 bits; a modern PHP on a 64-bit host would reach 64. The format's width is open-ended by design, and 32 bits was simply where 2004 stood.

**slap accepts offsets and lengths up to `maxBound :: Int` — about 63 bits — end to end, and refuses anything wider.** This is faithful to the spec; a visible consequence is that slap reads and writes NINJA1 the 2004 tool could not: ones that exceed 4GiB. This is one of the few places where we intentionally agree to create something the reference tool wouldn't be able to apply.

### In what order do records apply, and what happens when they overlap?

The format fixes neither. `ninja.php` writes its records in one forward pass — copying the source up to each offset, then the payload, moving only forward — which suits the records it makes: always in ascending order, never overlapping. A patch shaped that way reconstructs exactly. Records that arrive out of order, or that overlap, are outside what the forward pass has an answer for.

**slap places each record at its true offset in wire order, whatever that order is; where two meet, the later one wins.** This reconstructs the maker's ascending, disjoint patches identically, and gives out-of-order or overlapping records a defined result too. slap does not yet warn on either — a gap worth closing, noted so the quiet isn't taken for a decision.

### What does slap do with a binary stream that never reaches its `EOF` trailer?

The trailer is mandatory, and a well-formed patch always carries one; a truncated or damaged patch might not. `ninja.php`'s loop reads until it reaches the `EOF` trailer, which its own output always provides.

**slap treats a binary record stream that reaches the end of input — or hits a zero-width offset byte — without the trailer as a hard error, naming the missing trailer.** A patch with no terminator is malformed, and reconstructing part of a file from one slap knows is truncated would hand back a plausible-looking wrong result.

### How does slap read a record whose offset collides with the `EOF` trailer?

The trailer `03 45 4F 46` is byte-identical to the header of a record with a three-byte offset of `0x454F46` (`"EOF"`). A parser at a record boundary cannot tell them apart without looking past them.

**On parse, slap stops at those bytes, exactly as the reference applier does — no lookahead.** A disambiguating parser could exist, but it would accept patches only slap-class readers could, which defeats the point of reading the format at all. **On create, slap prevents the collision from ever being emitted:** a record landing on `0x454F46` is shifted back one byte, with the source byte at `0x454F45` prepended to its payload — a no-op overwrite that moves the offset field off the sentinel. When there is no preceding byte to borrow — the source is absent, or too short to reach `0x454F45` — slap refuses to create the patch rather than emit bytes it could not read back.

### What do bytes after the `EOF` trailer mean?

Nothing, per the format — the binary body is defined only up to the trailer. A patch could still carry trailing bytes, from a tool that appended something or from damage.

**slap stops at the trailer and ignores whatever follows.** By slap's trailing-junk policy this should warn rather than pass silently; that warning is not yet emitted, and this entry is the standing note that the silence is a gap, not a decision.

### When the source's real CRC32 is zero, can slap tell it apart from "no CRC"?

In the *binary* subformat the skip sentinel overloads a legal value: an all-zero field means "no value," but a source whose CRC32 genuinely is `0x00000000` produces exactly those bytes, and the two are indistinguishable on the wire. (The same holds, far less plausibly, for an all-zero MD5 or SHA1.) The *textual* subformat has no such overload — there the skip marker is the literal word `unk`, and a real zero is written `00000000`, which reads back as the value it is.

**In the binary subformat slap reads an all-zero field as "no value" and skips the check** — the only reading consistent with the format, and the one `ninja.php` settled on in v1.01. A binary patch built against a source that happens to hash to zero carries no usable check for it; that is a property of the binary sentinel choice, and the textual path does not share it.

### How does slap handle a headered or interleaved source on apply?

The reference detects a source's dump format, normalizes it — stripping a copier header, deinterleaving — verifies against the normalized form, and writes that form as output without restoring the header. One patch can then fit several dumps of a game. The format would have every applier run that procedure before it trusts a source.

**slap treats the procedure as optional and the hash as primary.** The CRC/MD5/SHA1 is what actually settles whether this is the right file; so where slap has a procedure — `snes`, `mega`, `gb`, forward-only, the stripped header not put back — it normalizes as a convenience, and otherwise it verifies by hash and applies. It does not kick a source back demanding a normalization when the hash already agrees. On apply this posture sits comfortably. On create it is less settled: why slap does it the way it does is not currently recoverable, and the behavior may change once the intent is re-derived. The forward-only choice and the per-platform procedures are in [`rom-normalization.md`](rom-normalization.md).
