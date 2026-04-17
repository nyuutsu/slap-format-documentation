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
