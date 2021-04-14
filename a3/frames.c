#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int ADDR_BITS = 32;
const int PAGE_SIZE_BITS = 12; // 4 KiB
const int VPN_BITS = ADDR_BITS - PAGE_SIZE_BITS;

enum strategy_t { OPT, FIFO, CLOCK, LRU, RANDOM };

struct cmdline_args_t {
  FILE *input_file;
  int num_frames;
  enum strategy_t strategy;
  bool verbose;
};

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
  int frame_num;
  bool valid;
  bool dirty;
};

struct page_table_entry page_table[1 << VPN_BITS];
int next_free_frame = 0;

struct {
  int mem_accesses; // Memory accesses
  int num_misses;   // Number of Page Faults
  int num_writes;   // Number of writes to the disk
  int num_drops;    // Number of drops
} stats;

void get_page_from_disk(struct page_table_entry *pte, int num_frames) {
  stats.num_misses++;
  pte->dirty = false;
  pte->valid = true;

  if (next_free_frame < num_frames) {
    pte->frame_num = next_free_frame++;
  } else {
    // Need to evict
    struct page_table_entry *pte_evict = get_page_evict();
    pte_evict->valid = false;
    if (pte_evict->dirty) {
      stats.num_writes++;
    } else {
      stats.num_drops++;
    }
    pte->frame_num = pte_evict->frame_num;
  }
}

void perform_read(struct page_table_entry *pte, int num_frames) {
  if (pte->valid) {
    return;
  }

  // Translation not valid, need to bring page from disk.
  get_page_from_disk(pte, num_frames);
  perform_read(pte, num_frames);
}

void perform_write(struct page_table_entry *pte, int num_frames) {
  if (pte->valid) {
    pte->dirty = true;
    return;
  }

  // Translation not valid, need to bring page from disk.
  get_page_from_disk(pte, num_frames);
  perform_write(pte, num_frames);
}

void perform_op(struct memory_op op, int num_frames) {
  assert(op.page_num < (1 << VPN_BITS) && op.page_num >= 0 &&
         "Virtual Page Number must fit into the bits reserved for it");

  stats.mem_accesses++;
  struct page_table_entry *pte = &page_table[op.page_num];
  switch (op.type) {
  case READ:
    perform_read(pte, num_frames);
    break;
  case WRITE:
    perform_write(pte, num_frames);
    break;
  }
}

int main(int argc, char *argv[]) {
  struct cmdline_args_t args = extract_cmdline_args(argc, argv);

  printf("Num frames: %d\n", args.num_frames);
  printf("Strategy: %d\n", args.strategy);
  printf("Verbose: %d\n", args.verbose);

  int num_accesses = 0;
  struct memory_op *mem_accesses =
      get_all_accesses(args.input_file, &num_accesses);
  for (int i = 0; i < num_accesses; i++) {
    printf("%x %d\n", mem_accesses[i].page_num, mem_accesses[i].type);
  }

  if (fclose(args.input_file)) {
    perror("fclose");
  }
}