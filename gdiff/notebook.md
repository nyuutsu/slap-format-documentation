# GDIFF — notebook

Ideas and maybes. Not committed work; graduate to a `todos.md` file once shape is clear.

### `encodeCopy` silent fallthrough on oversized copy lengths

`encodeCopy :: Int64 -> Int64 -> Builder` cascades through variants 249..255 by trying smaller-width opcodes first. Every branch uses `putWord32BE (fromIntegral copyLength)` to write the length field, including the "COPY long,int" (255) fallthrough. If `copyLength > 2^32 − 1` (possible for an unchanged region spanning more than 4 GB inside an 8 GB+ file), the length truncates silently via `fromIntegral` and the patch becomes malformed.

Fix shape: either split oversized copy runs the way `splitData` already splits oversized DATA commands, or widen the length field on the 255 variant to `Int64` (spec does label it as "long,int" — W3C says the length is `int` = signed 32-bit, so splitting is the correct answer).

Same concern for DATA: `encodeData` splits via `splitData` at `maxBound :: Int32`, which is correct — the asymmetry is that COPY never learned the same trick.

Not hit in practice for ROMs; files of that size aren't typical GDIFF inputs.
