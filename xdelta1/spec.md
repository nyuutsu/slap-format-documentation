# xdelta1 — Format Specification

There is no formal specification for xdelta1. This document is
derived from the canonical source code (`xdelta-1.1.4` by Joshua
MacDonald, LGPL/GPL), specifically `xdmain.c`, `xdelta.h`, and
`libedsio/default.c`. Where this document and the source disagree,
the source wins.

The canonical source is preserved at `upstream/` (the `xdelta-1.1.4`
distribution). slap is, to our knowledge, the only other
implementation.

## Identity

- **Name**: xdelta (version 1.x)
- **Author**: Joshua MacDonald
- **Era**: 1997–2003
- **License**: LGPL (library), GPL (tool)
- **Reference tool**: `xdelta` CLI (subcommands: `delta`, `patch`,
  `info`)
- **Relation to xdelta3**: none. Same author, completely different
  format and algorithm. They share only the name.

## Overall structure

```
[ magic prefix        ]  8 bytes
[ header              ] 24 bytes (6 × uint32 BE)
[ from-name           ] N1 bytes
[ to-name             ] N2 bytes
[ data segment        ]  D bytes (optionally gzip-compressed)
[ control segment     ]  C bytes (optionally gzip-compressed)
[ control offset      ]  4 bytes, uint32 BE
[ trailing magic      ]  8 bytes (must match prefix)
```

Minimum file size: 44 bytes (8 + 24 + 0 + 0 + 0 + 0 + 4 + 8).

The **data segment** contains literal bytes referenced by
instructions. The **control segment** is an EDSIO-serialized
`XdeltaControl` structure describing sources, instructions, and
integrity hashes. The **control offset** is the byte position (from
file start) where the control segment begins; it serves as the
boundary between data and control.

The trailing magic must be byte-identical to the leading prefix.
Mismatch indicates corruption or truncation.

## Magic prefix

| Prefix       | Version | Notes                          |
|--------------|---------|--------------------------------|
| `%XDZ004%`   | 1.1     | current; produced by 1.1.x     |
| `%XDZ003%`   | 1.0.4   | produced by 1.0.4              |
| `%XDZ002%`   | 1.0     | older                          |
| `%XDZ001%`   | 0.20    | older                          |
| `%XDZ000%`   | 0.18    | older                          |
| `%XDELTA%`   | 0.14    | oldest known                   |

Versions 1.0.4 and 1.1 share the same wire format (the control
segment structure is identical). Earlier versions use different
internal layouts and are not described here.

## Header

Six uint32 values, big-endian, starting at byte offset 8:

| Word | Field                | Encoding                               |
|------|----------------------|----------------------------------------|
| 0    | flags                | bitfield (see below)                   |
| 1    | name lengths         | `(from_name_len << 16) \| to_name_len` |
| 2    | reserved             | always 0                               |
| 3    | reserved             | always 0                               |
| 4    | reserved             | always 0                               |
| 5    | reserved             | always 0                               |

Words 2–5 are reserved for future extensions. The canonical tool
writes zeros and never reads them.

### Flags (word 0)

| Bit | Name                    | Meaning                                    |
|-----|-------------------------|--------------------------------------------|
| 0   | `FLAG_NO_VERIFY`        | MD5 fields are absent/garbage; skip verify  |
| 1   | `FLAG_FROM_COMPRESSED`  | from-file was gzipped at delta time         |
| 2   | `FLAG_TO_COMPRESSED`    | to-file was gzipped at delta time           |
| 3   | `FLAG_PATCH_COMPRESSED` | data and control segments are gzip-compressed |

Bits 4–31 are unused and always 0.

When `FLAG_PATCH_COMPRESSED` is set, both the data segment and the
control segment must be gzip-decompressed before interpretation. The
decompressed data segment is a flat byte blob. The decompressed
control segment is an EDSIO-serialized `XdeltaControl`.

When `FLAG_FROM_COMPRESSED` or `FLAG_TO_COMPRESSED` is set, the apply
step must transparently decompress/recompress the corresponding file.
See "Gzip transparency" below.

When `FLAG_NO_VERIFY` is set, the MD5 fields in the control segment
contain zeros or garbage. The apply step must skip MD5 verification.

## From-name and to-name

Immediately after the header (byte offset 32):

- **from-name**: `from_name_len` bytes (length from header word 1,
  high 16 bits)
- **to-name**: `to_name_len` bytes (length from header word 1,
  low 16 bits)

These are basenames (no directory component). The canonical tool
calls `g_basename()` before storing them.

The canonical tool uses these as default arguments: if the user
omits the source file on `patch`, the tool opens `from-name` in the
current directory. If the user omits the output file, the tool
writes to `to-name`. An implementation that always requires explicit
arguments may treat these as informational metadata.

## EDSIO varint encoding

Numeric fields within the control segment use EDSIO variable-length
integer encoding. This is a standard LEB128 variant:

**Encode** (value → bytes):

```
emit low 7 bits of value
if more bits remain: set high bit (0x80), shift value right 7, repeat
if no more bits: clear high bit (0x00), stop
```

**Decode** (bytes → value):

```
for each byte:
  append (byte & 0x7F) to an array
  if (byte & 0x80) == 0: stop
reassemble value from array, little-endian:
  value = arr[n-1]
  for i = n-2 downto 0:
    value = (value << 7) | arr[i]
```

High bit **set** means "more bytes follow." High bit **clear** means
"this is the last byte." Maximum encoded width for a uint32 is 5
bytes.

This is distinct from byuu varints (used by UPS/BPS) which have the
opposite stop-bit polarity and an add-one adjustment.

Source: `libedsio/default.c`, functions `sink_next_uint` (encode)
and `source_next_uint` (decode).

## Control segment (XdeltaControl)

After gzip decompression (if applicable), the control segment
contains:

```
[ type tag             ]  uint32 BE (EDSIO framing, value = ST_XdeltaControl)
[ allocation hint      ]  uint32 BE (EDSIO framing, deserializer bookkeeping)
[ to-md5               ] 16 bytes (output file MD5)
[ to-length            ]  EDSIO varint (output file size in bytes)
[ has-data             ]  1 byte, boolean (redundant; true iff any source has isdata=true)
[ source-count         ]  EDSIO varint
[ source 0             ]  (see XdeltaSourceInfo below)
[ source 1             ]
[ ...                  ]
[ source N-1           ]
[ instruction-count    ]  EDSIO varint
[ instruction 0        ]  (see XdeltaInstruction below)
[ instruction 1        ]
[ ...                  ]
[ instruction M-1      ]
```

The first 8 bytes (type tag + allocation hint) are EDSIO
serialization framing present on every serialized object. They are
not part of the XdeltaControl payload. The type tag identifies the
object type; the allocation hint assisted the C deserializer's
memory allocation and is ignored on read.

The `has-data` byte is redundant — it is true if and only if at
least one source has `isdata = 1`. It exists for the serializer's
convenience and can be ignored by parsers.

## XdeltaSourceInfo

Each source entry:

```
[ name-length          ]  EDSIO varint
[ name                 ]  name-length bytes
[ md5                  ] 16 bytes
[ length               ]  EDSIO varint (source size in bytes)
[ isdata               ]  1 byte, boolean
[ sequential           ]  1 byte, boolean
```

### Fields

- **name**: for file sources, the basename of the original file on
  the creator's disk. For data segments, conventionally the string
  `"(patch data)"` but not enforced by the format.
- **md5**: MD5 hash of the source's content. For file sources, this
  verifies the user's input file. For data segments, this verifies
  the embedded data survived transit.
- **length**: byte length of the source.
- **isdata**: `1` = data segment source (bytes come from the patch's
  embedded data segment). `0` = file source (bytes come from an
  external file the user provides).
- **sequential**: `1` = instructions targeting this source use
  sequential addressing (see below). `0` = absolute addressing.

### Source shapes

The canonical tool accepts exactly these source configurations and
rejects all others:

1. `[]` — zero sources
2. `[data]` — one data segment, no file source
3. `[file]` — one file source, no data segment
4. `[data, file]` — data segment at index 0, file source at index 1

Shape 4 is the normal case. All observed real-world patches use it.

Shapes with `source-count > 2`, or with orderings other than those
listed (`[file, data]`, `[file, file]`, `[data, data]`), are
rejected by the canonical tool's apply path
(`xdmain.c:1741-1768`, error `EC_XdIncompatibleDelta`).

## XdeltaInstruction

Each instruction:

```
[ index                ]  EDSIO varint (zero-based index into source array)
[ offset               ]  EDSIO varint (byte offset within the source)
[ length               ]  EDSIO varint (bytes to copy)
```

The instruction means: copy `length` bytes from `source[index]`
starting at `offset`, appending them to the output.

Instructions are applied in order. Each one produces a contiguous
run of output bytes. The concatenation of all instruction outputs
must equal `to-length` bytes.

### Sequential addressing

When a source has `sequential = 1`, instructions targeting it carry
`offset = 0` on the wire. The actual offset is implicit: a running
cursor per source, starting at 0, advancing by `length` after each
instruction that references that source.

The canonical tool writes sequential offsets at delta time and
reconstructs absolute offsets at apply time. An implementation may
resolve sequential offsets at parse time instead — the effect is the
same.

All observed real-world patches use absolute addressing
(`sequential = 0`) for both sources.

## Integrity

### MD5

The format uses MD5 for integrity checking:

- **to-md5** (in control segment): MD5 of the expected output file.
  Verified after apply.
- **Per-source md5** (in each XdeltaSourceInfo): MD5 of that
  source's content. For file sources, verified before apply. For
  data segments, verified on parse.

The canonical tool verifies MD5 by default. Its `--noverify` flag
disables verification and sets `FLAG_NO_VERIFY` in the header when
creating patches, producing garbage MD5 fields.

There is no CRC32. MD5 is the sole integrity mechanism.

### Trailing magic

The last 8 bytes of the file must match the first 8 bytes (the
magic prefix). This provides a simple corruption/truncation check
independent of MD5.

## Gzip transparency

When `FLAG_FROM_COMPRESSED` is set in the header:

- At delta time: the from-file was detected as gzipped (first two
  bytes `0x1F 0x8B`), decompressed to a temp file, and the delta
  was computed against the decompressed content.
- At apply time: the apply step must gzip-decompress the from-file
  before using it as a source.

When `FLAG_TO_COMPRESSED` is set:

- At delta time: the to-file was detected as gzipped, decompressed,
  and the delta targets the decompressed content.
- At apply time: the apply step must gzip-compress the output before
  writing it to disk.

The `--pristine` flag on the canonical tool disables this
transparency, forcing raw-byte treatment of all inputs regardless
of gzip magic bytes.

Caveat from the canonical README: "the recompressed content does not
always match byte-for-byte with the original compressed content."
The decompressed content matches; the compressed representation may
differ due to zlib version, compression level, or implementation
differences.

## Data segment

The data segment is a flat byte blob occupying the region between
the end of the to-name and the control offset. If
`FLAG_PATCH_COMPRESSED` is set, the blob is gzip-compressed and must
be decompressed before use.

After decompression, the data segment is addressed by instructions
whose source index refers to a source with `isdata = 1`. The
source's `length` field gives the decompressed size.

The data segment contains all novel bytes — content present in the
output but absent from the input. Its size directly reflects how
much new data the patch introduces.

## Unused file source

The canonical tool warns when the delta algorithm finds zero shared
chunks between from-file and to-file. In this case the entire output
is encoded in the data segment and no instruction copies from the
file source. The file source exists in the source array (with its
MD5 and length) but is never referenced by any instruction.

This can occur due to unrelated inputs, incompatible encoding, or
user error. The patch is structurally valid and applies correctly —
but the source file is irrelevant; the patch would produce the same
output with any input (or none).
