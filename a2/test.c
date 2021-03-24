#include "my_alloc.c"

#include <errno.h>

void print_mem() {
  print_memory();
  print_free_list();
}

int main(void) {
  if (my_init()) {
    perror("my_init");
  }

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

  my_clean();
}
