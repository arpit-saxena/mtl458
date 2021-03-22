#define _DEFAULT_SOURCE
//^ Defined for MAP_ANONYMOUS to be available

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static void *page;              // Don't allow access from outside this file
const int MMAP_SIZE = 4 * 1024; // 4 KB

int my_init(void) {
  page = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED) {
    return errno;
  }
  return 0;
}

int my_clean(void) {
  if (munmap(page, MMAP_SIZE) == -1) {
    return errno;
  }
  return 0;
}