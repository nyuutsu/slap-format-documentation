# Chatlog extract: 2026-04-11 aesthetic review

Suggestions from a "is this pretty" audit. IPS/Apply.hs was called out
but has since been polished to the BPS bar, so it's omitted here.

## 1. Detect.hs: guard ladder → probe table

**Status: done.** `MagicPrefix` newtype, `FormatProbe` record,
`magicProbes` table ordered by descending prefix length, walked with
`Data.List.find`. Each format's `Types.hs` now owns a named magic
bytes constant used by detection, parsing, and creation alike. PPF
and XDelta1 detection prefixes stay as inline literals (the format
modules don't use those short prefixes).

## 2. GDIFF/Types.hs: four small issues

All still present:

- `commandOutputSize :: GDiffCommand -> Int` — should return `Length`.
- `GDiffData ByteString` — positional constructor, sibling `GDiffCopy`
  has named fields. Should get a field name (`gdiffDataPayload` or
  similar).
- `gdiffCopyLength :: !FileSize` — a copy length is not a file size;
  `Length` is right there.
- `data GDiffPatch = GDiffPatch { gdiffCommands :: [GDiffCommand] }` —
  single-field wrapper that should be a `newtype`.

Ripple: GDIFF/Apply.hs, GDIFF/Parse.hs, GDIFF/Describe.hs,
GDIFF/Create.hs. All within the GDIFF family.

## 3. Length/FileSize slippage

Measure.hs defines `lengthToFileSize` etc., but not every call site
uses the right type. GDIFF is the clearest remaining case (see above).
IPS was the worst offender and has been cleaned up. A broader audit
may find a few more spots.

## 4. SomePatch.hs import wall

80+ lines of qualified imports, growing with each new format. This is
the architectural price of the closure-based existential spine. The
chatlog called it "the right architectural choice" and said there's no
obvious fix that keeps the spine. Not actionable — just noted.
