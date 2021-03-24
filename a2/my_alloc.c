#define _DEFAULT_SOURCE
//^ Defined for MAP_ANONYMOUS to be available
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static void *page;              // Don't allow access from outside this file
const int PAGE_SIZE = 4 * 1024; // 4 KB

int my_init(void) {
  page = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED) {
    return errno;
  }
  return 0;
}

// Frees the region of memory given by ptr. It must be the pointer that my_alloc
// defined, there's no check for it otherwise.
void my_free(void *ptr) {
  alloc_header_t *header = (alloc_header_t *)(ptr - sizeof(alloc_header_t));
}

void my_clean(void) {
  if (munmap(page, PAGE_SIZE) == -1) {
    perror("my_clean: munmap");
  }
}

void my_heapinfo() {
  int a, b, c, d, e, f;

  // Do not edit below output format
  printf("=== Heap Info ================\n");
  printf("Max Size: %d\n", a);
  printf("Current Size: %d\n", b);
  printf("Free Memory: %d\n", c);
  printf("Blocks allocated: %d\n", d);
  printf("Smallest available chunk: %d\n", e);
  printf("Largest available chunk: %d\n", f);
  printf("==============================\n");
  // Do not edit above output format
  return;
}

