# APS-GBA — putting a question to the original patcher

No specification for APS-GBA is known to us. The format's only authority is A-Ptch, a Visual Basic 6 application for Windows, and the only way to ask it what the format means is to run it and read what it writes. This describes how.

It is worth the setup. Two careful readings of the patcher's disassembly agreed with each other and were both wrong about how a partial block is checksummed; one run settled it in the other direction. Where a question can be put to the tool directly, put it to the tool.

## What you need

- `wine`, and `winetricks` to supply the Visual Basic 6 runtime
- `i686-w64-mingw32-gcc` to build the driver
- `python3`
- The patcher itself, at `upstream/A-Ptch.exe`

A-Ptch is UPX-compressed. It runs fine that way — unpacking is only useful for reading the disassembly, and a patch produced by the unpacked copy is byte-identical to one produced by the packed original, so unpacking does not perturb what it does.

## Setting up

```sh
export HARNESS=$(mktemp -d)
export WINEPREFIX=$HARNESS/wp
winetricks -q vb6run
mkdir -p "$WINEPREFIX/drive_c/aps"
cp docs/aps-gba/upstream/A-Ptch.exe "$WINEPREFIX/drive_c/aps/"
```

`vb6run` installs Microsoft's own `msvbvm60.dll` rather than a reimplementation, which matters: the behavior in question — what the runtime's file-read call leaves in the unread tail of a buffer — lives inside that DLL and not in anything wine substitutes for it. A default 64-bit prefix is fine; the 32-bit runtime lands in `syswow64` and works.

Build the driver:

```sh
i686-w64-mingw32-gcc -O2 -o "$WINEPREFIX/drive_c/aps/driver.exe" docs/aps-gba/harness/driver.c
```

`driver.c` is Windows C and will not compile on the host toolchain; an editor that lints it against the system headers will complain about `windows.h`. That is expected and not worth working around.

## Asking a question

A-Ptch is a GUI application with no command-line mode, so the driver launches it, finds its controls by walking the window tree, fills in the paths, clicks Run, and dismisses whatever dialog comes back. It takes the mode as text, matched against the radio button labels.

Build a source/target pair whose length is deliberately not a multiple of 64 KiB, so that a partial block exists to ask about:

```sh
python3 docs/aps-gba/harness/make-case.py "$WINEPREFIX/drive_c/aps" case1 0x1C123
```

Run the patcher over it:

```sh
cd "$WINEPREFIX/drive_c/aps"
wine driver.exe A-Ptch.exe \
    'C:\aps\case1-source.gba' \
    'C:\aps\case1-target.gba' \
    'C:\aps\case1-out.aps' \
    'Create'
```

The driver prints the control tree it found at each step, which is the thing to read when it does not work. Paths are passed in Windows form because they go into the application's own text boxes.

Then read what came out:

```sh
python3 docs/aps-gba/harness/read-patch.py "$WINEPREFIX/drive_c/aps" case1
```

For each record covering a partial block it prints the stored checksums beside all three candidate readings, and says which one agrees. It also reports whether the XOR payload past end of file is all zeros, which is a second and independent signal about the same question.

## What has been settled this way

- **Partial-block checksums cover the zero-extended block.** Two lengths, `0x1C123` and `0x3000`, both agreeing with the zero-extended reading and with no other. Corroborated by the payload tail being all zeros across a region where the two files differ elsewhere.
- **The header is 12 bytes**, `APS1` then two little-endian sizes taken from the two files' actual lengths.
- **Records are always 65544 bytes**, never short. A 12288-byte pair produces a 65556-byte patch: one whole record for a file a fifth of a block long.
- **Identical blocks emit no record**, which is what makes the emission loop's inclusive upper bound harmless.
- **Growth and shrinkage carry through unchanged.** A 100000-to-200000 pair and a 200000-to-100000 pair, both misaligned, were put to the tool. slap's own patch for each is byte-identical to A-Ptch's — same header sizes, same records, same checksums — and slap applies A-Ptch's patch for each to the exact target. The blocks that fall past the shorter file's end are checksummed zero-extended, the same as any other partial block, confirming the length-blind reading rather than only inferring it.

## What is still open

- **Overlapping records.** The format admits two records naming one offset, though only a hand-built patch could hold one: the maker walks distinct aligned blocks and never produces an overlap. Its apply path would still have to do something with one. Feeding it such a patch through the `Apply` mode would settle whether records compose or the last one wins — a curiosity about malformed input rather than a real ambiguity of the format.

## Notes

Runs take under a minute; the driver waits on the patcher and then polls for a result dialog, so give it time rather than assuming it has hung. If the control tree comes back empty, the application has usually failed to start at all, which almost always means the Visual Basic runtime is missing from the prefix.
