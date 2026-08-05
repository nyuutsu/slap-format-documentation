"""Read an APS-GBA patch and say which reading of a partial block its checksums agree with.

Usage:  python3 read-patch.py <caseDirectory> <caseName> [<caseName> ...]

Expects <caseName>-source.gba, <caseName>-target.gba and <caseName>-out.aps to sit in the case directory,
the first two as written by make-case.py and the third as produced by the original patcher.

For every record whose block runs past end of file,
the three candidate readings are computed and compared against what the patcher actually stored:

  zero-pad     real bytes, zero-extended to a whole 65536-byte block
  clamp        only the bytes that exist
  stale-buffer real bytes followed by whatever the previous block left behind

Measurement to date says zero-pad, and the checksum routine in the patcher takes no length argument, so clamp is not something it can express.
The other two rows are kept because a plausible reading of the binary predicted stale-buffer and was wrong;
anyone re-running this deserves to see the alternatives fail rather than take that on trust.
"""

import struct
import sys

BLOCK_LENGTH = 0x10000
HEADER_LENGTH = 12
RECORD_LENGTH = 8 + BLOCK_LENGTH


def crc16CcittFalse(data):
    accumulator = 0xFFFF
    for byte in data:
        accumulator ^= byte << 8
        for _ in range(8):
            if accumulator & 0x8000:
                accumulator = ((accumulator << 1) ^ 0x1021) & 0xFFFF
            else:
                accumulator = (accumulator << 1) & 0xFFFF
    return accumulator


def candidateBlocks(sourceBytes, targetBytes, blockOffset):
    """The three readings of a block, as (label, sourceBlock, targetBlock) triples."""
    realSource = sourceBytes[blockOffset:blockOffset + BLOCK_LENGTH]
    realTarget = targetBytes[blockOffset:blockOffset + BLOCK_LENGTH]
    padLength = BLOCK_LENGTH - len(realSource)

    # With no preceding block there is nothing stale to carry, so that candidate
    # collapses into the zero-extended one and the case cannot tell them apart.
    # A file at least one whole block long is needed to discriminate.
    previousSourceTail = sourceBytes[blockOffset - padLength:blockOffset]
    previousTargetTail = targetBytes[blockOffset - padLength:blockOffset]
    if len(previousSourceTail) < padLength:
        previousSourceTail = b'\x00' * padLength
        previousTargetTail = b'\x00' * padLength

    return [
        ('zero-pad    ', realSource + b'\x00' * padLength, realTarget + b'\x00' * padLength),
        ('clamp       ', realSource, realTarget),
        ('stale-buffer', realSource + previousSourceTail, realTarget + previousTargetTail),
    ]


def readCase(caseDirectory, caseName):
    with open('%s/%s-source.gba' % (caseDirectory, caseName), 'rb') as sourceFile:
        sourceBytes = sourceFile.read()
    with open('%s/%s-target.gba' % (caseDirectory, caseName), 'rb') as targetFile:
        targetBytes = targetFile.read()
    with open('%s/%s-out.aps' % (caseDirectory, caseName), 'rb') as patchFile:
        patchBytes = patchFile.read()

    magic = patchBytes[0:4]
    storedSourceSize, storedTargetSize = struct.unpack_from('<II', patchBytes, 4)
    recordCount, trailingBytes = divmod(len(patchBytes) - HEADER_LENGTH, RECORD_LENGTH)

    print('######## %s ########' % caseName)
    print('  magic %r  header sourceSize=0x%X targetSize=0x%X  actual length 0x%X'
          % (magic, storedSourceSize, storedTargetSize, len(sourceBytes)))
    print('  %d records, %d trailing bytes' % (recordCount, trailingBytes))

    for recordIndex in range(recordCount):
        recordBase = HEADER_LENGTH + recordIndex * RECORD_LENGTH
        blockOffset, storedSourceCrc, storedTargetCrc = struct.unpack_from('<IHH', patchBytes, recordBase)
        xorPayload = patchBytes[recordBase + 8:recordBase + 8 + BLOCK_LENGTH]

        realLength = len(sourceBytes[blockOffset:blockOffset + BLOCK_LENGTH])
        blockIsWhole = realLength == BLOCK_LENGTH

        print('  record[%d] offset=0x%08X realBytes=0x%X %s  stored: source=0x%04X target=0x%04X'
              % (recordIndex, blockOffset, realLength,
                 '(whole)' if blockIsWhole else '(PARTIAL)',
                 storedSourceCrc, storedTargetCrc))

        for label, sourceBlock, targetBlock in candidateBlocks(sourceBytes, targetBytes, blockOffset):
            computedSource = crc16CcittFalse(sourceBlock)
            computedTarget = crc16CcittFalse(targetBlock)
            agrees = computedSource == storedSourceCrc and computedTarget == storedTargetCrc
            print('      %s -> source=0x%04X target=0x%04X   %s'
                  % (label, computedSource, computedTarget, '<== AGREES' if agrees else ''))
            if blockIsWhole:
                break

        if not blockIsWhole:
            payloadTail = xorPayload[realLength:]
            nonzeroCount = sum(1 for byte in payloadTail if byte)
            print('      xor payload past end of file: %d bytes, %d nonzero'
                  % (len(payloadTail), nonzeroCount))
    print()


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    caseDirectory = sys.argv[1]
    for caseName in sys.argv[2:]:
        readCase(caseDirectory, caseName)
    return 0


if __name__ == '__main__':
    sys.exit(main())
