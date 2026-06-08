# N64 country code (`0x3E`) — label mapping

The APS-N64 type-1 header copies the N64 ROM's country byte (ROM header offset `0x3E`) for source verification. The APS spec itself does not enumerate the byte's values, and the third-party fan resources that do enumerate it disagree with each other. The labels in `Slap.APSN64.Types.apsN64CountryName` were reconciled by mining a N64 ROM set: reading each ROM's `0x3E` byte and correlating it with the region the ROM is independently known to target.

## Verified against the patches we have

These mappings held with no exceptions across the set:

| byte | char | label | notes |
|------|------|-------|-------|
| `0x44` | D | Germany | single-language market |
| `0x45` | E | USA | North America |
| `0x46` | F | France | single-language market |
| `0x49` | I | Italy | |
| `0x4A` | J | Japan | |
| `0x53` | S | Spain | |
| `0x50` | P | Europe | pan-European, multi-language |
| `0x55` | U | Australia | used when present; many AU releases ship the `P` (PAL) build instead |
| `0x42` | B | Brazil | official (Tec Toy) releases; pirate/relabeled carts keep their donor's byte |

`0x41` ('A') = **all regions / multi-region**: a single cart serving more than one market (e.g. a Japan+USA release).

`0x00` = **region-free / unstamped**: the country field was never written. We only saw this used on prototypes, test programs, and homebrew.

## The European alternate-SKU family: P / X / Y / Z

`0x58` ('X'), `0x59` ('Y'), and `0x5A` ('Z') are not separate territories. When a title shipped multiple parallel European language editions, the country byte spilled from `P` into `X`, then `Y`, then `Z`: same game, same cart ID, only the country byte differing to distinguish the language SKU. There is no fixed language→letter mapping. Examples:

- Carmageddon 64: `NCDX` (En,Fr,De,Es) and `NCDY` (En,Fr,Es,It)
- Shadowgate 64: `NSGP`, `NSGX` (Fr,De,Nl), `NSGY` (Es,It)
- Gex 3: `NX3P` (En,Es,It), `NX3X` (Fr,De)

So all four render as "Europe", with the letter shown to keep the SKU distinguishable.

## Documented but unconfirmed

No samples for these appeared among the patches we have; they are carried from the fan documentation as best-effort labels and have not been verified here: Beta (`0x37`), China (`0x43`), Gateway 64 NTSC (`0x47`), Netherlands (`0x48`), Korea (`0x4B`), Gateway 64 PAL (`0x4C`), Canada (`0x4E`), Scandinavia (`0x57`).

Any byte outside the table above is preserved verbatim as `APSN64CountryUnrecognized` and rendered as `unknown (0x__)`.
