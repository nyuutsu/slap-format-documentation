# BPS — todos

Known gaps between the design in `questions.md` and the code in `src/Slap/BPS/`. Each item is work slap has committed to doing; priority and shape are noted where they're not obvious. Speculative stuff lives in `notebook.md`.

### CRC-32 variant comment in `rusty-slap/src/crc32.rs`

The implementation uses `crc32fast`, which is unambiguously CRC-32/ISO-HDLC — but the file carries no commentary on that fact or on the surrounding design call. Add a comment recording:

1. The variant in use is **CRC-32/ISO-HDLC** (aka "CRC-32" plain, the zlib/PNG/PKZIP/gzip flavor; check value `0xCBF43926` against `"123456789"`). This is the variant every BPS tool in the ecosystem uses; it is what byuu's `beat` reference implementation produces despite the spec saying only "CRC32".
2. A case could be made for trial-fitting other CRC-32 variants when a declared checksum doesn't match, since detection is cheap given source and target on hand. That case is a parallel to the EBP JSON implementation question. It is incorrect here: EBP JSON flavor-support expansion doesn't muddy "is my patch corrupt or isn't it?"; CRC-32 variant-support expansion very much would.

See `docs/bps/questions.md` → "Which CRC-32 variant does BPS actually use?" for the full reasoning.

### Warn on negative-zero signed offset (`0x81`) on parse

`Slap/BPS/Types.hs:77-80` (`decodeSignedVarint`) silently maps both `0x80` and `0x81` to zero. The design (questions.md → "What do we do about the two encodings for zero-delta") says slap should accept `0x81` on parse with a warning. Need to (a) detect the `0x81` case at decode time and (b) surface the warning through a parse-time warnings channel.

Depends on parse gaining a warnings channel — same prerequisite as the IPS-family items that want to warn during parse (unsorted records, overlap, zero-count RLE). See `ips/todos.md` item 4 for that infrastructure work.
