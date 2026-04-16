# EBP format findings

Source: romhacking.net archive (2024-08-01). 9 archives, 10 `.ebp` files. All EarthBound/MOTHER SNES hacks.

## Format structure

```
PATCH [IPS records] EOF [raw JSON]
```

- No length prefix before JSON
- No tail marker (no `EBP1` or similar)
- JSON starts immediately after the 3-byte `EOF` IPS terminator

## JSON metadata

All 10 files have a JSON object with exactly 4 keys: `title`, `author`, `description`, `patcher`.

- `patcher` is always `"EBPatcher"` — tool signature, not user-supplied
- 7/10 have populated title/author/description
- 3/10 have all empty strings (no nulls, no missing keys)
- Encoding: UTF-8, no BOM

## Files

| ID | File | Metadata |
|----|------|----------|
| 1876 | HallowsEnd.ebp | populated |
| 1879 | HolidayHex.ebp | populated |
| 2518 | Mother2Deluxe_2.0.ebp | populated |
| 2644 | debugMenu.ebp | populated |
| 4257 | Consistency.ebp | empty strings |
| 5627 | Speedster.ebp | populated |
| 8172 | Mother_Rebound.ebp | populated |
| 8368 | New Debug Menu.ebp | populated |
| 8670 | RHS (Consistent).ebp | empty strings |
| 8670 | RHS.ebp | empty strings |

## slap compatibility

All 10 files parse, `info`, and `explain` correctly via `slap`. Metadata extraction works for both populated and empty-string cases. slap's partial JSON decoder (`Slap.JSON`) handles 100% of observed real-world EBP patches. The format's spec technically permits nested objects, non-UTF-8 Unicode, and novel fields, but none exist in the wild.

## Desired changes to `Slap.JSON`

The JSON string escape decoder in `scanQuoted` (within `takeQuoted`) currently handles `\"` and `\\` correctly but uses a catch-all for everything else: `'\\' : char : rest` strips the backslash and keeps the literal character. This means:

- `\n` becomes literal `n`, not a newline. Mother2Deluxe has `\n` in its description.
- `\t`, `\r`, `\b`, `\f` same problem.
- `\uXXXX` becomes `uXXXX` — the JSON-standard mechanism for encoding control characters is silently mangled.
- `\/` (legal, optional) becomes literal `/` — happens to be correct by accident.

**What to add:**
1. Named cases for the short escape forms (`\n` -> newline, `\t` -> tab, `\r` -> CR, `\b` -> BS, `\f` -> FF, `\/` -> solidus), placed between the `\\` case and the catch-all.
2. A `\uXXXX` handler that reads four hex digits and produces the corresponding `Char`. Needs a helper to parse four hex digit characters into a codepoint. The helper should have a descriptive name and properly typed parameters per slap's style conventions (no single-letter params, consider newtypes where they'd clarify intent).
3. The catch-all stays as a fallback for genuinely unrecognized escapes.

**What NOT to change:** the overall structure, the UTF-8-only assumption, the flat-object-only scope. Those limitations are deliberate and documented.

**Additional concerns to investigate (not for this change):**
- Round-tripping: if slap reads EBP metadata and writes it back out (e.g. `slap create --format ebp`), does the JSON come out identical? The reader now interprets escapes but does the writer re-escape them?
- Malformed JSON: if the trailing bytes after `EOF` aren't valid JSON (or are JSON that exceeds our flat-string-only scope), does slap produce a useful error or silently return empty metadata? Currently the parser returns `[]` for unparseable input — should this warn?
- UTF-16/32 detection: the first bytes of a JSON blob reveal the encoding (BOM or null-byte pattern per RFC 4627 section 3). slap could detect non-UTF-8 and emit a specific error rather than producing mojibake via `decodeUtf8Lenient`.
