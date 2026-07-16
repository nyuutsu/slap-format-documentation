# Header field research

Cases from the patch-collection research (romhacking.net archive, 2024-08-01) where a framing difference — not ROM content — was the whole interop problem. Checksums below were verified during that work.

## The NES 2.0 header migration is systemic

No-Intro is actively migrating NES sets from iNES 1.0 to NES 2.0 headers. The back-catalogue of NES patches was built against iNES 1.0, so every NES patch whose format checksums the whole file (BPS, UPS) progressively fails verification against modern dumps — the game data is unchanged; only header bytes moved.

The downgrade is exactly the spec-derived one (zero byte 7's NES 2.0 bits, zero bytes 8-15), verified twice during ROM sourcing for the BPS work:

| ROM | Built from | CRC32 after downgrade |
|-----|-----------|------------------------|
| Super Mario Bros. 2 (USA) (Rev A).nes | No-Intro `(Rev 1)`, CRC `dea55f53` | `e0ca425c` |
| Super Mario Bros. (World) (iNES 1.0).nes | No-Intro `(World)` | `3337ec46` |

## A worked iNES header, and why constructing one is hard

Super Mario Bros. (PC10) — the source for two shrink patches — required assembling a ROM from MAME chips under the header `4E 45 53 1A 02 01 01 00 00 00 00 00 00 00 00 00`. Bytes 4-6 (2× 16K PRG, 1× 8K CHR, vertical mirroring / mapper 0) had to be found by brute-force CRC matching: they encode per-game knowledge that no derivation from the body supplies. Constructing an iNES header from nothing is underdetermined; rewriting one you already hold is not.

The dump also carries the PC10 instruction ROM as 8 KiB silently appended past what the header declares — GoodNES-era convention for PlayChoice-10 dumps.

## An N64 byte-order reconciliation, verified

The Ocarina of Time MQ Debug patch targets the V64 (byteswapped) framing. The Z64 dump in hand had CRC32 `62f92704`; swapping every byte pair produced CRC32 `2ee3e247`, exactly the patch's expected source. Identical game data, different byte order — the permutation analogue of the header problem.

## Dual-patch distribution

ff2iset ships separate `(Header).ups` and `(No Header).ups` files for the same hack because the format cannot express "strip 512 bytes first" — the workaround the community standardized on. See `../ups/findings.md` and `../header-awareness.md`.

## Status notes from the same research

iNES headers are universal and load-bearing on NES. SNES copier headers are dead in practice — No-Intro strips them and nobody has them anymore. GB/GBC/GBA have no external headers. N64 is headerless but byte-order varies.
