
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
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
  struct timespec start_time_;

 public:
  Timer() {
    int rc = clock_gettime(CLOCK_MONOTONIC, &start_time_);
    assert(rc == 0);
  }

  double get_diff() {
    struct timespec end_time;
    int rc = clock_gettime(CLOCK_MONOTONIC, &end_time);
    assert(rc == 0);
    return (end_time.tv_sec - start_time_.tv_sec
            + (double) (end_time.tv_nsec - start_time_.tv_nsec) / 1e9);
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
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(g_mem != MAP_FAILED);

  printf("clear\n");
  memset(g_mem, 0xff, mem_size);

  Timer t;
  int iter = 0;
  for (;;) {
    printf("toggle %i\n", iter++);
    toggle(10, 8);

    printf("check\n");
    uint64_t *end = (uint64_t *) (g_mem + mem_size);
    uint64_t *ptr;
    int errors = 0;
    for (ptr = (uint64_t *) g_mem; ptr < end; ptr++) {
      uint64_t got = *ptr;
      if (got != ~(uint64_t) 0) {
        printf("error at %p: got 0x%" PRIx64 "\n", ptr, got);
        printf("after %.2fs\n", t.get_diff());
        errors++;
      }
    }
    if (errors)
      exit(1);
  }
}


int main() {
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
