# EBP

IPS with a JSON metadata payload. Created by the EarthBound hacking
community.

## Status in slap

- **Parse**: reads IPS records + JSON metadata.
- **Apply**: standard IPS apply.
- **Create**: supported. `--title`, `--author`, `--description` flags.
- **Info**: shows JSON metadata fields.

## Known issues

### JSON escape handling (Slap.JSON)

The JSON decoder in `Slap.JSON.scanQuoted` handles `\"` and `\\`
but uses a catch-all for other backslash sequences that strips the
backslash without interpreting the escape:

- `\n` becomes literal `n` (should be newline)
- `\t` becomes literal `t` (should be tab)
- `\uXXXX` becomes `uXXXX` (should be Unicode codepoint)

In practice this affects Mother2Deluxe's description which contains
`\n`. The bug is cosmetic (metadata display) not structural (apply
is unaffected).

Fix: add named cases for `\n`, `\t`, `\r`, `\b`, `\f`, `\/` before
the catch-all. Add `\uXXXX` handler for four-hex-digit Unicode
escapes. Keep UTF-8-only assumption and flat-object scope as
deliberate limitations.

## Files

- `spec.md` — format specification
