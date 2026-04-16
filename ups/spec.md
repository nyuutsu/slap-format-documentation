# UPS — Format Specification

This is a clean writeup of the on-disk structure of a UPS patch,
derived from byuu's authoritative specification document
(`upstream/ups-spec.pdf`, dated 2008-04-18). Where this document and
the PDF disagree, the PDF wins. The goal here is to have the spec
facts in a form that cross-references slap's implementation modules
and that flags edge cases the terse original spec doesn't address.

## Identity

- **Magic**: `UPS1` (4 bytes, ASCII)
- **Endianness**: not applicable at the wire level; the only
  fixed-width fields are the three CRC32s in the footer, stored as
  `uint32` little-endian by convention
- **Author**: byuu (now Near)
- **Released**: 2008-04-18
- **License**: CC BY-NC-ND 3.0 (spec document); public domain
  (the format itself)
- **Reference tool**: beat (byuu's own tool) and tsukuyomi

## Overall structure

```
[ "UPS1"                       ] 4 bytes
[ input-size                   ] byuu varint
[ output-size                  ] byuu varint
[ block 0                      ]
[ block 1                      ]
[ ...                          ]
[ block N-1                    ]
[ input CRC32                  ] 4 bytes, little-endian
[ output CRC32                 ] 4 bytes, little-endian
[ patch CRC32                  ] 4 bytes, little-endian
```

The input/output sizes are byuu-style variable-length integers (see
`Slap.Binary.putByuuVarint` / `getByuuVarint`, shared with BPS). The
three CRC32s together form a fixed 12-byte footer. Blocks fill the
space between the sizes and the footer; parsers detect end-of-blocks
by comparing the patch-read pointer against `(file-size − 12)`.

## Block structure

Each block encodes one contiguous run of bytes where the input and
output differ, preceded by a count of matching bytes to skip.

```
[ skip-count                   ] byuu varint
[ xorData[0]                   ] 1 byte, != 0x00
[ xorData[1]                   ] 1 byte, != 0x00
[ ...                          ]
[ xorData[xorLen-1]            ] 1 byte, != 0x00
[ terminator                   ] 1 byte, == 0x00
```

The skip count is the number of matching bytes between the previous
block's terminator and this block's first differing byte. For the
first block in the patch, it's counted from offset 0.

`xorData` holds the raw XOR between the input and output bytes for
the differing run. A byte of `0x00` in this position would mean
"input and output are equal here", which is indistinguishable from
the terminator, so `xorData` bytes are implicitly non-zero in a
well-formed patch.

The terminator is not ornamental: it corresponds to the first byte
where input and output are equal again after the differing run. Its
XOR value is genuinely `0x00` (equal bytes XOR to zero), so the
terminator is both an end-of-run marker and a legitimate data byte
at a real file offset. Apply logic must advance the file pointer
through the terminator position, not past it.

## Diff semantics

Per byte at position `p`:

```
output[p] = input[p] ^ xorByte(p)
```

where `xorByte(p)` is `0x00` for positions not covered by any
block's xorData or terminator, and the corresponding xorData /
terminator byte otherwise.

**Zero-fill past input EOF.** The spec explicitly states: *"If
reading past input file EOF, XOR with 0x00."* This means the input
file is virtually zero-padded to the maximum of input-size and
output-size for XOR purposes. In particular:

- If input is shorter than output, positions past input EOF are
  treated as virtual `0x00`. `output[p] = 0 ^ xorByte(p) = xorByte(p)`.
- If input is longer than output, positions past output EOF are
  ignored (not written, since the output buffer is only output-size
  long).

slap implements this in `Slap.UPS.Apply.xorSourceSlice` as a two-phase
loop: an in-bounds phase that reads source bytes normally and XORs
against `xorData`, and a zero-fill phase that writes `xorData` bytes
verbatim (since `x XOR 0 = x`).

## Bi-directional patching

This is the spec's headline feature and the reason for the XOR-based
encoding. The spec section 2 lists it as an advantage:

> "automatic bi-directional patching. The same patch can both patch
> and unpatch a game."

Because XOR is self-inverse, applying the same block sequence to the
output file produces the input file. The only thing that changes
direction-wise is which declared size the user's file should match
and which declared CRC32 it should verify against.

**slap's handling of bi-directionality.** slap splits direction into
two explicit commands: `slap apply` runs forward (input → output) and
`slap undo` runs backward (output → input). This is a design choice,
not a spec requirement. byuu's reference implementation (beat) and
Flips detect direction automatically by checking the user-supplied
file's size against both declared sizes. The spec describes the
capability without prescribing the dispatch mechanism; both
approaches are spec-faithful.

See `Slap.SomePatch` — the `patchApply` closure runs `applyUPS`
forward, and `patchUndo` reapplies the same patch to the target to
recover the source (pure self-inverse).

## Footer

Three CRC32 values appear in the last 12 bytes of the patch, all
stored little-endian:

| Offset from EOF | Field              | Notes                    |
|----------------:|--------------------|--------------------------|
|           −12   | input CRC32        | CRC32 of the input file  |
|            −8   | output CRC32       | CRC32 of the output file |
|            −4   | patch CRC32        | CRC32 of all bytes before this field |

The patch CRC32 is computed over `patch[0 .. len−4]` — i.e. every
byte before the patch CRC32 itself, inclusive of the other two
CRC32s. This means the patch CRC32 also transitively guards the
integrity of the input and output CRC32 fields.

## What the spec says about verification

Section 3.4 of the PDF says, verbatim:

> "Values should be verified when applying the UPS patch. This
> ensures the integrity of the patch itself, and that the patch is
> being applied to the correct file."

followed by the three CRC32 fields. "Values" here refers to the
three CRC32 fields, not to the file sizes. The verification
mechanism the spec explicitly describes is CRC-based.

The spec uses SHOULD-level language ("should be verified"), not
MUST. A patcher that skips CRC verification is not forbidden by the
spec, just inferior. slap chooses to verify CRCs by default and
downgrade them to warnings only under `--no-verify`.

**The spec does NOT say** anything explicit about size verification.
The sizes are described as "exact file sizes" (section 3.2), which
means the encoded values are accurate — but the spec does not
specify a procedure for what to do if the file the user supplies
has a different size.

slap populates `verifyFileSizeAdvisory` from the declared input size for the
forward-apply path, which triggers a warn-level "size mismatch"
diagnostic in `Main.verifySource` before the CRC hard-error fires.
This gives the user a more specific message than "CRC mismatch"
when the underlying problem is that they handed us the wrong file,
without inventing a rejection requirement the spec doesn't state.

## Encoding pointers (varint)

Section 3.5 of the spec gives the encoder:

```
def encode(offset):
  loop {
    x = offset & 0x7f
    offset >>= 7
    if offset == 0 {
      zwrite(0x80 | x)      // terminator
      break
    }
    zwrite(x)                // continuation
    offset -= 1              // subtract-one trick
  }
```

This is the same encoding BPS uses, implemented in
`Slap.Binary.putByuuVarint` / `getByuuVarint`. The subtract-one
trick is what makes the encoding canonical: each value has exactly
one valid byte representation, because the implicit `+128` (from
the inverse of the `offset -= 1`) in the decoder's accumulator
means any encoding with a leading-zero continuation byte decodes to
a value in a strictly higher range than a shorter encoding would.
Trying to encode `1` as `0x01 0x80` produces `129`, not a
non-canonical `1`. See `Props.SpecConformance.prop_varintCanonical`.

## Expressible, in-practice-rare conditions

The format can represent these cases:

- **Input size != output size.** Growth (input shorter) is handled
  by virtual zero-padding of input per the spec. Shrinkage (input
  longer) is possible but requires the extra input bytes be zero
  so they XOR cleanly with the nothing-follows region. slap's
  `createUPS` rejects shrinkage where source tail has non-zero
  bytes, with `UPSUnencodeablePair UPSSourceTailNonZero`.
- **Empty patch (no blocks).** Input and output are identical. The
  apply path's tail copy (source → target) handles this with zero
  blocks. Round-trips cleanly.
- **Last byte of output differs from last byte of input.** The
  block terminator requires input[p] == output[p] at the position
  after the last differing byte. If the last byte of output differs
  from input, there's no valid terminator position within the file.
  slap's `createUPS` rejects this with `UPSUnencodeablePair
  UPSLastByteDiffers`. byuu's beat handles this by extending the
  implicit file by a virtual zero byte, which works because reads
  past EOF are spec-defined to return zero.

## Format-legal but structurally malformed

The format lacks structural constraints that a well-formed patch
would honor but that a hand-crafted or corrupted patch could
violate:

- **Block xorData containing 0x00.** The parser uses `getUntilByte
  0x00` to split runs, so a 0x00 inside xorData effectively splits
  that run into two separate blocks (the second with skip=0).
  slap's `Slap.UPS.Types.UPSBlock` type documentation notes this is
  a property of the encoded form, not an in-memory invariant.
- **Block whose total span exceeds output size.** slap's
  `applyUPS` clips the out-of-bounds portion and continues;
  `detectOOBBlocks` emits a warning at parse time. See
  `Props.SpecConformance.upsApplyBlockPastTarget`.
- **Block without a terminator before end of body.**
  `getUntilByte` fails with "terminator not found at offset …". See
  `Props.SpecConformance.upsBlockMissingTerminator`.
- **Patch CRC32 mismatch.** Detected by `Slap.UPS.Parse.parseUPS`
  before the body is even decoded. See
  `Props.SpecConformance.upsWrongPatchCRC`.
- **Input/output CRC32 mismatch with user-supplied file.** This is
  caught at apply time by `Main.verifySource` (forward direction)
  via `checkCRC`, which dies by default and warns under
  `--no-verify`.

## Limitations of the format

- **CRC32 is the only integrity mechanism.** Adequate for catching
  accidental corruption; not cryptographically secure. Collision
  attacks are possible in principle (there is no known collision
  attack on the four-byte patch CRC32 that an adversary could use
  to smuggle a malicious patch past verification, but slap is not
  the right tool if this is a concern).
- **No metadata fields.** UPS patches cannot carry author,
  description, version, or any other metadata. The format is deliberately
  minimalist per the spec's "easy to implement" design goal. When
  converting a metadata-rich patch (BPS with manifest, NINJA1 with
  info fields, etc.) to UPS, the metadata must be dropped. slap's
  conversion contract system flags this as a dropped-field warning.
- **No compression.** The spec explicitly defers compression to
  external tools: *"The reason UPS does not include compression is
  because ZIP, RAR, and 7z have and will always do it better."*
- **No extension mechanism.** The spec is marked final:
  *"UPS is a finalized spec. Patches created will work with all
  future versions."* There is no version byte, no reserved space,
  and no way to negotiate features without breaking existing tools.

## Cross-reference to slap modules

| Concern                              | Module                       |
|:-------------------------------------|:-----------------------------|
| Parse (patch bytes → UPSPatch)       | `Slap.UPS.Parse`             |
| Apply (UPSPatch + source → target)   | `Slap.UPS.Apply`             |
| Create (source + target → UPSPatch)  | `Slap.UPS.Create`            |
| Describe (UPSPatch → ExplainData)    | `Slap.UPS.Describe`          |
| Types (UPSPatch, UPSBlock, lengths)  | `Slap.UPS.Types`             |
| Varint codec                         | `Slap.Binary`                |
| Verification plumbing                | `Slap.SomePatch`, `app/Main.hs` |
| Spec conformance tests               | `test/Props/SpecConformance.hs` |
