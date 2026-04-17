# IPS — design questions

### Record ceiling

The 24-bit offset and 16-bit size are independent field widths. Saturating
both gives a last-byte position of `0xFFFFFF + 0xFFFF - 1 = 0x100FFFD` —
about 64 KB past 16 MiB. The addressable range is `0x100FFFE` bytes.

**slap uses the arithmetic ceiling on both parse and create:** accept any
record with `offset + payload ≤ 0x100FFFE`, emit up to the same bound.
Symmetric — what we parse, we emit; what we emit, we parse.

Ecosystem:

- **Flips** apply has no ceiling check. Create refuses `targetlen >
  16777216` with the assertion "the IPS format doesn't support that"
  (`libips.h:20`). No supporting math.
- **RomPatcher.js** is the same shape — create refuses `offset >=
  0x1000000`, apply has no check.
- **lua-ips** has no check on either side.

`fe6.ips` (Fire Emblem 6 translation) has its final record at `0xFFFFFF`
with payload extending past `0x1000000`. Flips and RomPatcher.js both
apply it cleanly despite neither being able to produce it.

The format's math permits the full range; the ecosystem applies it fine;
what we emit is compatible with every existing applier. Flips's
conservative create-side refusal is a design choice, not a format rule.
We see no reason to inherit it.

### Growth

IPS has no target-size field in the wire format. Records name offsets;
nothing says whether those offsets have to be within source length. In
practice they don't: a record at an offset past the source end implicitly
extends the target to include that record's write region.

**slap derives target size at apply time.** When the patch has no
truncation marker, target size is `max(sourceSize, maxRecordEnd)` — the
larger of the source length and the highest byte position any record
touches. When a truncation marker is present, that marker's value is the
target size (covered in the shrink/truncation entry).

Bytes in the target that fall within `[0, sourceSize)` come from source
by default. Bytes in the growth region `[sourceSize, maxRecordEnd)` that
no record overwrites are zero-filled.

Ecosystem — all three appliers in `tools/` behave this way:

- **Flips** `memcpy`'s source then `memset`s the tail to zero
  (`libips.cpp:115`).
- **RomPatcher.js** allocates a new BinFile of the computed size
  (default-zero) and copies source in
  (`RomPatcher.format.ips.js:138-141`).
- **lua-ips** extends with explicit `string.char(0)` fill.

Not really a design decision, more a consequence of the format: no target
size declared, records name arbitrary offsets, so target size falls out
of the records themselves and any untouched growth region has no other
defined value.

### Record order

Records appear in a patch in whatever order the encoder emitted them. The
format doesn't require that order to match offset order; most encoders
emit in offset order, but it's not a wire-level rule.

**slap applies records in wire order, period.** The apply loop visits
`records[0]`, `records[1]`, etc., in index order. No sorting, no
reordering.

If a patch's records aren't in offset order — call them unsorted — slap
emits a warning. Unsorted records are unusual and we want to tell the
user.

Why wire order: reordering at apply time would change behavior when
records overlap (see overlap entry — the clobber semantics depend on
apply order). Any other choice introduces ambiguity that has to be
enforced by convention rather than by the wire itself. Wire order is
unambiguous.

Ecosystem: Flips, RomPatcher.js, and lua-ips all apply in wire order.
Flips has a commented-out `w_scrambled = true` at `libips.cpp:69-70` —
the author considered warning on unsorted records but didn't ship it.
