#define _DEFAULT_SOURCE
//^ Defined for MAP_ANONYMOUS to be available
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// Don't print unless DEBUG has been defined
#ifdef DEBUG
int dfprint(FILE *stream, const char *format, ...) {
  // From https://stackoverflow.com/a/150560/5585431
  va_list va;
  int ret;

  va_start(va, format);
  ret = vfprintf(stream, format, va);
  va_end(va);
  return ret;
}
int dprint(const char *format, ...) {
  // From https://stackoverflow.com/a/150560/5585431
  va_list va;
  int ret;

  va_start(va, format);
  ret = vfprintf(stdout, format, va);
  va_end(va);
  return ret;
}
#else  /* DEBUG */
int dfprint(FILE *stream, const char *format, ...) { return 0; }
int dprint(const char *format, ...) { return 0; }
#endif /* DEBUG */

static void *page;              // Don't allow access from outside this file
const int PAGE_SIZE = 4 * 1024; // 4 KB

enum header_type { FREE_BLOCK, ALLOC_BLOCK };

// type and size are defined as bitfields so that they can be packed together
// and total memory used by the header can be less.
// Note that width of size can be upto 31, but even 15 bits is more than enough
// for our page size of 4KB. It also helps in the case when unsigned int's size
// is 16 bits.
typedef struct free_header_t {
  unsigned int type : 1;
  unsigned int size : 15;     // Size of free memory (excluding header)
  struct free_header_t *next; // Next node in the free list
} free_header_t;

bool is_valid_addr(void *addr) {
  return ((char *)addr - (char *)page) < PAGE_SIZE;
}

free_header_t *next_fh; // next_free_header; Used to implement next fit algo.
free_header_t *prev_fh; // prev node of next_fh.
// ^These 2 never become null after initialization
free_header_t head_free_list = {
    .size = 0, .next = NULL}; // Head of the free list. Dummy node which points
                              // to the rest of the free list

// type, size and prev_free_size are bit-fields to pack them together and reduce
// the total size of the struct.
typedef struct {
  unsigned int type : 1;
  unsigned int size : 15; // Size of the allocated memory. Excludes header.
} alloc_header_t;

int my_init(void) {
  page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED) {
    return errno;
  }

  next_fh = (free_header_t *)page;
  next_fh->type = FREE_BLOCK;
  next_fh->size = PAGE_SIZE - sizeof(*next_fh);
  next_fh->next = NULL;

  head_free_list.next = next_fh;
  prev_fh = &head_free_list;

  return 0;
}

void *my_alloc(int size) {
  if (size % 8 != 0) {
    dfprint(stderr, "size given to my_alloc not a multiple of 8\n");
    return NULL;
  }

  int search_size = size + sizeof(alloc_header_t);
  free_header_t *init_fh = next_fh;
  alloc_header_t *alloc_header = NULL;
  dprint("Starting alloc of size %d\n", size);
  while (true) {
    int total_free_space = next_fh->size + sizeof(*next_fh);
    dprint("Total free space: %d\n", total_free_space);
    dprint("Search size: %d\n", search_size);
    if (total_free_space >= search_size) {
      int remaining_space = total_free_space - search_size;
      free_header_t free_header = *next_fh; // Make a copy of this header
      alloc_header = (alloc_header_t *)next_fh;
      alloc_header->size = size;
      alloc_header->type = ALLOC_BLOCK;

      if (remaining_space < sizeof(free_header)) {
        // No space for free_header, so allocate this too
        alloc_header->size += remaining_space;
        remaining_space = 0;
      }

      int updated_prev_free_size;
      if (remaining_space == 0) {
        prev_fh->next = free_header.next;
        // This is set so that we move correctly to the next free header.
        // Otherwise we would skip the block that was after the current free
        // block.
        next_fh = prev_fh;
        updated_prev_free_size = 0; // No free memory here now
      } else {
        next_fh = (free_header_t *)((char *)alloc_header +
                                    sizeof(*alloc_header) + alloc_header->size);
        next_fh->next = free_header.next;
        next_fh->size = remaining_space - sizeof(free_header);
        next_fh->type = FREE_BLOCK;
        prev_fh->next = next_fh;
        updated_prev_free_size = next_fh->size;
      }
    }

    // If we were able to allocate space, or even otherwise, move to the next
    // block
    if (next_fh->next) { // => Not the end of list
      prev_fh = next_fh;
      next_fh = next_fh->next;
    } else { // Resume at beginning of free list
      prev_fh = &head_free_list;
      next_fh = head_free_list.next;
    }

    if (alloc_header || next_fh == init_fh) {
      // Break out of the loop if allocated or having traversed the entire list
      break;
    }
  }

  if (alloc_header) {
    // => Successful allocation
    return (void *)((char *)alloc_header + sizeof(*alloc_header));
  }

  // Unable to allocate
  dfprint(stderr, "Unable to find space for allocation\n");
  return NULL;
}

// Frees the region of memory given by ptr. It must be the pointer that my_alloc
// defined, there's no check for it otherwise.
void my_free(void *ptr) {
  dprint("Starting free\n");
  alloc_header_t *alloc_header =
      (alloc_header_t *)((char *)ptr - sizeof(alloc_header_t));
  free_header_t *prev = &head_free_list;
  free_header_t *curr = head_free_list.next;

  free_header_t free_header = {.size = alloc_header->size +
                                       sizeof(*alloc_header) -
                                       sizeof(free_header_t),
                               .type = FREE_BLOCK};
  free_header_t *coalesced_fh_begin = NULL;

  free_header_t *insert_after_fh = prev;
  free_header_t *insert_before_fh = curr;
  while (curr) {
    char *next_addr = (char *)curr + curr->size + sizeof(*curr);
    if (next_addr == (char *)alloc_header) {
      coalesced_fh_begin = curr;
      free_header.size += curr->size + sizeof(*curr);
      insert_after_fh = prev;
      insert_before_fh = curr->next;
      break;
    } else if ((char *)curr->next > (char *)alloc_header) {
      insert_after_fh = curr;
      insert_before_fh = curr->next;
      break;
    }

    prev = curr;
    curr = curr->next;
  }

  if (!coalesced_fh_begin) { // No free block just before
    coalesced_fh_begin = (free_header_t *)alloc_header;
  }

  free_header_t *next_block =
      (free_header_t *)((char *)ptr + alloc_header->size);
  if (is_valid_addr(next_block) && next_block->type == FREE_BLOCK) {
    free_header.size += next_block->size + sizeof(*next_block);
    insert_before_fh = next_block->next;
  }

  free_header.next = insert_before_fh;
  memcpy(coalesced_fh_begin, &free_header, sizeof(free_header));
  insert_after_fh->next = coalesced_fh_begin;
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

void print_free_list() {
  dprint("Free list: ");
  dprint("HEAD -> ");
  free_header_t *curr = head_free_list.next;
  while (curr) {
    assert(curr->type == FREE_BLOCK);
    dprint("%d -> ", curr->size);
    curr = curr->next;
  }
  dprint("NULL\n\n");
}

void print_info() {
  dprint("Free Header size:\t%d\n", sizeof(free_header_t));
  dprint("Alloc Header size:\t%d\n", sizeof(alloc_header_t));
}

void print_memory() {
  dprint("\n----------------MEMORY-------------\n");

  char *ptr = (char *)page;
  while (ptr - (char *)page < PAGE_SIZE) {
    alloc_header_t *alloc_header = (alloc_header_t *)ptr;
    switch (alloc_header->type) {
    case ALLOC_BLOCK: {
      dprint("ALLOC\tSize:%d\n", alloc_header->size);
      ptr += alloc_header->size + sizeof(*alloc_header);
      break;
    }
    case FREE_BLOCK: {
      free_header_t *free_header = (free_header_t *)ptr;
      dprint("FREE\tSize:%d\n", free_header->size);
      ptr += free_header->size + sizeof(*free_header);
      break;
    }
    }
  }

  dprint("--------------END MEMORY-------------\n\n");
}