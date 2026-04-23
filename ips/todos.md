# IPS — todos

Known gaps between the design in `questions.md` and the code in `src/Slap/IPS/`. Each item is work slap has committed to doing; priority and shape are noted where they're not obvious. Speculative stuff lives in `notebook.md`.

## Slappy polish — committed, not urgent

### 9. `--require-smc-shaped-target-size` flag (draft name)

Optional create-time flag that refuses to emit a truncation marker whose declared target size doesn't satisfy `(size & 0xFFF) == 0x200`, making the emitted patch acceptable to SNESTool's parser. No-op for patches that don't need a marker (target ≥ source). See questions.md truncation entry for the scope caveat (necessary but not sufficient for end-to-end SNESTool success).
