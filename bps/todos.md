# BPS — todos

Known gaps between the design in `questions.md` and the code in `src/Slap/BPS/`. Each item is work slap has committed to doing; priority and shape are noted where they're not obvious. Speculative stuff lives in `notebook.md`.

### Warn on `0x81` negative-zero signed varint on parse

`Slap/BPS/Types.hs` `decodeSignedVarint` silently maps both `0x80` and `0x81`
to zero. questions.md → "What do we do about the two encodings for zero-delta"
says accept `0x81` with a warning. Detect the `0x81` case at decode time and
emit a `NegativeZeroInBPS` `SlapWarning` through the `Parsed BPSPatch` channel.
