// Copyright 2015, Google, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include <string>
#include <vector>


namespace {

// Time between refresh bursts, in nanoseconds.
const int kRefreshInterval = 64e-3 / 8192 * 1e9;

const double pi = 3.14159265;

void clflush(void *addr) {
  asm volatile("clflush (%0)" : : "r" (addr));
}

// Gather timing data by repeatedly accessing memory.
void gather_times(const char *filename) {
  // Allocate some aligned memory.  mmap() gives us page alignment,
  // which is more than enough.
  int mapping_size = 0x1000;
  void *mapped = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
  assert(mapped != MAP_FAILED);

  // Measure 1000 refresh intervals.
  uint64_t max_time = kRefreshInterval * 1000;

  int max_count = 1000000;
  uint64_t *times = new uint64_t[max_count];
  // Touch the memory to fault in the pages, to avoid unnecessary delays
  // due to page faults later.  (This is more portable than MAP_POPULATE.)
  memset(times, 0, max_count * sizeof(uint64_t));

  struct timespec ts0;
  int rc = clock_gettime(CLOCK_MONOTONIC, &ts0);
  assert(rc == 0);

  int count = 0;
  for (;;) {
    assert(count < max_count);
    *(volatile int *) mapped;
    clflush(mapped);

    // This appears not to be necessary when we're calling a syscall.
    // asm volatile("mfence");

    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(rc == 0);
    uint64_t time_offset =
        (ts.tv_sec - ts0.tv_sec) * 1000000000
        + (ts.tv_nsec - ts0.tv_nsec);
    times[count++] = time_offset;
    if (time_offset >= max_time)
      break;
  }

  // Write out the data to a file.
  FILE *fp = fopen(filename, "w");
  assert(fp);
  uint64_t prev = 0;
  for (int i = 0; i < count; i++) {
    fprintf(fp, "%llu\n", (long long) (times[i] - prev));
    prev = times[i];
  }
  rc = fclose(fp);
  assert(rc == 0);

  // Clean up.
  rc = munmap(mapped, mapping_size);
  assert(rc == 0);
}

void wrap_data_as_js_file(const char *input_filename,
                          const char *output_filename,
                          const char *var_name) {
  FILE *in = fopen(input_filename, "r");
  assert(in);
  FILE *out = fopen(output_filename, "w");
  assert(out);
  fprintf(out, "%s = \"", var_name);
  for (;;) {
    int ch = fgetc(in);
    if (ch == EOF)
      break;
    if (ch == '\n')
      fprintf(out, "\\n");
    else
      fputc(ch, out);
  }
  fprintf(out, "\";\n");
  fclose(in);
  fclose(out);
}

double square(double x) {
  return x * x;
}

struct TimePoint {
  uint32_t time; // Time from start of sampling, in nanoseconds.
  uint32_t taken; // Time taken to access memory, in nanoseconds.
};

void analyse_data(std::string base_filename) {
  uint32_t points_max = 200000;
  TimePoint points[points_max];
  uint32_t points_idx = 0;

  // Read in the data file.
  FILE *in = fopen(base_filename.c_str(), "r");
  assert(in);
  uint64_t total_time = 0;
  while (!feof(in)) {
    assert(points_idx < points_max);
    long long time_taken;
    int rc = fscanf(in, "%lld\n", &time_taken);
    assert(rc == 1);
    points[points_idx].time = total_time;
    points[points_idx].taken = time_taken;
    ++points_idx;
    total_time += time_taken;
  }
  fclose(in);

  // Output the data as a CSV file that is easily graphable using a
  // spreadsheet.
  FILE *out = fopen((base_filename + ".full_graph.csv").c_str(), "w");
  assert(out);
  for (uint32_t i = 0; i < points_idx; ++i) {
    fprintf(out, "%lli,%lli\n",
            (long long) points[i].time,
            (long long) points[i].taken);
  }
  fclose(out);

  // This is like full_graph.csv, but only covers a subset of the time
  // range.  This is even more easily graphable in a spreadsheet.
  out = fopen((base_filename + ".graph.csv").c_str(), "w");
  assert(out);
  unsigned start_ns = kRefreshInterval * 20;
  unsigned end_ns = kRefreshInterval * 40;
  for (uint32_t i = 0; i < points_idx; ++i) {
    if (points[i].time < start_ns)
      continue;
    if (points[i].time >= end_ns)
      break;
    fprintf(out, "%lli,%lli\n",
            (long long) points[i].time - start_ns,
            (long long) points[i].taken);
  }
  fclose(out);

  wrap_data_as_js_file((base_filename + ".graph.csv").c_str(),
                       (base_filename + ".graph.csv.js").c_str(),
                       "graph_data");

  // When a memory access was delayed due to a memory refresh, we
  // expect it to take longer than this time.
  // TODO: Calculate this from the data rather than hard-coding it.
  uint32_t cut_off_ns = 150;

  // Filter out the uninteresting shorter times.  This will leave the
  // refresh delay times plus some noisy times.
  std::vector<TimePoint> longer_times;
  for (uint32_t i = 0; i < points_idx; ++i) {
    if (points[i].taken >= cut_off_ns)
      longer_times.push_back(points[i]);
  }
  printf("Have %i time points, with %i longer than %i ns\n",
         (int) points_idx, (int) longer_times.size(), (int) cut_off_ns);

  // Calculate a Fourier transform of the data.  Note that this is a
  // non-fast Fourier transform.
  out = fopen((base_filename + ".fourier.csv").c_str(), "w");
  assert(out);
  fprintf(out, "Period (ns),Period (multiples of %i ns),"
          "Frequency (multiples of 1/%i ns),"
          "Magnitude,Derivative of magnitude,Scaled magnitude\n",
          kRefreshInterval, kRefreshInterval);
  uint32_t step = 1;
  double mag_prev = 0;
  double mag_deriv_prev = 0;
  for (uint32_t period = kRefreshInterval / 8;
       period < kRefreshInterval * 4;
       period += step) {
    double sum1 = 0;
    double sum2 = 0;
    double angle_multiplier = 2 * pi / period;
    for (uint32_t i = 0; i < longer_times.size(); ++i) {
      double angle = angle_multiplier * longer_times[i].time;
      // Unlike a normal Fourier transform, we don't multiple the
      // sin/cos values by the observed value (the "taken" field).  We
      // don't really care about the magnitude of the delay we saw.
      // In fact, it should be better to ignore the magnitude of the
      // larger delays because these are more likely to be noise.
      sum1 += sin(angle);
      sum2 += cos(angle);
    }
    double period_multiple = (double) period / kRefreshInterval;
    double freq_multiple = kRefreshInterval / (double) period;
    double mag = sqrt(square(sum1) + square(sum2));
    double mag_deriv = (mag - mag_prev) / step;
    double scaled_mag = mag / (total_time / period);
    fprintf(out, "%i,%f,%f,%f,%f,%f\n",
            (int) period,
            period_multiple,
            freq_multiple,
            mag,
            mag_deriv,
            scaled_mag);
    if (mag_deriv_prev > 0 && mag_deriv <= 0 &&
        scaled_mag >= 0.8 && scaled_mag < 1.1) {
      printf("Spike at freq %.1f (%.6f) -> %.2f us -> %.1f ms: "
             "saw %.3f of refreshes\n",
             freq_multiple, freq_multiple,
             period / 1e3,
             period * 8192 / 1e6,
             scaled_mag);
    }
    mag_prev = mag;
    mag_deriv_prev = mag_deriv;
  }
  fclose(out);
}

}

int main(int argc, char **argv) {
  const char *filename = "results";

  bool do_gather = false;
  bool do_analyse = false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--gather") == 0) {
      do_gather = true;
    } else if (strcmp(argv[i], "--analyse") == 0) {
      do_analyse = true;
    } else {
      printf("Unrecognised argument: %s\n", argv[i]);
      printf("Usage: %s [--gather] [--analyse]\n", argv[0]);
      return 1;
    }
  }

  // If no arguments are given, run both.
  if (argc == 1) {
    do_gather = true;
    do_analyse = true;
  }

  if (do_gather)
    gather_times(filename);
  if (do_analyse)
    analyse_data(filename);

  return 0;
}
