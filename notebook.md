# slap — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### Text-ify some things or even everything

CreateMeta and the metadata records across the program use String for user-intent text fields; eventual migration to Text is a plausile program-wide refactor to consider.

### `applyIPS` and `applyDPS`: `unsafePerformIO + IORef` around `create`

Both functions populate a preallocated output buffer via `Data.ByteString.Internal.create`, which forces the record-walk into `IO`. The walk can fail mid-way (a record writes past target, a source read out-of-bounds), and the error has to escape the `IO` block. Current pattern: allocate an `IORef (Maybe ApplyError)`, write to it on abort, `readIORef` after `create` returns, wrap in `unsafePerformIO`. Works, tested, not pretty.

Alternatives: pre-compute bounds-check pass (walks records twice, fine for typical sizes); push the apply loop into Rust (clean boundary but real implementation cost for marginal prettiness gain). If Rust grows an "apply record stream to buffer" primitive for performance reasons, inheriting the apply loop into it is the natural consolidation.
