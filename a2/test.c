#include "my_alloc.c"

#include <errno.h>

void print_mem() {
  print_memory();
  print_free_list();
}

void test1() {
  print_info();
  // print_mem();

  void *h = my_alloc(200);
  // print_mem();

  void *s = my_alloc(200);
  // print_mem();

  my_free(h);
  // print_mem();

  h = my_alloc(104);
  // print_mem();

  void *t = my_alloc(104);
  print_mem();

  my_free(s);
  print_mem();

  my_alloc(3504);
  print_mem();

  my_alloc(288);
  print_mem();
}

void test2() {
  void *a = my_alloc(80);
  void *b = my_alloc(8);
  void *c = my_alloc(80);

  print_mem();

  my_free(b);

  print_mem();
}

extern free_header_t *next_fh;
extern free_header_t *prev_fh;

void test3() {
  void *arr[100];
  int size = 0;
  while (arr[size++] = my_alloc(400)) {
    print_mem();
  }
  void *rem = my_alloc(16);

  print_mem();

  for (int i = 0; i < size; i += 2) {
    my_free(arr[i]);
  }
  my_free(arr[1]);

  print_mem();

  my_alloc(8);
  print_mem();

  my_alloc(1000);
  print_mem();

  my_alloc(1000);
  print_mem();
}

int main(void) {
  if (my_init()) {
    perror("my_init");
  }

  print_info();
  print_mem();

  void *arr[1000];
  int size = 0;
  while (arr[size++] = my_alloc(8)) {
  }
  print_mem();

  for (int i = 0; arr[i]; i += 2) {
    my_free(arr[i]);
  }
  print_mem();

  my_free(arr[1]);
  my_heapinfo();

  my_free(NULL);

  my_alloc(8);

  my_alloc(8);
  my_alloc(16);
  my_heapinfo();

  my_clean();
}
