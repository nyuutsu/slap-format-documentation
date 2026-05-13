# IPS — design questions

Questions tagged **(uses-frostmourne-to-butter-its-toast)** mark places where the spec grants expressive power wildly beyond what convention actually uses. A compliant implementation would have to handle a possible-space radically larger than the practical-space. These questions carry an extra dimension — not just "what do we do," but "how far do we chase the hypothetical."

### What record-end positions does slap accept on parse and emit on create?

The 24-bit offset and 16-bit size are independent field widths. Saturating both gives a last-byte position of `0xFFFFFF + 0xFFFF - 1 = 0x100FFFD` — about 64 KB past 16 MiB. The addressable range is `0x100FFFE` bytes.

**slap uses the arithmetic ceiling on both parse and create:** accept any record with `offset + payload ≤ 0x100FFFE`, emit up to the same bound. Symmetric — what we parse, we emit; what we emit, we parse.

### How is target size determined when IPS has no target-size field?

IPS has no target-size field in the wire format. Records name offsets; nothing says whether those offsets have to be within source length. In practice they don't: a record at an offset past the source end implicitly extends the target to include that record's write region.

**slap derives target size at apply time.** When the patch has no truncation marker, target size is `max(sourceSize, maxRecordEnd)` — the larger of the source length and the highest byte position any record touches.

### In what order does slap apply records, and what happens when they aren't offset-sorted?

Records appear in a patch in whatever order the encoder emitted them. The format doesn't require that order to match offset order; most encoders emit in offset order, but it's not a wire-level rule.

**slap applies records in wire order, period.** No sorting, no reordering.

If a patch's records aren't in offset order — call them unsorted — slap's plan is to emit a warning. Unsorted records are unusual and we want to tell the user. (Not yet implemented; see todos.md item 7.)

Why wire order: reordering at apply time would change behavior when records overlap (see overlap entry — the clobber semantics depend on apply order). Any other choice introduces ambiguity that has to be enforced by convention rather than by the wire itself. Wire order is unambiguous.

### What does slap do when records write to overlapping regions?

Two records can name write regions that share one or more bytes. The format doesn't forbid this. What the target ends up holding for an overlapping byte depends on which record wrote it most recently.

**slap permits overlap. Later records in wire order clobber earlier ones** — a direct consequence of wire-order apply semantics (see record-order entry). This is the same behavior every applier in the ecosystem exhibits; nobody checks for overlap at apply time.

Overlap is unusual in real-world patches — encoders typically emit disjoint regions — so slap's plan is to warn when it detects one. The warning is a hint that the patch is shaped oddly; the apply still succeeds. (Not yet implemented; see todos.md item 6.)

slap never emits overlapping records; any overlap the parser sees comes from patches produced by other tools.

### How does slap handle the post-EOF truncation marker, and when does it emit one?

The truncation marker is three big-endian bytes placed after the `EOF` trailer, declaring the target file's final size. When no marker is present, the final size comes out of the patch itself: either the source's length, or the point the records reach, whichever goes further.

slap emits the marker only when the target is smaller than the source.

slap only uses the marker to change the size of the target, if the marker would actually result in truncation.

slap prioritizes the marker. if the records write across the whole file, and the marker snips off the latter half of the file, then, the result is a halved file and some wasted effort. this should definitely emit at least one if not several warnings.

### What does slap do when a record's offset encodes as the EOF/EEOF sentinel?

The three-byte value `0x454F46` encodes, as an offset, to the same ASCII bytes as the `EOF` trailer. A parser reading a record boundary can't distinguish them. IPS32 has the analog collision at `0x45454F46`.

- **With source**: shift-and-prepend. When a record would emit at the sentinel, shift it back by one byte and prepend `source[offset-1]`. Record no longer sentinel-shaped; overlap is a no-op.

- **Without source** (direct format→IPS conversion, no ROM): reject. Detection is cheap (exact-equality scan); repair isn't possible without the neighbor byte. Reject-at-encode beats silent partial-apply downstream.

- **Parse**: no lookahead. A disambiguating parser is conceptually possible but produces IPS only slap-class parsers can read, defeating the point.

### What shapes of bytes after `EOF` does slap accept?

Four shapes are possible after the `EOF` trailer: nothing, a 3-byte truncation marker, an EBP metadata JSON blob, or something else.

- **Nothing**: vanilla IPS

- **Exactly 3 bytes**: truncation marker

- **Starts with `{`**: EBP metadata. See EBP entry.

- **Anything else**: error

IPS32's trailer is `EEOF`, not `EOF`. IPS32 has no documented truncation extension and no EBP analog — trailing bytes after `EEOF` have no defined meaning. Atmosphère (the canonical applier) silently ignores them, so patches in the wild may carry arbitrary junk there without breaking on-console. slap accepts trailing bytes with a warning and discards them on parse; they aren't round-tripped on re-emit.

### What does slap do with RLE records whose count is zero?

An RLE record with a count of zero is "write this byte zero times" — a no-op. The format is silent on whether this is legal; ZeroSoft's terse spec says "Any nonzero value" for the count field, which reads as disallowing zero without saying so directly. We accept on parse and warn.

### How does slap handle EBP patches?

EBP is a sibling format that uses IPS records as its substrate. Same magic (`PATCH`), same trailer (`EOF`), plus a UTF-8 JSON metadata blob after the trailer. File extension is `.ebp`. Reference implementation: Lyrositor/EBPatcher.

- **slap supports EBP.** On parse, detect by shape (trailer starts with `{`); capture the JSON blob verbatim; extract the canonical fields (`patcher`, `title`, `author`, `description`) leniently if they're present.

- **On create**, emit the four canonical fields with `"patcher":"slap"`. No custom fields.

- **No shrinking inside EBP.** The format does not have truncation.

- **Detection is shape-only, not schema-validating** (uses-frostmourne-to-butter-its-toast)

The canonical tool uses UTF-8 JSON to store four string key-value pairs. So far as I can tell the intention was "store these four strings somehow". One could construct a patch with arbitrary other stuff in the JSON that the tool would not object to but also not do anything with. We only care about plucking out these four strings.

### How does slap handle IPS32 patches?

IPS32 is a sibling format with widened offsets. Magic is `IPS32` (5 bytes); trailer is `EEOF` (4 bytes). Record offsets are 4 bytes big-endian instead of 3. Size and RLE encoding are unchanged (still 16-bit). Reference implementations: leoetlino/sips for create; Atmosphère's `libstratosphere` for apply.

**Our plan:**

- **slap supports IPS32**: parse, apply, create.
- **All the semantic rules from StandardIPS carry over** at the wire-adjusted level: apply in wire order; overlap clobbers (with the same not-yet-implemented warning as StandardIPS); unsorted records warn (likewise not yet implemented); RLE count = 0 warns (likewise); the record ceiling is the arithmetic sum of the widened offset cap (`0xFFFFFFFF`) and the unchanged size cap (`0xFFFF`). Sentinel collision applies at `0x45454F46` — same shift-and-prepend with source, reject without.
- **Trailing bytes after `EEOF`** have no defined meaning. slap accepts them with a warning and drops them on parse; Atmosphère, the canonical applier, silently ignores them.
- **EBP + IPS32 is not a thing.** No ecosystem tool recognizes the combination. slap rejects it on parse and never emits it.

In practice IPS32 is applied by Atmosphère against mapped Switch modules, so the applier does things slap doesn't — clipping writes past the module's mapped size, skipping writes into protected regions. slap is a file tool; these divergences are real but out of its scope.
