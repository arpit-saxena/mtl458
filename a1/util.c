#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Safe arbitrary length string input function taken from
// https://stackoverflow.com/a/16871702/5585431
extern const int init_str_size;
extern int got_eof;
char *input_str() {
  char ch;
  size_t len = 0;
  int size = init_str_size;

  char *str = malloc(sizeof(*str) * size);
  if (!str) { // Error. Return null. Caller handles error
    return str;
  }

  while (EOF != (ch = fgetc(stdin)) && ch != '\n') {
    str[len++] = ch;
    if (len == size) {
      str = realloc(str, sizeof(*str) * (size += 16));
      if (!str)
        return str;
    }
  }

  if (ch == EOF) {
    got_eof = 1;
  }

  str[len++] = '\0';

  return str;
}