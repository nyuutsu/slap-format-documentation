# Atari precursor: Snes-Famicom Utility (MCA / Elite / Elitendo)

Snes-Famicom Utility is MCA's Atari ST line of SNES-copier tooling: Elite → Elitendo group lineage, shipping in 1993–1994 across Atari ST, Mega ST, and Falcon 030. Versions in our archive: a dedicated `ST_PATCH` (Snes Patcher v1.0, Elitendo 1993); and the bundled utility line at v1.8 (`SNES/FAMICOM Utility V1.8`), v2.0, v2.1 (`SNES-FAMICOM UTILITY V2.0`/`V2.1`).

The v2.0 disk's bundled docs (`upstream/snes-famicom-utility-v2.0_doc.txt`, extracted from `SNES_V20.MSA`) include an inline IPS specification. Excerpt at `upstream/snes-famicom-utility-v2.0_ips-spec-excerpt.md`. Two facts the spec settles:

- **Format name**: *"Ips stands for 'International Patch Standard'."* MCA's own primary source. ST_PATCH's about-box adds *"Patcher format idea by DAX"*. Co-authorship is MCA + DAX.
- **Truncation marker**: one optional 3-byte field after `EOF`, framed as *"OPTIONAL length for CUTTING games"*.

## Magic check

Apply-side, m68k:

```
cmpi.l #$50415443, (a5)        ; "PATC" — only 4 bytes compared
beq.s  <success>
<reject path>
```

Confirmed in `ST_PATCH.PRG` at file 0x32A (1993) and `SNES_20.PRG` at file 0x20E0 (v2.0). The 5th byte of the magic (`H`, 0x48) is read but never checked. Create-side writes the full 5 bytes:

```
move.l #$50415443, (a6)+       ; "PATC" longword
move.b #$48, (a6)+             ; 'H' byte
```

Confirmed in `ST_PATCH.PRG` at file 0x55E. The create/apply asymmetry — write 5, validate 4 — is m68k-natural: a 32-bit longword cmpi lands on a 4-byte prefix.

## Truncation marker

Apply-side, after the record loop reaches `EOF`:

```
0x216C  cmpa.l  d0, a5           ; current ptr at end-3 means trailer present
0x216E  bne.w   $2244            ; otherwise: no trailer, success exit
0x2172  moveq   #$0, d0
0x2174  bsr.w   $1F02            ; d0 := file size  (Fseek mode=2, offset=0)
0x2178  moveq   #$0, d3
0x217A-0x2182                    ; read 3 BE bytes into d3 (trailer-declared size)
0x2184  cmp.l   d3, d0           ; compare current size to trailer
0x2186  bls.w   $2244            ; if size <= trailer: SKIP truncation
                                 ;   (bls = "branch if d0 ≤ d3" unsigned)
0x218A  ...                      ; otherwise: actual chunk-copy truncation
```

Confirmed in `SNES_20.PRG`. The contract: trailer means cut, never extend. When the trailer's declared size is at or above the post-apply file size, the program silently skips truncation and exits via the success path (`$2244`); the file is left at whatever size records produced. When the trailer's declared size is strictly below, a TEMP file is created, exactly trailer-size bytes are chunk-copied from the patched source, the original is `Fdelete`d, and TEMP is renamed in its place.

The trailer value is accepted as-is; there is no shape predicate on it.
