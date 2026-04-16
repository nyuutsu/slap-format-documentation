# [anosh](https://www.anosh.se/)

<div>

2023-08-14

# IPS format

------------------------------------------------------------------------

</div>

## Intro

This article intends to clarify the technical limitations of the
*International Patching System* (hereinafter IPS format). The IPS format
is an old<sup><a href="#foot1" id="ref1">[1]</a></sup> and widely used
binary-patching format. Older specifications on the web contain both
errors and ambiguous use of prefixes.

## Definitions

------------------------------------------------------------------------

Definition of prefixes used in this article:

### Binary prefixes

This article will mainly use the <span class="abbr"
title="International Electrotechnical
Commission">IEC</span>-prefixes (kibi and mebi)
<sup><a href="#foot2" id="ref2">[2]</a></sup>.

- 1 kibibyte (<span class="abbr">KiB</span>) = 2<sup>10</sup> bytes
- 1 mebibyte (<span class="abbr">MiB</span>) = 2<sup>20</sup> bytes

### SI prefixes

Other commonly used prefixes are <span class="abbr"
title="Le Système International d'Unités">SI</span>-prefixes in
combination with bits or bytes:

#### Bytes

| Amount | Unit     | Bytes          |
|--------|----------|----------------|
| 1      | kilobyte | 10<sup>3</sup> |
| 1      | megabit  | 10<sup>6</sup> |

#### Bits

| Amount | Unit    | Bits           |
|--------|---------|----------------|
| 1      | kilobit | 10<sup>3</sup> |
| 1      | megabit | 10<sup>6</sup> |

<div class="section">

## IPS-format

- A IPS file contains a variable number of records ("patches").
- Records can only overwrite data at given offsets.
- The format contains no checksums.

------------------------------------------------------------------------


    Type        Size (bytes)        Comments
    --------------------------------------------------------------------
    Header      5                   'PATCH' (not NULL-terminated)
    Record      5
    Footer      3                   'EOF'   (not NULL-terminated)
    --------------------------------------------------------------------

</div>

<div class="section">

### Record


    Type        Size (bytes)
    -------------------------
    Offset      3
    Size        2
    Data        size
    -------------------------

Source: *ZeroSoft* <sup><a href="#foot3" id="ref3">[3]</a></sup>

</div>

### RLE record

------------------------------------------------------------------------


    Type        Size (bytes)        Comments
    --------------------------------------------------------------------
    Offset      3
    Size        2                   Set to zero
    RLE size    2                   Any nonzero value
    Value       1                   Value to be written 'RLE size'-times
    --------------------------------------------------------------------

Source: *ZeroSoft* <sup><a href="#foot3" id="ref4">[3]</a></sup>

## Max size for record

------------------------------------------------------------------------

The <span class="abbr" title="Run-length encoding">RLE</span>-format
allows for 65535 bytes as maximum size for data in an single record.

| Amount | Unit      | Maths                          |
|--------|-----------|--------------------------------|
| 65535  | bytes     |                                |
| 63.999 | kibibytes | 65535 ÷ 2<sup>10</sup>         |
| 65.535 | kilobytes | 65535 ÷ 10<sup>3</sup>         |
| 524.28 | kilobits  | ( 65535 × 8 ) ÷ 10<sup>3</sup> |

Max size of data in record

## Max patchable file size

------------------------------------------------------------------------

The maximum file size that can be patched is defined as:

    MAX_OFFSET + MAX_RECORD_SIZE - 1

|  | Hex | Decimal | Maths |
|----|----|----|----|
| Max offset | 0xFFFFFF | 16777215 | 2<sup>24</sup> − 1 |
| Max record size (bytes) | 0xFFFF | 65535 | 2<sup>16</sup> − 1 |
| Max patchable offset | 0x100FFFD | 16842749 | ( 2<sup>24</sup> − 1 ) + ( 2<sup>16</sup> − 1 ) − 1 |

Upper limits

### Max writeable offset

Using a record (RLE or normal does not matter) lets us write 65535 bytes
(65534 bytes past the offset). Thus we are limited to patching files of
16842749 bytes.

<figure>
<pre><code>
00000000  50 41 54 43 48 ff ff ff  00 00 ff ff aa 45 4f 46  |PATCH........EOF|
00000010</code></pre>
<figcaption>IPS-file with RLE-record writing past max offset<sup><a
href="#foot4" id="ref5">[4]</a></sup></figcaption>
</figure>

### Summary max patchable file-size

| Amount    | Unit      | Maths                             |
|-----------|-----------|-----------------------------------|
| 16842749  | bytes     |                                   |
| 16447.997 | kibibytes | 16842749 ÷ 2<sup>10</sup>         |
| 16.062    | mebibytes | 16842749 ÷ 2<sup>20</sup>         |
| 16.843    | megabytes | 16842749 ÷ 10<sup>6</sup>         |
| 134.742   | megabits  | ( 16842749 × 8 ) ÷ 10<sup>6</sup> |

Max patchable file-size

## Max IPS file-size

Regular records are 5 bytes in size and RLE records are 8 bytes. So
theoretically a valid IPS file with no overlapping offsets can contain (
2<sup>24</sup> − 1 )
RLE-records.<sup><a href="#foot5" id="ref6">[5]</a></sup>. All but one
having the `RLE_SIZE` of 1 (nonsensical but nonetheless valid according
to the [spec](#rle-spec)).

The max size is thus defined as:

    ( 224 − 1 ) × RLE_RECORD_SIZE + HEADER_SIZE + EOF_SIZE

That is: 16777215 × 8 + 5 + 3 = 134217728 bytes (128 mebibytes).

## Minimum IPS file-size

The minimum valid IPS file size is 5 bytes: (`HEADER` + `EOF`). That is,
an IPS file containing zero records.

## Known bugs

The technical maximum number of offsets in a patch is
2<sup>24</sup> - 1. However since the offset value
<span class="mark">0x454F46</span> corresponds to the ASCII-value `EOF`
many patchers stop parsing records when encountering this value.

- Max number of offsets in patch: 2<sup>24</sup> - 1
- Max safe number of offsets in patch: 2<sup>24</sup> - 2

<div class="section">

## Summary

- IPS files cannot affect data past ~16 mebibytes.
- Data cannot be inserted, only overwritten.
- IPS files can contain up to 65535 records (assuming no overlap).
- IPS files are between 5 bytes and 80 mebibytes (with a theoretical
  upper limit of 128 mebibytes).

</div>

## References

------------------------------------------------------------------------

- <span id="foot1">[^](#ref1) Origin unknown. Believed to have been
  created in the early 1990's.</span>
- <span id="foot2">[^](#ref2)
  <a href="https://physics.nist.gov/cuu/Units/binary.html"
  target="_blank">Prefixes for binary multiples</a>, <span class="abbr"
  title="National Institute of Standards and
          Technology">NIST</span> (1998)</span>
- <span id="foot3">[^](#ref3)
  <a href="https://zerosoft.zophar.net/ips.php" target="_blank">IPS File
  Format</a>, Z.e.r.o (2002)</span>
- <span id="foot4">[^](#ref5) Output from `hexdump -C`. Data is stored
  in big-endian format.</span>
- <span id="foot5">[^](#ref6) The spec allows for overlapping
  offsets.</span>
