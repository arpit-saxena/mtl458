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

typedef struct {
  int curr_size;
  int allocated_blocks;
  int smallest_chunk_size;
  int largest_chunk_size;
} heap_info_t;

heap_info_t *heap_info = NULL;

int get_chunk_size(free_header_t *fh) { return fh->size; }

int my_init(void) {
  page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED) {
    return errno;
  }

  heap_info = (heap_info_t *)page;

  next_fh = (free_header_t *)((char *)page + sizeof(*heap_info));
  next_fh->type = FREE_BLOCK;
  next_fh->size = PAGE_SIZE - sizeof(*next_fh) - sizeof(*heap_info);
  next_fh->next = NULL;

  heap_info->curr_size = sizeof(*next_fh);
  heap_info->allocated_blocks = 0;
  heap_info->smallest_chunk_size = get_chunk_size(next_fh);
  heap_info->largest_chunk_size = get_chunk_size(next_fh);

  head_free_list.next = next_fh;
  prev_fh = &head_free_list;

  return 0;
}

int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }

void recalculate_chunk_sizes() {
  dprint("Recalculating chunk sizes!\n");
  int min_size = PAGE_SIZE;
  int max_size = 0;

  free_header_t *fh = head_free_list.next;

  if (!fh) { // No free node, nothing available
    min_size = 0;
    max_size = 0;
  }

  bool set_min = false;
  while (fh) {
    int curr_size = get_chunk_size(fh);
    if (curr_size != 0) { // Don't want to include 0 sized chunk when non-zero
                          // ones are present
      min_size = min(min_size, curr_size);
      set_min = true;
    }
    max_size = max(max_size, curr_size);
    fh = fh->next;
  }

  if (!set_min) { // => No non zero chunk, so set min to 0
    min_size = 0;
  }

  heap_info->smallest_chunk_size = min_size;
  heap_info->largest_chunk_size = max_size;
}

void *my_alloc(int size) {
  assert(prev_fh->next == next_fh);

  if (!next_fh) { // Nothing is free
    dfprint(stderr, "Unable to allocate. No free space!\n");
    return NULL;
  }

  if (size % 8 != 0) {
    dfprint(stderr, "size given to my_alloc not a multiple of 8\n");
    return NULL;
  }

  int search_size = size + sizeof(alloc_header_t);

  // Need to make sure that this can eventually be freed
  if (search_size < sizeof(free_header_t)) {
    size += sizeof(free_header_t) - search_size;
    search_size = sizeof(free_header_t);
  }

  free_header_t *init_fh = next_fh;
  alloc_header_t *alloc_header = NULL;
  dprint("Starting alloc of size %d\n", size);

  // There can be a new free block created at the end of this. We need to keep
  // track of its chunk size to be able to update heap_info if needed.
  int new_chunk_size = -1;

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

      int new_used_space = alloc_header->size + sizeof(*alloc_header);
      if (remaining_space == 0) {
        // Didn't need to make new free header
        new_used_space -= sizeof(free_header_t);
        prev_fh->next = free_header.next;
        // This is set so that we move correctly to the next free header.
        // Otherwise we would skip the block that was after the current free
        // block.
        next_fh = prev_fh;
      } else {
        next_fh = (free_header_t *)((char *)alloc_header +
                                    sizeof(*alloc_header) + alloc_header->size);
        next_fh->next = free_header.next;
        next_fh->size = remaining_space - sizeof(free_header);
        next_fh->type = FREE_BLOCK;
        int next_fh_chunk_size = get_chunk_size(next_fh);
        if (next_fh_chunk_size < heap_info->smallest_chunk_size &&
            next_fh_chunk_size != 0) {
          heap_info->smallest_chunk_size = next_fh_chunk_size;
        }
        prev_fh->next = next_fh;
      }

      int fh_chunk_size = get_chunk_size(&free_header);
      if (fh_chunk_size == heap_info->smallest_chunk_size ||
          fh_chunk_size == heap_info->largest_chunk_size) {
        // We just allocated a chunk which was either the smallest or largest
        // available chunk. Recalculate chunk sizes.
        recalculate_chunk_sizes();
      }
      heap_info->allocated_blocks++;
      heap_info->curr_size += new_used_space;
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

  // No op in case ptr is NULL
  if (!ptr) {
    return;
  }

  alloc_header_t *alloc_header =
      (alloc_header_t *)((char *)ptr - sizeof(alloc_header_t));
  free_header_t *prev = &head_free_list;
  free_header_t *curr = head_free_list.next;

  free_header_t free_header = {.size = alloc_header->size +
                                       sizeof(*alloc_header) -
                                       sizeof(free_header_t),
                               .type = FREE_BLOCK};
  free_header_t *coalesced_fh_begin = NULL;

  // These are set to satisfy curr == NULL i.e. fully empty free list
  // Otherwise, they'll get updated in the coming loop
  free_header_t *insert_after_fh = prev;
  free_header_t *insert_before_fh = curr;

  free_header_t *coalesced_block_before = NULL;
  free_header_t *coalesced_block_after = NULL;

  int freed_space = free_header.size;
  bool found_space = false;
  if ((char *)curr > (char *)alloc_header) {
    insert_after_fh = prev;
    insert_before_fh = curr;
    found_space = true;
  }

  while (curr && !found_space) {
    // INV: curr < alloc_header
    char *next_addr = (char *)curr + curr->size + sizeof(*curr);
    if (next_addr == (char *)alloc_header) {
      // Free block just before. Merge
      coalesced_block_before = curr;
      coalesced_fh_begin = curr;
      free_header.size += curr->size + sizeof(*curr);
      insert_after_fh = prev;
      insert_before_fh = curr->next;
      freed_space += sizeof(*curr);
      break;
    } else if ((char *)curr->next > (char *)alloc_header || !curr->next) {
      // We had not found any found any free block just before the block we want
      // to free, so if next free block is ahead of alloc_header or this is end
      // of the list, then break
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
    coalesced_block_after = next_block;
    free_header.size += next_block->size + sizeof(*next_block);
    insert_before_fh = next_block->next;
    freed_space += sizeof(*next_block);
  }

  free_header.next = insert_before_fh;
  memcpy(coalesced_fh_begin, &free_header, sizeof(free_header));
  insert_after_fh->next = coalesced_fh_begin;

  if (prev_fh == coalesced_block_before || prev_fh == coalesced_block_after ||
      next_fh == coalesced_block_before || next_fh == coalesced_block_after) {
    prev_fh = insert_after_fh;
    next_fh = coalesced_fh_begin;
  } else if (next_fh == coalesced_fh_begin->next) {
    prev_fh = coalesced_fh_begin;
  }

  bool recalculate = false;
  int new_fh_chunk_size = get_chunk_size(&free_header);
  if (new_fh_chunk_size > heap_info->largest_chunk_size) {
    heap_info->largest_chunk_size = new_fh_chunk_size;
  }
  if (new_fh_chunk_size < heap_info->smallest_chunk_size ||
      heap_info->smallest_chunk_size == 0) {
    heap_info->smallest_chunk_size = new_fh_chunk_size;
  }

  int chunk_sizes[] = {-1, -1};
  if (coalesced_block_before) {
    chunk_sizes[0] = get_chunk_size(coalesced_block_before);
  }
  if (coalesced_block_after) {
    chunk_sizes[1] = get_chunk_size(coalesced_block_after);
  }
  for (int i = 0; i < 2; i++) {
    if (chunk_sizes[i] == -1) {
      continue; // These chunks didn't exist
    }
    if (chunk_sizes[i] == heap_info->largest_chunk_size ||
        chunk_sizes[i] == heap_info->smallest_chunk_size) {
      recalculate = true;
    }
  }

  if (recalculate) {
    recalculate_chunk_sizes();
  }
  heap_info->curr_size -= freed_space;
  heap_info->allocated_blocks--;
}

void my_clean(void) {
  if (munmap(page, PAGE_SIZE) == -1) {
    perror("my_clean: munmap");
  }
}

void my_heapinfo() {
  int max_size = PAGE_SIZE - sizeof(*heap_info);
  // Do not edit below output format
  printf("=== Heap Info ================\n");
  printf("Max Size: %d\n", max_size);
  printf("Current Size: %d\n", heap_info->curr_size);
  printf("Free Memory: %d\n", max_size - heap_info->curr_size);
  printf("Blocks allocated: %d\n", heap_info->allocated_blocks);
  printf("Smallest available chunk: %d\n", heap_info->smallest_chunk_size);
  printf("Largest available chunk: %d\n", heap_info->largest_chunk_size);
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
  dprint("Heap info struct size:\t%d\n", sizeof(*heap_info));
}

void print_memory() {
  dprint("\n----------------MEMORY-------------\n");

  int max_size = PAGE_SIZE - sizeof(*heap_info);
  dprint("============== Heap Info ==============\n");
  dprint("Max Size:\t\t\t%d\n", PAGE_SIZE - sizeof(*heap_info));
  dprint("Current Size:\t\t\t%d\n", heap_info->curr_size);
  dprint("Free Memory:\t\t\t%d\n", max_size - heap_info->curr_size);
  dprint("Blocks allocated:\t\t%d\n", heap_info->allocated_blocks);
  dprint("Smallest available chunk:\t%d\n", heap_info->smallest_chunk_size);
  dprint("Largest available chunk:\t%d\n", heap_info->largest_chunk_size);
  dprint("=======================================\n");

  char *ptr = (char *)page + sizeof(*heap_info);
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
      dprint("FREE\tSize:%d", free_header->size);
      if (free_header == next_fh) {
        dprint("\t<--NEXT_FH");
      }
      if (free_header == prev_fh) {
        dprint("\t<--PREV_FH");
      }
      dprint("\n");

      ptr += free_header->size + sizeof(*free_header);
      break;
    }
    }
  }

  dprint("--------------END MEMORY-------------\n\n");
}