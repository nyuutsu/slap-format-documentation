# NINJA2 — notebook

Ideas and maybes. Not committed work; graduate to `todos.md` once shape is clear.

### `encodeFixedHeader` and `truncateField`: fold encoding and truncation

`createNINJA2` encodes each `Maybe String` field to `Maybe ByteString` using
`encodeNINJA2String encoding`, then passes the resulting `NINJA2Info` to
`encodeFixedHeader encoding info`, which calls `truncateField encoding` on each
field when the bytes exceed the field width. The `PatchEncoding` is consumed
in two places when it could be consumed in one: the encoding step and the
truncation step are the same decision.

Shape of the fix (not sure this is the right one, just the obvious one): a
single `encodeAndTruncateField :: PatchEncoding -> Int -> String -> ByteString`
that encodes a raw `String` under the given encoding with truncation applied
inline. `encodeFixedHeader` would take `NINJA2Metadata` directly (raw strings,
encoding, widths) instead of taking a pre-encoded `NINJA2Info`, and the
create-path would stop building an intermediate `NINJA2Info` just to pass it
through the header encoder.

`ninja2TruncationNotes` becomes interesting under this shape: today it
inspects pre-encoded byte lengths, which isn't quite the right question for
UTF-8 (a field that fits as codepoints may be reported as non-truncated even
when the codepoint-boundary truncation dropped a trailing character). Moving
to string-in-bytes-out means the truncation check can happen at the same
moment the truncation decision is made, with the actual pre-truncation length
in scope.

Keeping `NINJA2Info` unchanged — it remains the correct parse-side shape and
nothing on that side wants to move. This is a create-path-only refactor.

Also: `encodeFixedHeader` is currently in the module export list with no
external callers. If this notebook item graduates to a todo, dropping the
export is table-stakes; if not, the export should probably come out anyway
as a smaller independent cleanup.
