#include "my_alloc.c"

#include <errno.h>

int main(void) {
  if (my_init()) {
    perror("my_init");
  }

  print_info();
  print_memory();
  void *h = my_alloc(200);
  print_memory();
  void *s = my_alloc(200);
  print_memory();

  my_clean();
}
