# IPS — notebook

Idle thoughts. Things noted because we don't want to forget, not because we've committed to doing them. No entry here is a todo. Commitments live elsewhere.

### Coverage-mask apply without sorting

Current apply uses `initialFill`: copy the entire source into the output buffer, zero-fill any tail past source, then walk records in wire order and overlay each write. Every output byte a record touches gets written twice — once by `initialFill`, once by the record. For patches that overwrite a small fraction of a large source (`fe6.ips` is the pattern: ~610 KB of record payload on an ~8 MiB source extending toward 16 MiB), this is a few percent of redundant buffer writes.

One way to eliminate the redundancy without changing wire-order apply semantics: compute a coverage mask in a first pass — a bitmap, one bit per output byte, set where any record writes. Then copy source passthrough only into the *un*-covered regions, and apply records in wire order on top as before. Every output byte is written exactly once (either source passthrough or a record write). No sorting, no reordering, no change to clobber semantics.

Cost: `O(targetSize)` bits for the mask, plus `O(K)` to build it from the record list. Two passes total instead of one, but each pass writes less.

Not planned. The current approach is safe, reliable, and easy to reason about; the savings are small on typical inputs. Noted here in case the performance shape of some future use case changes.
