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

// This is required on Mac OS X for getting PRI* macros #defined.
#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#if !defined(TEST_MODE)
# define TEST_MODE 0
#endif

const size_t mem_size = 1 << 30;
const int toggles = 540000;

char *g_mem;
void *g_inject_addr1;
void *g_inject_addr2;

uint64_t g_address_sets_tried;
int g_errors_found;

char *pick_addr() {
  size_t offset = (rand() << 12) % mem_size;
  return g_mem + offset;
}

uint64_t get_physical_addr(uintptr_t virtual_addr) {
  int fd = open("/proc/self/pagemap", O_RDONLY);
  assert(fd >= 0);

  int kPageSize = 0x1000;
  off_t pos = lseek(fd, (virtual_addr / kPageSize) * 8, SEEK_SET);
  assert(pos >= 0);
  uint64_t value;
  int got = read(fd, &value, 8);
  assert(got == 8);
  int rc = close(fd);
  assert(rc == 0);

  uint64_t frame_num = value & ((1ULL << 54) - 1);
  return (frame_num * kPageSize) | (virtual_addr & (kPageSize - 1));
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
};

#define ADDR_COUNT 4
#define ITERATIONS 10

struct InnerSet {
  uint32_t *addrs[ADDR_COUNT];
};
struct OuterSet {
  struct InnerSet inner[ITERATIONS];
};

static void reset_mem() {
  memset(g_mem, 0xff, mem_size);
}

static void pick_addrs(struct OuterSet *set) {
  for (int j = 0; j < ITERATIONS; j++) {
    for (int a = 0; a < ADDR_COUNT; a++) {
      set->inner[j].addrs[a] = (uint32_t *) pick_addr();
    }
  }
}

static void row_hammer_inner(struct InnerSet inner) {
  if (TEST_MODE &&
      inner.addrs[0] == g_inject_addr1 &&
      inner.addrs[1] == g_inject_addr2) {
    printf("Test mode: Injecting bit flip...\n");
    g_mem[3] ^= 1;
  }

  uint32_t sum = 0;
  for (int i = 0; i < toggles; i++) {
    for (int a = 0; a < ADDR_COUNT; a++)
      sum += *inner.addrs[a] + 1;
    if (!TEST_MODE) {
      for (int a = 0; a < ADDR_COUNT; a++)
        asm volatile("clflush (%0)" : : "r" (inner.addrs[a]) : "memory");
    }
  }

  // Sanity check.  We don't expect this to fail, because reading
  // these rows refreshes them.
  if (sum != 0) {
    printf("error: sum=%x\n", sum);
    exit(1);
  }
}

static void row_hammer(struct OuterSet *set) {
  Timer timer;
  for (int j = 0; j < ITERATIONS; j++) {
    row_hammer_inner(set->inner[j]);
    g_address_sets_tried++;
  }

  // Print statistics derived from the time and number of accesses.
  double time_taken = timer.get_diff();
  printf("  Took %.1f ms per address set\n",
         time_taken / ITERATIONS * 1e3);
  printf("  Took %g sec in total for %i address sets\n",
         time_taken, ITERATIONS);
  int memory_accesses = ITERATIONS * ADDR_COUNT * toggles;
  printf("  Took %.3f nanosec per memory access (for %i memory accesses)\n",
         time_taken / memory_accesses * 1e9,
         memory_accesses);
  int refresh_period_ms = 64;
  printf("  This gives %i accesses per address per %i ms refresh period\n",
         (int) (refresh_period_ms * 1e-3 * ITERATIONS * toggles / time_taken),
         refresh_period_ms);
}

struct BitFlipInfo {
  uintptr_t victim_virtual_addr;
  int bit_number;
  uint8_t flips_to;  // 1 if this is a 0 -> 1 bit flip, 0 otherwise.
};

static bool check(struct BitFlipInfo *result) {
  uint64_t *end = (uint64_t *) (g_mem + mem_size);
  uint64_t *ptr;
  bool found_error = false;
  for (ptr = (uint64_t *) g_mem; ptr < end; ptr++) {
    uint64_t got = *ptr;
    uint64_t expected = ~(uint64_t) 0;
    if (got != expected) {
      printf("error at %p (phys 0x%" PRIx64 "): got 0x%" PRIx64 "\n",
             ptr, get_physical_addr((uintptr_t) ptr), got);
      found_error = true;
      g_errors_found++;

      if (result) {
        result->victim_virtual_addr = (uintptr_t) ptr;
        result->bit_number = -1;
        for (int bit = 0; bit < 64; bit++) {
          if (((got >> bit) & 1) != ((expected >> bit) && 1)) {
            result->bit_number = bit;
            result->flips_to = (got >> bit) & 1;
          }
        }
        assert(result->bit_number != -1);
      }
    }
  }
  return found_error;
}

bool narrow_to_pair(struct InnerSet *inner) {
  bool found = false;
  for (int idx1 = 0; idx1 < ADDR_COUNT; idx1++) {
    for (int idx2 = idx1 + 1; idx2 < ADDR_COUNT; idx2++) {
      uint32_t *addr1 = inner->addrs[idx1];
      uint32_t *addr2 = inner->addrs[idx2];
      struct InnerSet new_set;
      // This is slightly hacky: We reuse row_hammer_inner(), which
      // always expects to hammer ADDR_COUNT addresses.  Rather than
      // making another version that takes a pair of addresses, we
      // just pass our 2 addresses to row_hammer_inner() multiple
      // times.
      for (int a = 0; a < ADDR_COUNT; a++) {
        new_set.addrs[a] = a % 2 == 0 ? addr1 : addr2;
      }
      printf("Trying pair: 0x%" PRIx64 ", 0x%" PRIx64 "\n",
             get_physical_addr((uintptr_t) addr1),
             get_physical_addr((uintptr_t) addr2));
      reset_mem();
      row_hammer_inner(new_set);
      struct BitFlipInfo bit_flip_info;
      if (check(&bit_flip_info)) {
        found = true;
        printf("RESULT PAIR,0x%" PRIx64 ",0x%" PRIx64 ",0x%" PRIx64 ",%i,%i\n",
               get_physical_addr((uintptr_t) addr1),
               get_physical_addr((uintptr_t) addr2),
               get_physical_addr((uintptr_t) bit_flip_info.victim_virtual_addr),
               bit_flip_info.bit_number,
               bit_flip_info.flips_to);
      }
    }
  }
  return found;
}

bool narrow_down(struct OuterSet *outer) {
  bool found = false;
  for (int j = 0; j < ITERATIONS; j++) {
    reset_mem();
    row_hammer_inner(outer->inner[j]);
    if (check(NULL)) {
      printf("hammered addresses:\n");
      struct InnerSet *inner = &outer->inner[j];
      for (int a = 0; a < ADDR_COUNT; a++) {
        printf("  logical=%p, physical=0x%" PRIx64 "\n",
               inner->addrs[a],
               get_physical_addr((uintptr_t) inner->addrs[a]));
      }
      found = true;

      printf("Narrowing down to a specific pair...\n");
      int tries = 0;
      while (!narrow_to_pair(inner)) {
        if (++tries >= 10) {
          printf("Narrowing to pair: Giving up after %i tries\n", tries);
          break;
        }
      }
    }
  }
  return found;
}

void main_prog() {
  printf("RESULT START_TIME,%" PRId64 "\n", time(NULL));

  g_mem = (char *) mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE, -1, 0);
  assert(g_mem != MAP_FAILED);

  printf("Clearing memory...\n");
  reset_mem();

  Timer t;
  int iter = 0;
  for (;;) {
    printf("Iteration %i (after %.2fs)\n", iter++, t.get_diff());
    struct OuterSet addr_set;
    pick_addrs(&addr_set);
    if (TEST_MODE && iter == 3) {
      printf("Test mode: Will inject a bit flip...\n");
      g_inject_addr1 = addr_set.inner[2].addrs[0];
      g_inject_addr2 = addr_set.inner[2].addrs[1];
    }
    row_hammer(&addr_set);

    Timer check_timer;
    bool found_error = check(NULL);
    printf("  Checking for bit flips took %f sec\n", check_timer.get_diff());

    if (iter % 100 == 0 || found_error) {
      // Report general progress stats:
      //  - Time since start, in seconds
      //  - Current Unix time (seconds since epoch)
      //  - Number of address sets tried
      //  - Number of bit flips found (not necessarily unique ones)
      printf("RESULT STAT,%.2f,%" PRId64 ",%" PRId64 ",%i\n",
             t.get_diff(),
             (uint64_t) time(NULL),
             g_address_sets_tried,
             g_errors_found);
    }

    if (found_error) {
      printf("\nNarrowing down to set of %i addresses...\n", ADDR_COUNT);
      int tries = 0;
      while (!narrow_down(&addr_set)) {
        if (++tries >= 10) {
          printf("Narrowing to address set: Giving up after %i tries\n", tries);
          break;
        }
      }

      printf("\nRunning retries...\n");
      for (int i = 0; i < 10; i++) {
        printf("Retry %i\n", i);
        reset_mem();
        row_hammer(&addr_set);
        check(NULL);
      }
      if (TEST_MODE)
        exit(1);
    }
  }
}


int main() {
  // Turn off unwanted buffering for when stdout is a pipe.
  setvbuf(stdout, NULL, _IONBF, 0);

  // Start with an empty line in case previous output was truncated
  // mid-line.
  printf("\n");

  if (TEST_MODE) {
    printf("Running in safe test mode...\n");
  }

  // Fork a subprocess so that we can print the test process's exit
  // status, and to prevent reboots or kernel panics if we are running
  // as PID 1.
  int pid = fork();
  if (pid == 0) {
    main_prog();
    _exit(1);
  }

  int status;
  if (waitpid(pid, &status, 0) == pid) {
    printf("** exited with status %i (0x%x)\n", status, status);
  }

  if (getpid() == 1) {
    // We're the "init" process.  Avoid exiting because that would
    // cause a kernel panic, which can cause a reboot or just obscure
    // log output and prevent console scrollback from working.
    for (;;) {
      sleep(999);
    }
  }
  return 0;
}
