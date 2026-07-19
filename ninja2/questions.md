# NINJA2 — design questions

The wire format in `spec.md` is the part the spec sheet and `ninja2.php` state alike. This file collects the rest: the places they disagree, the corners the spec sheet leaves open, and the points where the several readers of the format — `ninja2.php`, librup, RomPatcher.js — diverge from each other and from slap. Each entry states the question, then slap's disposition.

A few entries carry the tag **(butters-its-toast-with-frostmourne)**, marking where the format's grammar grants expressive power far past anything a real patch uses.

### Is the preamble 1024 or 2048 bytes?

The spec sheet calls the header and info block "the first sector of the patch (1024 bytes)." The field widths it lists in the same section sum to 2048 — magic 6, encoding byte 1, then 84 + 11 + 256 + 48 + 48 + 8 + 512 + 1074 — and `ninja2.php`, librup, and RomPatcher.js all seek to `0x800` to reach the command stream. The prose and the field list disagree; the field list is right.

**slap uses 2048.** The preamble is `0x800` bytes and the command stream starts there; the "1024" is a slip in the spec sheet's prose.

### What does `system codepage` (PATCH_ENC `0`) mean, with no codepage named?

`PATCH_ENC` is `0` for the system codepage and `1` for UTF-8. But a `0` patch names no codepage — the spec sheet says to decode with the reading machine's, which is not a property the patch carries, so the same bytes decode differently on different hosts. (In `ninja2.php` the codepage is a configurable constant that no function ever reads, so the reference in practice hands the bytes straight to the terminal.)

**slap treats `0` as "encoding undeclared" and decodes the fields under the user's `--metadata-encoding` choice; `1` is UTF-8, read leniently.** A byte other than `0` or `1` is refused at parse — the spec defines only those two, and slap won't guess at an undefined encoding. There is nothing portable to derive from "system codepage," so for `0` slap asks the user rather than guessing a codepage the patch never stated.

### Is the overflow's `0xFF` masking part of the format?

The overflow bytes are stored XORed with `0xFF`. No written spec mentions it — not the filespec, not the conversion notes. It is nonetheless in every implementation: `ninja2.php` and librup both un-XOR on read and XOR on write, and RomPatcher.js the same. One patch we have shows it plainly — an `M`-mode overflow's first bytes, un-masked, spell the ASCII date text from the ROM's tail.

**slap XORs the overflow with `0xFF` on both read and write.** It is a canonical convention that simply never reached the written spec, not a quirk of any one reader.

### Are the overflow commands nested in the file record, or top-level?

Two readings of the command stream fit the bytes. The spec sheet places the `M`/`A` overflow inside the `0x01` file record, right after the two MD5s, present only when the sizes differ. `ninja2.php` and librup instead dispatch `M` and `A` from the outer command loop, as peers of `0x01`, `0x02`, and `0x00`. For any patch the reference writes the two readings consume identical bytes, because the writer emits the overflow exactly where the nested reading looks for it.

**slap uses the nested reading: the overflow is read only within the file record, and only when the two sizes differ.** It matches the spec sheet's own layout and reconstructs every real patch identically. The readings part only on input no writer produces — a stray `M` or `A` where the sizes are equal, which slap meets as an unknown command rather than an overflow. slap is the stricter reader there.

### What does slap do with a command stream that never opens a file?

The command grammar is flat: an XOR record and the end marker are peers of the open-file command, so a run of XOR records with no `0x01` ahead of them is admissible. No real patch is shaped that way — the reference always opens a file first, and handed a stream without one it reads an undefined file handle and stops there.

**slap applies the records over the source at the source's own size — no size change, no ROM type, no MD5 to verify against.** The open-file command is where a patch declares its target size, ROM type, and checksums; with none present, slap takes the source as the target's own frame and overlays the records. It is a shape the grammar allows and no writer produces.

### What does slap do with a command stream that never reaches its `0x00`?

The stream ends at a `0x00`, but a truncated or malformed patch can run out before one appears — a corner the spec sheet doesn't address. `ninja2.php` reads until it finds the `0x00` and treats a premature end as an error; librup and RomPatcher.js take the end of input as an end of stream.

**slap refuses a stream that reaches the end of input without its `0x00`.** A patch that just stops is truncated, and taking a cut-short patch as complete would hand back a wrong file with nothing said — so slap errors, matching `ninja2.php` and its own NINJA1, which refuses a missing binary footer the same way.

### Does slap apply a patch in reverse?

NINJA2 is built to run either way: the XOR records are self-inverse, the overflow reverses (an `A` that appends going forward truncates coming back), and the two MD5s let a reader tell which file it holds and so which direction to take. `ninja2.php`, librup, and RomPatcher.js all do this — a target matching the modified-MD5 is un-patched back to the source.

**slap runs a patch in both directions: `slap undo` hands the source back.** The direction is the verb's — apply goes forward, undo goes back — and the MD5s verify whichever end each direction claims. A shrinking patch's discarded tail comes back from its truncate-mode overflow; a shrink whose overflow is append-mode never stored that tail, and is the one shape that cannot go back.

### Does slap handle multi-file patches?

A single `.RUP` may carry several files: each `0x01` opens a new one, closing the previous. `ninja2.php`, librup, and RomPatcher.js all walk the whole list. The wild barely exercises it — one specimen in the survey is multi-file, and it is unreadable for a separate reason (see the preamble-variant question).

**slap reads a multi-file patch whole and shows it; applying, undoing, and converting are single-file, and create writes only the single-file form.** `info` and `explain` count the bundle and give each further file its row, and every act refuses the bundle by name rather than patching against its first file alone.

### What does slap do with the pre-filespec 2045-byte preamble?

One survey specimen — a 2004 patch by the format's own author, two years ahead of the 2006 spec — carries a 2045-byte preamble: its description field is three bytes short of the later 1074, so its first command sits at offset 2045 rather than 2048. Every reader hard-codes 2048 and lands three bytes into that patch's filename, reading garbage.

**slap hard-codes the 2048 preamble and cannot read the 2045-byte variant** — the same limitation every other reader has. It is a corner no tool in the ecosystem handles, not a slap-specific shortfall; a reader that reached for the earlier layout would be reaching past what any writer since 2006 has produced.

### How wide may a VLV grow? **(butters-its-toast-with-frostmourne)**

A VLV's count byte can name up to 255 value bytes, so the grammar admits an offset or length up to `2^2040` — the readme's boast of addressing "32,317,006 × 10⁶⁰⁰ bytes." No file approaches it; a real patch's counts are one or two bytes.

**slap accepts values up to `maxBound :: Int` and refuses a wider one at parse, loudly, rather than folding it to a wrong low value.** slap is an in-memory file tool; a value past what an `Int` can index has nowhere to land. The gap between that and `2^2040` is real and entirely hypothetical.

### How does slap read the metadata text fields?

The eight info fields are fixed-width and `0x00`-padded, and the spec sheet says little about their edges — how a full-width field with no terminator reads, what an embedded NUL means, whether the date's `YYYYMMDD` is one field or three, how a description's `\n` is meant. The reference program is no guide: its applier skips the whole info block, and the utility that would show it is empty in the preserved build, so it reads none of these fields.

**slap reads each field's bytes up to the first NUL, decodes them under the patch's declared encoding, and keeps them verbatim otherwise** — the date whole, the description's literal `\n` untranslated. These are presentation-level readings; they don't change which bytes a patch applies. Where a reader that does parse the fields differs — RomPatcher.js renders the `\n` as a line break — the divergence shows only in how the metadata reads, not in what the patch does.

### How does slap handle a headered or interleaved source?

Like NINJA1, NINJA2 detects a source's dump format and normalizes it — stripping a copier header, deinterleaving — before diffing and before hashing, so one patch fits several dumps. Where NINJA1 stops there, NINJA2 also puts the stripped header back on the output.

**slap normalizes the source for the eight ROM types that define a procedure, and restores the header on apply — matching the reference.** `raw` and FDS have no procedure and apply as-is. The per-platform procedures, the restore behavior, and the calls slap settled — including FDS, which the format lists but no tool gives a procedure — are in [`rom-normalization.md`](rom-normalization.md).
