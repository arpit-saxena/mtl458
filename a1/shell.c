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
char *input_str();
char **get_args(char *cmd);

char *const cd_cmd = "cd";
char *const history_cmd = "history";

void change_dir(char **args);
void run_cmd(char **args);
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
      free(cmd);
      if (got_eof) {
        printf("\n");
        break;
      }
      continue;
    }

    char **args = get_args(cmd);
    free(cmd);

    if (strcmp(args[0], cd_cmd) == 0) {
      change_dir(args);
    } else if (strcmp(args[0], history_cmd) == 0) {
      if (args[1]) {
        fprintf(stderr, "history: Too many arguments\n");
      } else {
        print_history();
      }
    } else {
      run_cmd(args);
    }

    int i = 0;
    while (args[i]) {
      free(args[i]);
      i++;
    }
    free(args);
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
void change_dir(char **args) {
  if (!args[1]) { // No argument to cd goes to starting directory
    if (chdir(start_dir)) {
      perror("cd");
      return;
    }
    update_curr_dir();
    return;
  }

  if (args[2]) {
    fprintf(stderr, "cd: Too many arguments\n");
    return;
  }

  char *dir;
  if (args[1][0] == '~') {
    // To allocate: Remove 1 for ~ and add 1 for \0
    dir = malloc((strlen(args[1]) + strlen(start_dir)) * sizeof(char));
    dir[0] = '\0';
    strcat(dir, start_dir);
    strcat(dir, args[1] + 1);
  } else {
    dir = strdup(args[1]);
  }

  if (chdir(dir)) {
    printf("%s\n", dir);
    perror("cd");
    free(dir);
    return;
  }

  free(dir);
  update_curr_dir();
}

void run_cmd(char **args) {
  int pid = fork();
  if (pid == -1) {
    perror("fork");
  } else if (pid == 0) {
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

// Returns an array of args, last of which is NULL.
// Arguments are taken from the string cmd, separated by spaces and enclosed by
// double quotes when argument has a space
char **get_args(char *cmd) {
  char **args = malloc(10 * sizeof(char *));
  int args_capacity = 10;
  int args_size = 0;

  int cmd_len = strlen(cmd);

  int i = 0;

  // INV: cmd[i] != ' ' || i == cmd_len
  while (1) { // Iterate over arguments
    if (args_size == args_capacity) {
      args = realloc(args, sizeof(*args) * (args_capacity += 10));
      if (!args) { // realloc failed
        perror("get_args: realloc");
        exit(1);
      }
    }

    while (i < cmd_len && cmd[i] == ' ') {
      i++;
    }
    if (i == cmd_len) {
      args[args_size++] = NULL;
      return args;
    }

    // Wasting memory somewhat, but I don't want to deal with reallocating again
    args[args_size++] = malloc((cmd_len + 1) * sizeof(char));
    args[args_size - 1][0] = '\0';

    int begin = i;
    int escape_space = 0;
    while (i <= cmd_len) {
      if (i == cmd_len) {
        strcat(args[args_size - 1], cmd + begin);
        break;
      } else if (cmd[i] == ' ' && !escape_space) {
        // Argument is cmd[begin..i-1]
        cmd[i] = '\0';
        strcat(args[args_size - 1], cmd + begin);
        cmd[i] = ' ';
        break;
      } else if (cmd[i] == '"') {
        escape_space = !escape_space;
        cmd[i] = '\0';
        strcat(args[args_size - 1], cmd + begin);
        cmd[i] = '"';
        begin = i + 1;
        i++;
      } else {
        i++;
      }
    }
  }
}

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