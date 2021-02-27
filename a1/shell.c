#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int init_str_size = 20; // For use in input_str

void init();
void cleanup();
char *get_cmd();

int main(void) {
  init();

  char *cmd = get_cmd();
  printf("You entered: %s\n", cmd);
  free(cmd);

  cleanup();
}

char *curr_dir = NULL;

// Current directory to print, containing appropriate transformations such as
// changing HOME to ~
char *curr_print_dir = NULL;

char *get_print_dir(char *dir) {
  char *print_dir = strdup(dir);

  char *const home_dir = getenv("HOME");
  if (home_dir) {
    int home_dir_len = strlen(home_dir);
    int match = strncmp(home_dir, dir, home_dir_len) == 0;

    if (match) {
      int print_dir_len = strlen(dir) - strlen(home_dir) + 1;
      free(print_dir); // We had dup'd dir earlier
      print_dir = (char *)malloc(sizeof(char *) * print_dir_len);
      print_dir[0] = '\0';

      strcat(print_dir, "~");
      strcat(print_dir, dir + home_dir_len);
    }
  }

  return print_dir;
}

void init() {
  curr_dir = getcwd(NULL, 0);
  curr_print_dir = get_print_dir(curr_dir);
}

void cleanup() {
  free(curr_dir);
  free(curr_print_dir);
}

char *get_cmd() {
  printf("MTL458:%s$ ", curr_print_dir);
  return input_str();
}