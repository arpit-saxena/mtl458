#include "my_alloc.c"

#include <errno.h>
#include <stdio.h>

int main(void) {
  if (my_init()) {
    perror("my_init");
  }

  void *h = my_alloc(200);
  void *s = my_alloc(200);
  print_free_list();

  my_clean();
}
