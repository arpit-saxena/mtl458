#include "util.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int init_str_size = 20; // For use in input_str
int got_eof = 0;              // input_str() sets this when it encounters EOF

void init();
void cleanup();
void sigint_handler(int sig_no);
char *get_cmd();

char *const cd_cmd = "cd";
char *const history_cmd = "history";

void change_dir(char *const remaining_cmd);

int main(void) {
  init();
  signal(SIGINT, sigint_handler);

  while (1) {
    char *cmd = get_cmd();

    if (strlen(cmd) == 0) {
      if (got_eof) {
        printf("\n");
        break;
      }
      continue;
    }

    char *tmp_cmd = strdup(cmd);
    char *first_cmd = strtok(tmp_cmd, " ");

    if (strcmp(first_cmd, cd_cmd) == 0) {
      change_dir(strtok(NULL, ""));
    } else if (strcmp(first_cmd, history_cmd) == 0) {
      printf("You wanted history!\n");
    } else {
      printf("I'll execute this command directly\n");
    }

    free(tmp_cmd);
    free(cmd);
  }

  cleanup();
}

char *curr_dir = NULL;

// Current directory to print, containing appropriate transformations such as
// changing HOME to ~
char *curr_print_dir = NULL;
char *start_dir = NULL;

char *get_print_dir(char *dir) {
  char *print_dir = strdup(dir);

  int start_dir_len = strlen(start_dir);
  int match = strncmp(start_dir, dir, start_dir_len) == 0;

  if (match) {
    int print_dir_len = strlen(dir) - strlen(start_dir) + 1;
    free(print_dir); // We had dup'd dir earlier
    print_dir = (char *)malloc(sizeof(char *) * print_dir_len);
    print_dir[0] = '\0';

    strcat(print_dir, "~");
    strcat(print_dir, dir + start_dir_len);
  }

  return print_dir;
}

// remaining_cmd should contain the rest of the command after "cd"
void change_dir(char *const remaining_cmd) {
  char *dir = strtok(remaining_cmd, " ");

  if (!dir) { // remaining_cmd is empty
    dir = start_dir;
  } else if (strtok(NULL, " ")) {
    fprintf(stderr, "cd: Too many arguments\n");
    return;
  }

  if (chdir(dir)) {
    perror("cd");
    return;
  }

  free(curr_dir);
  free(curr_print_dir);

  curr_dir = getcwd(NULL, 0);
  curr_print_dir = get_print_dir(curr_dir);
}

void init() {
  curr_dir = getcwd(NULL, 0);
  start_dir = strdup(curr_dir);
  curr_print_dir = get_print_dir(curr_dir);
}

void cleanup() {
  free(curr_dir);
  free(curr_print_dir);
  free(start_dir);
}

void sigint_handler(int sig_no) {
  cleanup();
  printf("\n");
  exit(0);
}

char *get_cmd() {
  printf("MTL458:%s$ ", curr_print_dir);
  return input_str();
}