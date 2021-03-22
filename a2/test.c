#include "my_alloc.c"

#include <errno.h>
#include <stdio.h>

int main(void) {
  if (my_init()) {
    perror("my_init");
  }

  if (my_clean()) {
    perror("my_clean");
  }
}