#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

// parse all the arguments entered on the command line
char **parse(char *line) {
  char **args = malloc(10 * sizeof(char *));
  int i = 0;
  char *tok = strtok(line, " ");
  while (tok != NULL) {
    args[i++] = tok;
    tok = strtok(NULL, " ");
  }
  args[i] = NULL;
  return args;
}

// jump directories
// to jump to recently visited directories directly save the visited directories
// in ~/.ion_history file , when jumping directories search for most matching
// directory in .ion_history and chdir() it.

void save_jump(const char *path) {
  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/.ion_history", getenv("HOME"));
  char paths[100][512];
  int counts[100];
  int total = 0;
  FILE *file = fopen(filepath, "r");
  if (file == NULL) {
    perror("failed opening file");
    return;
  }
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    sscanf(buffer, "%[^|]|%d", paths[total], &counts[total]);
    paths[total][strcspn(paths[total], "\n")] = '\0';
    total++;
  }
  fclose(file);

  int found = 0;
  for (int i = 0; i < total; i++) {
    if (strcmp(paths[i], path) == 0) {
      counts[i]++;
      found = 1;
    }
  }
  if (!found) {
    strcpy(paths[total], path);
    counts[total] = 1;
    total++;
  }

  FILE *fw = fopen(filepath, "w");
  if (fw == NULL) {
    perror("failed opening file");
    return;
  }
  for (int i = 0; i < total; i++) {
    fprintf(fw, "%s|%d\n", paths[i], counts[i]);
  }
  fclose(fw);
}

void jump(char *partial) {
  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/.ion_history", getenv("HOME"));
  char paths[100][512];
  int counts[100];
  int total = 0;
  FILE *file = fopen(filepath, "r");
  if (file == NULL) {
    perror("failed opening file");
    return;
  }
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    sscanf(buffer, "%[^|]|%d", paths[total], &counts[total]);
    paths[total][strcspn(paths[total], "\n")] = '\0';
    total++;
  }
  fclose(file);

  int bestcount = 0;
  int bestindex = -1;
  for (int i = 0; i < total; i++) {
    if (strstr(paths[i], partial) != NULL) {
      if (counts[i] > bestcount) {
        bestcount = counts[i];
        bestindex = i;
      }
    }
  }
  if (bestindex == -1) {
    printf("\r\nno recent visits to %s", partial);
    return;
  }
  printf("\rinside %s",paths[bestindex]);
  chdir(paths[bestindex]);
}

// handle builtin commands in parent process
int handle_builtins(char **args) {
  if (strcmp(args[0], "cd") == 0) {
    if (chdir(args[1]) != 0)
      perror("cd failed");
    char cwd[512];
    getcwd(cwd, sizeof(cwd));
    save_jump(cwd);
    return 1;
  } else if (strcmp(args[0], "exit") == 0) {
    exit(0);
    return 1;
  }else if(strcmp(args[0], "j")==0){
    if(args[1]==NULL){
      printf("\r\nusage: j<partial path> jump to recently visited dir");
      return 1;
    }
    jump(args[1]);
    return 1;
  }
  return 0;
}

// pipe handling
//
// find the pipe and split two parts of args
char **find_pipe(char **args) {
  int i = 0;
  while (args[i] != NULL) {
    if (strcmp(args[i], "|") == 0) {
      args[i] = NULL;
      return &args[i + 1];
    }
    i++;
  }
  return NULL;
}

// execute the parallel commands specified using pipes
void execute_pipe(char **left, char **right) {
  int fds[2];
  pipe(fds);
  pid_t child1 = fork();
  if (child1 == 0) {
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    execvp(left[0], left);
    perror("execvp failed");
    exit(1);
  } else if (child1 < 0) {
    perror("error forking child");
    exit(1);
  }
  pid_t child2 = fork();
  if (child2 == 0) {
    close(fds[1]);
    dup2(fds[0], STDIN_FILENO);
    close(fds[0]);
    execvp(right[0], right);
    perror("execvp failed");
    exit(1);
  } else if (child2 < 0) {
    perror("error forking child");
    exit(1);
  }
  close(fds[0]);
  close(fds[1]);
  int status;
  waitpid(child1, &status, 0);
  waitpid(child2, &status, 0);
}

// struct to redirect the operators in fd
typedef struct {
  char *input_file;
  char *output_file;
  int isappend;
} redirect;

// parse_redirects() parses the file operators and file names to perform the
// file operations
redirect parse_redirects(char **args) {
  redirect parseddata = {NULL, NULL,
                         0}; // default redirects,avoid garbage values be safe.
  int i = 0;
  while (args[i] != NULL) {
    if (strcmp(args[i], ">>") == 0) {
      parseddata.output_file = args[i + 1];
      parseddata.isappend = 1; // open in append to write
      args[i] = NULL;
      args[i + 1] = NULL;
    } else if (strcmp(args[i], ">") == 0) {
      parseddata.output_file = args[i + 1];
      parseddata.isappend = 0; // open in overwrite to write
      args[i] = NULL;
      args[i + 1] = NULL;
    } else if (strcmp(args[i], "<") == 0) {
      parseddata.input_file = args[i + 1];
      parseddata.isappend = 0; // open in readonly
      args[i] = NULL;
      args[i + 1] = NULL;
    }
    i++;
  }
  return parseddata;
}

// apply_redirects() opens the file in specified redirects and performs the
// operation
void apply_redirects(redirect r) {

  if (r.output_file) {
    if (r.isappend) {
      int fd = open(r.output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd < 0) {
        perror("open failed");
        exit(1);
      }
      dup2(fd, STDOUT_FILENO);
      close(fd);

    } else {
      int fd = open(r.output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        perror("open failed");
        exit(1);
      }
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
  }
  if (r.input_file) {
    int fd = open(r.input_file, O_RDONLY, 0644);
    if (fd < 0) {
      perror("open failed");
      exit(1);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
  }
}

// termios to enter raw mode
struct termios orig_termios; // saves original term settings

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // set the orig_termios
                                                     // back
}

// enable raw mode after saving the orig_termios
void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_lflag &=
      ~(ECHO | ICANON); // bit 1 and bit 3 of lower nibble i.e 0xA ~= 0x5 to
                        // clear only echo and canonical modes.
  raw.c_lflag |= ISIG;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// execute the external commands with child process using fork() and execvp()
void execute(char **args) {
  redirect r = parse_redirects(args);
  pid_t pid = fork();
  if (pid < 0) {
    printf("error forking child\n");
    exit(0);
  } else if (pid == 0) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    apply_redirects(r);
    execvp(args[0], args);
    perror("execvp failed");
    enable_raw_mode();
    exit(1);
  } else {
    int status;
    waitpid(pid, &status, 0);
  }
}

// tab completions
// auto complete the partial commands or show options
char **get_completions(char *partial, int *count) {
  char **options = malloc(100 * sizeof(char *));
  *count = 0;
  char *envpath = getenv("PATH");
  char *pathcopy = strdup(envpath);
  char *dirpath = strtok(pathcopy, ":");
  DIR *dir;
  struct dirent *entry;
  while (dirpath != NULL) {
    dir = opendir(dirpath);
    if (dir == NULL) {
      dirpath = strtok(NULL, ":");
      continue;
    }
    while ((entry = readdir(dir)) != NULL) {
      if (strncmp(entry->d_name, partial, strlen(partial)) == 0) {
        options[*count] = strdup(entry->d_name);
        (*count)++;
      }
    }
    closedir(dir);
    dirpath = strtok(NULL, ":");
  }
  return options;
}

int main() {

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  enable_raw_mode();
  while (1) {
    char *line = malloc(200);
    printf("\r\nmyshell>");
    fflush(stdout);
    int i = 0;
    char c;
    while (1) // read until enter
    {
      read(STDIN_FILENO, &c, 1);
      if (c == '\r' || c == '\n') {
        line[i] = '\0';
        write(STDOUT_FILENO, "\n", 1); // set alignment (back to col 0)
        break;
      } else if (c == 3) // ctrl-c
      {
        i = 0;
        line[0] = '\0';
        break;
      } else if (c == 4 && i == 0) // ctrl-d
      {
        disable_raw_mode();
        exit(0);

        // Rule:any special characters(backspace,tab) must be fully handled
        // before reaching the else block , otherwise it leaks into buffer and
        // gets printed or stored causing issues and segfaults in other
        // operations.

      } else if (c == 127) // handle backspace
      {
        if (i > 0) {
          i--;
          write(STDOUT_FILENO, "\b \b", 3);
        }
      } else if (c == 9) // tab autocomplete case
      {
        if (i <= 0)
          continue;
        line[i] = '\0'; // terminate with null
        int count = 0;
        char **matches = get_completions(line, &count);
        if (count == 1) // exact match clear the line and print match
        {
          for (int j = 0; j < i; j++)
            write(STDOUT_FILENO, "\b \b", 3);
          write(STDOUT_FILENO, matches[0], strlen(matches[0]));
          strcpy(line, matches[0]);
          i = strlen(matches[0]);
        } else if (count > 1) // multiple matches display all and reprint prompt
                              // and line
        {
          write(STDOUT_FILENO, "\r\n", 2);
          for (int j = 0; j < count; j++) {
            write(STDOUT_FILENO, matches[j], strlen(matches[j]));
            write(STDOUT_FILENO, "  ", 2);
            free(matches[j]); // free after use
          }
          free(matches); // free array
          char *prompt = "\r\nmyshell>";
          write(STDOUT_FILENO, prompt, strlen(prompt));
          write(STDOUT_FILENO, line, i);
        }
      } else {
        write(STDOUT_FILENO, &c, 1); // print the typed char
        line[i++] = c;
      }
    }

    line[strcspn(line, "\n")] = '\0';
    char **args = parse(line);
    if (args[0] == NULL) {
      perror("no args specified");
      free(line);
      free(args);
      continue;
    }

    if (handle_builtins(args)) {
      free(args);
      free(line);
      continue;
    }
    char **nextcmd = find_pipe(args);
    if (nextcmd != NULL) {
      execute_pipe(args, nextcmd);
      free(line);
      free(args);
      continue;
    }
    execute(args);
    // for (int i = 0; args[i] != NULL; i++) {
    //   printf("%s\n", args[i]);
    // }
    free(args);
    free(line);
  }
  return 0;
}
