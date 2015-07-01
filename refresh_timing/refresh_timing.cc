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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include <string>


namespace {

// Time between refresh bursts, in nanoseconds.
const int kRefreshInterval = 64e-3 / 8192 * 1e9;

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

struct TimePoint {
  uint32_t time; // Time from start of sampling, in nanoseconds.
  uint32_t taken; // Time taken to access memory, in nanoseconds.
};

void analyse_data(std::string base_filename) {
  uint32_t points_max = 200000;
  struct TimePoint points[points_max];
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
}

}

int main() {
  const char *filename = "results";
  // TODO: Allow these steps to be run individually via subcommands.
  gather_times(filename);
  analyse_data(filename);
  return 0;
}
