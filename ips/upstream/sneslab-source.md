
IPS file format
From SnesLab

This article is about the file format of IPS patches. Note that the address and length are stored in big endian.
Contents
Main format

    PATCH ASCII string, the header of the file
    (Records) Any number of records and/or RLE records
    EOF ASCII string, the footer of the file
    (Truncate) Truncate data is stored after the EOF

Record format

    Address (3 bytes) The start address of the data to modify
    Length (2 bytes) The length of the following data (greater than zero)
    Data ("Length" bytes) The data to be written to "Address"

RLE record format

An RLE record writes a single byte multiple times.

    Offset (3 bytes) The start address of the data to modify
    0000 (2 bytes) Two 00 bytes
    Length (2 bytes) Number of times to repeat the following byte
    Byte (1 byte) The byte to be written "Length" times to "Address"

Truncate

A later version of SNESTool and some other IPS patchers (For example: Lunar IPS, NINJA) support a file truncate feature. The feature is an extension to the IPS format in the form of a 3-byte offset stored after the EOF. IPS patchers that support this will truncate the patched file, removing all data after the offset. 