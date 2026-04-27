# IPS 2

SNESTool v1.2 (MCA/Elite, DOS, 1996) ships with a doc describing a format it calls "IPS 2" for file truncation, and a binary implementing it. Artifact: `snestl12.zip` on scene.org, containing `SNESTL12.EXE` (23,978 bytes, 1996-02-12) and `SNESTL12.DOC`.

The doc's v1.0→v1.01 changelog lists "IPS 2 ( cutting files ) Create and Use of a IPS 2 file work ok now" under bug fixes — so the format existed in v1.0, broken, and v1.01 onward has it working. The "Use IPS" section explains the purpose: "IPS2 files are ment to 'Cut' a file."

The binary matches. `SNESTL12.EXE` contains the string `No File Cut, IPS2 size error !` at file offset `0x412a`; the code that emits it lives in a truncation routine around image offset `0xB47`–`0xBE9`, reachable only via the EOF handler of the IPS apply loop. The create path has a symmetric trailer-emission routine at image offset `0xDEB`.

Wire format, as a delta against IPS: a trailer of exactly 3 bytes big-endian after `EOF`, giving the final target file size. Absence of the trailer means no truncation. SNESTool additionally rejects the trailer unless `(size & 0xFFF) == 0x200` — the SMC-shaped-size pattern — printing the size-error string above. Modern appliers (Flips, RomPatcher.js) accept the trailer without that filter; the wire bytes are the same.

The size-check itself, transcribed from `SNESTL12.EXE` at image offset `0xB6F`–`0xB80`, is two instructions and a branch:

```
0B6F  81 E1 FF 0F      and cx, 0x0FFF
0B73  81 F9 00 02      cmp cx, 0x0200
0B77  74 0A            jz  0x0B83        ; success → fall into Mbit calc
0B79  8D 36 3A 08      lea si, [0x083A]  ; → "No File Cut, IPS2 size error !"
0B7D  E8 D6 19         call 0x2556       ; print
0B80  E9 58 F5         jmp  0x00DB       ; abort
```

`cx` holds the low 16 bits of the parsed 24-bit trailer size; the high byte sits in `dl` and is irrelevant to the predicate (the mask is 12 bits) but does feature in the subsequent Mbit-display calculation at `0xB83`–`0xB89`: `(size - 0x200) / 0x2000`. The arithmetic restatement of the predicate is `size mod 4096 == 512`.

The "SMC-shaped-size" framing for that predicate is the *motivating* case, not the rule. A commercial SMC-headered SNES ROM passes by construction: the header-stripped payload is a multiple of 4 KiB (every commercial SNES title is a much larger power of two), and the 512-byte SMC copier header tacks `0x200` onto the size. The check is necessary but not sufficient for SMC-ness — a hypothetical `0x1200`-byte file also passes — but that looseness is in MCA's binary, and any reproduction has to reproduce it verbatim.

Separately, the "Use IPS" doc section contains a first-person attribution for the IPS format as a whole: "This type was invented by DAX and ME" (ME = MCA). Uncorroborated, not contradicted by any source we've found.
