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

int main(void) {
  if (my_init()) {
    perror("my_init");
  }

  test2();

  my_clean();
}
