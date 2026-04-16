# DPS -- Research and Proposal

Slap's design decisions for DPS (Deufeufeu Patching System). DPS has no
formal written spec; the format's behavior is reverse-engineered from
`dpspatcher.exe` (the sole reference implementation) and from the
format's one known specimen (the Jump Ultimate Stars English translation
patch, v0.8). The canonical reference is the binary, not any document.

For the wire-format details and field research behind these decisions,
see `findings.txt`. This document is slap-specific: what we chose, why
we chose it, and what alternatives were rejected.

**Quick orientation**: DPS is a record-based, NDS-oriented patch format.
A 198-byte header carries three 64-byte text fields (name, author,
version), a stability flag, a format version byte, and a 4-byte LE
original ROM size. Records follow until EOF, each tagged with a mode
byte: mode 0 = CopyFromROM (copy a range from the source), mode 1 =
EnclosedData (literal bytes carried in the patch). No checksums, no
CRCs, no hashes. The original ROM size field is the only integrity
check the format offers.

## Strictness and symmetry

DPS has exactly two integrity surfaces: the original ROM size field in
the header, and the record mode byte. slap's discipline for both:

- **Source-size validation** defaults to verify: a mismatch between the
  source file's actual size and the header's `dpsOriginalSize` field is
  a `SlapError`. The `--no-verify` flag downgrades this to a warning,
  matching slap's discipline for CRC checks in other formats. This is
  slap's only available integrity check for DPS -- the format has no
  checksums -- so the default is strict.

- **Unknown record modes** are `SlapError`. The format defines exactly
  two modes (0 and 1). The spec is silent on what to do with other
  values; slap's position is that an unknown mode byte means the patch
  is either corrupt or from a format revision slap doesn't understand,
  and guessing is worse than refusing.

- **Output size** is determined by record extent, not source size. The
  output is sized to `max(recordOffset + recordLength)` across all
  records, matching `dpspatcher.exe`.

- **Output initialization** is zero-filled, not source-prefilled. Gaps
  between records contain zeros, not source bytes.

Parse rejects spec violations with `SlapError`. `SlapWarning` is
reserved for unusual-but-conformant input and loss during format
conversion. There is currently no known case where a DPS patch would
trigger a warning rather than an error or clean acceptance -- the
format is too simple and too narrowly used for ambiguous-but-legal edge
cases to have arisen.

## Resolved design questions

### 1. Output initialization: zero-filled, not source-prefilled

**What slap does**: Allocates a zeroed output buffer before applying
records. Gaps between records -- regions of the output that no record
writes to -- contain `0x00`.

**Why**: `dpspatcher.exe` opens a fresh output file (C `fopen` with
`"wb"`, which creates a zero-length file; `fseek`/`fwrite` extends it,
and gaps between seeks are zero-filled by the OS). It never reads from
or copies the source ROM into the output wholesale; the source is only
accessed when a mode-0 (CopyFromROM) record explicitly copies a range.

This was confirmed empirically: comparing slap's former source-prefilled
output against `dpspatcher.exe`'s output for the JUS patch revealed
85,757 differing bytes across 393 gap regions. Every single diff was
monodirectional: slap held the original ROM byte, `dpspatcher.exe` held
`0x00`. Zero differences in the other direction.

**Alternative rejected**: Prefilling the output with the source ROM
(as IPS apply does). This would leave source bytes in gaps, producing
output that does not match the reference implementation. DPS's design
intent is that the patch fully describes the output -- CopyFromROM
records explicitly request source data where it's wanted, and everything
else is new data or zeros.

### 2. Output size: furthest record write extent

**What slap does**: Sizes the output to
`max(recordOffset + recordLength)` across all records.

**Why**: `dpspatcher.exe` writes records via `fseek`/`fwrite` into a
file opened with `"wb"`. The file's final size is the furthest byte
written, which is the maximum of `(offset + length)` across all records.
It is not the source size, and it is not declared in the header.

This was confirmed empirically: for the JUS patch, `dpspatcher.exe`
produces 65,942,936 bytes. The source ROM is 67,108,864 bytes. slap
formerly produced output equal to the source size, which was 1,165,928
bytes too large.

**Alternative rejected**: Sizing output to the source ROM size. This is
wrong -- it pads the output with 1.1 MB of trailing zeros (or source
bytes, compounding with bug #1). The format has no "output size" header
field; the output size is an emergent property of the records.

### 3. Unknown record modes: reject with SlapError

**What slap does**: Matches record mode bytes against the two defined
values (0 = CopyFromROM, 1 = EnclosedData) and rejects anything else
with a structured `SlapError`.

**Why**: The format defines exactly two modes. The spec (such as it is)
does not address unknown modes. `dpspatcher.exe`'s apply logic has a
two-branch `if`/`else` on the mode byte -- mode 0 takes the CopyFromROM
path, everything else falls through to EnclosedData. This is an
implementation artifact (a missing bounds check in C), not a design
decision to treat unknown modes as enclosed data.

slap's position: an unknown mode byte means the patch is either corrupt,
hand-edited, or from an unrecognized format revision. All three cases
warrant refusal rather than silent reinterpretation. This matches slap's
discipline elsewhere (unknown encoding methods in APS N64, unknown image
formats in APS N64, unknown record types in other formats).

**Alternative rejected**: Treating any non-zero mode as EnclosedData
(wildcard match). This was slap's former behavior. It silently accepts
corrupt or unknown-version patches and interprets their record bodies
under the wrong framing -- a mode-2 record might have a completely
different field layout, and reading its bytes as if they were
EnclosedData could produce silent corruption.

### 4. No NDS header fixup

**What slap does**: Writes the output exactly as the records describe
it. If a record writes an NDS header with a stale ROM-size field, that
stale value appears in the output. slap does not recalculate or correct
any NDS header fields.

**Why**: The patch's records fully describe the output. If a record
writes bytes to the NDS header region, those bytes are part of what the
patch asked for. slap's job on apply is to execute the patch faithfully
-- to produce what the records describe, not what slap thinks the output
"should" have looked like. This was specifically investigated: the JUS
patch's final record (record 400) writes a stale ROM-size of 884,552 at
NDS header offset `0x80` -- an intermediate write-cursor value from the
patch creation tool, not the actual output size. `dpspatcher.exe` applies
this record verbatim; so does slap. Hardware testing confirmed that NDS
firmware does not check this field for booting, so the stale value is
cosmetically wrong but functionally harmless.

slap is not committed to reference-implementation parity in general --
slap deviates from reference behavior in other formats where deviation is
justified (rejecting malformed input that other tools silently accept,
refusing to claim a false identity in EBP metadata). The principle here
is narrower: don't invent corrections the patch didn't ask for. If a
future workflow needs "apply this DPS patch and normalize the NDS
header," that's a separate operation the user invokes deliberately, not a
silent fixup bolted onto apply.

**Alternative rejected**: Post-processing the NDS header in the apply
path. This would mean slap's output silently differs from what the
patch's records describe -- the user asked slap to apply a patch, and
slap did more than that without being asked. Patches with correct headers
would be unaffected, but patches with stale headers would silently
change, and the user would have no way to request the faithful "do what
the records say" behavior. A dedicated header-normalization operation,
invoked deliberately, is the right shape for that workflow -- not an
automatic side effect of apply.

### 5. Source-size validation: verify by default, --no-verify to override

**What slap does**: Compares the source file's actual size against the
header's `dpsOriginalSize` field. Mismatch is `SlapError` by default.
The `--no-verify` flag downgrades this to a warning and proceeds.

**Why**: DPS has no checksums. The original ROM size is the only
integrity check the format provides. `dpspatcher.exe` enforces it:
`n64aps.c`-style `ftell`-vs-header comparison, hard exit on mismatch.
Defaulting to strict matches slap's discipline for CRC checks in BPS,
UPS, and APS -- all of which verify by default and offer `--no-verify`
as an escape hatch.

The `--no-verify` override exists because the check is size-only and
therefore weak: feeding `dpspatcher.exe` a wrong ROM of the right size
produces silent corruption with no diagnostic. A user who knows their
source is correct despite a size mismatch (trimmed ROM, different dump)
should be able to proceed.

**Alternative rejected**: Making source-size checking advisory (warn
only). This would be inconsistent with slap's treatment of checksums
elsewhere and would weaken the only integrity check DPS offers. The
format is already dangerously permissive (no checksums); slap should
not make it more so by default.

## Sources

- **`dpspatcher.exe`**: 27 KB PE32 binary, compiled with MinGW GCC
  3.4.5. The sole reference implementation. Apply-only; the creation
  tool was separate and not distributed. Behavioral analysis via
  disassembly and Wine testing.

- **Field research** (`findings.txt`): Byte-level comparison of slap
  output against `dpspatcher.exe` output for the JUS patch. Established
  the zero-fill, output-size, and header-fixup behaviors empirically.
  85,757-byte diff analysis, NDS header inspection, hardware boot
  testing.

- **UniPatcher wiki**: Published spec reverse-engineered from `dps.c`
  source. Note: the wiki has record modes 0 and 1 swapped relative to
  actual `dpspatcher.exe` behavior.
