# EBP corpus findings

Source: romhacking.net archive, 2024-08-01. 9 archives, 10 `.ebp` files. All EarthBound/MOTHER SNES hacks.

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

## Open concerns

- **Round-tripping.** If slap reads EBP metadata containing `\n` and writes it back out (e.g. `slap create --format ebp`), does the JSON come out identical? The reader interprets escapes (after the `Slap.JSON` fix — see README.md); does the writer re-escape them?
- **Malformed JSON.** If the trailing bytes after `EOF` aren't valid JSON — or are JSON that exceeds our flat-string-only scope — does slap produce a useful error or silently return empty metadata? Currently the parser returns `[]` for unparseable input. Should this warn?
- **Non-UTF-8 detection.** The first bytes of a JSON blob reveal the encoding per RFC 4627 §3 (BOM or null-byte pattern). slap currently `decodeUtf8Lenient`s everything and could emit mojibake for a non-UTF-8 payload. Could detect and error specifically instead.
