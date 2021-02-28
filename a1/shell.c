#include "util.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
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
void run_cmd(char *const cmd);
void print_history();

struct {
  char *arr[5];
  int oldest_pos;
  int size;
} history;

int main(void) {
  init();
  signal(SIGINT, sigint_handler);

  while (1) {
    char *cmd = get_cmd();

    if (strlen(cmd) == 0) {
      if (got_eof) {
        free(cmd);
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
      print_history();
    } else {
      run_cmd(cmd);
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

void update_curr_dir() {
  free(curr_dir);
  free(curr_print_dir);

  curr_dir = getcwd(NULL, 0);
  curr_print_dir = get_print_dir(curr_dir);
}

// remaining_cmd should contain the rest of the command after "cd"
void change_dir(char *const remaining_cmd) {
  if (!remaining_cmd) {
    if (chdir(start_dir)) {
      perror("cd");
      return;
    }
    update_curr_dir();
    return;
  }

  int rem_len = strlen(remaining_cmd);
  char *dir = malloc((rem_len + 1) * sizeof(char));
  int dir_pos = 0;
  int escape_space = 0;
  int last_non_blank_pos = 0;

  int start_pos = 0;
  char *prefix = malloc(1 * sizeof(char));
  prefix[0] = '\0';
  if (remaining_cmd[0] == '~') {
    free(prefix);
    prefix = strdup(start_dir);
    start_pos = 1;
  }

  for (int i = start_pos; i < rem_len; i++) {
    if (remaining_cmd[i] == ' ' && !escape_space) {
      char *rem = strtok(remaining_cmd + i, " ");
      if (rem && rem[0] != '\0') {
        fprintf(stderr, "cd: Too many arguments\n");
        free(dir);
        free(prefix);
        return;
      }
    } else {
      last_non_blank_pos = i;
      if (remaining_cmd[i] == '\"') {
        escape_space = !escape_space;
      } else {
        dir[dir_pos++] = remaining_cmd[i];
      }
    }
  }
  dir[dir_pos++] = '\0';

  prefix = realloc(prefix, strlen(prefix) + dir_pos);
  if (!prefix) {
    perror("cd");
    return;
  }

  strcat(prefix, dir);
  free(dir);
  dir = prefix;

  if (chdir(dir)) {
    printf("%s\n", dir);
    perror("cd");
    return;
  }

  free(dir);
  update_curr_dir();
}

void run_cmd(char *const cmd) {
  int pid = fork();
  if (pid == -1) {
    perror("fork");
  } else if (pid == 0) {
    char *temp_cmd = strdup(cmd);

    // Possibly inefficient, going through the string twice
    char *tok = strtok(temp_cmd, " ");
    int num_args = 0;
    while (tok) {
      num_args++;
      tok = strtok(NULL, " ");
    }

    free(temp_cmd);

    // + 1 since last element in the array must be NULL
    char **args = malloc(sizeof(char *) * (num_args + 1));
    temp_cmd = strdup(cmd);
    tok = strtok(temp_cmd, " ");
    int i = 0;
    while (tok) {
      args[i++] = strdup(tok);
      tok = strtok(NULL, " ");
    }
    args[i] = NULL;
    free(temp_cmd);

    execvp(args[0], args);

    // If execvp returned, it means an error must have occurred
    perror("exec");
    exit(1); // Exit out of child process
  } else {
    if (wait(NULL) == -1) {
      perror("wait");
    }
  }
}

void print_history() {
  int init_pos = 0;
  if (history.size >= 5) {
    init_pos = history.oldest_pos;
  }
  for (int i = 0; i < history.size; i++) {
    printf("%d %s\n", i, history.arr[(init_pos + i) % 5]);
  }
}

void init() {
  curr_dir = getcwd(NULL, 0);
  start_dir = strdup(curr_dir);
  curr_print_dir = get_print_dir(curr_dir);

  history.oldest_pos = 0;
  history.size = 0;
  for (int i = 0; i < 5; i++) {
    history.arr[i] = NULL;
  }
}

void cleanup() {
  free(curr_dir);
  free(curr_print_dir);
  free(start_dir);
  for (int i = 0; i < history.size; i++) {
    free(history.arr[i]);
  }
}

void sigint_handler(int sig_no) {
  cleanup();
  printf("\n");
  exit(0);
}

char *get_cmd() {
  printf("MTL458:%s$ ", curr_print_dir);
  char *cmd = input_str();

  free(history.arr[history.oldest_pos]);
  history.arr[history.oldest_pos++] = strdup(cmd);
  history.oldest_pos %= 5;
  if (history.size != 5) {
    history.size++;
  }

  return cmd;
}