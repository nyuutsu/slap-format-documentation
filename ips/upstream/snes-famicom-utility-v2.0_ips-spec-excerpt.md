# IPS spec — primary source

Excerpted verbatim from `snes-famicom-utility-v2.0_doc.txt`, the bundled documentation file inside `SNES_V20.MSA` (Atari ST disk image; tool: "SNES-FAMICOM UTILITY V2.0", coded by The Magician/MCA, with Sledge & Lowlife of Hotline; tested on Atari 1040 ST(e) / Mega ST 1 / Falcon 030, hence at-or-after late 1992).

This is the earliest dated primary-source description of the format we have located.

```
USE *.IPS
~~~~~~~~~

Ips stands for "International Patch Standard", it is a powerful and
easy way to glue trainers/intro's/special slowrom fixes/etc.. to
a game file. IPS is compatible with AMIGA & PC.
First pick the ips file, then the game file. It uses fileseek routines
to locate places for patching.
Format of patch method:

        dcb    "PATCH"          ;id ascii (5 bytes)
loop    dct     $123456         ;24 bit offset from start (3 bytes)
        dcw     $0005           ;number of bytes to change (2 bytes)
        nop                     ;actual patches
        jsl     trainercode     ;1+4
        .......                 ;as many loops in between

optional dct     $123456         ;offset
         dcw     $0000           ;packer id = 0000 !!
         dcw     $1234           ;number of packed bytes
         dcb     $12             ;packed byte

         dcb     "EOF"           ;End of File
         dct     $123456         ;OPTIONAL length for CUTTING games.(3 bytes)
```

The notation is m68k assembler: `dcb` = define-constant-byte (a byte sequence), `dcw` = define-constant-word (16 bits, big-endian on m68k), `dct` = define-constant-triple (24 bits, big-endian).
