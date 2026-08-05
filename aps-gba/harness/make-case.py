"""Build a source/target pair for putting a question to the original APS-GBA patcher.

Usage:  python3 make-case.py <outputDirectory> <caseName> <fileLength> [seed]

Writes <caseName>-source.gba and <caseName>-target.gba into the output directory.
Lengths are accepted in any Python integer literal form, so 0x1C123 and 114979 both work;
a length that is not a multiple of 65536 is the interesting case.

Differences are scattered at a regular stride through every block,
including into the region of the final block that lies past end of file.
That placement is deliberate:
it makes the three candidate readings of a partial block's checksum produce three different answers,
so the patcher's output distinguishes them.
"""

import os
import random
import sys

BLOCK_LENGTH = 0x10000

DIFFERENCE_STRIDE = 0x2000
DIFFERENCE_RUN_LENGTH = 0x10
FIRST_DIFFERENCE_POSITION = 0x40


def buildPair(fileLength, seed):
    random.seed(seed)
    sourceBytes = bytearray(random.randbytes(fileLength))
    targetBytes = bytearray(sourceBytes)

    position = FIRST_DIFFERENCE_POSITION
    while position + DIFFERENCE_RUN_LENGTH < fileLength:
        targetBytes[position:position + DIFFERENCE_RUN_LENGTH] = random.randbytes(DIFFERENCE_RUN_LENGTH)
        position += DIFFERENCE_STRIDE

    return bytes(sourceBytes), bytes(targetBytes)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1

    outputDirectory = sys.argv[1]
    caseName = sys.argv[2]
    fileLength = int(sys.argv[3], 0)
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 1

    sourceBytes, targetBytes = buildPair(fileLength, seed)

    os.makedirs(outputDirectory, exist_ok=True)
    with open(os.path.join(outputDirectory, caseName + '-source.gba'), 'wb') as sourceFile:
        sourceFile.write(sourceBytes)
    with open(os.path.join(outputDirectory, caseName + '-target.gba'), 'wb') as targetFile:
        targetFile.write(targetBytes)

    wholeBlocks, trailingBytes = divmod(fileLength, BLOCK_LENGTH)
    print('%s: length 0x%X (%d) = %d whole blocks + 0x%X trailing bytes'
          % (caseName, fileLength, fileLength, wholeBlocks, trailingBytes))
    if trailingBytes == 0:
        print('  note: this length is a whole number of blocks, so no partial block exists')
    return 0


if __name__ == '__main__':
    sys.exit(main())
