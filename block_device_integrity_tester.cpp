/* Block Device Integrity Tester
 * (c) copyright 2015 TJ <hacker@iam.tj>
 * Licensed on the terms of the GMU General Public License version 3
 *
 * Useful for validating whether suspect flash storage devices are lying
 * about their true storage capacity, as is common with cheap and
 * counterfeit devices.
 *
 */
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

using namespace std;

struct _version
{
  unsigned major;
  unsigned minor;
};

struct Performance {
  unsigned long bytes_qty_claimed;
  unsigned long blocks_qty_claimed;
  unsigned long blocks_qty_actual;
  unsigned block_size;
  unsigned error_count;
  double elapsed_total;
  double elapsed_min;
  double elapsed_max;
  double elapsed_avg;

  Performance() : bytes_qty_claimed(0), blocks_qty_claimed(0), blocks_qty_actual(0), block_size(512), error_count(0),
                   elapsed_total(0), elapsed_min(0), elapsed_max(0), elapsed_avg(0) {}
};

const struct _version version = { 0, 8};
const string urandom("/dev/urandom");

const unsigned MB = 1000000;
const unsigned MiB = 1048576;
const unsigned GB = MB * 1000;
const unsigned GiB = MiB * 1024;
const unsigned block_qty = 2 * 1024 * 128;
const unsigned nanosecond = 1000000000;
const char *format_BLK = "BLK %012ld";
const char *switch_verbose = "--verbose";
const char *switch_limit = "--limit";
const char *switch_skip = "--skip";

bool verbose = false;

void usage(char *prog_name)
{
  cerr << endl << "Usage: " << prog_name << " [" << switch_verbose << "] [" << switch_limit << " blocks] DEVICE_NAME" << endl
       << switch_verbose << ": be very verbose" << endl
       << switch_limit << " blocks: maximum number of blocks (sectors) to test" << endl
       << switch_skip << ": program test option which only writes to the last 128MiB" << endl
       << "DEVICE_NAME: name of block device (or file) to write to. e.g. /dev/sdh" << endl;
}

/* Kludge to obtain access to an fstream's underlying file-descriptor
 * in order to call fsync/fdatasync to flush data to physical media
 * @param filebuf reference to a stream's filebuf object
 * @returns integer file descriptor
 */
int get_file_descriptor(filebuf& filebuf) {
  class hacked_filebuf : public filebuf {
    public:
    int handle() { return _M_file.fd(); }
  };

  return static_cast<hacked_filebuf&>(filebuf).handle();
}

/* Calculate the difference between two nanosecond-accurate timespec structures
 * @param x      ending time
 * @param y      starting time
 * @param result difference
 * @returns 1 if the difference is negative, otherwise 0.
 */
int
timespec_difference(struct timespec *result, struct timespec *x, struct timespec *y)
{
  // Perform the carry for the later subtraction by updating y
  if (x->tv_nsec < y->tv_nsec) {
    int nsec = (y->tv_nsec - x->tv_nsec) / nanosecond + 1;
    y->tv_nsec -= nanosecond * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_nsec - y->tv_nsec > nanosecond) {
    int nsec = (x->tv_nsec - y->tv_nsec) / nanosecond;
    y->tv_nsec += nanosecond * nsec;
    y->tv_sec -= nsec;
  }

  // Compute the difference. tv_nsec is certainly positive
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  // Return 1 if result is negative
  return x->tv_sec < y->tv_sec;
}

int
main(int argc, char **argv, char **env)
{
  int result = 0;
  bool skip = false;
  char *target_file = nullptr;
  double elapsed_last, elapsed_diff;
  fstream target;
  ifstream urand;
  Performance pf_write, pf_read;

  cerr << "Block Device Integrity Tester" << endl
       << "Version " << version.major << "." << version.minor << endl
       << "Copyright 2015 TJ <hacker@iam.tj>" << endl
       << "Licensed on the terms of the GNU General Public License version 3" << endl;
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  unsigned long block_count_limit = 0L;
  // parse the command line
  for (unsigned i = 1; i < argc; ++i) {
    if (strncmp(argv[i], switch_verbose, 3) == 0) {
      verbose = true;
    }
    else if (strncmp(argv[i], switch_limit, 3) == 0) {
      ++i;
      istringstream ss(argv[i]);
      if (ss >> block_count_limit) {
       if (block_count_limit)
        cerr << "Limiting test to " << block_count_limit << " blocks" << endl;
      }
    }
    else if (strncmp(argv[i], switch_skip, 3) == 0) {
      cerr << "Skipping to end of device" << endl;
      skip = true;
    }
    else {
      target_file = argv[i];
      // discover claimed size by seeking to end of device
      target.open(target_file, ios_base::in);
      if (target.good()) {
        target.seekg(0, target.end);
        if (!target.good()) {
          cerr << "Error seeking to end of " << target_file << endl;
          target.close();
          return 2;
        }
        pf_read.bytes_qty_claimed = pf_write.bytes_qty_claimed = target.tellg();
        pf_read.blocks_qty_claimed = pf_write.blocks_qty_claimed = pf_write.bytes_qty_claimed / pf_write.block_size;
        target.close();
      }
      else {
        cerr << "Error opening " << target_file << " for reading" << endl;
      }

      target.open(target_file, ios_base::out);
      if (!target.good()) {
        cerr << "Error opening " << target_file << " for writing" << endl;
        return 2;
      }
      cerr << "Target '" << target_file << "' reports size " << pf_write.blocks_qty_claimed << " blocks, " 
           << (double)pf_write.bytes_qty_claimed / GiB << " GiB, " 
           << (double)pf_write.bytes_qty_claimed / GB << " GB" << endl;
    }
  }

  urand.open(urandom);
  if (!urand.good()) {
    cerr << "Error opening " << urandom << " for reading" << endl;
    target.close();
    return 3;
  }
  cerr << "Source " << urandom << endl;

  unsigned buffer_size = block_qty * pf_write.block_size;
  char* buffer = new char[buffer_size];
  if (!buffer) {
    cerr << "Error allocating memory buffer of " << buffer_size << " bytes" << endl;
    urand.close();
    target.close();
    return 4;
  }
  cerr << "Preparing data buffer with " << block_qty << " blocks of " << pf_write.block_size << " bytes from Source" << endl;

  urand.read(buffer, buffer_size);
  if (!urand.good()) {
    cerr << "Error whilst filling data buffer" << endl;
    urand.close();
    target.close();
    delete[] buffer;
    buffer = nullptr;
    return 5;
  }
  cerr << "Data buffer ready" << endl;
  urand.close();

  // repeatedly write data to target until an error occurs
  if (skip) {
    pf_write.blocks_qty_actual = pf_write.blocks_qty_claimed - block_qty;
    target.seekp(pf_write.blocks_qty_actual * pf_write.block_size);
  }
  timespec timespec_start, timespec_now, timespec_elapsed;
  clock_gettime(CLOCK_MONOTONIC, &timespec_start);

  while (target.good()) {
    if (verbose) cerr << "Writing to block " << pf_write.blocks_qty_actual;
    unsigned blocks_to_write = 0;
    // switch from buffer-size to 1-block writes after reaching reported target device size
    for (unsigned i = 0;
         pf_write.blocks_qty_actual < pf_write.blocks_qty_claimed && i < block_qty ||
           pf_write.blocks_qty_actual >= pf_write.blocks_qty_claimed && i < 1;
         ++i) {
      // insert a unique sequential human-readable (ASCII) number at the start of each block
      snprintf( &buffer[pf_write.block_size * i], 17, format_BLK, pf_write.blocks_qty_actual++);
      ++blocks_to_write;
      if ((block_count_limit && block_count_limit == pf_write.blocks_qty_actual) 
          || i > 0 && pf_write.blocks_qty_actual == pf_write.blocks_qty_claimed)
        break; // from inner for() loop
    }
  
    target.write(buffer, pf_write.block_size * blocks_to_write);
    target.flush();
    
    // contrived way to force OS to sync data to physical media when using C++ streams
    fsync(get_file_descriptor(*target.rdbuf()));

    // track elapsed time in order to calculate device's sustained sequential write speed
    clock_gettime(CLOCK_MONOTONIC, &timespec_now);
    timespec_difference(&timespec_elapsed, &timespec_now, &timespec_start);
    elapsed_last = pf_write.elapsed_total;
    pf_write.elapsed_total = timespec_elapsed.tv_sec  + ((double)timespec_elapsed.tv_nsec / nanosecond);
    elapsed_diff = pf_write.elapsed_total - elapsed_last;
    pf_write.elapsed_min = elapsed_diff < pf_write.elapsed_min ? elapsed_diff : pf_write.elapsed_min;
    pf_write.elapsed_max = elapsed_diff > pf_write.elapsed_max ? elapsed_diff : pf_write.elapsed_max;

    if (!target.good()) {
      ++pf_write.error_count;
      if (pf_write.blocks_qty_actual > pf_write.blocks_qty_claimed)
        --pf_write.blocks_qty_actual;
      cerr << endl << (pf_write.blocks_qty_actual == pf_write.blocks_qty_claimed ? "Device end? " : "Error? ") 
           << (target.eof()  ? " End of file reported"     : "")
           << (target.fail() ? " Fail (Logical I/O error)" : "")
           << (target.bad()  ? " Bad (Write I/O error)"    : "")
           << endl;
    }

    if (verbose) {
      cerr << " " << pf_write.block_size * pf_write.blocks_qty_actual / MiB << "MiB ";
      cerr << "(" << pf_write.block_size * pf_write.blocks_qty_actual / MiB / pf_write.elapsed_total << " MiB/s), ";
      cerr << pf_write.block_size * pf_write.blocks_qty_actual / MB << "MB ";
      cerr << "(" << pf_write.block_size * pf_write.blocks_qty_actual / MB / pf_write.elapsed_total << " MB/s)" << endl;
    }

    if ((block_count_limit && block_count_limit == pf_write.blocks_qty_actual) || pf_write.error_count)
      break; // from outer while() loop
  }

  cerr << "Wrote " << pf_write.blocks_qty_actual << " blocks of " << pf_write.block_size << " bytes (";
  cerr << pf_write.block_size * pf_write.blocks_qty_actual / MiB << " MiB, ";
  cerr << pf_write.block_size * pf_write.blocks_qty_actual / MB << " MB) in " << pf_write.elapsed_total << " seconds (";
  cerr << pf_write.block_size * pf_write.blocks_qty_actual / MiB / pf_write.elapsed_total << " MiB/s, ";
  cerr << pf_write.block_size * pf_write.blocks_qty_actual / MB / pf_write.elapsed_total << " MB/s)" << endl;

  if (!block_count_limit && pf_write.blocks_qty_actual != pf_write.blocks_qty_claimed) {
    cerr << "Error: target size: " << pf_write.bytes_qty_claimed 
         << ", wrote " << pf_write.block_size * pf_write.blocks_qty_actual << " bytes" << endl;
  }

  target.close();

  // now read back every block
  target.open(target_file, ios_base::in);
  if (!target.good()) {
    cerr << "Error opening " << target_file << " for reading" << endl;
    delete[] buffer;
    buffer = nullptr;
    return 6;
  }
  cerr << "Reading back from " << target_file << endl;
  
  // repeatedly read data from target until an error occurs
  char expected[17];
  if (skip) {
    pf_read.blocks_qty_actual = pf_read.blocks_qty_claimed - block_qty;
    target.seekg(pf_read.blocks_qty_actual * pf_read.block_size);
  }
  clock_gettime(CLOCK_MONOTONIC, &timespec_start);

  while (target.good()) {
     if(verbose) 
       cerr << "Reading " << buffer_size / MiB << "MiB, " 
            << buffer_size / MB << "MB, starting at block " << pf_read.blocks_qty_actual << endl;
    target.read(buffer, buffer_size);
    for (unsigned i = 0; i < block_qty; ++i) {
      // insert the expected unique sequential human-readable (ASCII) number into temporary buffer
      snprintf(expected, 17, format_BLK, pf_read.blocks_qty_actual);

      if (memcmp(expected, &buffer[pf_read.block_size * i], 16) != 0) {
        cerr << "Error at block " << pf_read.blocks_qty_actual << endl;
        cerr << "Expected '" << setw(16) << expected << "'" << endl;
        cerr << "Found    '" << setw(16) << &buffer[pf_read.block_size * i] << "'" << endl;

        ++pf_read.error_count;
        if (pf_read.error_count > 8) {
          cerr << "Error count exceeded; terminating. The target media does not contain the same data that was written" << endl;
          target.close();
          delete[] buffer;
          return 7;
        }
      } // memcmp()

      ++pf_read.blocks_qty_actual;
      if (pf_read.blocks_qty_actual == block_count_limit || pf_read.blocks_qty_actual == pf_read.blocks_qty_claimed)
        break; // from inner for() loop
    } // for()

    // track elapsed time in order to calculate device's sustained sequential read speed
    clock_gettime(CLOCK_MONOTONIC, &timespec_now);
    timespec_difference(&timespec_elapsed, &timespec_now, &timespec_start);
    elapsed_last = pf_read.elapsed_total;
    pf_read.elapsed_total = timespec_elapsed.tv_sec  + ((double)timespec_elapsed.tv_nsec / nanosecond);
    elapsed_diff = pf_read.elapsed_total - elapsed_last;
    pf_read.elapsed_min = elapsed_diff < pf_read.elapsed_min ? elapsed_diff : pf_read.elapsed_min;
    pf_read.elapsed_max = elapsed_diff > pf_read.elapsed_max ? elapsed_diff : pf_read.elapsed_max;

    if (verbose) {
      cerr << " " << pf_read.block_size * pf_read.blocks_qty_actual / MiB << "MiB ";
      cerr << "(" << pf_read.block_size * pf_read.blocks_qty_actual / MiB / pf_read.elapsed_total << " MiB/s), ";
      cerr << pf_read.block_size * pf_read.blocks_qty_actual / MB << "MB ";
      cerr << "(" << pf_read.block_size * pf_read.blocks_qty_actual / MB / pf_read.elapsed_total << " MB/s)" << endl;
    }

    if (pf_read.blocks_qty_actual == block_count_limit || pf_read.blocks_qty_actual == pf_read.blocks_qty_claimed)
      break; // from outer while() loop
  } // while()

  cerr << "Read " << pf_read.blocks_qty_actual << " blocks of " << pf_read.block_size << " bytes (";
  cerr << pf_read.block_size * pf_read.blocks_qty_actual / MiB << " MiB, ";
  cerr << pf_read.block_size * pf_read.blocks_qty_actual / MB << " MB) in " << pf_read.elapsed_total << " seconds (";
  cerr << pf_read.block_size * pf_read.blocks_qty_actual / MiB / pf_read.elapsed_total << " MiB/s, ";
  cerr << pf_read.block_size * pf_read.blocks_qty_actual / MB / pf_read.elapsed_total << " MB/s)" << endl;

  cerr << (pf_read.error_count == 0 ? "Good News: no errors" : "Bad News: there were errors") << endl;

  if (!block_count_limit && pf_read.blocks_qty_actual != pf_read.blocks_qty_claimed) {
    cerr << "Error: target size: " << pf_read.bytes_qty_claimed 
         << ", read " << pf_read.block_size * pf_read.blocks_qty_actual << "bytes" << endl;
  }
  target.close();

  if (buffer) {
    delete[] buffer;
    buffer = nullptr;
  }

  return result;
}

// EOF
