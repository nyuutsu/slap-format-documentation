# BPS — wire format

A BPS patch encodes the byte-by-byte construction of a target file, using a source file as a dictionary. The patch carries a magic string, the declared sizes of source and target, an opaque metadata field, a stream of actions, and three CRC32 checksums.

BPS is linear. The target begins as a zero-byte file and is written strictly forward: each action writes some number of bytes to the end of the target, advancing the write position by that many bytes. There is no seek, no backtrack, no overwrite. A patch applier produces the target by reading actions in order and executing them in order.

## Glossary

- **source** — the original (input) file the patch is applied against.
- **target** — the modified (output) file the patch produces.
- **number** — a variable-length unsigned integer. The encoding is described in its own section below.
- **action** — one of the four patch-stream commands: SourceRead, TargetRead, SourceCopy, TargetCopy.
- **length** — how many bytes a single action writes to the target. Every action writes at least one byte.
- **outputOffset** — the write cursor into the target. Starts at 0; increments by one for every byte written.
- **sourceRelativeOffset** — a read cursor into the source, used only by SourceCopy. Starts at 0; moved by a signed delta at the start of each SourceCopy, then increments by one per byte read.
- **targetRelativeOffset** — a read cursor into the already-written target, used only by TargetCopy. Starts at 0; moved by a signed delta at the start of each TargetCopy, then increments by one per byte read.
- **metadata** — an opaque byte string carried in the header, whose length is declared explicitly.
- **footer** — the final twelve bytes of the patch: three 32-bit CRC values.

## Layout

A BPS patch consists of:

1. The four ASCII bytes `BPS1`.
2. Three numbers — `source-size`, `target-size`, `metadata-size` — in that order.
3. `metadata-size` bytes of opaque metadata.
4. Zero or more actions.
5. Twelve bytes of footer: `source-checksum`, `target-checksum`, `patch-checksum`.

```
┌──────┬─────────────┬─────────────┬───────────────┬─────────────────┬─────────┬────────┐
│ BPS1 │ source-size │ target-size │ metadata-size │    metadata     │ actions │ footer │
│ (4)  │    (var)    │    (var)    │     (var)     │ (metadata-size) │   (*)   │  (12)  │
└──────┴─────────────┴─────────────┴───────────────┴─────────────────┴─────────┴────────┘
```

The widths of `source-size`, `target-size`, `metadata-size`, and the actions are not known in advance; they are discovered by decoding.

## Numbers

BPS uses a single variable-length unsigned encoding for every integer field in the header and for every integer inside an action. The format imposes no upper bound on the representable value.

Each byte of an encoded number carries seven payload bits in its low positions, and uses the high bit as a terminator flag.

```
Each byte:

  ┌───┬───────────────────────┐
  │ t │      data (7 bits)    │
  └───┴───────────────────────┘
    │
    terminator
    1 = last byte of this number
    0 = another byte follows
```

A decoder reads bytes until it sees one with the terminator bit set. The payload bits are assembled least-significant-first — the first byte's seven bits are the lowest, the next byte's are the next seven up, and so on.

There is one subtlety. A naive "seven bits per byte, high bit terminates" scheme would give the value 1 two different encodings: as `0x81` (one byte, terminator set, payload 1), or as `0x01 0x80` (continuation byte with payload 1, then a terminator with payload 0 — which by naive reassembly would also be 1). BPS prevents this by subtracting one from the running value after every continuation byte on encode, and correspondingly adding a compensating offset on decode (128 after the first continuation byte, 128 + 16384 after the second, and so on). Each non-negative integer has exactly one encoding.

Some examples:

| Value | Encoding  |
|-------|-----------|
| 0     | `80`      |
| 1     | `81`      |
| 127   | `FF`      |
| 128   | `00 80`   |
| 129   | `01 80`   |
| 255   | `7F 80`   |
| 256   | `00 81`   |

Every number byuu's spec refers to — the three header sizes, the packed action prefix, the signed copy offset — uses this same encoding. The format never embeds fixed-width integers anywhere except in the footer.

## Header

Immediately after the `BPS1` magic come three numbers:

- `source-size` — the length in bytes of the source file this patch was built against.
- `target-size` — the length in bytes of the target file this patch produces.
- `metadata-size` — the length in bytes of the metadata field that follows.

If no metadata is present, `metadata-size` is still explicitly encoded (as the single byte `0x80`); the metadata field then occupies zero bytes on the wire.

The metadata itself is an opaque byte string of exactly `metadata-size` bytes. There is no null-terminator; the length is declared, not marked. byuu's spec makes two statements about its contents, which apply at different levels. Officially, metadata should be XML 1.0 encoding UTF-8 data — that is the recommended form. Separately, the format accepts anything: the spec explicitly states that the contents are entirely domain-specific and that a patch with arbitrary metadata contents is still considered valid.

## Actions

After the header, the patch contains zero or more actions. Each action begins with a single number — the packed prefix — that carries two fields: the action code in its low two bits, and one less than the transfer length in the remaining high bits.

```
Packed prefix (the decoded number):

  (high bits)                         (low bits)
  ┌─────────────────────────────────┬─────┐
  │          length − 1             │  a  │
  │        (arbitrary width)        │ (2) │
  └─────────────────────────────────┴─────┘
                                    action
                                     code
```

After the prefix, some actions have a body; others have nothing.

| Code | Action      | Body following the prefix  |
|------|-------------|----------------------------|
| 0    | SourceRead  | (none)                     |
| 1    | TargetRead  | `length` raw bytes         |
| 2    | SourceCopy  | one signed-offset number   |
| 3    | TargetCopy  | one signed-offset number   |

Because length is encoded as `length − 1`, an action always writes at least one byte; a zero-length action has no encoding. byuu's spec notes that this also slightly helps with patch size reduction in some cases.

### SourceRead

SourceRead copies `length` bytes from the source file to the target file. For each byte, the source is read at the *current* `outputOffset`, and the byte is written to the target at that same `outputOffset`. The write cursor advances one step per byte. SourceRead does not consult, and does not modify, `sourceRelativeOffset` or `targetRelativeOffset`.

Because each byte is read from the source at the current `outputOffset`, every read must satisfy `outputOffset < source-size`. byuu's general rule against reading past the end of a file applies here as elsewhere: if it would be violated, the patch is invalid and the applier must abort.

This is the "no change here" action: the corresponding region of source and target are bit-identical, so the actual bytes need not be stored in the patch. byuu's spec notes that SourceRead is rarely useful in delta-style patch creation; it is mainly intended to allow for linear-based patchers, though it can also be useful in delta patches when source and target happen to be identical at the same offset.

### TargetRead

TargetRead reads `length` bytes directly from the patch stream — literally the next `length` bytes after the packed prefix — and writes them to the target at the current `outputOffset`. The write cursor advances one step per byte.

This is the "new data" action: the payload is stored inline in the patch because it is not available to the applier any other way.

TargetRead does not consult, and does not modify, `sourceRelativeOffset` or `targetRelativeOffset`.

### SourceCopy

SourceCopy treats the entire source file as a dictionary, in the manner of LZ compression. After its packed prefix, it reads one further number: a signed delta that adjusts `sourceRelativeOffset`.

```
Signed offset (the decoded number):

  (high bits)                         (low bits)
  ┌─────────────────────────────────┬─────┐
  │           |offset|              │  s  │
  │        (arbitrary width)        │ (1) │
  └─────────────────────────────────┴─────┘
                                    sign bit
                                1 = negative
                            0 = non-negative
```

The low bit is the sign (1 for negative, 0 for non-negative), and the rest is the absolute value of the delta. Zero encodes naturally as `0 | (0 << 1) = 0`.

The delta is applied once, at the start of the action. Then `length` bytes are copied from the source: the byte at `sourceRelativeOffset` is written to the target, the cursor advances, the byte at the new `sourceRelativeOffset` is written, and so on, until `length` bytes have been copied. The write cursor advances in lockstep.

`sourceRelativeOffset` must remain in `[0, source-size)` at every point during the action — both after the initial delta is applied and after each increment. If it would leave that range, byuu's spec says the patch is invalid and the applier must abort.[^cursor-range]

### TargetCopy

TargetCopy is identical in shape to SourceCopy, but it reads from the already-written portion of the target rather than from the source. The cursor is `targetRelativeOffset` rather than `sourceRelativeOffset`.

`targetRelativeOffset` must remain in `[0, outputOffset)` at every point during the action — both after the initial delta is applied and after each increment. The upper bound is `outputOffset`, the current write position — not `target-size` — so TargetCopy can only read from bytes the patch has already produced. If `targetRelativeOffset` would leave that range, byuu's spec says the patch is invalid and the applier must abort.

One useful consequence: TargetCopy can perform run-length expansion. Suppose a region of the target is a long run of a single byte. A TargetRead can write one copy of that byte; the next action can be a TargetCopy whose cursor points at the byte just written. As the TargetCopy reads, `outputOffset` advances in step with `targetRelativeOffset`, so the cursor stays exactly one position behind the write — and the byte is re-read and re-written for as many rounds as `length` dictates. A long run costs one byte of target data plus two action prefixes.

### Termination

Actions repeat until the end of the patch. A decoder tracks its position in the patch file and stops once that position reaches `size − 12` — the start of the footer. byuu's spec gives the stopping condition as `offset() >= size() − 12`.

## Footer

The last twelve bytes of a BPS patch are three 32-bit CRC32 checksums:

```
┌──────────────────┬──────────────────┬──────────────────┐
│  source-checksum │  target-checksum │  patch-checksum  │
│        (4)       │        (4)       │        (4)       │
└──────────────────┴──────────────────┴──────────────────┘
```

- `source-checksum` — the CRC32 of the source file. Its purpose is to verify that the applier has been given the correct input.
- `target-checksum` — the CRC32 of the target file the patch produces. Its purpose is to verify that the patch has been applied successfully.
- `patch-checksum` — the CRC32 of all patch bytes before this field. Its purpose is to verify that the patch itself has not been corrupted.

The checksums appear at the end of the patch so that a patcher can compute them incrementally as the patch is produced.

byuu's spec characterises the three checksums as integrity-only: they catch corruption and mistakenly incorrect files, but are deliberately not cryptographically secure. The spec suggests that if security is a serious concern, a stronger hash such as SHA-256 can be placed in the metadata — or else BPS is not the right format for the use case.

## Example

A minimal patch that turns a four-byte source containing `00 11 22 33` into a five-byte target containing `00 11 22 33 FF`.

```
  42 50 53 31        "BPS1"
  84                 source-size  = 4
  85                 target-size  = 5
  80                 metadata-size = 0
                     (metadata: zero bytes, not present)

  8C                 action: packed prefix
                       low 2 bits = 00  → SourceRead
                       rest       = 3   → length 4
                     — copies source[0..3] to target[0..3]

  81                 action: packed prefix
                       low 2 bits = 01  → TargetRead
                       rest       = 0   → length 1
  FF                 inline payload byte
                     — writes 0xFF to target[4]

  XX XX XX XX        source-checksum  (CRC32 of the source)
  XX XX XX XX        target-checksum  (CRC32 of the target)
  XX XX XX XX        patch-checksum   (CRC32 of all preceding bytes)
```

Total on the wire: 22 bytes, not counting the CRC32 values.

## What the format doesn't specify

The grammar above is byuu's spec. Several practical questions are silent, ambiguous, or addressed only informally:

- **Endianness of the footer.** byuu writes the checksums as `uint32` without stating byte order.
- **Which CRC32.** "CRC32" admits more than one standard polynomial; byuu names none.
- **Header-size agreement.** What happens if the source file handed to the applier is not exactly `source-size` bytes, or if the actions produce a stream of bytes whose length differs from `target-size`, is not stated.
- **Checksum mismatches.** Whether a mismatched `source-checksum` or `target-checksum` is fatal or advisory is not stated.
- **Metadata contents.** Officially UTF-8 XML 1.0, but the spec explicitly permits any byte sequence.
- **Integer width.** The variable-length encoding has no upper bound; the reference code uses 64-bit integers, and the spec acknowledges that wider integers would be representable but impractical.
- **Abort semantics.** byuu's spec calls for the applier to abort on out-of-range cursor moves, but does not prescribe what "abort" entails — diagnostics, partial-output disposal, exit status, or recovery.
- **Termination overshoot.** byuu's stopping condition is `offset() >= size() − 12` — greater than or equal. The spec does not say what a decoder should do if, having finished an action, its patch position has passed `size − 12`, or if it finds itself mid-action at that boundary.
- **Syntactic vs. semantic invalidity.** byuu uses the single word "invalid" both for patches that violate the wire format and for patches whose cursor operations run out of range against a particular source file. The spec does not draw the distinction.
- **Empty patches.** Whether a patch with zero actions — header, no action stream, footer — is well-formed is not stated. The grammar ("zero or more actions") would permit it.

These are answered separately; see `questions.md`.

[^cursor-range]: byuu's prose states that `sourceRelativeOffset` "can never be less than zero, or greater than or equal to the source file size," and likewise for `targetRelativeOffset` against `outputOffset`. Read strictly, this is violated by the reference pseudocode itself, which increments the cursor after the final read of the action and so leaves it briefly equal to the upper bound. No read ever actually happens from that position — the loop exits first — so the distinction between the prose ("cursor must never reach the bound") and the pseudocode ("no read from the bound or beyond") doesn't change what the applier does.
