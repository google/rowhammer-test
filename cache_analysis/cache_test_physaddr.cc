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
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>

// This program attempts to pick sets of memory locations that map to
// the same L3 cache set.  It tests whether they really do map to the
// same cache set by timing accesses to them and outputting a CSV file
// of times that can be graphed.  This program assumes a 2-core Sandy
// Bridge CPU.


// Dummy variable to attempt to prevent compiler and CPU from skipping
// memory accesses.
int g_dummy;

namespace {

const int page_size = 0x1000;
int g_pagemap_fd = -1;

// Extract the physical page number from a Linux /proc/PID/pagemap entry.
uint64_t frame_number_from_pagemap(uint64_t value) {
  return value & ((1ULL << 54) - 1);
}

void init_pagemap() {
  g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  assert(g_pagemap_fd >= 0);
}

uint64_t get_physical_addr(uintptr_t virtual_addr) {
  uint64_t value;
  off_t offset = (virtual_addr / page_size) * sizeof(value);
  int got = pread(g_pagemap_fd, &value, sizeof(value), offset);
  assert(got == 8);

  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = frame_number_from_pagemap(value);
  return (frame_num * page_size) | (virtual_addr & (page_size - 1));
}

// Execute a CPU memory barrier.  This is an attempt to prevent memory
// accesses from being reordered, in case reordering affects what gets
// evicted from the cache.  It's also an attempt to ensure we're
// measuring the time for a single memory access.
//
// However, this appears to be unnecessary on Sandy Bridge CPUs, since
// we get the same shape graph without this.
inline void mfence() {
  asm volatile("mfence");
}

// Measure the time taken to access the given address, in nanoseconds.
int time_access(uintptr_t ptr) {
  struct timespec ts0;
  int rc = clock_gettime(CLOCK_MONOTONIC, &ts0);
  assert(rc == 0);

  g_dummy += *(volatile int *) ptr;
  mfence();

  struct timespec ts;
  rc = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(rc == 0);
  return (ts.tv_sec - ts0.tv_sec) * 1000000000
         + (ts.tv_nsec - ts0.tv_nsec);
}

// Given a physical memory address, this hashes the address and
// returns the number of the cache slice that the address maps to.
//
// This assumes a 2-core Sandy Bridge CPU.
int get_cache_slice(uint64_t phys_addr) {
  // On a 4-core machine, the CPU's hash function produces a 2-bit
  // cache slice number, where the two bits are defined by "h1" and
  // "h2":
  //
  // h1 function:
  //   static const int bits[] = { 18, 19, 21, 23, 25, 27, 29, 30, 31 };
  // h2 function:
  //   static const int bits[] = { 17, 19, 20, 21, 22, 23, 24, 26, 28, 29, 31 };
  //
  // This hash function is described in the paper "Practical Timing
  // Side Channel Attacks Against Kernel Space ASLR".
  //
  // On a 2-core machine, the CPU's hash function produces a 1-bit
  // cache slice number which appears to be the XOR of h1 and h2.

  // XOR of h1 and h2:
  static const int bits[] = { 17, 18, 20, 22, 24, 25, 26, 27, 28, 30 };

  int count = sizeof(bits) / sizeof(bits[0]);
  int hash = 0;
  for (int i = 0; i < count; i++) {
    hash ^= (phys_addr >> bits[i]) & 1;
  }
  return hash;
}

bool in_same_cache_set(uint64_t phys1, uint64_t phys2) {
  // For Sandy Bridge, the bottom 17 bits determine the cache set
  // within the cache slice (or the location within a cache line).
  uint64_t mask = ((uint64_t) 1 << 17) - 1;
  return ((phys1 & mask) == (phys2 & mask) &&
          get_cache_slice(phys1) == get_cache_slice(phys2));
}

int timing(int addr_count) {
  size_t size = 16 << 20;
  uintptr_t buf =
    (uintptr_t) mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  assert(buf);

  uintptr_t addrs[addr_count];
  addrs[0] = buf;
  uintptr_t phys1 = get_physical_addr(addrs[0]);

  // Pick a set of addresses which we think belong to the same cache set.
  uintptr_t next_addr = buf + page_size;
  uintptr_t end_addr = buf + size;
  int found = 1;
  while (found < addr_count) {
    assert(next_addr < end_addr);
    uintptr_t addr = next_addr;
    next_addr += page_size;

    uint64_t phys2 = get_physical_addr(addr);
    if (in_same_cache_set(phys1, phys2)) {
      addrs[found] = addr;
      found++;
    }
  }

  // Time memory accesses.
  int runs = 10;
  int times[runs];
  for (int run = 0; run < runs; run++) {
    // Ensure the first address is cached by accessing it.
    g_dummy += *(volatile int *) addrs[0];
    mfence();
    // Now pull the other addresses through the cache too.
    for (int i = 1; i < addr_count; i++) {
      g_dummy += *(volatile int *) addrs[i];
    }
    mfence();
    // See whether the first address got evicted from the cache by
    // timing accessing it.
    times[run] = time_access(addrs[0]);
  }
  // Find the median time.  We use the median in order to discard
  // outliers.  We want to discard outlying slow results which are
  // likely to be the result of other activity on the machine.
  //
  // We also want to discard outliers where memory was accessed
  // unusually quickly.  These could be the result of the CPU's
  // eviction policy not using an exact LRU policy.
  std::sort(times, &times[runs]);
  int median_time = times[runs / 2];

  int rc = munmap((void *) buf, size);
  assert(rc == 0);

  return median_time;
}

int timing_mean(int addr_count) {
  int runs = 10;
  int sum_time = 0;
  for (int i = 0; i < runs; i++)
    sum_time += timing(addr_count);
  return sum_time / runs;
}

} // namespace

int main() {
  init_pagemap();

  // Turn off stdout caching.
  setvbuf(stdout, NULL, _IONBF, 0);

  // For a 12-way cache, we want to pick 13 addresses belonging to the
  // same cache set.  Measure the effect of picking more addresses to
  // test whether in_same_cache_set() is correctly determining whether
  // addresses belong to the same cache set.
  int max_addr_count = 13 * 4;

  printf("Address count,Time (ns)\n");

  for (int addr_count = 0; addr_count < max_addr_count; addr_count++) {
    printf("%i,%i\n", addr_count, timing_mean(addr_count));
  }
  return 0;
}
