
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>


const size_t mem_size = 1 << 30;
const int toggles = 540000;

char *g_mem;

void *pick_addr() {
  size_t offset = (rand() << 12) % mem_size;
  return g_mem + offset;
}

void main_prog() {
  g_mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(g_mem != MAP_FAILED);

  printf("clear\n");
  memset(g_mem, 0xff, mem_size);

  int iter = 0;
  for (;;) {
    printf("toggle %i\n", iter++);
    int j;
    for (j = 0; j < 40; j++) {
      const int addr_count = 4;
      uint32_t *addrs[addr_count];
      int a;
      for (a = 0; a < addr_count; a++)
        addrs[a] = pick_addr();

      uint32_t sum = 0;
      int i;
      for (i = 0; i < toggles; i++) {
        for (a = 0; a < addr_count; a++)
          sum += *addrs[a] + 1;
        for (a = 0; a < addr_count; a++)
          asm volatile("clflush (%0)" : : "r" (addrs[a]) : "memory");
      }

      /* Sanity check.  We don't expect this to fail, because reading
         these rows refreshes them. */
      if (sum != 0) {
        printf("error: sum=%x\n", sum);
        exit(1);
      }
    }

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
