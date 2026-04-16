# PPF format family — specification

Distilled from reference source code and format documents. PPF1/2/3
were invented by Icarus of Paradox; PPF4 is an independent format by
Pyriel. PPF4's header layout is field-for-field the same shape as
PPF3's (same offsets, same field widths, same set of fields at
56-59), though it does not use those fields semantically. Each claim
in this document is tagged with its source; inferences are called
out.

**Claim-tag key used in this file:**
- *(doc)* — stated in a Paradox-authored text file (`ppf.txt`,
  `ppf-doc.txt`, `PPF2.txt`, `PPF3.txt`)
- *(code: XXX)* — behavior of the reference source file XXX
- *(inference)* — not directly stated anywhere; derived from
  combining evidence
- *(inference, contested)* — I am extrapolating; a reasonable
  alternative reading exists

## Provenance of reference material

| Version | Source we used |
|--------:|----------------|
| PPF1 spec doc | `pdx-ppf1/ppf.txt` and `pdx-ppf1/ppf-doc.txt` (era-contemporary) |
| PPF1 creator source | `pdx-ppf1/sources/makeppf.c` (era-contemporary) |
| PPF1 applier source | `pdx-ppf1/sources/applyppf.c` (era-contemporary) |
| PPF1 Amiga binaries | `pdx-ppf1/amiga-bin/{MakePPF,ApplyPPF}` (m68k, no source) |
| PPF2 spec doc | `pdx-ppf2/ppftools/ppfdev/PPF2.txt` (era-contemporary) |
| PPF2 source | None released. Only binaries ship. |
| PPF3 spec doc | `ppf-master/ppfdev/PPF3.txt` |
| PPF3 creator source | `ppf-master/ppfdev/makeppf_src/makeppf3_{linux,vc}.c` |
| PPF3 applier source | `ppf-master/ppfdev/applyppf_src/applyppf3_{linux,vc}.c` (also reimplements PPF1 and PPF2 apply) |
| PPF4 creator source | `gs2-bugfixes-master/tools/ppfmaker (custom)/src/ppfmaker.cpp` |
| PPF4 applier source | `gs2-bugfixes-master/patcher.lua` (with inline format comment) |

The linux and vc PPF3-era variants agree on the format. A normalized
diff shows the differences are confined to I/O API (`fseeko64`/`fread`
on FILE* handles vs. `_lseeki64`/`_read` on integer file descriptors)
and the presence of the endian-swap macros (linux version includes
them gated on `__BIG_ENDIAN__`; vc version omits them). The
format-interpretation logic — header layout, record wire format,
termination — is identical between them. *(code, verified by diff)*

---

## PPF1

### Header (56 bytes) *(doc: ppf.txt)*

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 5 | Magic `"PPF10"` |
| 5 | 1 | Encoding method byte: `0x00` |
| 6 | 50 | Description (see Text encoding section) |

After the header, record data begins at offset 56. *(doc, code)*

### Records (repeated until EOF) *(doc: ppf.txt)*

Each record:

| Size | Field |
|-----:|-------|
| 4 | Offset in target file |
| 1 | Count N |
| — | If N != 0: N data bytes (literal mode) |
| — | If N == 0: 2 bytes `(data_byte, repeat_count)` (RLE mode) |

The size-0 RLE case is documented in ppf.txt as "If parameter 'y' is
set to zero then parameter 'z' will be a two byte field. Byte zero
will be the data and byte one will be the number of repetitions."
The reference applier implements this branch; the reference creator
(`makeppf.c`) **never emits RLE records**. *(doc, code)*

### Offset size and endianness

The reference applier uses `long Offset` and `fread(&Offset, sizeof(Offset), ...)`.
The shipped PC binaries in `pdx-ppf1/pc-bin/` are MS-DOS 16-bit
executables compiled with Turbo C++ 1990 (verified from the copyright
string in the binary). On Turbo C++ 1990 and other MS-DOS compilers
of that era, `long` is 32-bit. *(code; binary inspection)*

The era-contemporary code performs **no byte-swapping**. Since
`sizeof(long)` bytes are read directly into a native-endian integer,
a patch produced on a little-endian host (PC) stores offsets
little-endian on disk, and one produced on a big-endian host (Amiga,
m68k) stores them big-endian. The two would therefore be mutually
incompatible when applied cross-platform. The shipped Amiga and PC
binaries are compiled from this same source with no conditional
endian logic. *(code: pdx-ppf1/sources/\*.c + binary inspection of
pdx-ppf1/amiga-bin/)*

The spec documents that ship with pdx-ppf1 (`ppf.txt`, `ppf-doc.txt`)
do not mention endianness at all. *(doc)*

The later PPF3 distribution's `ApplyPPF1Patch` (a reimplementation,
not the original) inserts `Endian32_Swap` on big-endian hosts,
effectively treating PPF1 on-disk as little-endian and byte-swapping
when read on a BE host. Combined with PPF2.txt's explicit LE
declaration, this reads as the ecosystem converging on "PPF1 is
little-endian on disk" after the fact. *(code: applyppf3_*.c;
PPF2.txt declaration)*

**Practical guidance for modern implementers**:
- The LibCrypt corpus of 339 PPF1 patches we inspected is uniformly
  PC-origin (LE on disk). That corpus appears to be collected from
  a single scene-mirror archive, so "all PC-origin" should be read
  as "all of the 339 files we looked at happen to be PC-origin" —
  not a statement about every PPF1 that exists.
- Default to LE. Provide an override flag for BE-origin (Amiga)
  patches. Warn when an override is used.

### File size limits

The ppf-doc.txt says "PPF facilitate patching of iso-files up to 2Gb."
*(doc)* The era-contemporary applier passes the offset as `long` to
`fseek`. On the MS-DOS compilers of that era, `long` is 32-bit
signed, which caps `fseek` positioning at 2^31 − 1 bytes. *(inference
about the compiler and fseek prototype used at the time)* Modern
`fseek` equivalents accept larger values, so a modern reimplementation
wouldn't be limited by era toolchain semantics — but the **format
cannot express offsets beyond 2^32 anyway** because the offset field
is 4 bytes. *(code)*

### What PPF1 does not have

No file size field, no validation block, no undo data, no FILE_ID.DIZ
trailer. *(code, doc)*

### Record stream termination

The reference applier (`applyppf.c`) terminates when `fread` of the
offset field returns 0 bytes (clean EOF). No count-arithmetic. No
explicit terminator byte. *(code: applyppf.c `ReadPatch`)*

The PPF3-era `ApplyPPF1Patch` (a reimplementation in the same file
that handles PPF3) instead loops `while(count != 0)` where `count`
is declared `unsigned int`, starts as `file_size - 56`, and
decrements by `5 + anz` per record. If a patch is truncated such
that `count` would go negative, `unsigned int` arithmetic wraps
around to a large positive value and the loop keeps running.
The era-contemporary EOF-based approach does not have this failure
mode; a modern PPF1 applier should prefer the EOF-based pattern.
*(code: compare applyppf.c and applyppf3_*.c)*

### Size-changing patches

Neither ppf.txt nor ppf-doc.txt contains any prose rule about input
and output being the same size. The only size-related statement
is ppf-doc.txt's upper bound: "PPF facilitate patching of iso-files
up to 2Gb." *(doc)*

The era-contemporary maker (`makeppf.c`) calls `GetImgSize()` before
creating the patch and refuses when original and modified differ in
size. *(code)* This is a maker-side policy, not something the wire
format or the spec mandates.

The era-contemporary applier (`applyppf.c`) performs no bounds
checking on record offsets; it seeks blindly and writes. A patch
whose offset is past the original target's EOF would extend the
file via `fwrite` in binary mode (modern POSIX/Windows semantics).
No integrity, parity, or size checks of any kind are performed.
*(code, inference about platform fwrite)*

**Summary for PPF1**: the format spec is silent on size matching.
Same-size emerges from the maker's policy plus the format's
disc-image context. An implementation may freely choose whether to
replicate the maker's refusal or allow size-changing creation —
neither choice violates the spec.

---

## PPF2

### Header (1084 bytes) *(doc: PPF2.txt)*

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 5 | Magic `"PPF20"` |
| 5 | 1 | Encoding method byte: `0x01` |
| 6 | 50 | Description |
| 56 | 4 | Original binfile size (uint32) |
| 60 | 1024 | Validation block: 1024 bytes from the original target at offset `0x9320` |

Header is always 1084 bytes; validation block is **always present**
(no on/off flag). *(doc)*

### Endianness *(doc: PPF2.txt)*

PPF2.txt explicitly says: *"Be careful! watch the endian format!!!
If you own an Amiga and want to do a PPF2-Patcher for Amiga don't
forget to swap the endian-format of the OFFSET to avoid seek errors!"*

So the on-disk format is declared little-endian. *(doc)*

PPF2's reference tools are DOS-only; no Amiga build exists and no
source was released. *(observation of pdx-ppf2 contents)*

### Records

4-byte LE offset + 1-byte count + count bytes. Records start at
offset 1084. *(doc: PPF2.txt format section)*

**No RLE mode.** PPF2.txt's record format section describes only the
literal mode and gives a single example. It does not mention the
size-zero RLE clause that ppf.txt (PPF1) documents. The PPF3-era
applier's `ApplyPPF2Patch` reads `anz` bytes unconditionally and
never branches on `anz==0`, consistent with PPF2 having no RLE.
No PPF2 patch in our corpus contains a size-zero record. *(doc,
code: applyppf3_*.c, corpus: 41 PPF2 files)*

### FILE_ID.DIZ trailer *(doc: PPF2.txt)*

Optional trailer, appended after all records:

| Size | Field |
|-----:|-------|
| 18 | Literal `"@BEGIN_FILE_ID.DIZ"` |
| L | Content, `L ≤ 3072` |
| 16 | Literal `"@END_FILE_ID.DIZ"` |
| 4 | L as uint32 LE |

PPF2.txt explicitly says "an Integer (4 byte long) with the length."
*(doc)*

Detection: reference applier seeks to `EOF − (lenidx + 4)` where
`lenidx` is 4 for PPF2 and 2 for PPF3, reads 4 bytes, and checks for
`".DIZ"` (matching the last 4 bytes of `@END_FILE_ID.DIZ`). If the
check passes, it then reads `lenidx` bytes at `EOF − lenidx` as the
DIZ body length. *(code: applyppf3_\*.c `ShowFileId`)*

### Size-changing patches

PPF2.txt does not contain prose rules about size matching. The
header has a 4-byte "Size of the file" field at offset 56, which
PPF2.txt describes as "Used for Identification." *(doc)* The
user-facing readme.txt expands: filesize checking gives "a 100%
secure way to find out if the binfile the user gives as input later
while applying is REALLY THE SAME ONE AS THE ONE THE PPF patch was
made of." *(doc: pdx-ppf2 readme.txt)*

So the filesize field's purpose is **input identification**, not an
assertion that output will be the same size. The spec doc makes no
statement about the output size.

The user-facing `MakePPF.txt` instructs the creator (as a user-level
instruction, not enforced by the tool): "Please be sure that the
filesize of the Original and Patched iso files are identical!"
*(doc: pdx-ppf2 MakePPF.txt)* We have no source for the PPF2 maker
so we can't tell whether the binary enforces this the way the PPF1
maker did.

The PPF3-era `ApplyPPF2Patch` checks `obinlen != binlen` and prompts
"The size of the bin file isn't correct, continue ? (y/n)" —
**advisory**, proceeds on user confirmation. The same source's
user-facing `ApplyPPF.txt` explicitly says this warning "is not to
be taken toooo seriously" because "Different CDRWin versions may
add some more bytes to the binfile." *(code: applyppf3_\*.c; doc:
pdx-ppf2 ApplyPPF.txt)*

**Summary for PPF2**: the stored filesize is for input identification
only. The spec doesn't require output size to match input size;
user docs ask creators to produce same-size patches; applier-side
mismatch is an advisory warning. No level mandates same-size.

---

## PPF3

Fully documented in PPF3.txt. This is the only version of PPF that
has a formal written specification the author released.

### Header *(doc: PPF3.txt)*

60 bytes without validation block, 1084 bytes with.

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 5 | Magic `"PPF30"` |
| 5 | 1 | Encoding method byte: `0x02` |
| 6 | 50 | Description |
| 56 | 1 | Imagetype: `0x00`=BIN, `0x01`=GI (PrimoDVD) |
| 57 | 1 | Blockcheck: `0x00`=disabled, `0x01`=enabled |
| 58 | 1 | Undo: `0x00`=absent, `0x01`=present |
| 59 | 1 | Dummy (spec says "Not used", creator writes 0) |
| 60 | 1024 | Validation block — present iff blockcheck == 0x01 |

Validation block is copied from the original target at:
- `0x9320` if imagetype == BIN *(doc)*
- `0x80A0` if imagetype == GI *(doc)*

### Records *(doc: PPF3.txt)*

| Size | Field |
|-----:|-------|
| 8 | Offset (int64 LE) |
| 1 | Count N |
| N | Replacement bytes |
| N (iff undo) | Undo bytes (original bytes at that offset) |

PPF3.txt says "Endian format is Intel!" — explicit LE. *(doc)*

Undo data, when present, is stored inline after each record's
replacement data. Enabling undo at creation time roughly doubles
the record-stream portion of the patch. *(doc, code)*

Max file size: spec says up to 2^63 − 1 bytes. *(doc)*

### FILE_ID.DIZ trailer *(doc: PPF3.txt)*

Same shape as PPF2 but with **2-byte length** (uint16 LE), not 4.
*(doc: "unsigned short (2 byte) with the length")*

### Size-changing patches

PPF3.txt does not contain prose rules about size matching. The
header has no file size field at all — the `obinlen` field that
existed at offset 56 in PPF2 was removed.

The ppf-master readme.txt explicitly states why: *"Image filesize
checking is GONE. Over the years it became clear that this feature
was simply too inaccurate and confused most of the users with
warnings which were mostly incorrect."* *(doc: ppf-master readme.txt)*

So PPF3 **dropped the only size-related metadata the format had
ever carried**. A PPF3 patch carries no statement about input size
or output size. The validation block's 1024 bytes identify a
specific disc image, but say nothing about size.

The reference creator has code that *looks* like a size-equality
check — `fl1=ftello64(bin); fl2=ftello64(bin);` followed by
`if(fl1 != fl2) printf("Error: bin files are different in size.")` —
but both `ftello64` calls read from the same file handle (`bin`),
so the comparison is effectively `fl1 == fl1`, which always passes.
The code appears to be a size-enforcement check that doesn't
actually enforce. *(code: makeppf3_linux.c lines ~498)*

The format itself has no mechanism to record or describe a size
change. Records are offset-and-data replacements only; there's no
"append" or "truncate" directive. A patch that references offsets
past the original's EOF would cause the applier to seek past end
and write, which in binary mode would extend the file. This holds
on POSIX and on Windows as those platforms' standard C library and
kernel behave today; strictly this is a platform-behavior inference,
not a format rule. *(inference from `fwrite` semantics on current
platforms)*

**Summary for PPF3**: the format is aggressively silent on size.
PPF2's removed filesize field signals a deliberate move away from
even identifying size, not an assumption that size is always equal.
The reference maker's shape-of-a-check hints at same-size intent
on the create side but enforces nothing at the implementation level.

---

## PPF4

A separate format by Pyriel (GS2 bugfixes project). Shares the PPF3
header layout but the record stream is different and is not
backward-compatible.

### Context *(code: ppfmaker.cpp header comment)*

> PPF was chosen mostly because it was easy to implement later in
> Lua. The patch header contains PPF40 which is not used by other
> tools, and will hopefully prevent individual files from being seen
> as patches that tools like PPFomatic could apply.

### Header (60 bytes) *(code: ppfmaker.cpp PatchHeader; patcher.lua)*

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 5 | Magic `"PPF40"` |
| 5 | 1 | Encoding byte: `0xFF` |
| 6 | 50 | Description (zero-padded, not space-padded — see Text encoding) |
| 56 | 1 | Image type — always 0 in practice |
| 57 | 1 | Validation flag — always 0 (format does not support) |
| 58 | 1 | Undo flag — always 0 (format does not support) |
| 59 | 1 | Expansion — always 0 |

The reference applier reads bytes 56-59 as a single uint32 and
rejects the patch if the value is nonzero (`ERROR_BAD_FLAGS`).
Because the check is a zero-equality, the endianness of that u32
read doesn't matter — any four zero bytes pass. *(code: patcher.lua)*

### Records (repeated until EOF) *(code + inline doc comment in patcher.lua)*

| Size | Field |
|-----:|-------|
| 1 | Command: `0`=REPLACE, `1`=ADD |
| 4 | Offset (uint32) — 0 and ignored for ADD |
| 1 | Count N (max 255) |
| N | Data bytes |

On-disk byte order for the offset is not declared anywhere in either
ppfmaker.cpp or patcher.lua. The writer emits bytes via
`fwrite(&offset, sizeof(u32), ...)`, which serializes host-native
byte order. The Lua reader calls `readU32` whose endianness depends
on the binding implementation. Inferring x86-to-x86 usage (a fair
inference given the project's Windows build and the Lua 5.x era,
but not formally stated anywhere), the effective byte order is LE.
A strictly-correct reading of the format is "4 bytes, read back the
same integer" — endianness is a property of the creator/applier
pair, not of the format itself. *(code: ppfmaker.cpp; patcher.lua;
inference about intended platform)*

### Ordering constraint *(code: patcher.lua)*

REPLACE records must precede all ADD records. Once the first ADD is
seen, the applier enters append mode and cannot accept further
REPLACEs. A REPLACE after an ADD produces `ERROR_BAD_REPLACE`.

### Offset width *(code: ppfmaker.cpp comment)*

PPF4 uses 32-bit offsets (PPF3 uses 64-bit). Per the ppfmaker.cpp
comment, this was because "the Lua patcher [has] no native support
for 64-bit types." So PPF4 inherits a 4GB ceiling even though the
header superficially matches PPF3.

### What PPF4 does not have

No per-record undo data, no validation block, no FILE_ID.DIZ trailer.

### Size-changing patches

PPF4 has no prose spec document. The `ppfmaker.cpp` header comment
says the format "adds support for adding data to the end of files.
The latter is necessary for translations" — making growth an
explicit design goal. *(code: ppfmaker.cpp header comment)*

The Lua applier's REPLACE branch checks `if(TargetFileEnd < offset)`
and returns `ERROR_BAD_OFFSET` if the offset is past the original
file's end. REPLACE is therefore bounded to the original target's
byte range. *(code: patcher.lua)*

ADD, by contrast, seeks to `TargetFileEnd` and appends. It does not
take an offset into the original file — the "offset" field is
written as 0 by the maker and ignored by the applier. ADD grows
the target. *(code)*

**Summary for PPF4**: growth via ADD is a first-class feature of
the format. REPLACE is bounded to the original target. Shrinking
is not expressible — no command exists for truncation or for
declaring a smaller output size. Thus PPF4 is the only PPF variant
that supports growth, and it supports growth only via append, not
via arbitrary resize.

---

## Text encoding (all versions)

The description field is a **50-byte buffer of opaque bytes.** No
version of PPF declares what encoding those bytes are in.

### What the reference tools do

At creation, the reference tools copy bytes directly from argv into
the field: `description[i] = Arg.description[i]` in makeppf3, same
pattern in makeppf.c. *(code)* No transformation, no encoding
declared. Whatever bytes the shell passed in end up in the patch.

At application, reference tools read the 50 bytes and `printf` them
to stdout: `printf("Description : %s\n", desc);` *(code)* No
interpretation, no validation. The terminal's codepage determines
what the user sees.

### Practical consequence

The description's encoding is whatever the creator's shell/locale
produced at patch-creation time. That information is not recorded
in the patch. Likely encodings the bytes might be in, given historic
contexts: a 1999 Windows DOS environment produced CP437 or CP1252;
a Japanese-locale shell produced Shift-JIS; a modern Linux/macOS
shell produces UTF-8. These are guesses about common cases, not
statements about any specific patch's content. *(characterization,
not verified per-patch)*

### Padding differs between PPF1-3 and PPF4

- PPF1, PPF2, PPF3: space-padded (`memset(description, 0x20, 50)`)
  *(code)*
- PPF4: zero-padded (`memset(description, 0, 50)`) *(code)*

Readers trimming trailing whitespace from the displayed description
should trim both `' '` and `'\0'`.

### Reference tool behavior on non-ASCII input

- **Creation (PPF3 maker)**: copies bytes from argv into a 128-byte
  local buffer, then writes the first 50 of those bytes to the
  patch. Descriptions shorter than 50 bytes are space-padded;
  descriptions from 50 up to ~128 bytes are silently truncated at
  50; descriptions longer than ~128 bytes overflow the buffer.
  Multi-byte encodings may be cut mid-codepoint. *(code:
  makeppf3_linux.c `PPFCreateHeader`)*
- **Creation (PPF1 maker)**: a different pattern. `WriteStdHeader`
  declares a local 50-byte buffer, `memset`s it to spaces, then
  does `strcpy(Description, Desc)` followed by
  `Description[strlen(Desc)] = ' '`. For descriptions under 50
  bytes this works. For longer descriptions `strcpy` writes past
  the 50-byte buffer (stack overflow). The caller in `main` also
  stores argv in an 81-byte local buffer via `strcpy`, so long
  descriptions corrupt the stack before even reaching
  `WriteStdHeader`. In practice no sensible user hits this, but
  the overflow is genuinely there. *(code: pdx-ppf1 makeppf.c)*
- **Application**: does not choke. Reads 50 bytes and prints.
  Mismatch between creator locale and viewer terminal produces
  mojibake but does not affect the patch data. *(code)*

A modern implementation should clip to the last fitting codepoint
rather than matching either era tool's truncation or overflow
behavior.

### Implementation guidance

The description is an opaque 50-byte buffer; treat it as such at the
wire level. When encoding user-provided text into it, pad to exactly
50 bytes and truncate at an encoding-safe boundary where possible.
When decoding for display, apply lenient decoding appropriate to the
application's context; note that there is no correct answer.

---

## Summary table

| | PPF1 | PPF2 | PPF3 | PPF4 |
|---|---|---|---|---|
| Magic | `PPF10` | `PPF20` | `PPF30` | `PPF40` |
| Encoding byte | `0x00` | `0x01` | `0x02` | `0xFF` |
| Spec document exists | yes (ppf.txt) | yes (PPF2.txt) | yes (PPF3.txt) | no (only inline source comment) |
| Creator source released | yes | no | yes | yes |
| Header size | 56 | 1084 | 60 or 1084 | 60 |
| Offset width on disk | 32-bit | 32-bit | 64-bit | 32-bit |
| Max effective file size | ≤ 2 GB per ppf-doc.txt and era `fseek` limit | not declared in PPF2.txt; format fields are u32 so ≤ 4 GB-1 by inference | 2^63 − 1 per PPF3.txt | not declared; u32 offsets imply ≤ 4 GB-1 by inference |
| Endianness stance | undeclared; platform-native in pdx-ppf1 code; retroactively LE | LE, explicit | LE, explicit | LE |
| RLE record mode | yes (in doc) | doc silent | no | no |
| File size field | — | u32 at 56 | — | — |
| Validation block | — | always | optional | — |
| Undo data | — | — | per-record optional | — |
| FILE_ID.DIZ trailer length | — | u32 (4-byte) | u16 (2-byte) | — |
| ADD command (growth) | — | — | — | yes, tail-only |

### Size-matching: cross-version summary

The whole picture in one place, because the facts differ non-
trivially across versions and the short story is easy to misread:

| | PPF1 | PPF2 | PPF3 | PPF4 |
|---|---|---|---|---|
| Spec doc mandates same-size? | no | no | no | N/A |
| Any size metadata in the patch? | no | u32 "Size of the file" at offset 56, labeled "Used for Identification" | no — the PPF2 field was removed as "too inaccurate" | no |
| Maker-side enforcement | yes: pdx-ppf1 refuses mismatched inputs | unknown (no source), user docs tell the creator to use same-size | shape of a check exists in the source but is buggy and passes always | N/A — maker produces growth patches intentionally |
| Apply-side check | none (no bounds checking at all) | advisory: prompts y/n if target ≠ declared size | none | REPLACE is bounded to original-target size; ADD appends |
| Format-level growth | not expressible in prose spec, technically possible in wire format | same | same | supported via ADD command |
| Format-level shrink | same as growth | same | same | **not expressible** (no truncate command) |

The short version: **no PPF version's spec mandates same-size. The
property emerges from tool policy, disc-image use case, or (for
PPF4) is explicitly rejected for growth.** A PPF2 patch carries a
size field but that field identifies the input, not a rule about
the output.

---

## Edge cases / quirks worth surfacing

### PPF1 endianness has no documented rule

Era-contemporary PPF1 code is not endian-aware. The shipped Amiga
binary and shipped PC binary are compiled from the same source with
no conditional endian logic, so they would produce mutually-
incompatible patches when run on their respective architectures.
*(code)*

Our corpus (339 PPF1s, PC/LE origin) contains no Amiga-origin
examples. However: the Amiga PPF1 tool ships in Icarus's own
distribution, and the tool remains discoverable online on period-
correct PSX-scene mirror sites as of 2026. The combined evidence
— the tool exists, was distributed for use, remains available —
makes the existence of BE Amiga-origin patches somewhere in the
world highly likely, even if we haven't located specimens. We
treat this as sufficient justification to implement BE support as
a feature, not as a hypothetical.

For a modern implementation: default to LE; apply-time warning
that this is an assumption and that `--byte-swap-offsets` (or
equivalent) exists as an override for the Amiga case.

### PPF2 endianness

PPF2.txt explicitly declares LE on disk and tells Amiga implementers
to byte-swap. The shipped reference tools are DOS-only (no Amiga
build). No BE PPF2 patches exist in our corpus. We default to LE and
do not attempt auto-detection; a hypothetical BE PPF2 would be
indistinguishable from a corrupt LE PPF2 without knowing its
provenance.

### PPF1 RLE disambiguation

Unambiguous at the wire level: read size byte, branch on zero. A
compliant applier must handle both branches. *(doc)*

### PPF3 blockcheck offsets are disc-oriented

`0x9320` (BIN) and `0x80A0` (GI) are offsets into CD-ROM-sector-sized
structures. For a target smaller than those offsets + 1024, the
creator will read short (and emit garbage pad bytes) and the applier
will read short (and fail validation). The format does not forbid
setting `blockcheck=0x01` on a small target; it just becomes
meaningless. Implementers may wish to warn.

### PPF4 reserved bytes

The applier strict-checks that bytes 56-59 are all zero. The Lua
spec comment labels these fields image_type / validation_flag /
undo_flag / expansion and says each is "always 0, not supported."
An implementation should validate == 0 and reject otherwise.

### Empty patch

A PPF file that is exactly the header size (56/1084/60/60 bytes) is
structurally valid but describes no changes. The reference appliers
accept this (EOF immediately on the first record-read attempt).

### FILE_ID.DIZ marker consistency

The reference applier only checks for `@END_FILE_ID.DIZ` (and only
the tail `.DIZ` of it) before the length field. A patch with a
correct end-marker and length but a missing `@BEGIN_FILE_ID.DIZ`
would technically apply correctly under the reference. In-corpus
PPF2/PPF3 patches all have both markers. *(code, corpus observation)*

### Out-of-order and overlapping records

Neither the spec docs nor the reference code constrains record order
or forbids overlap. In-corpus findings:
- 4 PPF3 patches (out of 440) have non-monotonic offsets *(corpus)*
- Zero patches have overlapping records *(corpus: all versions)*

The reference applier processes records sequentially in file order,
so "later record wins" for overlapping bytes. This is implementation
behavior; the format does not mandate it.

**Interplay**: because order and overlap are both unconstrained,
record order is semantically meaningful — for overlapping regions,
the later record in file order wins. Any reordering changes the
patch's meaning. This means file order is the correct order not
just for application but also for display, explanation, inspection,
and round-trip serialization. Presenting records sorted by offset
in a human-readable view would misrepresent what the patch does
whenever overlaps are present.

An implementation must preserve file order through parse → (any
intermediate representation) → apply, and through parse → display.
It may additionally detect and reject overlaps up-front as a
stricter policy; this is safe because the reference behavior for
overlap isn't meaningfully useful. But it must not normalize order
as a supposed optimization.

### Zero-count records

For PPF2, PPF3, and PPF4, a record with count N=0 (meaning "write
0 bytes at this offset") is structurally valid: nothing in the
format forbids it, and the reference appliers would process the
header (offset + size) and then read 0 data bytes before advancing
to the next record. Zero-count records appear in zero patches of
our corpus across all versions. *(code, corpus: 339 PPF1 / 41 PPF2 /
440 PPF3 / 131 PPF4)*

For PPF1 specifically, size=0 is not "write 0 bytes" — it's the
RLE sentinel per ppf.txt. The format has no way to express a
"write 0 bytes" record.

### PPF4 is not backward-compatible

A PPF4 patch cannot be applied by PPF-O-Matic or any pre-Pyriel
tool. The `0xFF` encoding byte was chosen specifically to make
incompatible tools fail loudly instead of silently misinterpreting.
*(code: ppfmaker.cpp header comment)*
