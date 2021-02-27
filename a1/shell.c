#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const int init_str_size = 20; // For use in input_str

int main(void) {
  printf("What's your name? ");
  char *const name = input_str();
  printf("Hi %s\n", name);
  free(name);
  return 0;
}