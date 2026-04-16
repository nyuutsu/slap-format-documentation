# xdelta1 findings

Read of canonical source `xdelta-1.1.4` (Joshua MacDonald, LGPL/GPL) compared
against slap's current implementation (`Slap.XDelta1.*`). Documents what slap
gets right, gets wrong, misses, gets right without understanding why, and
what design decisions are tool-implementation-details slap can decline to
copy.

This document captures both **what is** (the current state of xdelta1 and
slap's handling of it) and **where we want to be** (design calls for the
eventual beautification pass). Sections named "slap today" describe current
reality; sections named "slap intent" describe agreed-upon direction for
later work.

## Provenance

Research sources:
- Canonical source `xdelta-1.1.4/` provided by user (Joshua MacDonald, 1997-2001).
  This is the reference implementation — the format has no formal spec,
  so the source code *is* the spec.
- Slap's current implementation in `Slap.XDelta1.*`.
- Empirical testing against 7 `.xdelta` files from the romhacking.net
  archive (2024-08-01). Seven out of ~2,026 `.xdelta` files in that
  archive were xdelta1; the rest were VCDIFF/xdelta3.

**xdelta1 is genuinely rare.** Most tools and patches called "xdelta"
in the romhacking ecosystem are xdelta3, a completely different format
by the same author (different wire format, different algorithms — they
share only the author and name).

## Format: spec vs patcher

This document distinguishes two authorities:

- **The format** (wire syntax): what bytes can legally appear, what their
  structural relationships are, what type/value constraints apply at
  the byte level. This is the authority for slap's parser.
- **The patcher** (canonical tool): how `xdelta-1.1.4/xdmain.c` interprets
  those bytes, what shapes it accepts at apply time, what behaviors it
  produces. This is the authority for slap's apply semantics when the
  format is silent or permissive.

A format with no other implementations *converges* these two — the
patcher effectively becomes the spec. xdelta1 is in this position: no
written spec, one reference implementation, and slap as the second
implementation. Slap's discipline is to match the canonical tool's
interpretation unless there's a principled reason to diverge, and to
name such divergences explicitly when they occur.

Divergences fall into categories:

- **Tool-implementation details** (CLI shape, message wording, error
  machinery). Slap freely diverges — these aren't wire-format concerns
  and slap's own discipline produces different choices.
- **Wire-format features the canonical tool implements that slap doesn't**
  (yet). These are missing implementations, not divergences. Slap should
  add them to match the canonical tool's output.
- **Wire-format permissiveness the canonical tool restricts semantically**.
  The format permits syntax the tool rejects. Slap can either match the
  restriction (convergence) or take a different stance, but the choice
  wants to be principled and documented.
- **Wire-format extensions slap might invent**. Slap adding semantics to
  cases the format leaves undefined. These are always opt-in, always
  loud, always documented as slap-specific.

## The format

### Outer file structure

```
[8 bytes]   Magic prefix
            "%XDZ004%" = v1.1       (slap supports)
            "%XDZ003%" = v1.0.4     (slap supports)
            "%XDZ002%" = v1.0       (slap rejects as unsupported)
            "%XDZ001%" = v0.20      (slap rejects as unsupported)
            "%XDZ000%" = v0.18      (slap rejects as unsupported)
            "%XDELTA%" = v0.14      (slap rejects as unsupported)
[24 bytes]  Header: 6 x uint32 BE (HEADER_WORDS * 4)
            word[0] = flags (see below)
            word[1] = (from_name_len << 16) | to_name_len
            word[2..5] = reserved (always zero)
[N1 bytes]  from name (length per word[1] high bits)
[N2 bytes]  to name (length per word[1] low bits)
[D bytes]   Data segment (zlib-compressed if FLAG_PATCH_COMPRESSED)
[C bytes]   Control segment (EDSIO-serialized, zlib-compressed if FLAG_PATCH_COMPRESSED)
[4 bytes]   Control offset (uint32 BE) — byte position where control segment starts
[8 bytes]   Trailing magic — must match leading prefix
```

Minimum fixed overhead: 44 bytes (8 prefix + 24 header + 4 control offset + 8 trailing magic).

### Header flags (word[0])

```
FLAG_NO_VERIFY        = 1   // patch was created with --noverify; stored MD5s are garbage
FLAG_FROM_COMPRESSED  = 2   // original from file was gzipped at delta time
FLAG_TO_COMPRESSED    = 4   // original to file was gzipped at delta time
FLAG_PATCH_COMPRESSED = 8   // data and control segments are zlib-compressed
```

Bits 4–31 are reserved and always 0 in the observed corpus.

### Header words 2–5 (reserved)

The canonical tool always writes zeros and never reads these words. They
exist for future format extensions. Slap does not currently parse or
validate them; fine because the canonical tool doesn't either. A future
format extension using these would warrant a new version prefix.

### XdeltaControl (the control segment payload, after decompression)

```
[uint32]    type tag (ST_XdeltaControl) — EDSIO framing
[uint32]    allocation hint — EDSIO framing, deserializer bookkeeping
[16 bytes]  to_md5 — output file MD5
[varint]    to_len — output file size
[byte]      has_data — boolean: true if any source has isdata=true
[varint]    source_info_len — number of sources
[source..]  source_info[] — each source per below
[varint]    inst_len — number of instructions
[inst..]    inst[] — each instruction per below
```

The first 8 bytes (type tag + allocation hint) are EDSIO serialization
framing, not XdeltaControl payload. Slap correctly skips them.

The `has_data` byte is a redundant flag — it's true iff any source has
`isdata=true`, which slap can recompute from per-source kinds. It exists
for the serializer's benefit. Slap reads and discards it.

### XdeltaSourceInfo (per source)

```
[varint]    name_len
[name_len]  name (bytes)
[16 bytes]  md5
[varint]    len
[byte]      isdata — boolean: 1 = data segment, 0 = file source
[byte]      sequential — boolean: 1 = sequential offsets, 0 = absolute offsets
```

### XdeltaInstruction (per instruction)

```
[varint]    index — zero-based index into source_info[]
[varint]    offset — byte offset within the referenced source
[varint]    length — bytes to copy
```

When a source's `sequential` flag is set, instructions targeting that
source write `0` for the offset on the wire. The apply step reconstructs
absolute positions by maintaining a running per-source cursor.

### Varint encoding (EDSIO)

Distinct from byuu/UPS varints and standard LEB128. The canonical
implementation lives in `libedsio/edsio.c` (function `edsio_get_uint32`).
Slap implements this in `Slap.Get.edsioVarint`. The 7 corpus patches
parse without issue; no formal test against synthesized vectors exists.

## What is a source

A source is a self-contained byte-provider. Each source has:

- **name** (varint-prefixed bytes). For file sources: the original
  filename's basename (the canonical creator tool calls `g_basename()`
  before storing). For data segments: conventionally the string
  `"(patch data)"` but not enforced.
- **md5** (16 bytes) verifying the source's content.
- **len** (varint) giving the source's byte length.
- **isdata** (boolean) discriminating data segment from file source.
- **sequential** (boolean) addressing mode for instructions targeting
  this source.

Sources live in an array. Instructions reference them by zero-based
index.

### The isdata distinction (user-facing meaning)

Both data segments and file sources are byte-providers the apply step
copies from. The difference is *where the bytes physically live*:

- **Data segment** (`isdata=true`): bytes embedded inside the patch's
  data segment block. Apply reads from this block using the source's
  offset and length. The MD5 verifies the embedded bytes survived
  transit unmodified — slap handles this integrity check automatically
  on parse.
- **File source** (`isdata=false`): bytes come from a file on disk
  that the user must provide. Apply reads from this file. The MD5
  verifies the user provided the correct file — slap checks this on
  apply and fails/warns on mismatch.

To the user: data segments are "what the patch carries internally"
(slap's responsibility); file sources are "the ROM you must provide"
(the user's responsibility).

## Source-count semantics

This is where format-permissive meets patcher-restrictive.

### What the format allows

The wire format permits any `source_info_len`. The engine's internal
rsync table (`xdelta.h:67-68`) caps at 15 sources theoretically. The
parser for XdeltaControl reads `source_info_len` as an unbounded varint.

### What the canonical patcher accepts

The apply path in `xdmain.c:1741-1768` enforces these shapes and rejects
everything else with `EC_XdIncompatibleDelta`:

1. `[]` — zero sources (degenerate; apply produces empty or no-op output)
2. `[data]` — one source, isdata=true (patch is fully self-contained,
   no external file needed)
3. `[file]` — one source, isdata=false (patch references only the
   external file with no embedded data; very unusual but legal)
4. `[data, file]` — two sources: data segment at index 0, file source
   at index 1 (the normal pattern, all 7 corpus patches use this)

Shapes `[file, data]`, `[file, file]`, `[data, data]`, and anything with
`source_info_len > 2` are rejected. No semantics are defined for these
cases.

### Design call: slap's position on multi-source patches

The question is: what does a patch with 3+ sources *mean*? Three
interpretations:

- **Incoherent / undefined behavior.** Format permits the syntax, no
  semantics defined, tools shouldn't try to interpret.
- **Future extension point.** Structural capacity reserved for a future
  version; current version rejects as unimplemented.
- **Alternate file sources for dump variants.** Multi-dump patches
  carrying MD5s for several accepted inputs, apply picks the one that
  matches. This would be a real workflow improvement over today's
  "one MD5, mismatch means `--no-verify`." But it's slap inventing
  semantics the format doesn't define, and patches slap produced would
  not apply in the canonical tool.

**Slap intent**: reject `source_info_len > 2` at parse time with a
structured error, matching canonical's discipline. Citing the canonical
tool's behavior in the error message. Not inventing multi-dump semantics.

Implementation shape: type the wire field permissively (matching what
the wire allows — `xdelta1Sources :: [XDelta1Source]`), validate
semantically at parse time with a dedicated checker
(`validateXDelta1SourceCount`). The type system says "this can be any
list" because the wire really does permit that. The validator is the
one place where the slap-specific constraint lives, with a comment
citing the reason (canonical tool's apply-time rejection). If the
constraint ever needs revisiting, the validator is the one place to
change.

This pattern applies to other wire-permits-but-semantics-restricts cases
across slap's formats. Type the wire faithfully; enforce semantic
constraints in dedicated validators with cited reasoning.

### Source-shape constraint

Same shape of design call: canonical rejects `[file, data]`, `[file, file]`,
`[data, data]` orderings. Slap should match, through the same validator
pattern, with the same cited reasoning.

## Gzip transparency

The canonical tool implements *gzip transparency* for source and target
files. At delta time, if either input is gzipped, the tool decompresses
to a temporary location, computes the delta against decompressed content,
and records flags in the patch header indicating which inputs were
compressed.

At apply time, the flags determine handling:

- `FLAG_FROM_COMPRESSED`: the apply expects a gzipped from-file,
  decompresses on read so the delta operates on uncompressed content.
- `FLAG_TO_COMPRESSED`: the apply re-compresses the output before
  writing to disk.

The mechanism uses zlib's `gzopen`/`gzread`/`gzwrite` through filehandle
wrappers (`xd_gzread`, `xd_gzwrite`, `xd_gzclose` in `xdmain.c:516-540`).

Magic-byte detection at apply time: `file_gzipped()` (`xdmain.c:587-610`)
reads the first 2 bytes and checks for `0x1F 0x8B` (standard gzip
magic). If the input starts with these bytes, it's treated as gzipped
regardless of filename extension.

The `--pristine` / `-p` CLI flag disables gzip transparency entirely,
forcing raw-byte treatment. The README rationale: "the recompressed
content does not always match byte-for-byte with the original compressed
content. The uncompressed content still matches, but if there is an
external integrity check such as cryptographic signature verification,
it may fail."

### ROM-application context

For ROM patching, gzipped inputs are rare:
- ROM files are typically not gzipped in the wild.
- Patches downloaded from romhacking.net are uncompressed `.xdelta`
  files carrying delta data for raw ROMs.

All 7 corpus patches have `FLAG_FROM_COMPRESSED=0` and
`FLAG_TO_COMPRESSED=0`. None exercises gzip transparency.

**Slap intent**: implement the feature. The format defines it, the
canonical tool implements it, and a patch created from gzipped inputs
is structurally legal — it just hasn't shown up in the romhacking
corpus. Slap should support it for spec-conformance, not because any
observed patch needs it. A user with a gzipped ROM (rare) or a patch
created from gzipped inputs (currently unobserved but legal) currently
gets silently wrong output from slap.

Slap will also want an analog of `--pristine` for users who care about
byte-exact compressed content preservation, but that's downstream of
implementing the mechanism.

## FLAG_NO_VERIFY

The canonical tool can be invoked with `--noverify` at delta time,
which suppresses MD5 computation. Resulting patches have
`FLAG_NO_VERIFY` set in the header and zero/garbage MD5 fields in
both the control (`to_md5`) and each source (`md5`).

At apply time, canonical reads the flag (`xdmain.c:1689`) and sets
`no_verify = TRUE`, which skips verification — because there's nothing
meaningful to verify against.

### Slap today

Slap reads the flag bit during parse but ignores it. MD5 verification
runs unconditionally. A hypothetical patch with `FLAG_NO_VERIFY=1`
would fail slap's apply with an MD5 mismatch error (because the stored
MD5s are garbage), where canonical would apply successfully. Pending
empirical verification — no corpus patch has this flag set.

### Slap intent

When `FLAG_NO_VERIFY=1`, slap skips MD5 verification automatically and
emits a warning naming what it did and why:

> note: this patch was created without MD5 verification
> (FLAG_NO_VERIFY set in patch header). slap is skipping the standard
> MD5 check because the patch contains no MD5 values to verify
> against. the integrity of the apply cannot be confirmed.

The user's `--no-verify` CLI flag has a slightly different role for
these patches: it suppresses the warning. For patches without
FLAG_NO_VERIFY, `--no-verify` downgrades a real MD5 mismatch from
error to warning. For patches with FLAG_NO_VERIFY, there's no MD5 to
mismatch; `--no-verify` just says "yes I know there's no integrity
check, stop telling me."

`slap info` should also surface the flag's presence — a user inspecting
a patch before applying wants to know "this patch has no MD5s" upfront,
not discover it from a warning at apply time.

## Unused file source ("nothing in common")

The canonical tool warns (`xdmain.c:1598-1602`) when the delta algorithm
finds no shared chunks between the from-file and the to-file. In this
case, every byte of output is novel; the entire output goes into the
data segment; the file source is referenced in the source array but no
instruction copies from it.

Put plainly: the input file is irrelevant, the entire content is
expressed inside the patch. The patch would produce the same result
with any source file (or none).

### Practical risk

How this can happen in practice:

- **Unrelated files.** Someone invokes `xdelta delta X Y patch.out`
  where X and Y have nothing to do with each other (different ROMs,
  mismatched files, user error).
- **Incompatible encoding.** Two encrypted versions of the same
  plaintext; two gzip streams of the same content compressed at
  different levels. xdelta1's gzip transparency helps with the gzip
  case specifically; other compression/encryption it doesn't help.
- **To-file smaller than block size.** If the to-file is under 16
  bytes (default block size), the rolling-checksum algorithm has no
  window to match against. Edge case.
- **Deliberate self-contained patch.** Creator wants a single `.xdelta`
  that produces output without requiring specific input. Would feed
  a dummy from-file and a real to-file; the resulting patch technically
  references the dummy but never reads from it.
- **Creation-tool bug.** A tool claiming xdelta1 output produces
  no-match patches when it should have found matches. The canonical
  binary is mature; other tools (slap's eventual Create, third-party
  libraries) could have bugs.

In the romhacking context, the vast majority of xdelta1 use targets
heavily-related files (same ROM, different localization or hack). A
no-match patch almost certainly indicates user error or a tool bug.
Low practical risk but nonzero.

### Slap intent

Detect the condition at parse time by walking the instruction list and
counting source references. If a file source has zero references, emit
a `SlapWarning` (unusual-but-conformant category):

> note: this xdelta1 patch contains the entire output as embedded data
> and does not actually use the source file you provided. the patch
> would produce the same result with any source file (or none).

The condition isn't an error — the patch is structurally valid and
slap can apply it correctly — but it's uncommon enough that the user
almost certainly wants to know it occurred. This connects to slap's
broader principle of surfacing unusual-but-legal conditions because
they're often signals of upstream problems.

## Slap comparison: what it gets right, wrong, or misses

### Gets right

- **Magic prefix detection**: recognizes `%XDZ004%` and `%XDZ003%`;
  explicitly rejects older versions with `UnsupportedSubformat`.
- **Trailing magic validation**: checks last 8 bytes match leading
  prefix, emits `TrailingMagicMismatch` on mismatch.
- **Control offset extraction**: reads 4-byte uint32 BE at
  `totalLength - 12`.
- **FLAG_PATCH_COMPRESSED handling**: correctly reads bit 3 and
  decompresses both data and control segments.
- **Source kind dispatch**: reads `isdata` byte, dispatches to
  `DataSegmentSource` / `FileSource`. Matches wire semantics.
- **Offset mode dispatch**: reads `sequential` byte, dispatches to
  `SequentialOffsets` / `AbsoluteOffsets`. Matches wire semantics.
- **Sequential offset reconstruction**: `fixSequentialOffsets` maintains
  per-source cursors and replaces wire-zero offsets with reconstructed
  absolute positions. Slap does this at parse time; canonical does it
  at apply time. Different stage, same effect.
- **Apply-time MD5 verification**: `SomePatch` populates
  `verifySourceMD5` from the first file source and `verifyTargetMD5`
  from `xdelta1ToMD5`, gated by `--no-verify`. Matches canonical's
  default behavior.

### Gets right without understanding why

- **EDSIO framing skip at `parseControlBody:100`**: `skip (Length 8)`
  with comment "type tag + allocation (deprecated)". The skip is
  correct — these are EDSIO serialization framing, not XdeltaControl
  payload — but the comment is wrong. They aren't deprecated fields;
  they're EDSIO framing that always precedes a serialized object.
  Comment fix only; behavior fine.

- **`has_data` skip**: line 103, comment "has_data boolean". Correct
  to skip (the flag is redundant — recomputable from per-source
  `isdata`). Worth a comment update clarifying that it's redundant
  rather than just describing its type.

### Gets wrong (slap today)

These are real deviations from canonical behavior.

- **FLAG_FROM_COMPRESSED / FLAG_TO_COMPRESSED ignored.** Patches created
  from gzipped inputs produce silently wrong output. Real spec gap.
  See "Gzip transparency" above.

- **FLAG_NO_VERIFY ignored.** Patches created with canonical's
  `--noverify` will spuriously fail apply with MD5 mismatch errors.
  See "FLAG_NO_VERIFY" above.

- **Multi-source over-permissiveness.** Parser accepts
  `source_info_len > 2`, which canonical rejects. No observed patch
  exercises this but slap is structurally more permissive than
  canonical. See "Source-count semantics" above.

- **Source-shape over-permissiveness.** Parser accepts source orderings
  canonical rejects (`[file, data]`, `[file, file]`, `[data, data]`).
  Same character as multi-source case.

### Misses entirely

- **Unused file source detection.** No warning when the file source is
  referenced but not read from. See "Unused file source" above.

- **xdelta1 Create.** Slap cannot produce xdelta1 patches. Out of scope
  for the current beautification pass; eventually wanted so slap can
  round-trip xdelta1.

## Tool details slap can decline

These are behaviors of the canonical CLI tool that slap reasonably
declines because they're not wire-format concerns:

- **From/to filename as default CLI arguments.** Canonical uses stored
  names as defaults when the user omits them. Slap requires explicit
  arguments and uses its own output naming convention
  (`"rom [patch].ext"`). Slap's choice is reasonable — explicit input
  is more predictable. Stored names remain useful as metadata for
  `info` output.

- **TMPDIR for decompression.** Canonical decompresses gzipped inputs
  to a temporary location. Slap operates in-memory and doesn't need
  this. For ROM-sized inputs in-memory is fine; if slap ever grows
  support for very large files this may warrant revisiting.

- **EC-event-based error reporting.** Canonical emits abstract events
  that get translated to error messages. Slap uses structured
  `SlapError` constructors directly. Different machinery, same effect.

- **C-style boolean-as-integer sloppiness.** Canonical compares
  `gboolean` to `gint` (`xdmain.c:1598`) as part of the "no matches"
  detection. Slap uses distinct types per role and detects the
  condition differently. Same outcome.

- **zlib compression level (0-9).** Only relevant to Create. When slap
  adds xdelta1 Create, default probably matches canonical's default
  (level 6).

## Open questions / follow-ups

**Empirical verification of FLAG_NO_VERIFY handling.** Synthesize a
patch with canonical's `--noverify` and confirm slap's current behavior
(expected: spurious MD5 mismatch failure). After implementing the fix,
verify slap handles it correctly.

**Empirical verification of gzip-transparency handling.** Synthesize
patches with FLAG_FROM_COMPRESSED=1 and FLAG_TO_COMPRESSED=1 and
confirm slap's current behavior (expected: silently wrong output).
After implementing the fix, verify slap handles both.

**EDSIO varint test vectors.** Slap's `edsioVarint` parses all 7 corpus
patches without obvious failure but has no formal test against
known-good encoded values. The canonical implementation in
`libedsio/edsio.c` could provide vectors via its test suite
(`libedsio/edsiotest.c`). Worth doing in the dedicated test pass.

**Sequential offset mode in the wild.** Zero corpus patches use
sequential offset mode. Slap's `fixSequentialOffsets` is unexercised
by real-world data. Could be tested by synthesizing a small sequential
patch with canonical.

**Chained-patch workflow.** Separate from xdelta1-specific concerns:
real-world chained patches (the OoE-Redrawn + Albus Recolored case,
see "The 7 patches" section) require manual `--no-verify` at
intermediate steps because slap's CLI handles one patch at a time.
A hypothetical `slap apply a.xdelta b.xdelta rom.nds` would handle
the chain internally, computing intermediate hashes and verifying
against each patch's declared expectation. This is a slap-wide
feature, not xdelta1-specific.

**`slap info` redundancy for multi-source patches.** See the existing
"slap info: redundancy and labeling for multi-source patches" section
below. Deferred until more formats have been audited for analogous
user-vocabulary leaks.

## The 7 patches

### [2050] Tokimeki Memorial Girl's Side 2nd Season V6 Patch
- **Platform**: NDS
- **From**: `2032 - Tokimeki Memorial Girl's Side 2nd Season (J)(6rz).nds`
- **Target size**: 257,902,496 bytes (246 MB) — largest patch
- **Instructions**: 51,038
- **Data segment**: 6.3 MB
- **Sources**: 2 (file + data)
- **Notes**: Ships with `xdelta.exe` and `xdeltaUI mod.exe`. The bundled
  xdelta.exe is the original v1.x tool.

### [2441] MarioBrosClassic (MIXED archive)
- **Platform**: GBA
- **From**: `Super Mario Advance (J) [!].gba`
- **Target size**: 587,584 bytes (574 KB) — smallest target
- **Instructions**: 1,891
- **Data segment**: 310 KB
- **Sources**: 2
- **Notes**: This archive also contains an `.xdelta3` file for the same
  hack — dual-format, cross-validation possible. Smallest patch in the set.

### [4077] OoE-Redrawn (2 patches)
- **Platform**: NDS (Castlevania: Order of Ecclesia, US)
- **From**: `2809 - Castlevania - Order of Ecclesia (U)(Venom).nds`
- **Target size**: 67,108,864 bytes (64 MB) each
- **Instructions**: 5,655 (Player & Glyphs), 3,615 (Player Only)
- **Data segment**: ~30 KB each
- **Notes**: Sprite redraw mod. Requires "xdeltaUI mod 1.0" per readme.
  The `from` name includes the scene-group tag `(Venom)` — targets a
  specific dump.

### [6406] Albus Recolored (3 patches)
- **Platform**: NDS (same game)
- **From**: varies — the "Original Version" targets the base ROM, the
  other two target OoE-Redrawn patched ROMs
- **Target size**: 67,108,864 bytes (64 MB) each
- **Instructions**: 22, 16, 16 — very small
- **Data segment**: 1,824 / 644 / 644 bytes — tiny
- **Notes**: Chained patches. Two of the three require OoE-Redrawn as
  prerequisite (their `from` field names a patched ROM, not the
  original). The base version has only 22 instructions with a median
  size of 169 bytes — a palette swap modifying very little data.

## Structural observations

- **All use 2 sources**: every patch has source index 0 (embedded data
  segment) and source index 1 (file source / input ROM). The ordering
  is consistent: data segment first, file source second. Matches the
  canonical tool's `[data, file]` shape (shape #4 in
  "Source-count semantics").
- **Explain output shows 100% coverage**: the instructions collectively
  cover the entire output file. This is because xdelta1's model is
  "the output is assembled entirely from copy instructions" —
  unchanged regions are copied from source, changed regions come
  from the data segment. There's no concept of "skip unchanged bytes."
- **Data segment sizes vary wildly**: from 644 bytes (palette swap) to
  6.3 MB (full NDS translation). The data segment IS the new content —
  its size directly reflects how much novel data the patch introduces.
- **Filename metadata is incidental-but-useful**: the `from`/`to` fields
  preserve the creator's exact filenames, encoding scene-group tags
  (`(Venom)`, `(6rz)`), revision markers (`[!]`), and intermediate
  patch states. The creator probably didn't intend this as
  documentation, but it's valuable for identifying which dump the
  patch targets.

## Chained dependency graph

```
Base ROM: Castlevania - OoE (U)(Venom).nds
  ├── OoE-Redrawn Player & Glyphs.xdelta → patched ROM A
  │     └── Albus Recolored P&G.xdelta → patched ROM
  ├── OoE-Redrawn Player Only.xdelta → patched ROM B
  │     └── Albus Recolored PO.xdelta → patched ROM
  └── Albus Recolored Original.xdelta → patched ROM (standalone)
```

To apply the Albus Recolored + Shanoa Redrawn combo, you'd need to apply
OoE-Redrawn first, then Albus Recolored on top. The `from` names encode
which intermediate state each patch expects.

## Apply results

| Patch | ROM | Instructions | Output MD5 | Result |
|-------|-----|-------------|-----------|--------|
| MarioBrosClassic | SMA (J).gba | 1,891 | 8b16c146... verified | SUCCESS |
| Albus Recolored Original | OoE (USA)(En,Fr).nds | 22 | 31ef0f3c... verified | SUCCESS |
| Tokimeki Memorial GS2 | TMGS2 (J).nds | 51,038 | 3e313350... verified | SUCCESS |
| OoE-Redrawn P&G | OoE (U)(Venom).nds | 5,655 | edb9675f... verified | SUCCESS |
| OoE-Redrawn PO | OoE (U)(Venom).nds | 3,615 | c4349fa1... verified | SUCCESS |
| Albus+Shanoa P&G (chained) | OoE-Redrawn P&G on No-Intro | 16 | 20c44084... verified | SUCCESS |
| Albus+Shanoa PO (chained) | OoE-Redrawn PO on No-Intro | 16 | bfcba141... verified | SUCCESS |

**7 of 7 verified end-to-end.**

### Chained patch resolution

The two chained patches initially failed when fed the OoE-Redrawn output
from the Venom dump. Investigation revealed: the Albus author applied
OoE-Redrawn to the **No-Intro dump** `(USA) (En,Fr)`, not the Venom
scene dump `(U)(Venom)`. The OoE-Redrawn patch was *created* against
Venom, but it *also works* on No-Intro (the changes land at the same
offsets — the dumps differ only trivially). The intermediate output
has a different MD5 depending on which base you start from.

The successful chain:
1. Apply OoE-Redrawn to No-Intro dump (`--no-verify` required — source
   MD5 mismatch, patch expects Venom)
2. Apply Albus chained patch to that output (no `--no-verify` needed —
   MD5 matches)
3. Final output MD5 verified

This demonstrates that real-world chained patches can span dump
variants: step 1 uses a patch created for dump A applied to dump B,
producing an intermediate that step 2 was built against. The chain
works because the patch's changes are position-identical across dumps,
but the integrity metadata disagrees at step 1.

### ROMs used

- `Super Mario Advance - Super Mario USA + Mario Brothers (Japan).gba` —
  4,194,304 bytes
- `Castlevania - Order of Ecclesia (USA) (En,Fr).nds` — 64 MB
- `2809 - Castlevania - Order of Ecclesia (U)(Venom).nds` — 64 MB
- `Tokimeki Memorial Girl's Side - 2nd Season (Japan).nds` — 256 MB

## slap's handling of from/to names

Slap parses and displays the from/to names in `info` output but
otherwise ignores them:
- Output naming uses slap's own `"rom [patch].ext"` convention, not
  the stored `to` name.
- Source verification checks MD5, not filename match.
- Apply doesn't reference the names at all.

This is correct behavior — slap always takes explicit arguments, so
there's no "default to stored name" path like the canonical tool has.
But the names carry useful context (scene-group tags, dump identifiers)
and deserve a note at apply time.

**Proposed note** (draft for future implementation):

> note: patch expects source "2809 - Castlevania - Order of Ecclesia
> (U)(Venom).nds" and target "built_rom_ooe player and glyphs.nds"
> (xdelta1 stores original filenames; slap uses its own output
> naming)

One line, explains what the data is, why it's there, and why slap isn't
honoring it. No alarm, no action needed. The parenthetical preempts
"should I rename my file?"

The from/to names should also be promoted to the metadata layer in
`SomePatch.hs` (currently `patchMetadata = Nothing`). This is
low-effort and would let the conversion layer warn when they're
dropped. No other format has equivalent fields, so they'll always be
dropped on conversion — but at least the loss is visible.

## slap info: redundancy and labeling for multi-source patches

The `info` output for xdelta1 patches contains two blocks that both
list source MD5s with different amounts of context. The flat
meta-fields summary block (`source 1 MD5: ...`, `source 2 MD5: ...`)
gives MD5s with positional indices but no kind annotation. The
per-source detail block lower down (`[0] (patch data) (data) ...`,
`[1] filename.gba (file) ...`) gives MD5s with full context including
source filename and kind annotation.

A reader scanning the output can be misled if they read the summary
block and not the detail block. The summary's position-based labels
treat file sources and data segments as equivalent peers when they
have very different user implications: file-source MD5s are what the
user must verify their input ROM against; data-segment MD5s are
patch-internal integrity checks slap handles automatically.

This is a presentation issue, not a correctness issue — `Slap.SomePatch`
correctly filters to file sources for verification, and the detail
block carries the right information.

### Vocabulary considerations

slap's internal terminology (`DataSegmentSource`, `FileSource`) is
correct in the type system but should not leak into user-facing output.
The terms `(data)` and `(file)` in the existing detail block are
abbreviations of those internal terms; they're better than nothing
but still require the user to learn what they mean. User-facing
language should describe what each thing *is* to the user: a file
source is "the input ROM you provide"; a data segment is "auxiliary
bytes the patch carries inside itself." A user reading either phrase
doesn't need to know slap's source code idioms to understand it.

The principle generalizes: bare types and internal vocabulary belong
in the source code; user-facing output should translate to terms a
user already knows. Other formats may have analogous leaks (IPS
describe shows magic and EOF marker, BSDiff shows ctrl block/diff
block/extra block, etc.) — these are wire-format terms that an expert
audience will understand but a casual user won't.

### Framing options for a future redesign

A useful structural framing separates the user's questions: "what do
I need to provide" vs "what will I get" vs "what does slap check
automatically." A reasonable shape (illustrative, not prescriptive):

```
patch:    /path/to/patch.xdelta
format:   xdelta1 v1.1
patch integrity:   verified (this format carries its own checksum;
                   slap confirmed it on parse)

apply this patch to:
  Super Mario Advance (J) [!].gba
  expected size: 4,194,304 bytes
  expected MD5:  c5240b871426f8bef985a540b9fcadfe

result will be:
  Super Mario Advance (J) (Plays Only Mario Bros. Classic Hack).gba
  expected size: 587,584 bytes
  expected MD5:  8b16c1465960dc37a8af0ad1707e1020

instructions: 1,891
```

The user reads top-to-bottom and gets, in order: what this is
(format/version), whether the patch arrived intact (integrity check
answered up front, no homework), what they need to provide (input ROM
with name/size/MD5), what they'll receive (output ROM with name/size/MD5).

For formats without patch checksums (IPS, APS, etc.), the integrity
line stays present and honest: `patch integrity: not checked (this
format does not carry a checksum of itself)`. A user comparing
IPS-with-no-checksum against xdelta1-with-checksum sees the difference
and understands why one inspires more confidence.

For chained patches (multiple file sources), the "apply this patch to"
section grows naturally:

```
apply this patch to:
  Castlevania - Order of Ecclesia (USA).nds  [base ROM]
  expected size: 134,217,728 bytes
  expected MD5:  abc123...

  PLUS the output of: OoE-Redrawn.xdelta  [previous patch in chain]
  expected size: 134,217,728 bytes
  expected MD5:  def456...
```

Chain ordering becomes visible without the user reading source code
or external documentation.

**Scope**: this wants to wait until more formats have been audited for
analogous issues, since the right structure should be informed by
what multiple formats need rather than designed from one format's
needs.

## Summary: severity tiers

### Real bugs to fix (matches canonical on input, produces wrong/bad output)

- Gzip transparency (FLAG_FROM_COMPRESSED, FLAG_TO_COMPRESSED) — real
  spec gap, slap silently produces wrong output on patches created
  from gzipped inputs.
- FLAG_NO_VERIFY handling — slap spuriously fails on patches canonical
  accepts. Pending empirical verification.

### Validator gaps (wire-permits-semantics-restricts)

- Multi-source rejection — slap parses what canonical rejects.
- Source-shape rejection — same character for malformed orderings.

### Warning conditions to detect

- Unused file source — patch contains entire output as embedded data,
  file source is referenced but no instruction reads from it.

### Comment fixes (no behavioral change)

- `parseControlBody:100` "type tag + allocation (deprecated)" — these
  are EDSIO framing, not deprecated fields.
- Line 103 "has_data boolean" — could note that the flag is redundant.

### Feature gaps (not bugs, but real absences)

- Create support — slap currently can't produce xdelta1 patches.
- Empirical test for sequential offsets.
- EDSIO varint test vectors against the canonical implementation.

### Display-layer concerns (deferred pending broader audit)

- Source MD5 labeling in `info` output — position-based, user-vocabulary
  leaks.

### Things slap can continue to decline

- From/to filename as default CLI arguments (tool-implementation detail).
- TMPDIR for decompression (slap operates in-memory).
- `--pristine` flag (only meaningful once gzip transparency is
  implemented; then worth adding the analog).
