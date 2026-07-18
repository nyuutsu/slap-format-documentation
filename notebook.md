# slap — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### `applyIPS` and `applyDPS`: `unsafePerformIO + IORef` around `create`

Both functions populate a preallocated output buffer via `Data.ByteString.Internal.create`, which forces the record-walk into `IO`. The walk can fail mid-way (a record writes past target, a source read out-of-bounds), and the error has to escape the `IO` block. Current pattern: allocate an `IORef (Maybe ApplyError)`, write to it on abort, `readIORef` after `create` returns, wrap in `unsafePerformIO`. Works, tested, not pretty.

Alternatives: pre-compute bounds-check pass (walks records twice, fine for typical sizes); push the apply loop into Rust (clean boundary but real implementation cost for marginal prettiness gain). If Rust grows an "apply record stream to buffer" primitive for performance reasons, inheriting the apply loop into it is the natural consolidation.


### A patch that can't be verified should say so, uniformly

Three formats can arrive without verification data: PPF3 (validation block omitted), xdelta1 (`FLAG_NO_VERIFY`), and xdelta3 (Adler32 off — ~3% of the patches we have). Today only xdelta1 says anything — it emits `VerificationOptedOutByCreator` from parse. PPF3 is silent, even though that warning's own documentation names PPF3's absent block as a case it covers. The VCDIFF rewrite will want the same notice for an Adler-less xd3-arc patch.

The wanted shape: on apply and convert-to, an unverifiable patch gets the notice; on create with verification omitted, a brief confirmation that the setting was respected — informational, with no scolding. RFC-arc VCDIFF and other formats that simply *have* no verification mechanism stay silent: absence of a slot is a fact about the format, not about the patch.

Probably a small generalization — the parse-side "verification posture" surfaced uniformly instead of per-format ad hoc — or possibly just two more call sites. Kin to the deferred verification-requirement modeling idea (making mandatory/optional/absent first-class per format).

### What trailing bytes after a well-formed patch mean, if anything

A patch's structure can be complete before its bytes run out — there is room after an IPS `EOF`, a NINJA `END`, and similar terminators. Some trailing shapes are recognized and have a home (a truncation marker, an EBP metadata blob); arbitrary bytes are just there. Handling of the arbitrary kind is currently uneven — some formats warn on it, others (NINJA1 and NINJA2 among them) drop it silently — and that mix is where various fixes happened to land, not a considered stance. Whether such bytes should warn, be refused, or pass without comment, and whether the answer is one program-wide rule or a per-format call, is a decision worth making deliberately at some point.
