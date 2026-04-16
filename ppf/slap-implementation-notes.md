# slap implementation notes: PPF family

Observations and recommendations about how slap should structure
its PPF handling. Not a format specification — see `spec.md` for
that. Not a research findings file — see `findings.md` for corpus
observations.

## Current state (as of this review)

slap's PPF handling lives in a single module `Slap.PPF` with files
`Types.hs`, `Parse.hs`, `Apply.hs`, `Create.hs`, `Describe.hs`.
Four versions (PPF1/2/3/4) are treated as variants of a single
format. `PPFPatch` carries per-version data in `Maybe`-gated fields
(`ppfFileSize`, `ppfValidation`, `ppfImageType`, `ppfHasUndo`).

Dispatch: `Slap.Detect.magicProbes` has a single entry for PPF:

```haskell
FormatProbe (MagicPrefix "PPF") (PatchDirect FormatPPF)
```

When a file starts with the 3-byte `"PPF"` prefix, it's routed to
`FormatPPF`. The actual version (1/2/3/4) is then resolved inside
`Slap.PPF.Parse.detectVersion` by reading byte 3 as an ASCII digit.

## Why this structure is awkward

### 1. `FormatPPF` conflates four formats

There is no single format called "PPF." There are four distinct
formats that happen to share a 3-byte magic prefix. The dispatch
layer pretends it has identified the format when it hasn't. The
real identification happens inside the module after commit.

Contrast the NINJA case: `ninja1MagicBytes` and `ninja2MagicBytes`
appear as two separate probes mapping to distinct `FormatNINJA1`
and `FormatNINJA2` variants. NINJA1 and NINJA2 are related scene
formats, just like PPF1 and PPF4 are related scene formats. slap
correctly treats them as distinct at the dispatch layer. PPF is the
outlier that doesn't.

### 2. The 3-byte prefix is not a truthful magic

Each of the four PPF formats has a 5-byte magic: `"PPF10"`,
`"PPF20"`, `"PPF30"`, `"PPF40"`. These unambiguously identify
a single format each. The current code throws away the last two
bytes of that identification and then has to redo it internally.

A file starting with `"PPFX5"` or `"PPF5..."` currently matches the
probe, routes to the PPF module, and is rejected there — but with
an awkward error label (`BadVersion LabelPPF1`, because the error
machinery needs to attach a version and PPF1 is the arbitrary
default). With four probes, the same file wouldn't match any
probe at all and would fall through to the real "unknown format"
path with a clean, accurate error.

### 3. The four formats don't share much at the type level

Looking past the 6-byte preamble (magic + encoding byte) and the
50-byte description, the formats diverge:

- **PPF1**: 32-bit offsets, has RLE, no validation, no trailer,
  2GB-limit, endianness ambiguous
- **PPF2**: 32-bit offsets, no RLE, mandatory 1024-byte
  validation, u32 input-size field, 4-byte DIZ trailer
- **PPF3**: 64-bit offsets, no RLE, optional validation with
  imagetype variant, optional per-record undo, 2-byte DIZ
  trailer, 2^63 limit
- **PPF4**: 32-bit offsets, no RLE, no validation, no trailer, no
  undo, REPLACE+ADD commands with ordering constraint,
  grow-capable, zero-padded description, encoding byte `0xFF`

The `PPFPatch` type's `Maybe`-field soup is a symptom: every
version-specific field is optional because each field is only
meaningful in some versions. That's a sum type dressed as a
product type.

### 4. PPF4 is genuinely a different format

The README already acknowledges this in its footnote: PPF4 is
Pyriel's distinct format. Its record encoding has nothing in common
with PPF3 — `1 byte command + 4-byte offset + 1-byte count` vs
PPF3's `8-byte offset + 1-byte count`. The header coincidentally
has fields at the same offsets 56-59, but those fields are all
forced to zero in PPF4 while they carry real semantics in PPF3.
The encoding byte being `0xFF` (vs PPF3's `0x02`) is an explicit
"I am not a version of your PPF" sentinel, per ppfmaker.cpp's
author comment.

## Recommendation: four-way split

Treat PPF1, PPF2, PPF3, PPF4 as four distinct formats with four
distinct modules:

- `Slap.PPF1` — handles only PPF1 (magic `"PPF10"`)
- `Slap.PPF2` — handles only PPF2 (magic `"PPF20"`)
- `Slap.PPF3` — handles only PPF3 (magic `"PPF30"`)
- `Slap.PPF4` — handles only PPF4 (magic `"PPF40"`)

Each module:
- Has its own `Types.hs` with the fields it actually uses (no more
  version-gated `Maybe`s)
- Has its own `Parse.hs` / `Apply.hs` / `Create.hs` / `Describe.hs`
- Exposes its own magic prefix as a constant

`Slap.Detect.magicProbes` gets four PPF entries:

```haskell
, FormatProbe (MagicPrefix "PPF10") (PatchDirect FormatPPF1)
, FormatProbe (MagicPrefix "PPF20") (PatchDirect FormatPPF2)
, FormatProbe (MagicPrefix "PPF30") (PatchDirect FormatPPF3)
, FormatProbe (MagicPrefix "PPF40") (PatchDirect FormatPPF4)
```

`PatchFormat` gets four PPF variants instead of one.

The conversion engine gets four conversion descriptors instead of
one PPF-with-four-modes descriptor. This surfaces the real
per-version capabilities (PPF4 can grow, others can't; PPF3 has
undo, others don't; PPF2 has a mandatory validation block, others
don't) in the conversion compatibility matrix naturally.

## Costs of the split

- Four `Parse.hs` files instead of one. In practice the current
  `Parse.hs` already has `parsePPF1`, `parsePPF2`, `parsePPF3`,
  `parsePPF4` as separate functions; the split is largely about
  distributing them into per-format modules.
- Four `Types.hs` files. Actually simpler on net — no more
  `Maybe`-gating for fields that only apply to some versions.
- Four `Describe.hs` files. `versionString`-style helpers become
  trivial (each module just reports its own version).
- Some shared utility code (50-byte description handling, FILE_ID.DIZ
  trailer parsing with variable length-width, encoding-byte
  validation) could live in a `Slap.PPF.Common` module or be
  duplicated. The 50-byte description handling is shared across all
  four; DIZ trailer is shared between PPF2 and PPF3; everything else
  is per-format.

## Benefits of the split

- Dispatch is honest: magic bytes identify a format, not a family.
- Error messages are honest: a file with bad version byte falls off
  the dispatch table and produces an "unknown format" error rather
  than a confusingly-labeled "BadVersion LabelPPF1."
- Types are honest: each format's data shape is what it is, not a
  union masquerading as a product.
- Conversion matrix is honest: four source formats × four target
  formats, with the real capability constraints (size, growth,
  undo, validation) surfaced per pair.
- Documentation and user-facing naming aligns with the structure:
  slap already says "PPF1/2/3/4" in its version strings and README
  footnotes. The code would match.
- Makes it easier to later drop/deprecate/harden per-format
  support without perturbing the others.

## Implementation order

If doing the refactor:

1. Extract `PPF4` first. It's the cleanest case for separation
   because it really is a different format. The existing PPF4
   code in `Parse.hs`/`Apply.hs` is already well-isolated. This
   is a low-risk move that demonstrates the pattern.
2. Audit the Describe.hs / CLI paths that currently branch on
   `PPFVersion`. Those become straightforward per-format code.
3. Extract `PPF2`. It's the next cleanest because its mandatory
   validation block and input-size field are structurally distinct.
4. Extract `PPF1`. Its RLE record branch is the unique bit; the
   rest is straightforward.
5. `PPF3` is what remains of the original module and can stay in
   `Slap.PPF3`.

At each step, `Slap.Detect.magicProbes` gains one new entry and
loses nothing (the old `"PPF"` entry gets removed at the end, once
all four versions have their own probes).

## What stays the same

- `spec.md` documents the family as a family, with cross-version
  tables. The docs don't need to change structure just because the
  code does.
- User-facing CLI flags like `--format ppf3` / `--format ppf4`
  already exist and match the four-way naming.
- The `findings.md` corpus observations stay as-is; they already
  bucket by version.
