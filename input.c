#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
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

// handle builtin commands in parent process
int handle_builtins(char **args) {
  if (strcmp(args[0], "cd") == 0) {
    if (chdir(args[1]) != 0)
      perror("cd failed");
    return 1;
  } else if (strcmp(args[0], "exit") == 0) {
    exit(0);
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
    exit(1);
  } else {
    int status;
    waitpid(pid, &status, 0);
  }
}

int main() {

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  while (1) {
    char *line = malloc(200);
    printf("\n myshell>");
    if (fgets(line, 200, stdin) == NULL) {
      free(line);
      break;
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
