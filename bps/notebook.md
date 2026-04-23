# BPS — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### `verifyFileSizeAdvisory` warning is dead code in practice

Tracing `verifySource` in `app/Main.hs:694-714`:

1. `checkCRC` runs first. Without `--no-verify`, CRC mismatch calls `die`.
2. The `unless noVerify $ do` block — which contains the advisory warning — is skipped when `--no-verify` *is* set.

So the advisory warning only fires when (a) CRC passes or is absent, (b) file size disagrees, and (c) `--no-verify` is not set. For any format with a CRC over the whole source (BPS, UPS, NINJA1, and others), (a) and (b) almost never hold simultaneously — a file of the wrong size CRCs wrong too, and `checkCRC` dies first.

The advisory was designed to add a specific diagnostic on top of the CRC gate. Current code ordering means it adds nothing in the common path.

Open question: revive it, or remove it?

- **Revive**: the "your file is N bytes but the patch declares M" message is genuinely sharper than a CRC mismatch. Options: (i) run the advisory check *before* `checkCRC` so its warning fires regardless of what the CRC does next, or (ii) bundle the file-size detail into the CRC error message itself ("source CRC mismatch — note also that your file is N bytes but the patch declared M").
- **Remove**: if we don't plumb a pre-CRC warning, the advisory is clutter; everything it's supposed to provide is subsumed by the CRC fatal.

Slap-wide concern, not BPS-specific. Every format populating `verifyFileSizeAdvisory` inherits the same dead-code outcome.

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
