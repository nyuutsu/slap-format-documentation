# APS-GBA — notebook

Ideas and maybes. Not committed work; graduate to a `todos.md` file once shape is clear.

### `createAPSGBA` size-guard (4 GB headers)

`createAPSGBA` encodes source and target sizes as `Word32` via `putWord32LE (fromIntegral (ByteString.length …) :: Word32)`. A source or target over 2^32 − 1 bytes would silently truncate to a mis-sized header, producing a malformed patch. Block offsets (`blockIndex * apsGbaBlockSize`) are also `Word32`-encoded, so the same concern propagates — though it's a subset of the source/target size guard (a file small enough to fit in 4 GB also has offsets that fit in `Word32`).

Shape of the fix would be the same as `FileExceedsAddressableRange` in BPS, but with a format-specific cap of `2^32 − 1` rather than `maxBound :: Int`. Currently not done because APS-GBA is a GBA ROM format and 32 MB is the real-world cap; the 4 GB corner doesn't fire in practice.
