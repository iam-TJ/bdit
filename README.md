# bdit - Block Device Integrity Tester

A simple tool to a determine the true capacity and read/write speeds of flash devices.

    $ ./bdit
    Block Device Integrity Tester
    Version 0.8
    Copyright 2015 TJ <hacker@iam.tj>
    Licensed on the terms of the GNU General Public License version 3

    Usage: ./bdit [--verbose] [--limit blocks] DEVICE_NAME
    --verbose: be very verbose
    --limit blocks: maximum number of blocks (sectors) to test
    --skip: program test option which only writes to the last 128MiB
    DEVICE_NAME: name of block device (or file) to write to. e.g. /dev/sdh

*All* output is written to stderr so if you want to capture it to file remember to redirect

    $ ./bdit ... 2>&1 | <command>


A 128MiB memory buffer is filled with pseudo-random generated numbers. This buffer is
treated as a collection of contiguous blocks, each block representing a (512 byte) device sector.

Each block has its first 16 bytes written with an ASCII string of the form:

    BLK 000000000000

With the decimal number incrementing by 1 for each new block.

    BLK 000000000001
    BLK 000000000002
    ...

The entire device is filled with this data then it is read back and the
block numbers are checked. If the expected number differs from the number in
the data read from the device an error is reported and the block numbers displayed.

This is intended to help identify the true capacity of a counterfeit or faked capacity
device, e.g., an 8GB device claiming to be 32GB.

During writing and reading the data transfer speed is constantly monitored. In --verbose
mode this information is reported for each 128MiB block.

Capacities and transfer speeds are reported in SI decimal (Mega-byte, MB) and binary (Mibi-byte, MiB) units.

When using the "--skip" testing option the reported transfer speeds will be incorrect.

Sample output in test mode:

    $ ./bdit --verbose --skip /dev/sdb
    Block Device Integrity Tester
    Version 0.8
    Copyright 2015 TJ <hacker@iam.tj>
    Licensed on the terms of the GNU General Public License version 3
    Skipping to end of device
    Target '/dev/sdb' reports size 62521344 blocks, 29.8125 GiB, 32.0109 GB
    Source /dev/urandom
    Preparing data buffer with 262144 blocks of 512 bytes from Source
    Data buffer ready
    Writing to block 62259200 30528MiB (2470.46 MiB/s), 32010MB (2590.39 MB/s)
    Writing to block 62521344
    Device end?  Fail (Logical I/O error) Bad (Write I/O error)
     30528MiB (2470.36 MiB/s), 32010MB (2590.28 MB/s)
    Wrote 62521344 blocks of 512 bytes (30528 MiB, 32010 MB) in 12.3577 seconds (2470.36 MiB/s, 2590.28 MB/s)
    Reading back from /dev/sdb
    Reading 128MiB, 134MB, starting at block 62259200
     30528MiB (2745.71 MiB/s), 32010MB (2879 MB/s)
    Read 62521344 blocks of 512 bytes (30528 MiB, 32010 MB) in 11.1184 seconds (2745.71 MiB/s, 2879 MB/s)
    Good News: no errors

Sample output when limiting testing to the first 512MiB:

    $ ./bdit --verbose --limit 1048576 /dev/sdb 2>&1 | tee /tmp/bdit.log
    Block Device Integrity Tester
    Version 0.8
    Copyright 2015 TJ <hacker@iam.tj>
    Licensed on the terms of the GNU General Public License version 3
    Limiting test to 1048576 blocks
    Target '/dev/sdb' reports size 62521344 blocks, 29.8125 GiB, 32.0109 GB
    Source /dev/urandom
    Preparing data buffer with 262144 blocks of 512 bytes from Source
    Data buffer ready
    Writing to block 0 128MiB (9.77612 MiB/s), 134MB (10.2344 MB/s)
    Writing to block 262144 256MiB (9.77877 MiB/s), 268MB (10.2371 MB/s)
    Writing to block 524288 384MiB (9.73661 MiB/s), 402MB (10.193 MB/s)
    Writing to block 786432 512MiB (9.74801 MiB/s), 536MB (10.2049 MB/s)
    Wrote 1048576 blocks of 512 bytes (512 MiB, 536 MB) in 52.5235 seconds (9.74801 MiB/s, 10.2049 MB/s)
    Reading back from /dev/sdb
    Reading 128MiB, 134MB, starting at block 0
     128MiB (11.5663 MiB/s), 134MB (12.1084 MB/s)
    Reading 128MiB, 134MB, starting at block 262144
     256MiB (11.6023 MiB/s), 268MB (12.1461 MB/s)
    Reading 128MiB, 134MB, starting at block 524288
     384MiB (11.619 MiB/s), 402MB (12.1636 MB/s)
    Reading 128MiB, 134MB, starting at block 786432
     512MiB (11.6317 MiB/s), 536MB (12.1769 MB/s)
    Read 1048576 blocks of 512 bytes (512 MiB, 536 MB) in 44.0177 seconds (11.6317 MiB/s, 12.1769 MB/s)
    Good News: no errors

