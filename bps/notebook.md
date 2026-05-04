# BPS — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### Output-severity split

slap is chatty by design. A `--quiet` flag would suppress non-error narration
for batch use. No severity split planned.

### Typed metadata at the presentation layer

BPS metadata is stored as an opaque `ByteString` by the parser — correct; XML-awareness at parse time would be spec-exceeding. The "make it a type" ergonomics is tempting at the display boundary (`info`, `explain`, `--extract-metadata`), where a sum over opaque-bytes and attempted-XML could live without dragging the XML dep into the core.

Sketch — not a commitment, an idea:

```haskell
data MetadataDisplay
  = OpaqueBytes !ByteString
  | AttemptedXml !ByteString (Either XmlParseError XmlDocument)
```

We've seen no BPS patches carrying metadata in the wild. Reopen when one shows up.

### Inline metadata on create

slap exposes `--metadata FILE` for users who want to embed metadata when creating a BPS patch. A hypothetical `--metadata-inline "..."` would be structurally equivalent — no format obstacle — but isn't currently exposed. Not a decision against inline; just a CLI affordance that hasn't come up.

### In-memory-ness of slap

Everything slap does currently operates on at-least-one full copy of the ROM in memory, and often more (BPS delta creation holds source + target + a suffix array of source, together running several multiples of the source size). We are at least idly curious about ways to be more efficient with memory. Streaming — processing source and target in chunks without loading everything — is one option we have heard of. It appears to have real downsides: some creation modes become impractical, some apply operations need buffered state, cross-format conversion gets harder.

### Streaming CRC computation

slap computes each CRC over the whole relevant buffer once, rather than pipelining CRC computation into the parse/apply loops. This reflects slap's whole-file in-memory architecture. Streaming becomes relevant only if that premise changes — if it does, the ordering of the three CRC checks (patch first, then source, then target) survives; only the mechanics of how each is computed would change.

### Widening slap to honor BPS's arbitrary-width-integer frostmourne

The BPS spec endorses arbitrary-width integers; slap caps at `Int`. Removing the cap would mean widening the integer types in the shared `Measure` module (which every format imports from) along with the matching types in `rusty-slap`'s FFI boundary. A real refactor that touches a lot of files, but mechanical — not complicated.

We have seen no patches that come even close. Not a current priority.
