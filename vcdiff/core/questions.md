# VCDIFF core — design questions

Questions about the shared core wire mechanics (see `spec.md`). This set is **short and edge-heavy on purpose**: RFC §4–6 specify the core mechanics tightly — the instruction tuple format, the address cache, the default table — so little is left *open*. What remains are the edges the RFC nails the happy path of but doesn't spell out the failure modes for. Gritty is the honest register for this layer.

Anchored to `rfc3284.txt` by section and xd3 by source line. Answered entries carry the agreed disposition in bold; the rest are open. Flavor tags as in `../questions.md`.

## Source-segment bounds

### When a COPY would read past the actual source file, do we error or zero-fill — and do we bound the segment eagerly or lazily? `(one-right-answer-unknown)`

A VCD_SOURCE window declares a source slice `[position, position+length)` for its COPYs. The RFC is silent on what happens if that slice — or a COPY into it — runs past the real source. So: error or zero-fill the overrun, and check the slice up front (eager) or only when a read crosses the end (lazy)?

**Error, and check eagerly — both arcs.** A read past the source can't be honored: it means a wrong/too-short source or a damaged patch, and there's no honest byte to put there, so we refuse rather than fabricate one. Eagerly = reject a window whose declared slice is bigger than the source we were handed, before decoding it. A real patch never points past the source it was built against (verified against real ROMs), so this never refuses a good patch — and since VCDIFF has no checksum, it's the only check that catches a wrong source at all.

The check's shape and error type is implementation, not decided here.

## Consuming the three sections

### What happens when an instruction wants more bytes than its section holds? `(one-right-answer-unknown)`

Decoding walks pointers through the inst/data/addr sections. The RFC gives no rule for a pointer hitting its section's end mid-decode: an ADD wants `size` data bytes the data section is short on, a coded size varint runs off the inst section, a COPY's address runs off the addr section.

**Error — there's nothing else to do.** Each names a byte that isn't there: an index past the section's declared end, not a `0` or any value. The instruction has no defined result, so the patch is malformed and we reject it. No real choice here, unlike a checksum mismatch or a reserved bit, where a decoder could reasonably proceed.

One non-case: a window's decode ends when its declared target size is filled, so the inst section running dry *at that point* is normal completion, not a fault.

The error's shape is implementation, not decided here.

### After a window's instruction stream ends, are leftover bytes in its data or addr section an error? `(open-design)`

The instruction stream drives decode; data and addr are pulled on demand. A window whose instructions stop with bytes still unconsumed in the data or addr section is over-supplied — structurally odd but it *decoded fine*. Tolerate (the bytes are simply unused) or reject (the encoder produced something malformed)? This is the intra-window cousin of the family-level trailing-bytes question, and may want the same disposition.

**Reject — both arcs.** A window declares its three section lengths up front; if its instructions finish with bytes still unconsumed in the data or addr section, the patch has contradicted its own declaration. The decode itself is fine — the output is correct and the surplus bytes are simply never read — so this is a *self-consistency* check (the patch disagreeing with itself), not a question of whether the decode produces the right output. We reject the self-disagreement. It's the same kind of check as the delta-encoding-length cross-check below, and a different kind from the `spec.md` invariants, which are about producing correct output. xd3 enforces exactly this (it rejects leftover data and addr at the end of a window), and it appears in none of the patches we have. The instruction section can't leave a remnant — it is the decode driver, so it is always fully consumed by construction — making this a data/addr-only check.

It does **not** share the family-level trailing-bytes disposition, despite the cousin framing. There, the leftover lies *outside* everything the patch declares, breaks no claim, and is tolerated-with-a-note; here it falls *inside* the window's own declared lengths, so it is a contradiction. Same shape, opposite nature, opposite call.

## The copy invariants, precisely

### What is the exact legal range for a COPY address, given overlap is allowed? `(one-right-answer-unknown)`

Two distinct bounds, and getting the boundary exactly right is correctness-critical (too loose lets a patch read uninitialized memory; too tight rejects valid periodic-copy patches):

- The address's **start** must be strictly before the current output position: `addr < here` (xd3 errors "address too large" at `xdelta3-decode.h:310`). This is what forbids reading not-yet-produced target.
- A copy whose start lies **in the source segment** (`addr < cpylen`) must not extend past the segment: `addr + size > cpylen` is an error ("size too large", `xdelta3-decode.h:318`).
- But a copy whose start lies **in the target region** (`addr ≥ cpylen`) is *not* bounded at its end by `here` — it may overlap and extend past the current position as it writes, which is exactly the periodic / run-length case the RFC blesses (§4.3).

So the rule is asymmetric: bound the start always, bound the end only for source-segment copies. Is this precisely the invariant we encode, and is it expressed so the overlap case can't be accidentally rejected by an over-eager end check?

**Confirmed — the asymmetric rule is exactly right, and it is already `spec.md`'s core invariants #1 and #2.** RFC §4.3 mandates the no-spanning half outright: a COPY's substring "must be entirely contained in either S or T" (lines 277–278), so the source-start end bound (`addr + size ≤ cpylen`) is the RFC's rule, not an xd3 quirk — both flavors enforce it, so it is core, not ejected. The overlap half is the RFC's too: a target-start copy is "contained in T" by virtue of `addr ≥ cpylen` (not by staying within already-produced bytes), and the spec explicitly blesses extending past `here` "as long as the latter starts earlier" (the worked example, lines 304–308). So a target-start copy needs only the start bound; only source-start copies carry an end check.

## Internal consistency

### Do we cross-check a window's declared delta-encoding-length against the sum of its fields? `(open-design)`

The delta-encoding-length should equal exactly `sizeof(target-size) + 1 + sizeof(A)+sizeof(I)+sizeof(C) + [4 if adler] + A + I + C`. xd3 computes this and compares (`xdelta3-decode.h:1118`), with its own comment hedging it as "redundency, otherwise it is not really used." The RFC defines the length field but never says a decoder must verify it. It's a free internal-consistency check that catches a class of corruption before any section is read. Do we enforce it (and is its error distinct from "a section ran past the window"), or skip it as the redundancy xd3 calls it?

**They must agree — a window whose declared length disagrees with the sum of its fields is malformed, and rejected.** The length is in the patch, so "is it correct?" is free signal on the weird-to-malformed axis; that a mismatch might still be forensically applyable doesn't matter, the goal is to be correct, not to salvage. This is Q3's principle one level up — the window's framing must not contradict its contents — so it's core (both arcs) and the same self-consistency family, and free on real input: a conformant encoder derives the length from the fields, so it never fires on a good patch.

What makes the check load-bearing rather than mere pedantry is that we *rely* on the declared length as the window boundary — the field's intended job, and what lets a reader reach the Nth window without fully parsing the rest. Once we steer by it, checking it is obligatory: an unverified length we navigate by silently skips a gap when too large, or misaligns every following window when too small.

## Degenerate but structurally valid

### Which wire-valid-but-degenerate window shapes are coherent, and which fail (and where)? `(open-design)`

Several field values are individually legal but produce odd windows. Modeled on the BPS degenerate-patch question:

- **target-window-size = 0**: a window that produces no output. Legal no-op, or malformed?
- **zero-length data/inst/addr section**: e.g. a window with no instructions (inst length 0) — produces nothing; same question as zero target size.
- **zero-size ADD / COPY** (whether from the table or a coded size): a no-op instruction. Does it advance any pointer; is it legal?
- **a window with neither source bit set and no COPYs** — pure ADD/RUN from inline data. Legal (self-contained), presumably — confirm.

Likely disposition (per BPS) is "accept the wire-valid ones at parse, fault the incoherent ones at decode via the normal bounds checks" — but which bucket each falls in needs stating, and zero-output windows in particular need a deliberate call (legal no-op vs rejected).

**Accept all of them — none is malformed. slap does not forbid degenerate-but-structurally-valid shapes; it applies them, and where an action is *literally pointless* it says so.** Confirmed against xd3: every shape here is accepted and applied (zero-output window, empty sections, zero-size ADD/RUN/COPY), and the RFC prohibits none — sizes are unsigned varints, so "negative" isn't even representable and only the zero-vs-positive edge exists. The only rejection nearby is not degeneracy-specific: a COPY whose address is out of bounds (`addr ≥ here`) is refused even at size 0, which is just invariant #1 applying regardless of size.

The reporting line: a *literally pointless action* earns a granular "weird thing happened, FYI" advisory; a merely *arbitrary structural truth* — a section length of 0 because the window has no instruction of that kind — stays silent. The pointless-action reports, one per kind:

1. **zero-size ADD** — appends nothing.
2. **zero-size RUN** — writes nothing, but still consumes one fill byte.
3. **zero-size COPY** — copies nothing, but still decodes its address and updates the address cache, so it is not inert. To accept it correctly we must do everything a COPY does except the empty write — decode the address, update the cache, bounds-check — or we fall out of step with any later COPY whose address is read relative to the cache.
4. **zero-output window** — a window contributing no bytes to the target.
5. **source segment declared but never copied from** — `VCD_SOURCE` set and a segment positioned, yet no COPY reads it.
6. **a NOOP-only opcode** — reachable only through a custom code table, so this one is rfc-arc, not core (the default table has no all-NOOP entry).

None is an accusation of malformation. They are also rare by construction: in the clean patches we have, #1–#4 never occur, and #5 occurs once — in genuine xd3 output (a window whose copies are all target-internal, leaving its declared source segment unused). So a firing report reliably means the patch is unusual.

## Varints

### A truncated varint — continuation bit still set at end of input `(one-right-answer-unknown)`

A varint whose high bit promises another byte when the input has run out.

**Reject — and it is a distinct failure from over-width.** An incomplete number has no coherent value to recover, and the fault is "ran out," not "too big," so it earns its own error rather than folding into the width cap. Both byuu and vcdiff readers already do this (`VarintRanPastEndOfInput`); xd3 agrees ("further input required" / "end-of-input in read_integer").

### The decoded width cap `(uses-frostmourne-to-butter-its-toast)`

RFC §1–2 makes word-size independence an explicit design goal: the varint is unbounded on purpose, so a value encoded on a 128-bit machine decodes anywhere. slap's sizes and offsets flow through the host signed `Int` (`Int64` on any real build), so we knowingly narrow that guarantee.

**Cap at signed `Int64`, at two thresholds.** A decoded value in `[2⁶³, 2⁶⁴)` — one that sets the 64th bit — is representable by xd3's `uint64` reader but not by slap's signed `Int`, so it gets a dedicated, fatal, *apologetic* error: the explicit concession that xd3 (the format's effective definition) admits these and we decline. It will never fire on a real patch; it exists for completeness, and for the small joy of naming the exact bit we give up. A value `≥ 2⁶⁴` is beyond `uint64` too — xd3 rejects it as well — so it is a plain over-width rejection, no apology owed.

The representation is signed because `Int` is the buffer-index type (a size *is* an index, no conversion) and signedness lets underflow be caught cheaply rather than wrapping; the only cost is that one top bit — an ~8-exabyte value nothing can index, allocate, or (xd3 included) actually apply. This is the home of the narrowing stance; the family-level frostmourne question points here.

### Non-canonical (overlong) varint encodings `(open-design)`

Base-128 admits leading zero-groups, so a value has more than one encoding: `0x80 0x00` and `0x00` both decode to 0. The RFC forbids neither, and xd3 accepts the long form (its reader has no canonicality check).

**Accept, and emit an FYI advisory.** Accepting overlong forms only ever accepts more, never rejects a real patch, and matches xd3; the advisory is the house-consistent "weird thing happened" note — sibling to BPS flagging its non-canonical `0x81` "negative zero" (`NegativeZeroInBPS`). A slap encoder emits only canonical forms.
