#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADDR_BITS 32
#define PAGE_SIZE_BITS 12
const int VPN_BITS = ADDR_BITS - PAGE_SIZE_BITS;

enum strategy_t { OPT, FIFO, CLOCK, LRU, RANDOM };

struct cmdline_args_t {
  FILE *input_file;
  int num_frames;
  enum strategy_t strategy;
  bool verbose;
} cmdline_args;

struct cmdline_args_t extract_cmdline_args(int argc, char *argv[]) {
  struct cmdline_args_t args;
  if (argc == 5) {
    assert(strcmp(argv[argc - 1], "-verbose") == 0 &&
           "-verbose should be the last argument passed, if it is present");
    args.verbose = true;
  } else {
    args.verbose = false;
  }

  if (argc != 4 && argc != 5) {
    fprintf(stderr, "Usage: ./frames <tracefile> <number of frames> "
                    "<replacement policy> [-verbose]\n");
    exit(1);
  }

  char *filename = argv[1];
  char *num_frames_str = argv[2];
  char *strat_str = argv[3];

  args.input_file = fopen(filename, "r");
  if (!args.input_file) {
    perror("Opening trace file");
    exit(1);
  }

  char *end;
  long res = strtol(num_frames_str, &end, 10);
  if (res == LONG_MIN || res == LONG_MAX) {
    perror("Number of frames");
    exit(1);
  }
  if (res <= 0 || res > 1000) {
    fprintf(stderr, "The number of pages should be in range [1, 1000]\n");
    exit(1);
  }
  if (end[0] != '\0' || num_frames_str[0] == '\0') {
    fprintf(stderr, "Invalid number of page frames\n");
    exit(1);
  }
  args.num_frames = res;

  if (strcmp(strat_str, "OPT") == 0) {
    args.strategy = OPT;
  } else if (strcmp(strat_str, "FIFO") == 0) {
    args.strategy = FIFO;
  } else if (strcmp(strat_str, "CLOCK") == 0) {
    args.strategy = CLOCK;
  } else if (strcmp(strat_str, "LRU") == 0) {
    args.strategy = LRU;
  } else if (strcmp(strat_str, "RANDOM") == 0) {
    args.strategy = RANDOM;
  } else {
    fprintf(stderr, "Unrecognized page replacement strategy\n");
    exit(1);
  }

  return args;
}

struct memory_op {
  int page_num;
  enum { READ, WRITE } type;
};

struct memory_op get_next_access(FILE *file) {
  struct memory_op op;
  char access_type;
  unsigned virtual_addr;
  if (EOF == fscanf(file, "%x %c", &virtual_addr, &access_type)) {
    if (errno) {
      perror("Reading trace file");
      exit(1);
    }
    op.page_num = -1;
    return op;
  }

  op.page_num = virtual_addr >> PAGE_SIZE_BITS;
  switch (access_type) {
  case 'R':
    op.type = READ;
    break;
  case 'W':
    op.type = WRITE;
    break;
  default:
    fprintf(stderr, "Unknown memory access type %c\n", access_type);
    exit(1);
  }
  return op;
}

const int INIT_SIZE = 20;
struct memory_op *get_all_accesses(FILE *file, int *size) {
  int capacity = INIT_SIZE;
  struct memory_op *mem_accesses = malloc(capacity * sizeof(struct memory_op));
  *size = 0;

  while (true) {
    struct memory_op op = get_next_access(file);
    if (op.page_num == -1) { // File finished
      break;
    }

    if (*size == capacity) {
      capacity *= 2;
      mem_accesses = realloc(mem_accesses, capacity * sizeof(*mem_accesses));
      if (!mem_accesses) {
        perror("Inputting trace file");
      }
    }

    mem_accesses[*size] = op;
    *size += 1;
  }

  return mem_accesses;
}

struct page_table_entry {
  int page_num;
  int frame_num;
  bool valid;
  bool dirty;
  bool use;     // For CLOCK
  int used_at;  // FOR LRU
  int num_uses; // FOR OPT
};

struct page_table_entry *page_table;
int next_free_frame = 0;

struct {
  int mem_accesses; // Memory accesses
  int num_misses;   // Number of Page Faults
  int num_writes;   // Number of writes to the disk
  int num_drops;    // Number of drops
} stats;

void print_stats() {
  printf("Number of memory accesses: %d\n", stats.mem_accesses);
  printf("Number of misses: %d\n", stats.num_misses);
  printf("Number of writes: %d\n", stats.num_writes);
  printf("Number of drops: %d\n", stats.num_drops);
}

// Trying to store results of future uses in the page table itself. This will
// help keep track of till which index in the mem_accesses list have we looked.
int counted_till = -1;

// Assuming this doesn't overflow. This is realistic since >1e9 memory accesses
// would take a long time to simulate
int access_num = 0;
void count_access(struct page_table_entry *pte, int access_idx) {
  pte->use = 1;
  pte->used_at = access_num++;

  // pte->num_uses counts the number of uses of this page in the future.
  // Decrement it since this access is no longer in the future
  if (access_idx <= counted_till) {
    pte->num_uses--;
  }
}

struct memory_op *mem_accesses;
int num_accesses;
int curr_access;

struct page_table_entry **frame_list;
int fifo_pos = 0;

struct page_table_entry *evict_page_fifo(struct page_table_entry *new_page) {
  struct page_table_entry *ret = frame_list[fifo_pos];
  frame_list[fifo_pos] = new_page;
  fifo_pos = (fifo_pos + 1) % cmdline_args.num_frames;
  return ret;
}

int max(int a, int b) { return a >= b ? a : b; }

struct page_table_entry *evict_page_opt() {
  // Initially missing_frame = 0 ^ 1 ^ 2 ^ ... ^ (num_frames - 1)
  // For each frame i accessed we do missing_frame ^= i
  // When (num_frames - 1) frames are accessed, missing_frame would contain
  // index of the frame which wasn't accessed

  int missing_frame = 0;
  for (int i = 0; i < cmdline_args.num_frames; i++) {
    missing_frame ^= i;
  }

  int accesses[cmdline_args.num_frames];
  int num_frames_accessed = 0;
  for (int i = 0; i < cmdline_args.num_frames; i++) {
    accesses[i] = frame_list[i]->num_uses;
    if (accesses[i] > 0) {
      num_frames_accessed++;
      missing_frame ^= i;
    }
  }

  for (int i = max(curr_access, counted_till + 1); i < num_accesses; i++) {
    assert(num_frames_accessed < cmdline_args.num_frames);
    if (num_frames_accessed == cmdline_args.num_frames - 1) {
      counted_till = i - 1;
      return frame_list[missing_frame];
    }

    struct page_table_entry *pte = &page_table[mem_accesses[i].page_num];
    if (pte->valid) {
      accesses[pte->frame_num]++;

      if (pte->num_uses == 0) {
        num_frames_accessed++;
        missing_frame ^= pte->frame_num;
      }
    }
    pte->num_uses++;
  }

  // There are more than one frames which aren't accessed anywhere in the
  // future, so we'll evict the one with minimum frame number
  counted_till = cmdline_args.num_frames - 1;
  for (int i = 0; i < cmdline_args.num_frames; i++) {
    if (frame_list[i]->num_uses == 0) {
      return frame_list[i];
    }
  }

  assert(0 && "The code should never reach here");
}

struct page_table_entry *evict_page_clock(struct page_table_entry *new_page) {
  return NULL;
}

struct page_table_entry *evict_page_lru(struct page_table_entry *new_page) {
  int min_used_at = frame_list[0]->used_at;
  int min_index = 0;
  for (int i = 1; i < cmdline_args.num_frames; i++) {
    if (frame_list[i]->used_at < min_used_at) {
      min_used_at = frame_list[i]->used_at;
      min_index = i;
    }
  }
  struct page_table_entry *ret = frame_list[min_index];
  frame_list[min_index] = new_page;
  return ret;
}

struct page_table_entry *evict_page_random(struct page_table_entry *new_page) {
  int frame_num = rand() % cmdline_args.num_frames;
  struct page_table_entry *ret = frame_list[frame_num];
  frame_list[frame_num] = new_page;
  return ret;
}

struct page_table_entry *get_page_evict(struct page_table_entry *new_page) {
  switch (cmdline_args.strategy) {
  case OPT:
    return evict_page_opt(new_page);
  case FIFO:
    return evict_page_fifo(new_page);
  case CLOCK:
    return evict_page_clock(new_page);
  case LRU:
    return evict_page_lru(new_page);
  case RANDOM:
    return evict_page_random(new_page);
  }
  fprintf(stderr, "Unrecognized strategy\n");
  exit(1);
}

void print_verbose(int written_page, int read_page, bool dirty) {
  if (!cmdline_args.verbose)
    return;
  if (dirty) {
    printf(
        "Page 0x%5x was read from disk, page 0x%5x was written to the disk.\n",
        read_page, written_page);
  } else {
    printf("Page 0x%5x was read from disk, page 0x%5x was dropped (it was not "
           "dirty).\n",
           read_page, written_page);
  }
}

void get_page_from_disk(struct page_table_entry *pte) {
  stats.num_misses++;

  if (next_free_frame < cmdline_args.num_frames) {
    pte->frame_num = next_free_frame++;
  } else {
    // Need to evict
    struct page_table_entry *pte_evict = get_page_evict(pte);
    assert(pte_evict->valid &&
           "Page to evict must be in memory in the first place");
    pte_evict->valid = false;
    if (pte_evict->dirty) {
      stats.num_writes++;
    } else {
      stats.num_drops++;
    }
    pte->frame_num = pte_evict->frame_num;
    print_verbose(pte_evict->page_num, pte->page_num, pte_evict->dirty);
  }

  frame_list[pte->frame_num] = pte;
  pte->dirty = false;
  pte->valid = true;
}

void perform_read(struct page_table_entry *pte) {
  if (pte->valid) {
    return;
  }

  // Translation not valid, need to bring page from disk.
  get_page_from_disk(pte);
  perform_read(pte);
}

void perform_write(struct page_table_entry *pte) {
  if (pte->valid) {
    pte->dirty = true;
    return;
  }

  // Translation not valid, need to bring page from disk.
  get_page_from_disk(pte);
  perform_write(pte);
}

void perform_op(struct memory_op op, int access_idx) {
  assert(op.page_num < (1 << VPN_BITS) && op.page_num >= 0 &&
         "Virtual Page Number must fit into the bits reserved for it");

  stats.mem_accesses++;
  struct page_table_entry *pte = &page_table[op.page_num];
  count_access(pte, access_idx);
  pte->page_num = op.page_num;
  switch (op.type) {
  case READ:
    perform_read(pte);
    break;
  case WRITE:
    perform_write(pte);
    break;
  }
}

void init() {
  srand(5635);
  frame_list =
      malloc(cmdline_args.num_frames * sizeof(struct page_table_entry *));
  page_table = malloc((1 << VPN_BITS) * sizeof(struct page_table_entry));
  memset(page_table, 0, (1 << VPN_BITS) * sizeof(struct page_table_entry));
}

void cleanup() {
  free(frame_list);
  free(page_table);
  if (fclose(cmdline_args.input_file)) {
    perror("fclose");
  }
}

int main(int argc, char *argv[]) {
  cmdline_args = extract_cmdline_args(argc, argv);

  init();

  num_accesses = 0;
  mem_accesses = get_all_accesses(cmdline_args.input_file, &num_accesses);

  for (curr_access = 0; curr_access < num_accesses; curr_access++) {
    perform_op(mem_accesses[curr_access], curr_access);

    printf("Current Access: %d\n", curr_access);
    for (int i = 0; i < cmdline_args.num_frames; i++) {
      assert(!frame_list[i] || frame_list[i]->valid);
    }
  }
  print_stats();

  free(mem_accesses);
  cleanup();
}