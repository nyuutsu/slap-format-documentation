# EBP — Format Specification

EBP is IPS with a JSON metadata payload appended after the IPS EOF
marker. It was created by and for the EarthBound hacking community
(the "EBPatcher" tool). There is no formal specification; this
document is derived from observed patches and the EBPatcher tool's
behavior.

## Identity

- **Name**: EBP (EarthBound Patch)
- **Author**: EarthBound hacking community
- **Tool**: EBPatcher
- **Era**: 2010s
- **Relation to IPS**: strict superset. Every EBP file is a valid
  IPS file (the JSON payload is invisible to IPS parsers, which stop
  at `EOF`).

## Overall structure

```
[ "PATCH"              ]  5 bytes, ASCII (standard IPS magic)
[ IPS record 0         ]
[ IPS record 1         ]
[ ...                  ]
[ IPS record N-1       ]
[ "EOF"                ]  3 bytes, ASCII (standard IPS terminator)
[ JSON metadata        ]  variable length, raw UTF-8
```

No length prefix before the JSON. No tail marker after it. The JSON
starts immediately after the 3-byte `EOF` and runs to the end of
the file.

## IPS records

Standard IPS record format. See `docs/ips/spec.md`.

## JSON metadata

A single JSON object. All observed instances use exactly four keys:

| Key           | Type   | Content                        |
|---------------|--------|--------------------------------|
| `title`       | string | patch title (may be empty `""`) |
| `author`      | string | patch author (may be empty)     |
| `description` | string | free-text description (may be empty) |
| `patcher`     | string | tool signature, always `"EBPatcher"` |

All values are strings. No nesting, no arrays, no nulls observed.
Key order varies across patches (JSON objects are unordered).

The `patcher` field appears to be a tool signature rather than
user-supplied metadata — always `"EBPatcher"` in observed data.

## Encoding

UTF-8, no BOM. No exotic characters observed in the corpus (10
files). The JSON is not escaped beyond standard JSON string escaping.

## Integrity

None. EBP inherits IPS's lack of checksums. There is no CRC, MD5,
or hash of any kind.

## Corpus observations (romhacking.net 2024-08-01)

- 9 archives, 10 `.ebp` files
- All SNES EarthBound/MOTHER hacks
- 7/10 have populated title/author/description; 3/10 have all empty strings
- `patcher` is always `"EBPatcher"` (10/10)
