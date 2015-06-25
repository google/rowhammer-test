#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

const size_t page_size = 1 << 12;

uint64_t get_physical_frame_num(uintptr_t virtual_addr) {
  int fd = open("/proc/self/pagemap", O_RDONLY);
  assert(fd >= 0); 

  off_t pos = lseek(fd, (virtual_addr / page_size) * 8, SEEK_SET);
  assert(pos >= 0); 
  uint64_t value;
  int got = read(fd, &value, 8); 
  assert(got == 8); 
  int rc = close(fd);
  assert(rc == 0); 

  uint64_t frame_num = value & ((1ULL << 54) - 1); 
  return frame_num;
}

uint64_t get_physical_addr(uintptr_t virtual_addr) {
  uint64_t frame_num = get_physical_frame_num(virtual_addr);
  return (frame_num * page_size) | (virtual_addr & (page_size - 1));
}


int main(int argc, char **argv) {
  size_t page_num = 256;
  size_t sleep_sec = 30;

  int c;
  while ((c = getopt(argc, argv, "a:s:")) != -1) {
    switch(c) {
      case 'a':
        page_num = atoi(optarg);
        break;
      case 's':
        sleep_sec = atoi(optarg);
        break;
      default:
        fprintf(stderr, "invalid argument\n");
        exit(1);
    }
  }

  fprintf(stderr, "using allocation size: %lu pages\n", page_num);
  fprintf(stderr, "using sleep interval: %lu seconds\n", sleep_sec);

  FILE* out = fopen("phymem_alloc_results", "w+");
  if (out == NULL) {
    perror("open result file: ");
    exit(1);
  }

  size_t chunk_size = page_num * page_size; 
  size_t iter = 0;

  while(1) {
    fprintf(stderr, "iteration %lu ... \n", iter); 
    iter++;

    char *chunk = (char *)mmap(NULL, chunk_size, PROT_READ | PROT_WRITE,
        MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(chunk != MAP_FAILED);

    size_t i;
    for (i = 0; i < page_num; i++) {
      char *va = chunk + page_size * i;
      uint64_t pfn = get_physical_frame_num((uintptr_t)va);
      fprintf(out, "0x%" PRIx64 " ", pfn);
    }

    fprintf(out, "\n");
    fflush(out);
    sleep(sleep_sec);
  }
}
