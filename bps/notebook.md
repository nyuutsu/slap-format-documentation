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

### Info channel alongside the warning channel

slap has exactly one severity-level — `SlapWarning` in `Error.hs:311`, rendered and emitted via `warn` in `Main.hs:810-811`. 24 constructors currently; several of them aren't warning-shaped under the "something is off, the user may want to intervene" test.

Clearly info-shaped (nothing is off; slap is narrating what it did for you):

- `DefaultRomType`, `DefaultImageType` — "you didn't specify, we defaulted."
- `IncludingUndoByDefault`, `IncludingValidationByDefault` — "we added feature X for you by default."
- `OffsetShiftApplied` — "we did the shift."
- `SubformatConverted` — "we converted subformat X to Y."

Its own subcategory:

- `SourceHashesMissing` — actionable note: "here's a gap in what we could do; provide `--with SOURCE` to fill it." Not "info discarded" (my first read was wrong). Its own shape; maybe action-required-info or similar.

Genuinely ambiguous — the "info discarded" class (fires during format conversion to explain what couldn't be preserved):

- `FieldDropped`, `UndoDataDropped`, `ValidationBlockDropped`, `DisabledEntriesDropped`, `BlockDescriptionsDropped`, `MetadataDropped`, `FieldTruncated`, `EncodingGap`.

The uncertainty here is genuine:

- **Case for keeping as warnings:** some of these are significant (undo data, validation blocks) — user might want to pick a different target format. Warnings push visibility.
- **Case for moving all to info:** they're predictable from the format choice the user made. "You asked for IPS, IPS doesn't have undo, so undo got dropped" — that's reality-narration, not intervention-prompting.
- **Case for a third dedicated channel** named something like `field-lost-due-to-target-format`: carries the specific "this was information loss, not just a side-note" semantic without either overstating ("warning!") or understating ("info").

**If one of these moves, all of them arguably should.** Or: they all stay warnings because treating them uniformly is simpler. Or: they all become info because they're all narrative. The whole class seems to move together or not at all.

Genuinely staying warning-shaped (the "something to notice and possibly fix" items):

- `EmptyPatch`, `NoEOFMarker` — patches that look broken.
- `EBPTruncationMetaConflict`, `PlatformNotAvailable`, `PlatformAmbiguous` — actual conflicts.
- `ApplyOOBBlocksSkipped` — silent clipping during apply.

Slap-wide concern. Prerequisite for the metadata-parse question's "info channel surfacing" from the BPS questions doc. If pursued: introduce `SlapInfo` type and renderer; migrate the clearly-info-shaped constructors; make a decision (or continue deferring) on the "info discarded" class.

### Typed metadata at the presentation layer

BPS metadata is stored as an opaque `ByteString` by the parser — which is correct; XML-awareness in parse would be spec-exceeding, costly, and unhelpful. But the "make it a type" ergonomics is tempting for the places where metadata *is* actually looked at: `info`, `explain`, `--extract-metadata`.

Sketch — not a commitment, an idea:

```haskell
data MetadataDisplay
  = OpaqueBytes !ByteString
  | AttemptedXml !ByteString (Either XmlParseError XmlDocument)
```

Parse stays `ByteString`. The `MetadataDisplay` sum blooms at the `info`/`explain` boundary, computed lazily from the raw bytes. The XML parser dep lives in the presentation module only; the core parser stays dep-free. `AttemptedXml` carries the original bytes alongside the parse attempt so round-trip preservation is trivial — re-emitting the metadata just re-emits the bytes, no re-serialization step, no canonicalization drift, no `patch-checksum` breakage on round-trip.

Real question for later: does the XML-awareness *actually* pay for itself even in info/explain? If 0 of 1,495 corpus patches carry any metadata, and the rare ones that do might not even be XML, the typed variant is machinery for a vanishingly rare case. Opaque-bytes display (hex dump or plain text attempt, with a "this is metadata" header) might be sufficient.

Reopen when/if info-channel work happens. Until then: parse captures bytes, info displays bytes, type-shaping waits.
