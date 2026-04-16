# APS (N64) — Research and Proposal

## Proposal summary

This document is both a research writeup and a concrete proposal for how slap
should handle the APS N64 format. Full C source for the reference tool
(bb-aps 1.2 by Silo and Fractal of Blackbag, 1998) was recovered, so all
behavioral decisions are grounded in the authoritative implementation.

The proposal in broad strokes:

1. **Accept all three N64 byte orders on input (Z64, V64, .n64) via internal
   normalization.** bb-aps only knows about Z64 and V64. The three formats are
   losslessly interconvertible; slap can transparently handle .n64 by
   normalizing. Warn when conversion happens.
2. **Emit type 1 when source is N64, type 0 otherwise.** bb-aps only produces
   type 1; other tools (RomPatcher.js) produce type 0. Both are in the spec.
   slap should be liberal on create: produce the most-informative patch it can
   given the input. Warn about interop consequences in both cases.
3. **Reject on malformed or contradictory input.** Destination size violations,
   unknown encoding method, unknown image format, unknown patch type, incomplete
   trailing bytes. These are all format-level errors, not unusual inputs.
4. **Warn on format-legal but unusual input.** Padding bytes non-zero, RLE
   count 0, overlapping records, type 0 encountered on apply.
5. **Respect the format's dumbness without trying to fix it.** Description
   encoding is unspecified; slap passes bytes through. Consider introducing an
   `OpaqueDescriptionBytes` newtype to make the lack of encoding information
   visible at the type level (tentative, deferred).

### Warning vs. error principle

The distinction is **informational vs. malformed**, not common vs. rare.

- **Error** when the input violates the format's own rules or would force slap
  to guess at semantics (unknown encoding method, unknown image format,
  destination size violation, incomplete trailing bytes).
- **Warning** when the input is format-legal and we proceeded correctly, but
  something interesting happened that the user might want to know
  (normalization from .n64 to Z64, type 0 patch with no verification, unusual
  padding bytes).
- **Silent** almost never.

### On not fixing dumb formats

Some format behaviors are coherent but bad (description encoded in system
locale with no flag, fixed 255-byte record chunks forcing splits, 16-bit CRCs
for collision-vulnerable integrity checks). slap does not try to fix these.
Any "fix" would require a new format dialect, which would be a new format,
which isn't slap's job. Instead, slap implements the dumbness faithfully and
surfaces it through warnings and type-level structure where useful.

The one exception: **the .n64 byte order is a reference-tool omission, not a
format limitation.** The format's records are physical-offset-bound to the
source's byte layout, but the three N64 byte orders are losslessly
interconvertible, so slap can normalize internally without changing anything
on disk. slap-produced patches are still bit-for-bit valid APS N64 per the
spec.

For the on-disk structure of the format itself, see `spec.md`. For
historical context and the recovered upstream archive, see `README.md`
and `upstream/`. This document is slap-specific: what we warn on, what
we reject, how we handle edge cases, and why.

## Tools

| Tool                  | Apply | Create | Platform        | Status |
|-----------------------|-------|--------|-----------------|--------|
| bb-aps 1.2 (Blackbag) | yes   | yes    | DOS/Win, 1998   | Lost-ish (in archive mirrors) |
| RomPatcher.js         | yes   | yes    | web/Node        | Active |
| UniPatcher            | yes   | no     | Android         | Active |
| shygoo's Web Patcher  | yes   | no     | web (hack64.net)| Active |
| Everdrive 64 firmware | yes (auto) | no | hardware       | Active (since 2013) |
| slap                  | yes   | yes    | Haskell CLI     | This project |

The Everdrive 64 firmware (Krikzz, OS v2.00, October 2013) auto-applies APS files
from an `AUTO/` folder when loading ROMs. This is the primary modern usage:
**VI-blur removal patches** for N64 anti-aliasing fixes are distributed as APS for
seamless Everdrive use. Large collections exist on Archive.org
(`everdrive-64-ntsc-aps-pack-complete`).

## Possibility matrices

### Create

Inputs: source file, target file, output = APS N64 patch.

| Source type | slap behavior | Rationale |
|---|---|---|
| Z64 N64 ROM | Emit type 1, image format 1, extract cart ID/country/CRC from physical offsets directly | Canonical case; matches bb-aps n64caps |
| V64 N64 ROM | Emit type 1, image format 0, extract cart ID/country/CRC from physical offsets and byte-swap to canonical Z64 form before storing | Matches bb-aps n64caps for V64 inputs |
| .n64 N64 ROM (32-bit swapped) | Normalize to Z64 internally, then proceed as Z64 case. Warn that normalization happened. | bb-aps doesn't know about .n64 but the format can represent the same data as Z64. Normalization is lossless. |
| Non-N64 file | Emit type 0, warn about limited interop | Type 0 is in the spec. Other tools produce it. slap should too. User asked, slap delivers, slap explains. |

### Apply

Inputs: patch file, source file, output = patched file.

Columns: patch type × image format. Rows: source file format.

| Source \ Patch | Type 0 (no image format field) | Type 1, fmt=0 (V64) | Type 1, fmt=1 (Z64) |
|---|---|---|---|
| Z64 N64 ROM | Apply + warn "type 0, no verification" | Error: patch expects V64 but source is Z64 | Apply, verify directly |
| V64 N64 ROM | Apply + warn "type 0, no verification" | Apply, verify after byte-swap | Error: patch expects Z64 but source is V64 |
| .n64 N64 ROM | Normalize to Z64, apply + warn about normalization | Normalize to V64, apply + warn | Normalize to Z64, apply + warn |
| Non-N64 file | Apply + warn "type 0, no verification" | Error: type 1 patch requires N64 source | Error: type 1 patch requires N64 source |

Notes:
- "Apply" always means: resize source to declared destination size, then apply
  records at physical offsets.
- Normalization preserves input format on output: if the user fed slap a .n64
  file, slap writes a .n64 file back (after applying via the normalized
  intermediate form).
- Any patch-vs-source-format mismatch where the patch expects a specific byte
  order and the source is in a different N64 byte order is handled via
  normalization with a warning, not an error. The error rows above are for the
  strict case where the source isn't convertible (non-N64 file vs type 1
  patch).

### Realistic frequency

| Case | Rough frequency | Notes |
|---|---|---|
| Apply type 1 Z64 patch to Z64 source | ~95%+ | The Everdrive 64 anti-aliasing scenario. Must be rock-solid. |
| Apply type 1 V64 patch to V64 source | <1% | Pre-Everdrive obsolete scenario |
| Apply with source format needing normalization | 1-2% | User has different byte order than the patch expects |
| Apply type 0 patches | <1% | Produced by RomPatcher.js, rejected by bb-aps |
| Create from Z64 source | low | Users making APS patches is rare; this is the modal create case |
| Create from V64 / .n64 source | ~0% | Almost nobody creates patches from these |
| Create from non-N64 source | ~0% | Why would you? But allowed. |
| Parse encoding method != 0 | 0% | Never produced by any tool |
| Parse unknown image format | 0% | Never produced |
| Parse unknown patch type | ~0% | Never produced (except type 0 from RomPatcher.js) |

Frequency is a consideration for testing priority and error message quality,
not for decisions about whether to error vs. warn.

## Major findings vs slap (after reading the C source)

### slap's create always emits type 0; bb-aps can't apply type 0

slap's `Create.hs` always emits `APSSimple` (type 0). The reference apply tool
rejects anything that isn't type 1 (`n64aps.c:83`). This isn't a hard bug in
the correctness sense — type 0 is in the spec, RomPatcher.js produces it,
slap can apply it — but it means slap-produced APS N64 patches are not
interoperable with bb-aps.

The proposal: when the source is an N64 ROM, slap should emit type 1 with the
verification fields populated. When the source is not an N64 ROM, slap should
continue emitting type 0 with a warning about interop. This is a feature gap,
not a bug: slap is currently choosing the less-informative-but-always-applicable
option, and the proposal is to choose the more-informative option when possible.

### Destination size IS enforced

slap treats the destination size field as advisory. bb-aps does not.
`n64aps.c:225-244` actively resizes the source file before applying records:

```c
if (OrigSize != APSOrigSize) // Do File Resize
{
    if (APSOrigSize < OrigSize)
        ftruncate (x, APSOrigSize);  // shrink
    else
        for (i=0; i<(APSOrigSize-OrigSize); i++) fputc (0, ORGFile);  // grow
}
```

The destination size is the contract: the patched file WILL be exactly that size,
even before any records are applied. Records writing past it would extend the
file, but the resize-to-target-size happens first.

slap should:
1. Treat destination size as binding, not advisory
2. Resize the source to match before applying records (or, equivalently,
   allocate the output buffer to exactly destinationSize)
3. Reject any record whose offset+length exceeds destinationSize (since bb-aps
   would silently extend the file beyond, but that's almost certainly a bug)

### Encoding method != 0 must be rejected

`n64aps.c:91`:
```c
if (EncodingMethod != 0)
{
    printf ("Unknown or New Encoding Method\n");
    exit (1);
}
```

Confirmed: bb-aps hard-rejects unknown encoding methods. slap should too.

### bb-aps create never emits RLE records

`n64caps.c:WritePatch` (lines 247-249):
```c
fwrite (&ChangedStart,sizeof (ChangedStart),1,APSFile);
fputc  ((ChangedLen&0xff),APSFile);
fwrite (NEWBuffer+(ChangedOffset),1,ChangedLen,APSFile);
```

The create tool only emits literal records with length 1-255. RLE records exist
in the format and bb-aps APPLIES them (lines 281-297 of `n64aps.c`), but the
reference creator never produces them. RLE is a "decoder-only" feature in
practice.

This means: any APS N64 patch with RLE records was either created by a different
tool (Blackbag had a different creator? unlikely), modified by hand, or
constructed by RomPatcher.js/UniPatcher's create logic if they support RLE on
create.

### N64 ROM byte-swapping handling

For V64 (Doctor format) ROMs, `n64aps.c` reads cart ID, country, and CRC from
their physical positions and byte-swaps them BEFORE comparing against the patch
fields (which are stored in Z64 byte order):

- Cart ID: bytes at offset 60-61, swapped if V64
- Country: at offset 62 in Z64, offset 63 in V64 (note: physical, not logical)
- CRC: bytes at offset 16-23, byte-pair-swapped (8 bytes = 4 pairs of swap) if V64

The patch always stores values in Z64 byte order regardless of the source ROM's
format. The image format byte (0 vs 1) tells the patcher which way to swap on
read.

slap currently does fixed-offset byte checks via `ByteCheck` in SomePatch.hs.
These offsets are correct for Z64 ROMs but not for V64. **slap likely fails on
V64 source ROMs.**

### bb-aps source verification is strict by default

- Wrong image format: hard exit
- Wrong cart ID: hard exit, NOT bypassable
- Wrong country/territory: warning, bypassable with `-f` flag
- Wrong CRC: warning, bypassable with `-f` flag

The cart ID check is the immutable line. slap should treat cart ID mismatch as
a hard error and CRC/country mismatches as warnings (or errors with a force flag).

### The 5 padding bytes are always zero on create

`n64caps.c:157-161`:
```c
fputc (0,APSFile); // PAD
fputc (0,APSFile); // PAD
fputc (0,APSFile); // PAD
fputc (0,APSFile); // PAD
fputc (0,APSFile); // PAD
```

slap could verify they're zero on parse and warn if not.

## Edge cases and ambiguities — full inventory

Same format as the APS GBA doc. Each case gets a verdict.

### 1. Destination size field semantics

**What**: The header stores a "destination image size". slap currently treats this
as advisory and computes the actual output size as
`max(sourceLength, max(record end offsets))`.

**bb-aps behavior** (n64aps.c:225-244): RESIZES the source file to exactly the
declared destination size BEFORE applying any records. Truncates if shrinking,
zero-fills if growing.

**Decision**: Resolved. Treat destination size as binding. The output is exactly
that size. Records writing past it are malformed -- reject. Records leaving gaps
get zero-filled (which happens automatically if you allocate the output buffer
to destinationSize before applying records).

### 2. Incomplete tail in record stream

**What**: slap's `parseN64Records` (Parse.hs:75) returns an empty list if fewer
than 5 bytes remain. A truncated patch silently loses its trailing record(s).

**Spec position**: No EOF marker. Records terminate at EOF. The spec doesn't
address corruption.

**Is it stupid?** This is an outright bug, not a format ambiguity. 1-4 trailing
bytes is corruption, not a clean record boundary.

**Decision**: Reject if any bytes remain that don't form a complete record. A
clean parse means `position == fileSize` after reading the last record.

### 3. RLE count of 0

**What**: An RLE record with `count == 0` is a no-op (fill 0 bytes). slap accepts
it silently.

**Spec position**: The spec says RLE count is a byte; doesn't forbid 0.

**Is it stupid?** It's nonsensical but format-legal. No creation tool would
produce it.

**Decision**: Accept but warn. It's valid by the letter of the spec; it's
suspicious because no tool produces it.

### 4. Encoding method != 0

**bb-aps behavior** (n64aps.c:91): Hard exit with "Unknown or New Encoding Method".

**Decision**: Resolved. Reject at parse. slap should NOT preserve and apply.

### 5. Patch type != 1 on apply

**bb-aps behavior** (n64aps.c:83): Rejects everything except type 1. Even type 0
(documented in the spec) is rejected.

**Decision**: Accept type 0 on apply with a warning ("type 0 patch, no source
verification was possible"). Reject types 2+ (unknown layout). Accept type 1
normally.

### 5b. Patch type > 1 on apply

**Decision**: Error. Types 0 and 1 are the only defined values. A value of 2+
means an unknown header layout; we can't proceed without guessing.

### 6. Image format != 0 and != 1 (in N64-specific header)

**What**: Image format 0 = Doctor V64 (byteswapped), 1 = CD64/Z64/Wc/SP
(big-endian). slap wraps unknown values in `UnknownImageFormat` and continues.

**Spec position**: Only 0 and 1 defined.

**Is it stupid?** It's an enum with only two valid values. Other values mean
either spec extension (which never happened) or corruption.

**Decision**: Reject. Same rationale as encoding method -- guessing at byte order
for an unknown image format is unsafe.

### 7. N64 ROM byte order handling (Z64, V64, .n64)

**The format's position**: Records are physical-offset-bound to the source's
byte layout. The image format byte (0 = V64, 1 = Z64) declares which layout
the patch targets. bb-aps rejects mismatch.

**The format doesn't know about .n64** (32-bit byte-swapped). The image format
field has no value for it. But all three N64 byte orders are topologically
identical: they contain the same bytes in different physical arrangements,
losslessly interconvertible. Detection is trivial (magic at offset 0).

**bb-aps behavior**:
- Reads 4 bytes at offset 0 as a native long
- `0x12408037` = V64 (Doctor format)
- `0x40123780` = Z64 (big-endian, as read by LE system) — actually n64aps
  treats anything that isn't V64 as Z64, which silently misinterprets .n64
- On verification: reads physical bytes at fixed offsets and byte-swaps if V64

**slap decision**: Accept all three N64 byte orders as input. Normalize to the
patch's declared byte order internally. Apply records against the normalized
view. For create, detect the source format and emit the matching image format
byte (or, if source is .n64, normalize to Z64 and emit image format 1).
Preserve the input's byte order in the output (apply normalization, then
byte-swap back before writing).

The end result: every patch slap produces is bit-for-bit valid APS N64 per the
spec (image format 0 or 1 only). slap is liberal about source byte orders
because the conversions are lossless and detection is unambiguous. Warn when
normalization happens so the user knows what slap did on their behalf.

See also: the create and apply possibility matrices above.

### 8. Padding bytes (N64-specific header, offsets 69-73)

**bb-aps behavior**:
- Create (n64caps.c:157-161): writes 5 zeros
- Apply (n64aps.c:203-207): reads 5 bytes via `fgetc`, discards them (no check)

**Decision**: bb-aps silently discards. slap could be more rigorous and warn
on non-zero padding, since no created-by-bb-aps patch will ever have non-zero
padding. This is a "warn but accept" case.

### 9. Out-of-bounds record offsets

**What**: A record's offset + length could exceed the destination size, or even
the 4GB address space (theoretically). slap's in-memory apply uses `unsafeCreate`
with raw pointer arithmetic. No bounds checking before the write.

**Decision**: Reject any record where `offset + length > destinationSize`. (And
once #1 is decided, this falls out from validating against destinationSize.) For
the in-memory path specifically, defense-in-depth bounds checks should exist
regardless.

### 10. Overlapping records

**What**: Multiple records writing to overlapping offsets. Last-write-wins.

**Spec position**: Not addressed. No tool warns about this.

**Decision**: Accept but warn. It's valid (the result is well-defined: latest
record wins) but suspicious -- creation tools don't produce overlap.

### 11. Description encoding

**What**: 50-byte description, no encoding declared. System locale assumed.
slap uses `encodeBoundedLocale`.

**Spec position**: Format is silent on encoding. The original bb-aps tool was
probably ASCII-only in practice; modern tools might write UTF-8 or local
codepages.

**Is it stupid?** Yes, in the bank-account-1d6 sense. But it's unambiguous: the
format says "50 bytes of description" and that's what they are. The bytes are
what they are. The interpretation is the consumer's problem.

**Decision**: Not a problem. Implement what the format says: 50 raw bytes.
Decode for display using whatever heuristic makes sense (try UTF-8, fall back to
locale, fall back to hex). Already handled via the `EncodingGap` warning
mechanism.

### 12. All-zero or all-space description

**What**: A description that's all spaces (0x20) or all nulls is essentially
empty.

**Decision**: Already handled correctly in Describe.hs -- such descriptions are
filtered out of the metadata display. No issue.

### 13. Description longer than 50 bytes (on create)

**What**: User provides a description longer than 50 bytes; slap truncates and
emits a `FieldTruncated` warning.

**Decision**: Already handled correctly.

### 14. Creation: type 1 when source is N64, type 0 otherwise

**bb-aps behavior**: Hardcoded to type 1. Always extracts cart ID, country, CRC
from the source ROM and embeds them. Rejects non-N64 sources outright.

**Decision**: slap should be more liberal than bb-aps here.

**When source is an N64 ROM**: emit type 1.
1. Detect N64 format from magic at offset 0 (Z64, V64, or .n64)
2. If .n64: normalize to Z64 internally (warn about normalization)
3. Extract cart ID from offset 60-61 (physical, byte-swap from V64 if source
   was V64 — though .n64 is normalized first so this only applies to true V64)
4. Extract country from offset 62 (Z64) or 63 (V64 physical)
5. Extract CRC from offset 16-23, byte-pair-swap if V64
6. Emit type 1 header: image format 0 or 1 matching the normalized source
7. Cart ID, country, CRC stored in canonical Z64 byte order
8. 5 zero padding bytes
9. Destination size (4-byte LE long) = target file size

**When source is not an N64 ROM**: emit type 0 with a warning.
- The format permits type 0 (Simple).
- Other tools (RomPatcher.js) produce type 0.
- bb-aps will refuse to apply; warn the user about this interop gap.
- slap itself will apply type 0 patches fine.

The principle: don't assume user intention. The user asked slap to make an
APS N64 patch; slap makes the best-informed APS N64 patch it can given the
input. Surfaces interop consequences via warnings.

### 15. Record data length encoding ambiguity

**What**: A normal record with `data length == 0` can't exist (it would be
parsed as RLE). So normal records always have 1-255 bytes. slap handles this
correctly because the parser branches on `dataLength == 0`.

**Decision**: Not an issue. Just noting the constraint.

### 16. File size impossibly small

**What**: The minimum valid APS N64 patch file is 5 (magic) + 1 (type) + 1
(encoding) + 50 (description) + 4 (size) = 61 bytes for type 0, or
5+1+1+50+1+2+1+8+5+4 = 78 bytes for type 1. Plus at least one record (5 bytes
for normal of length 0... wait that's RLE... so 6 bytes minimum: 4 offset + 1
length + 1 byte data). But a patch with zero records is technically valid (just
the header).

**Decision**: Reject if file is shorter than the required header. Accept
zero-record patches (they're valid no-ops).

### 17a. Records writing past destination size

**What**: A record with `offset + length > destinationSize`. bb-aps's `fseek`
will happily seek past EOF and `fwrite` will silently extend the file.

**Decision**: Reject. bb-aps's behavior is a quirk of POSIX file I/O, not an
intentional spec feature. Records that write past the declared destination size
are inconsistent with the resize-to-destination semantics.

### 17b. Source verification: per-field strictness

**bb-aps behavior** (n64aps.c apply path):
| Field | Mismatch behavior | Bypassable with -f? |
|-------|-------------------|---------------------|
| Image format (V64 vs Z64) | hard exit | NO |
| Cart ID | hard exit | NO |
| Country/territory | warning | YES |
| CRC | warning | YES |

**Decision**: slap should match this. Cart ID is the load-bearing identifier
(it identifies the actual game). Country and CRC mismatches happen with
revision differences and shouldn't block applying.

### 18. Records ordering

**bb-aps create behavior**: emits records in source-file order (sequential scan).
**bb-aps apply behavior**: applies records in patch-file order. No sort.

**Decision**: Accept any order on parse. No warning needed -- order is not
semantically meaningful. (Compare APS GBA where I suggested warning on
out-of-order; for APS N64 there's no reason to.)

### 19. Description encoding

**bb-aps behavior**: Treats description as 50 raw bytes. Apply tool prints with
`printf("%s", ...)` after null-terminating the buffer at offset 50, so it stops
at the first null. No encoding declared anywhere.

**Decision**: Not a problem. Per the user's framing: the format unambiguously
says "50 raw bytes." How those bytes are interpreted is the consumer's problem.
slap stores them as `ByteString` which is correct. Display logic can try UTF-8,
fall back to locale, fall back to hex.

### 20. Record offset == 0 with payload that overlaps the ROM header

**What**: A record at offset 0 of an N64 ROM would patch over the cart header
(magic, cart ID, CRC). For type 1 patches, this would mean the post-patch source
validation fails its own checks. But that's the consumer's problem.

**Decision**: Not a problem. Patches can legitimately modify the ROM header.
The verification happens before the patch is applied, against the source.

## Decision matrix

All items have a verdict: error, warn-and-proceed, silent, or feature-add.

### Errors (reject)

| # | Case | Reason |
|---|------|--------|
| 1  | Records cause output to exceed destination size | Contradicts the declared dest size, which is binding |
| 2  | Incomplete trailing bytes at EOF | Corruption — cannot be a clean record boundary |
| 4  | Encoding method != 0 | Unknown record format, cannot guess |
| 5b | Patch type > 1 | Unknown header layout, cannot guess |
| 6  | Image format other than 0 or 1 | Unknown source byte order, cannot guess |
| 16 | File too small for header + any records | Structurally invalid |
| —  | Type 1 patch applied to non-N64 source | Type 1 metadata requires N64 source |
| —  | Type 1 patch with source in a byte order not convertible to the patch's declared format | Would produce corrupt output |

### Warnings (accept and proceed)

| # | Case | What we tell the user |
|---|------|------------------------|
| 3  | RLE record with count == 0 | No-op record, unusual, not produced by any known tool |
| 5  | Type 0 patch on apply | No source verification was embedded; bb-aps would reject this |
| 8  | Padding bytes non-zero | Unexpected content, might indicate non-bb-aps origin or corruption |
| 10 | Overlapping records | Later record wins, result well-defined but no known creator produces this |
| —  | Source byte order normalized (.n64 → Z64/V64, etc.) | Conversion happened on your behalf |
| —  | Create from non-N64 source | Emitted type 0 for interop; bb-aps will refuse to apply |
| —  | Description truncated on create (>50 bytes) | Already handled; keep as-is |
| —  | Destination size differs from source size | Informational: file will be resized before applying |

### Silent (no notification needed)

| # | Case | Why silent |
|---|------|------------|
| 12 | All-spaces or all-nulls description | Nothing interesting happened |
| 13 | Description encoding "issues" | Format has no encoding flag; nothing for us to say |
| 18 | Records in arbitrary order on parse | Order is not semantically meaningful |
| 20 | Record overwriting ROM header bytes | Legitimate; consumer problem if it breaks verification on next apply |

### Feature additions (new capability, not a bug fix)

| # | Item | Effort |
|---|------|--------|
| 14 | Emit type 1 (with N64 metadata) when source is an N64 ROM | Medium: need N64 detection, field extraction, V64/Z64 handling |
| 7  | Source byte order detection and normalization (Z64/V64/.n64) | Medium: detection + conversion + output byte-order preservation |
| —  | Encoding method registry shim (list of supported methods, currently `[0]`) | Trivial: three-line refactor |
| —  | Image format registry shim (list of supported values, currently `[0, 1]`) | Trivial: three-line refactor |
| —  | Patch type registry shim (list of supported values, currently `[0, 1]`) | Trivial: three-line refactor |

### Tentative (deferred)

| Item | Notes |
|------|-------|
| `OpaqueDescriptionBytes` newtype for free-text fields with no encoding flag | Cross-cutting refactor affecting PPF and APS N64. Makes format-level dumbness visible at the type level. Runtime behavior unchanged. Worth doing if it would clean up `Convert.hs` encoding-gap logic; skip otherwise. |

## What slap currently gets right (keep as-is)

- Magic check (`APS10`)
- Record parsing logic (offset, length, RLE branch, data)
- 50-byte description field handling
- LE encoding for offsets and sizes
- Storing cart ID, country, CRC as raw ByteString (no endianness assumption baked in)
- 255-byte chunk splitting on create
- Description truncation warning on create via `FieldTruncated`
- Filtering all-whitespace descriptions from metadata display
- Handling `data length == 0` as the RLE marker

## Suggested implementation order

A rough order for turning this proposal into code, cheapest-first:

1. **Trivial registry shims**: encoding method, patch type, image format.
   Three three-line changes. Zero risk. Sets up the rejection logic for the
   next steps.
2. **Rejection cases**: destination size enforcement, incomplete trailing
   bytes, records past destination size. These are small local changes to
   Parse.hs and Apply.hs. Each comes with a test for the error condition.
3. **Warning cases**: RLE count 0, overlapping records, non-zero padding,
   type 0 on apply. Small local changes; each adds a single `SlapWarning`.
4. **N64 byte order support**: the biggest piece. Detection, V64 handling for
   verification, .n64 normalization. Needs new test fixtures for each byte
   order. Can be split into two phases:
   - 4a. V64 support (matches bb-aps, enables the existing verification path)
   - 4b. .n64 normalization (new capability)
5. **Type 1 creation**: extract N64 metadata on create when source is an N64
   ROM, emit type 1 header. Depends on step 4 being done so V64 sources work
   correctly. Comes with interop tests against bb-aps (or at least against
   cross-validation with RomPatcher.js).
6. **Tentative**: `OpaqueDescriptionBytes` newtype if the team wants to do
   the cross-cutting refactor. Not blocking on anything; could be its own
   separate work item covering PPF and APS N64 together.

## Open questions

1. ~~Did Silo or Fractal write reference code that's still recoverable?~~ YES.
   Full C source for both `n64aps` (apply) and `n64caps` (create) is in
   `bb-aps12.zip`. Recovered from icequake.net mirror.
2. ~~Does the original bb-aps tool support `Apply` only, or also `Create`?~~ Both,
   as separate binaries (`n64aps.exe` and `n64caps.exe`).
3. **Open**: Are there APS N64 patches in the wild that use encoding method != 0
   or patch type 0? Probably not for encoding (no tool ever produced non-zero).
   Type 0 patches probably exist from RomPatcher.js users.
4. **Open**: Are there APS N64 patches with destination size disagreeing with
   record extents? Probably not from bb-aps. Possibly from other tools.
5. **Open**: When applying to a source whose byte order differs from the
   patch's (and both are valid N64 formats), should slap preserve the input's
   byte order in the output, or match the patch's declared format? The
   proposal says "preserve input" but a `--output-format=z64` flag could
   override this if anyone actually needs it.

