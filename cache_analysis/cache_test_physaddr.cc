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
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <vector>

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

bool g_randomise;

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

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                     int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

class Perf {
  int fd_;

 public:
  Perf() {
    struct perf_event_attr pe = {};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CACHE_MISSES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    fd_ = perf_event_open(&pe, 0, -1, -1, 0);
    assert(fd_ >= 0);
  }

  void start() {
    int rc = ioctl(fd_, PERF_EVENT_IOC_RESET, 0);
    assert(rc == 0);
    rc = ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0);
    assert(rc == 0);
  }

  int stop() {
    int rc = ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
    assert(rc == 0);
    long long count;
    int got = read(fd_, &count, sizeof(count));
    assert(got == sizeof(count));
    return count;
  }
};

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
  // cache slice number which appears to be the XOR of h1 and h2, with
  // one further change: Bit 32 appears to be included too.  (This is
  // despite the fact that the paper says "It turned out that only the
  // bits 31 to 17 are considered as input values".)
  //
  // This is based on testing on a Thinkpad X220 with low memory
  // pressure.

  static const int bits[] = { 17, 18, 20, 22, 24, 25, 26, 27, 28, 30, 32 };

  int count = sizeof(bits) / sizeof(bits[0]);
  int hash = 0;
  for (int i = 0; i < count; i++) {
    hash ^= (phys_addr >> bits[i]) & 1;
  }
  return hash;
}

uint32_t get_cache_set(uint64_t phys) {
  // For Sandy Bridge, the bottom 17 bits determine the cache set
  // within the cache slice (or the location within a cache line).
  int bits = 17 - 6;
  uint32_t bottom_part = (phys >> 6) & ((1 << bits) - 1);
  return bottom_part | (get_cache_slice(phys) << bits);
}

class AddrFinder {
  static const size_t size = 16 << 20;
  uintptr_t buf_;
  typedef std::vector<uintptr_t> PageList;
  PageList pages_;

 public:
  AddrFinder() {
    buf_ = (uintptr_t) mmap(NULL, size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    assert(buf_ != (uintptr_t) MAP_FAILED);
    for (uintptr_t ptr = buf_; ptr < buf_ + size; ptr += page_size)
      pages_.push_back(ptr);
    if (g_randomise)
      std::random_shuffle(pages_.begin(), pages_.end());
  }

  ~AddrFinder() {
    int rc = munmap((void *) buf_, size);
    assert(rc == 0);
  }

  // Pick a set of addresses which we think belong to the same cache set.
  void get_set(uintptr_t *addrs, int addr_count) {
    PageList::iterator next = pages_.begin();
    addrs[0] = *next++;
    uint32_t cache_set = get_cache_set(get_physical_addr(addrs[0]));

    int found = 1;
    while (found < addr_count) {
      assert(next != pages_.end());
      uintptr_t addr = *next++;

      if (get_cache_set(get_physical_addr(addr)) == cache_set) {
        addrs[found++] = addr;
      }
    }
  }
};

int timing(int addr_count) {
  AddrFinder finder;
  uintptr_t addrs[addr_count];
  finder.get_set(addrs, addr_count);

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

  return median_time;
}

int timing_mean(int addr_count) {
  int runs = 10;
  int sum_time = 0;
  for (int i = 0; i < runs; i++)
    sum_time += timing(addr_count);
  return sum_time / runs;
}

void access_time_graph() {
  // For a 12-way cache, we want to pick 13 addresses belonging to the
  // same cache set.  Measure the effect of picking more addresses to
  // test whether get_cache_set() is correctly determining whether
  // addresses belong to the same cache set.
  int max_addr_count = 13 * 4;

  printf("Address count,Time (ns) for randomise=false"
         ",Time (ns) for randomise=true\n");

  for (int addr_count = 0; addr_count < max_addr_count; addr_count++) {
    g_randomise = false;
    int t0 = timing_mean(addr_count);
    g_randomise = true;
    int t1 = timing_mean(addr_count);
    printf("%i,%i,%i\n", addr_count, t0, t1);
  }
}

void miss_table() {
  int addr_count = 13;
  AddrFinder finder;
  uintptr_t addrs[addr_count];
  finder.get_set(addrs, addr_count);

  Perf perf;

  // Test memory accesses.
  const int runs = 20;
  int misses[runs][addr_count];
  for (int run = 0; run < runs; run++) {
    if (run == runs / 2) {
      // Pause half way to see the effects of memory pressure from
      // other processes.
      sleep(1);
    }
    for (int i = 0; i < addr_count; i++) {
      perf.start();
      g_dummy += *(volatile int *) addrs[i];
      mfence();
      misses[run][i] = perf.stop();
    }
  }

  // Print table of misses.
  for (int run = 0; run < runs; run++) {
    if (run == runs / 2) {
      printf("After pause:\n");
    }
    int miss_count = 0;
    for (int i = 0; i < addr_count; i++) {
      int count = misses[run][i];
      printf("%i", std::min(count, 9));
      miss_count += count;
    }
    printf("  (total: %i)\n", miss_count);
  }
  printf("\n");
}

} // namespace

int main(int argc, char **argv) {
  init_pagemap();

  // Turn off stdout caching.
  setvbuf(stdout, NULL, _IONBF, 0);

  if (argc == 2 && strcmp(argv[1], "access_time_graph") == 0) {
    access_time_graph();
  } else if (argc == 2 && strcmp(argv[1], "miss_table") == 0) {
    for (;;)
      miss_table();
  } else {
    printf("Usage: %s [access_time_graph | miss_table]\n", argv[0]);
    return 1;
  }

  return 0;
}
