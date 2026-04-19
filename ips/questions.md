# IPS — design questions

Questions tagged **(uses-frostmourne-to-butter-its-toast)** mark places where the spec grants expressive power wildly beyond what convention actually uses. A compliant implementation would have to handle a possible-space radically larger than the practical-space. The archetype is EBP's trailing-JSON field: convention is a UTF-8 JSON object holding four specific strings, but EBP has no spec — the only actual rule is "it has a JSON at the end of it, therefore it is a valid EBP." The lack of rules creates the anarchy: a UTF-32-encoded JSON of any shape passes the same check, and a strictly-compliant parser has to accept it. These questions carry an extra dimension — not just "what do we do," but "how far do we chase the hypothetical."

### What record-end positions does slap accept on parse and emit on create?

The 24-bit offset and 16-bit size are independent field widths. Saturating both gives a last-byte position of `0xFFFFFF + 0xFFFF - 1 = 0x100FFFD` — about 64 KB past 16 MiB. The addressable range is `0x100FFFE` bytes.

**slap uses the arithmetic ceiling on both parse and create:** accept any record with `offset + payload ≤ 0x100FFFE`, emit up to the same bound. Symmetric — what we parse, we emit; what we emit, we parse.

Ecosystem:

- **Flips** apply has no ceiling check. Create refuses `targetlen > 16777216` with the assertion "the IPS format doesn't support that" (`libips.h:20`). No supporting math.
- **RomPatcher.js** is the same shape — create refuses `offset >= 0x1000000`, apply has no check.
- **lua-ips** has no check on either side.

`fe6.ips` (Fire Emblem 6 translation) has its final record at `0xFFFFFF` with payload extending past `0x1000000`. Flips and RomPatcher.js both apply it cleanly despite neither being able to produce it.

The format's math permits the full range; the ecosystem applies it fine; what slap emits is compatible with every existing applier. Flips's conservative create-side refusal is a design choice, not a format rule. slap doesn't inherit it.

### How is target size determined when IPS has no target-size field?

IPS has no target-size field in the wire format. Records name offsets; nothing says whether those offsets have to be within source length. In practice they don't: a record at an offset past the source end implicitly extends the target to include that record's write region.

**slap derives target size at apply time.** When the patch has no truncation marker, target size is `max(sourceSize, maxRecordEnd)` — the larger of the source length and the highest byte position any record touches. When a truncation marker is present, that marker's value is the target size (covered in the shrink/truncation entry).

Bytes in the target that fall within `[0, sourceSize)` come from source by default. Bytes in the growth region `[sourceSize, maxRecordEnd)` that no record overwrites are zero-filled.

Ecosystem — all three appliers in `tools/` behave this way:

- **Flips** `memcpy`'s source then `memset`s the tail to zero (`libips.cpp:115`).
- **RomPatcher.js** allocates a new BinFile of the computed size (default-zero) and copies source in (`RomPatcher.format.ips.js:138-141`).
- **lua-ips** extends with explicit `string.char(0)` fill.

Not really a design decision, more a consequence of the format: no target size declared, records name arbitrary offsets, so target size falls out of the records themselves and any untouched growth region has no other defined value.

### In what order does slap apply records, and what happens when they aren't offset-sorted?

Records appear in a patch in whatever order the encoder emitted them. The format doesn't require that order to match offset order; most encoders emit in offset order, but it's not a wire-level rule.

**slap applies records in wire order, period.** No sorting, no reordering.

If a patch's records aren't in offset order — call them unsorted — slap's plan is to emit a warning. Unsorted records are unusual and we want to tell the user. (Not yet implemented; see todos.md item 7.)

Why wire order: reordering at apply time would change behavior when records overlap (see overlap entry — the clobber semantics depend on apply order). Any other choice introduces ambiguity that has to be enforced by convention rather than by the wire itself. Wire order is unambiguous.

Ecosystem: Flips, RomPatcher.js, and lua-ips all apply in wire order. Flips has a commented-out `w_scrambled = true` at `libips.cpp:69-70` — the author considered warning on unsorted records but didn't ship it.

### What does slap do when records write to overlapping regions?

Two records can name write regions that share one or more bytes. The format doesn't forbid this. What the target ends up holding for an overlapping byte depends on which record wrote it most recently.

**slap permits overlap. Later records in wire order clobber earlier ones** — a direct consequence of wire-order apply semantics (see record-order entry). This is the same behavior every applier in the ecosystem exhibits; nobody checks for overlap at apply time.

Overlap is unusual in real-world patches — encoders typically emit disjoint regions — so slap's plan is to warn when it detects one. The warning is a hint that the patch is shaped oddly; the apply still succeeds. (Not yet implemented; see todos.md item 6.)

slap never emits overlapping records; any overlap the parser sees comes from patches produced by other tools.

Ecosystem: Flips, RomPatcher.js, and lua-ips all apply overlapping records in wire order with no warning. slap's intended warning would be slightly novel in that respect.

### How does slap handle the post-EOF truncation marker, and when does it emit one?

IPS records write bytes; they can't remove them. Target size is either declared explicitly by the truncation marker — three big-endian bytes after `EOF` — or derived as `max(sourceSize, maxRecordEnd)` when no marker is present. The marker declares the final size absolutely: a value greater than `maxRecordEnd` grows the target (zero-filling the gap); a value less than `sourceSize` shrinks it. Shrinking specifically requires the marker; growing doesn't — records can grow on their own.

**slap accepts the marker on parse.** On create, slap's plan is to emit the marker when `target < source`. Same semantics as Flips and RomPatcher.js. (Emit-on-shrink is not yet implemented; see todos.md item 2.)

Historically the marker wasn't an extension to a pre-existing marker-less format. The earliest tooling we can examine — SNESTool (MCA/Elite, 1996, DOS) — treats patches with and without it as two distinct formats, "IPS" and "IPS 2." Later tooling collapsed the distinction into a single format with an optional trailer.

What varies across implementations is the acceptance policy, not the wire format:

- **Modern appliers** (Flips, RomPatcher.js) accept any 3-byte marker and resize to its value.
- **SNESTool** accepts the marker only when `(size & 0xFFF) == 0x200` — the SMC-copier-header pattern any "2ᴺ KiB + 0x200" SNES ROM exhibits. Otherwise it rejects with `No File Cut, IPS2 size error !`.

**Future flag: `--require-smc-shaped-target-size`** (draft name). When set on create, slap refuses to emit a marker whose declared target size doesn't satisfy the SMC pattern, making the patch acceptable to SNESTool's parser. No-op for patches that don't need a marker. Not yet implemented.

Scope of the flag: necessary but not sufficient for end-to-end success in SNESTool. slap can check the IPS-layer predicate. slap can't check whether the user's source is actually an SMC ROM, whether copier-header conventions match, or whether the DOS environment can see the files.

See `ips2-snestool-report.md` for the evidence trail.

### What does slap do when a record's offset encodes as the EOF/EEOF sentinel?

The three-byte value `0x454F46` encodes, as an offset, to the same ASCII bytes as the `EOF` trailer. A parser reading a record boundary can't distinguish them. IPS32 has the analog collision at `0x45454F46`.

**Our plan:**

- **With source**: shift-and-prepend. When a record would emit at the sentinel, shift it back by one byte and prepend `source[offset-1]`. Record no longer sentinel-shaped; overlap is a no-op.
- **Without source** (direct format→IPS conversion, no ROM): reject. Detection is cheap (exact-equality scan); repair isn't possible without the neighbor byte. Reject-at-encode beats silent partial-apply downstream.
- **Parse**: no lookahead. A disambiguating parser is conceptually possible but produces IPS only slap-class parsers can read, defeating the point.

**Precision**: the rejection is exact-equality. Not a range, not probabilistic. No false positives.

### What shapes of bytes after `EOF` does slap accept? (uses-frostmourne-to-butter-its-toast)

Four shapes are possible after the `EOF` trailer: nothing, a 3-byte truncation marker, an EBP metadata JSON blob, or something else.

**Our plan:**

- **Nothing**: vanilla IPS, no further action.
- **Exactly 3 bytes**: Flips-style truncation marker. See truncation entry.
- **Starts with `{`**: EBP metadata. See EBP entry.
- **Anything else**: error. Silent-ignore would mask corrupted files, version mismatches, and patches produced by tools slap doesn't recognize. Refusing gives the user a sharper signal.

The three legitimate shapes don't overlap, so dispatch is unambiguous.

IPS32's trailer is `EEOF`, not `EOF`. IPS32 has no documented truncation extension and no EBP analog — trailing bytes after `EEOF` have no defined meaning. Atmosphère (the canonical applier) silently ignores them, so patches in the wild may carry arbitrary junk there without breaking on-console. slap accepts trailing bytes with a warning and discards them on parse; they aren't round-tripped on re-emit.

### What does slap do with RLE records whose count is zero?

An RLE record with a count of zero is "write this byte zero times" — a no-op. The format is silent on whether this is legal; ZeroSoft's terse spec says "Any nonzero value" for the count field, which reads as disallowing zero without saying so directly. Implementations split: Flips rejects; RomPatcher.js and lua-ips silently no-op.

**Our plan**: accept on parse, warn, never emit. A zero-count RLE is unusual enough to flag — it's almost certainly either a bug in the encoder or adversarial input — but it isn't corruption and doesn't need to fail the whole patch.

slap never emits a zero-count RLE; the question only comes up when parsing patches from elsewhere.

### How does slap handle EBP patches?

EBP is a sibling format that uses IPS records as its substrate. Same magic (`PATCH`), same trailer (`EOF`), plus a UTF-8 JSON metadata blob tacked on after the trailer. File extension is `.ebp`. Reference implementation: Lyrositor/EBPatcher, from the EarthBound translation community.

**Our plan:**

- **slap supports EBP.** On parse, detect by shape (trailer starts with `{`); capture the JSON blob verbatim; extract the canonical fields (`patcher`, `title`, `author`, `description`) leniently if they're present.
- **On create**, emit the four canonical fields with `"patcher":"slap"`. No custom fields.
- **No shrinking inside EBP.** The reference implementation has no concept of truncation. If a truncation marker were emitted before the JSON, EBPatcher and other EBP-aware tools would either ignore it (best case) or misparse the first 3 bytes of the JSON as a truncation value (worst case). slap doesn't go there. An EBP patch can grow or preserve target size; it can't shrink. Users needing shrink use a StandardIPS patch with a truncation marker instead.
- **Detection is shape-only, not schema-validating** (uses-frostmourne-to-butter-its-toast). The JSON is captured as opaque bytes. No actual JSON parser, no encoding validation. The EBP spec allows any Unicode encoding for the blob, which is absurd-but-true; building a parser to handle the absurd case is end-of-project polish at best.
- **Vanilla IPS tools reject EBP files.** They see trailing bytes that aren't a 3-byte truncation marker and error. EBP works only with EBP-aware tooling.

### How does slap handle IPS32 patches?

IPS32 is a sibling format with widened offsets. Magic is `IPS32` (5 bytes); trailer is `EEOF` (4 bytes). Record offsets are 4 bytes big-endian instead of 3. Size and RLE encoding are unchanged (still 16-bit). Reference implementations: leoetlino/sips for create; Atmosphère's `libstratosphere` for apply.

**Our plan:**

- **slap supports IPS32**: parse, apply, create.
- **All the semantic rules from StandardIPS carry over** at the wire-adjusted level: apply in wire order; overlap clobbers (with the same not-yet-implemented warning as StandardIPS); unsorted records warn (likewise not yet implemented); RLE count = 0 warns (likewise); the record ceiling is the arithmetic sum of the widened offset cap (`0xFFFFFFFF`) and the unchanged size cap (`0xFFFF`). Sentinel collision applies at `0x45454F46` — same shift-and-prepend with source, reject without.
- **Trailing bytes after `EEOF`** have no defined meaning. slap accepts them with a warning and drops them on parse; Atmosphère, the canonical applier, silently ignores them.
- **EBP + IPS32 is not a thing.** No ecosystem tool recognizes the combination. slap rejects it on parse and never emits it.

In practice IPS32 is applied by Atmosphère against mapped Switch modules, so the applier does things slap doesn't — clipping writes past the module's mapped size, skipping writes into protected regions. slap is a file tool; these divergences are real but out of its scope.
