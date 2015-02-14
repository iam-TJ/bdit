# bdit - Block Device Integrity Tester

A simple tool to determine the true capacity and read/write speeds of flash devices.

    $ ./bdit
    Block Device Integrity Tester
    Version 0.8
    Copyright 2015 TJ <hacker@iam.tj>
    Licensed on the terms of the GNU General Public License version 3

    Usage: ./bdit [--verbose] [--limit blocks] [--skip] DEVICE_NAME
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

With the decimal number incrementing by 1 for each successive block.

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
    Source '/dev/urandom'
    Preparing data buffer with 262144 blocks of 512 bytes from Source
    Data buffer ready
    Writing to block 62259200 30528MiB 32010MB
    Writing to block 62521344
    Device end?  Fail (Logical I/O error) Bad (Write I/O error)
     30528MiB 32010MB
    Wrote 62521344 blocks of 512 bytes (30528 MiB, 32010 MB, 29.8125 GiB 32.0109 GB) in 12.5639 seconds
    Reading back from /dev/sdb
    Reading 128MiB, 134MB, starting at block 62259200
     30528MiB 32010MB
    Read 62521344 blocks of 512 bytes (30528 MiB, 32010 MB, 29.8125 GiB 32.0109 GB) in 11.1887 seconds
    Good News: no errors

Sample output when limiting testing to the first 512MiB:

    $ ./bdit --verbose --limit 1048576 /dev/sdb 2>&1 | tee /tmp/bdit.log
    Block Device Integrity Tester
    Version 0.8
    Copyright 2015 TJ <hacker@iam.tj>
    Licensed on the terms of the GNU General Public License version 3
    Limiting test to 1048576 blocks
    Target '/dev/sdb' reports size 62521344 blocks, 29.8125 GiB, 32.0109 GB
    Source '/dev/urandom'
    Preparing data buffer with 262144 blocks of 512 bytes from Source
    Data buffer ready
    Writing to block 0 128MiB 134MB (9.58176 MiB/s 10.0309 MB/s)
    Writing to block 262144 256MiB 268MB (9.58654 MiB/s 10.0359 MB/s)
    Writing to block 524288 384MiB 402MB (9.54898 MiB/s 9.99659 MB/s)
    Writing to block 786432 512MiB 536MB (9.54764 MiB/s 9.99518 MB/s)
    Wrote 1048576 blocks of 512 bytes (512 MiB, 536 MB, 0.5 GiB, 0.536871 GB) in 53.6258 seconds (9.54764 MiB/s, 9.99518 MB/s)
    Reading back from /dev/sdb
    Reading 128MiB, 134MB, starting at block 0
     128MiB 134MB (11.497 MiB/s 12.0359 MB/s)
    Reading 128MiB, 134MB, starting at block 262144
     256MiB 268MB (11.53 MiB/s 12.0705 MB/s)
    Reading 128MiB, 134MB, starting at block 524288
     384MiB 402MB (11.5577 MiB/s 12.0994 MB/s)
    Reading 128MiB, 134MB, starting at block 786432
     512MiB 536MB (11.5618 MiB/s 12.1037 MB/s)
    Read 1048576 blocks of 512 bytes (512 MiB, 536 MB, 0.5 GiB, 0.536871 GB) in 44.2839 seconds (11.5618 MiB/s, 12.1037 MB/s)
    Good News: no errors
