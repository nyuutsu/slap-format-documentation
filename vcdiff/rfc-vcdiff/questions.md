# RFC-3284 VCDIFF — design questions

Questions specific to the RFC arc (see `spec.md`): the two features RFC 3284 defines that xdelta3 refuses — VCD_TARGET windows and custom code tables. Cross-arc disagreements (version byte, both-source-bits) live in the family `../questions.md`.

The defining fact of this arc: **both features have zero real-world occurrences and no working reference implementation.** xdelta3 parses their grammar and then refuses; nothing else emits them. So the questions here are unusually high-altitude for a wire format — they're "what should our implementation, very possibly the first working one, *be*" — and we are reading them out of RFC prose with the only cross-check being xdelta3's *dead* (compiled-out) code paths, which encode an implementer's reading we can study but not run. Anchored to `rfc3284.txt` and those dead paths; dispositions are recorded in bold below.

## VCD_TARGET windows

### Is the source-segment offset absolute into the produced target, bounded by the current window's start — and does the RFC prose actually say so, or are we reading it off xd3's dead code? `(one-right-answer-unknown)`

A VCD_TARGET window's segment is drawn from the target produced so far. xd3's compiled-out path treats `dec_cpyoff` as an absolute target offset and enforces `cpyoff + cpylen ≤ winstart` ("VCD_TARGET window out of bounds", `xdelta3-decode.h:1037-1043`) — i.e. the whole segment must lie within output completed before this window. RFC §4.2 describes the feature in prose without that arithmetic. So: is "absolute offset, must lie within earlier output" the *RFC's* semantics, or merely how the one implementer who started coding it (and stopped) interpreted it? We're inferring intent from abandoned code; that inference needs to be conscious.

**Absolute offset into the produced target, whole segment before the current window's start — and it is the RFC's semantics.** §4.2 defines position in one sentence for both bits ("position of the source data segment in the relevant file"); for VCD_SOURCE that can only be an absolute offset, so it is for VCD_TARGET too. §3 requires the segment be "completely known when T is being decoded", which bounds the whole segment — not just its start — to before the current window (`cpyoff + cpylen ≤ winstart`); well-defined because §3 also makes window order strictly sequential. xd3's dead-code check is the same arithmetic: corroboration, not source.

### Can a VCD_TARGET segment reach across all earlier windows, or only the most recent one? `(one-right-answer-unknown)`

xd3's dead path carries a second, tighter bound: it rejects `cpyoff < dec_laststart` as "unsupported VCD_TARGET offset" (`xdelta3-decode.h:109`), where `dec_laststart` is the start of the *previous* window — implying it could only copy from the immediately preceding target window, not arbitrary earlier output. Is that the RFC's intent (the segment is a single prior window) or an xd3 implementation shortcut (it only kept the last window buffered)? The answer changes both correctness *and* the memory model — "all prior target addressable" means the whole output stays live; "last window only" is bounded. RFC §1-2's windowing discussion ("source and target windows with corresponding addresses") doesn't settle it.

**Full reach: the segment may lie anywhere in `[0, winstart)`.** The RFC states no narrower bound than "earlier than T".

### What does a first-window VCD_TARGET mean, given there's no prior output? `(one-right-answer-unknown)`

If a VCD_TARGET window is the first window, `winstart = 0`, so any nonempty segment violates `cpyoff+cpylen ≤ winstart`. Is a first-window VCD_TARGET therefore always malformed, with an empty segment the only legal (pointless) case? An edge a naive decoder built from the prose might not guard.

**A nonempty segment is refused; an empty one is applied, with a note.** The first window starts at byte 0, so there is no earlier output to point at: a segment of any length reaches past `winstart` and is refused by the same bound as any other segment reaching too far — nothing first-window-specific about it. A zero-length segment does fit (zero bytes of nothing), and the window applies normally; slap just mentions the oddity, the way it mentions any harmless pointless thing it finds in a patch.

### With no encoder and no real patches, how do we establish that our VCD_TARGET implementation is correct at all? `(is-this-the-right-question)`

This is the question under the questions. Every other feature in slap is validated against real patches or a reference tool. VCD_TARGET has neither. Options: hand-construct synthetic VCD_TARGET patches from our reading of the prose and assert round-trips (but that only proves we're self-consistent, not correct); find any tool anywhere that emits one; or accept that "we implemented VCD_TARGET" is a weaker claim than for everything else and gate accordingly. Until this has an answer, the feature's correctness rests on prose interpretation alone.

**Correct means: matches the document.** There is nothing else to match. The documentation will say so, somewhere: at time of writing only slap makes or applies VCD_TARGET, so in one sense our implementation could be wrong (we may have misread the prose), and in another sense it cannot be (there is no other behavior to disagree with). We will hand-build VCD_TARGET patches as tests — they keep the code matching our recorded reading — and we will look for any other tool that does the thing, expecting not to find one.

### What gates VCD_TARGET *emission*, and can convert ever choose it? `(open-design)`

Decoding VCD_TARGET costs nothing and breaks nothing (no patch in the wild uses it). *Emitting* it produces patches only a total-RFC decoder — realistically only slap — can read. So emission is gated. What's the flag, is it off by default, and does any path other than an explicit opt-in create ever reach for VCD_TARGET (e.g. could a convert *to* RFC-VCDIFF produce one)? The asymmetry (decode freely, emit guardedly) is the shape; the controls are the question.

**VCD_TARGET is the optimizer's tool, not a flag.** The encoder may use it wherever it helps, the same way it chooses any other instruction. Under the default single-window shape there is no earlier output to reference, so it never appears; when the user's window-size choice produces multiple windows, it can. Convert-to-RFC behaves like create.

## Custom code tables

### How strictly do we enforce the no-nested-tables rule, and what's the error when it's violated? `(one-right-answer-unknown)`

RFC §7c is explicit: the inner delta that encodes the custom table "MUST use the default code table for encoding the delta instructions." So the inner delta must be decoded with custom-tables *disallowed*. If an inner delta itself sets VCD_CODETABLE, that's a violation — do we reject it, and with an error that names *table decode* (not generic parse failure) so the user understands the header, not the body, was malformed?

**Reject, with an error that names the table decode.** The MUST is load-bearing: a table defined in terms of another custom table recurses with no base case. The error points at the header's table declaration, not the patch body — the existing `VCDIFFNestedCustomCodeTable` already has this shape.

### What happens when the inner delta fails to produce exactly 1536 bytes, or fails to decode? `(one-right-answer-unknown)`

RFC §7a fixes the serialized table at 1536 bytes (six 256-byte arrays: types1, types2, sizes1, sizes2, modes1, modes2). The inner delta could fail to parse, fail to apply, or apply cleanly to something that isn't 1536 bytes. Each is a distinct way "produce a valid custom table" fails, and each wants an error that points at the table, not the windows that would have used it.

**All three are fatal, and each is reported precisely.** The patch's header promised a table and couldn't deliver one; falling back to the default table would be dishonest (the patch explicitly said not to use it). The three cases are logically distinct and stay distinct in the status: didn't parse, parsed but an instruction made an invalid demand, or applied cleanly to something that isn't 1536 bytes (which names the length it got). The inner parse and apply failures keep their ordinary fine-grained vocabulary, wrapped so the status reads "while decoding the custom code table: ⟨the precise inner failure⟩" — the usual granularity, plus an arrow pointing at the table.

### Where are invalid decoded-table entries caught — eagerly at table-build, or lazily when an instruction indexes them? `(one-right-answer-unknown)`

A decoded table can contain entries the RFC's instruction grammar doesn't actually permit: an instruction type byte > 3 (COPY is the max); a COPY whose mode exceeds `2 + s_near + s_same` for the table's declared cache sizes; or a fully-degenerate NOOP+NOOP entry (RFC §5.4: NOOP "means no instruction is specified," so indexing such an entry produces nothing and consumes nothing — legal-but-useless, or rejected?). Do we validate the whole table the moment it's built (fail early, before any window decodes) or only fault when a window's instruction stream happens to index a bad entry (fail late, mid-apply)? Eager is friendlier and matches "a malformed table is a header problem"; lazy is less work.

**The whole table is checked as soon as it's built; an invalid entry fails the patch then, used or not.** Invalid means: an instruction type above 3, or a COPY mode past what the table's own declared cache sizes allow. No encoder has a reason to write such an entry, so one showing up means the table bytes are wrong — and then the *valid-looking* entries can't be trusted either. There's no checksum in this arc to catch garbage output later, so this check is the tripwire. NOOP+NOOP is a valid entry — it breaks no rule, does nothing, consumes nothing. Its presence in a built table is announced (weird enough in itself), and if instructions actually index it, that's a second note, aggregate-counted — the NOOP-only-opcode advisory core's degenerate-shapes answer already assigned to this arc.

### What bounds the custom cache sizes, given a tiny patch can demand a huge allocation? `(one-right-answer-unknown)`

The custom-table header is two bytes: `s_near` and `s_same`, each 0–255 (RFC §7). `s_same = 255` demands a same-cache of `255 × 256 = 65,280` slots; the cache is reset (reallocated/cleared) per window, so a tiny patch with many windows amplifies that. Is there a cap on what a custom table may declare, or do we honor the full byte range and eat the allocation? What's legal per the RFC (which states no bound) versus what we'll actually accept? (Cousin of the xdelta3-arc `dec_size` allocation question and the core window-size allocation question — they may want a single, patch-wide allocation-budget answer.)

**Deferred to the family allocation-budget question — this is one of its consumers, not its own decision.** The facts that question will need, recorded here: the RFC states no bound; the wire maximum is `s_near = 255, s_same = 255`; the worst single demand is 65,280 same-slots plus 255 near-slots; and the caches are re-initialized at every window, so window count multiplies the work (though the allocation itself can be reused — only the clearing repeats).

### Does a custom table — and its cache sizes — apply to the whole patch, and can nothing change them mid-stream? `(one-right-answer-unknown)`

VCD_CODETABLE is a header field (RFC §4.1), so a custom table and the `s_near`/`s_same` it carries presumably govern every window in the patch, with no per-window table mechanism. Worth confirming there's no way for cache sizes to shift mid-patch — a decoder that reset to the *default* sizes between windows while keeping the custom table would silently corrupt every COPY address.

**Confirmed: one table, one size pair, fixed at the header for the whole patch.** The header occurs once, before any window, and the window grammar carries nothing that could change a table or its cache sizes. The per-window re-initialization (RFC §5.1) touches cache *contents* only — slots zeroed, the near pointer reset — never the sizes. Those are two different resets, and conflating them (re-initializing to the default sizes while keeping the custom table) would decode every COPY address through the wrong cache from window two onward, silently; the decode keeps them distinct.

### Do we ever emit a custom code table on create? `(open-design)`

Like VCD_TARGET: emitting one yields patches only a total-RFC decoder can read. So presumably create always emits the default table. Is there ever a reason to emit a custom one (RFC §7's motivation is files with many identically-sized repeated segments — a real if narrow win), and if so, gated how?

**Yes, when it pays — the custom table is the optimizer's tool, like VCD_TARGET.** Having encoded under the default table, the encoder knows which instruction shapes dominate its own stream; when a table shaped around them would save more than the table costs to ship, it emits one — cache sizes included in that choice.
