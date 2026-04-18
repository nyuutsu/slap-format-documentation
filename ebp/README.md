# EBP

EBP is an IPS variant — `PATCH` records then `EOF`, followed by a JSON metadata blob. See `../ips/` for the format proper and slap's handling of it. This folder collects EBP-specific findings that don't fit in the IPS docs.

## JSON escape handling bug in `Slap.JSON`

`Slap.JSON.scanQuoted` handles `\"` and `\\` correctly but uses a catch-all for everything else that strips the backslash without interpreting the escape:

- `\n` becomes literal `n` (should be newline).
- `\t` becomes literal `t` (should be tab).
- `\r`, `\b`, `\f` have the same shape.
- `\uXXXX` becomes `uXXXX` (should be the corresponding codepoint).
- `\/` becomes `/`, which is accidentally correct.

In practice this affects `Mother2Deluxe_2.0.ebp`'s description, which contains `\n`. The bug is cosmetic (metadata display) — apply is unaffected.

Fix: named cases for `\n`, `\t`, `\r`, `\b`, `\f`, `\/` between the `\\` case and the catch-all; a `\uXXXX` handler that reads four hex digits. Keep the UTF-8-only assumption and flat-object scope as deliberate limitations. The catch-all stays as a fallback for genuinely unrecognized escapes.
