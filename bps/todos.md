# BPS — todos

Known gaps between the design in `questions.md` and the code in `src/Slap/BPS/`. Each item is work slap has committed to doing; priority and shape are noted where they're not obvious. Speculative stuff lives in `notebook.md`.

### Warn on negative-zero signed offset (`0x81`) on parse

`Slap/BPS/Types.hs:77-80` (`decodeSignedVarint`) silently maps both `0x80` and `0x81` to zero. The design (questions.md → "What do we do about the two encodings for zero-delta") says slap should accept `0x81` on parse with a warning. Need to (a) detect the `0x81` case at decode time and (b) surface the warning through a parse-time warnings channel.

Depends on parse gaining a warnings channel — same prerequisite as the IPS-family items that want to warn during parse (unsorted records, overlap, zero-count RLE). See `ips/todos.md` item 4 for that infrastructure work.
