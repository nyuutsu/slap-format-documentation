# IPS 2

SNESTool v1.2 (MCA/Elite, DOS, 1996) ships with a doc describing a format it calls "IPS 2" for file truncation, and a binary implementing it. Artifact: `snestl12.zip` on scene.org, containing `SNESTL12.EXE` (23,978 bytes, 1996-02-12) and `SNESTL12.DOC`.

The doc's v1.0→v1.01 changelog lists "IPS 2 ( cutting files ) Create and Use of a IPS 2 file work ok now" under bug fixes — so the format existed in v1.0, broken, and v1.01 onward has it working. The "Use IPS" section explains the purpose: "IPS2 files are ment to 'Cut' a file."

The binary matches. `SNESTL12.EXE` contains the string `No File Cut, IPS2 size error !` at file offset `0x412a`; the code that emits it lives in a truncation routine around image offset `0xB47`–`0xBE9`, reachable only via the EOF handler of the IPS apply loop. The create path has a symmetric trailer-emission routine at image offset `0xDEB`.

Wire format, as a delta against IPS: a trailer of exactly 3 bytes big-endian after `EOF`, giving the final target file size. Absence of the trailer means no truncation. SNESTool additionally rejects the trailer unless `(size & 0xFFF) == 0x200` — the SMC-shaped-size pattern — printing the size-error string above. Modern appliers (Flips, RomPatcher.js) accept the trailer without that filter; the wire bytes are the same.

Separately, the "Use IPS" doc section contains a first-person attribution for the IPS format as a whole: "This type was invented by DAX and ME" (ME = MCA). Uncorroborated, not contradicted by any source we've found.
