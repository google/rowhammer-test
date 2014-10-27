
// This is required on Mac OS X for getting PRI* macros #defined.
#define __STDC_FORMAT_MACROS

#include <assert.h>
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


const size_t mem_size = 1 << 30;
const int toggles = 540000;

char *g_mem;

char *pick_addr() {
  size_t offset = (rand() << 12) % mem_size;
  return g_mem + offset;
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
    printf("  %.3f nanosec per iteration: %g sec for %" PRId64 " iterations\n",
           iter_time * 1e9, total_time, iterations);
  }
};

static void toggle(int iterations, int addr_count) {
  Timer t;
  for (int j = 0; j < iterations; j++) {
    uint32_t *addrs[addr_count];
    for (int a = 0; a < addr_count; a++)
      addrs[a] = (uint32_t *) pick_addr();

    uint32_t sum = 0;
    for (int i = 0; i < toggles; i++) {
      for (int a = 0; a < addr_count; a++)
        sum += *addrs[a] + 1;
      for (int a = 0; a < addr_count; a++)
        asm volatile("clflush (%0)" : : "r" (addrs[a]) : "memory");
    }

    // Sanity check.  We don't expect this to fail, because reading
    // these rows refreshes them.
    if (sum != 0) {
      printf("error: sum=%x\n", sum);
      exit(1);
    }
  }
  t.print_iters(iterations * addr_count * toggles);
}

void main_prog() {
  g_mem = (char *) mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE, -1, 0);
  assert(g_mem != MAP_FAILED);

  printf("clear\n");
  memset(g_mem, 0xff, mem_size);

  Timer t;
  int iter = 0;
  for (;;) {
    printf("Iteration %i (after %.2fs)\n", iter++, t.get_diff());
    toggle(10, 8);

    Timer check_timer;
    printf("check\n");
    uint64_t *end = (uint64_t *) (g_mem + mem_size);
    uint64_t *ptr;
    int errors = 0;
    for (ptr = (uint64_t *) g_mem; ptr < end; ptr++) {
      uint64_t got = *ptr;
      if (got != ~(uint64_t) 0) {
        printf("error at %p: got 0x%" PRIx64 "\n", ptr, got);
        errors++;
      }
    }
    printf("  (check took %fs)\n", check_timer.get_diff());
    if (errors)
      exit(1);
  }
}


int main() {
  // In case we are running as PID 1, we fork() a subprocess to run
  // the test in.  Otherwise, if process 1 exits or crashes, this will
  // cause a kernel panic (which can cause a reboot or just obscure
  // log output and prevent console scrollback from working).
  int pid = fork();
  if (pid == 0) {
    main_prog();
    _exit(1);
  }

  int status;
  if (waitpid(pid, &status, 0) == pid) {
    printf("** exited with status %i (0x%x)\n", status, status);
  }

  for (;;) {
    sleep(999);
  }
  return 0;
}
