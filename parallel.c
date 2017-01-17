#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>

#define VERSION "0.2"
#define MAX_CMD_LEN 1024
#define INITIAL_QUEUE_CAPACITY 100

typedef struct {
  char *command;
  pid_t pid;
  int status;
} command_t;

command_t **command_queue;
size_t number_of_commands;

void usage(const char *name){
  fprintf(stderr, "Parallel version %s. That's it, no bullshit\n", VERSION);
  fprintf(stderr, "Usage: %s <input file>\n", name);
}

size_t trim(char *string){
  if (string == NULL)
    return 0;
  size_t len = strlen(string);
  if (len == 0)
    return len;

  char *tmp = string;
  while(isspace(tmp[len - 1])) tmp[--len] = '\0';
  while(tmp != NULL && isspace(*tmp)) ++tmp, --len;
  memmove(string, tmp, len + 1);
  return strlen(string);
}

void set_chld_status(const int pid, const int status){
  for (size_t i = 0; i < number_of_commands; i++){
    if (command_queue[i]->pid == pid){
      command_queue[i]->status = status;
      break;
    }
  }
}

void sigchld_handler(int signum){
    pid_t pid;
    int   status;
    pid = waitpid(-1, &status, WNOHANG);
    set_chld_status(pid, status);
}

int main(const int argc, const char **argv){

  if (argc < 2){
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  FILE *fp = fopen(argv[1], "r");
  if (!fp){
    perror("fopen");
    return EXIT_FAILURE;
  }

  signal(SIGCHLD, sigchld_handler);

  size_t number_of_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  number_of_commands = 0;
  size_t q_capacity = INITIAL_QUEUE_CAPACITY;
  command_queue = malloc(q_capacity * sizeof **command_queue);
  size_t *running_proc = mmap(NULL, sizeof *running_proc, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  *running_proc = 0;
  if (command_queue == NULL){
    perror("malloc");
    return EXIT_FAILURE;
  }

  char command[MAX_CMD_LEN];
  while (fgets(command, MAX_CMD_LEN, fp) != NULL) {
    if (trim(command) == 0)
      continue;

    command_queue[number_of_commands] = malloc(sizeof(command_t));
    command_t *cmd = command_queue[number_of_commands];
    if (cmd == NULL){
      perror("malloc");
      return EXIT_FAILURE;
    }
    cmd->command = calloc(strlen(command) + 1, 1);
    if (cmd->command == NULL){
      perror("calloc");
      return EXIT_FAILURE;
    }
    memmove(cmd->command, command, strlen(command) + 1);
    number_of_commands++;
    if (number_of_commands == q_capacity){
      q_capacity *= 2;
      command_queue = realloc(command_queue, q_capacity * sizeof **command_queue);
      if (command_queue == NULL){
        perror("realloc");
        return EXIT_FAILURE;
      }
    }
  }
  fclose(fp);

  for (size_t i = 0; i < number_of_commands; i++){
    pid_t pid = fork();
    if (pid == -1){
      perror("fork");
      return EXIT_FAILURE;
    }

    if (pid == 0){
      int stat = system(command_queue[i]->command);
      (*running_proc)--;
      munmap(running_proc, sizeof *running_proc);
      exit(WEXITSTATUS(stat));
    } else {
      (*running_proc)++;
      command_queue[i]->pid = pid;
    }

    while(*running_proc >= number_of_cpus){
      sleep(1);
    }
  }

  while (*running_proc > 0) {
    sleep(1);
  }

  bool all_good = true;
  for (size_t i = 0; i < number_of_commands; i++){
    if (!WIFEXITED(command_queue[i]->status) || WEXITSTATUS(command_queue[i]->status) != 0){
      fprintf(stderr, "Command [%s] with PID %d ret status != 0, exit status: %d\n",
        command_queue[i]->command,
        command_queue[i]->pid,
        WEXITSTATUS(command_queue[i]->status)
      );
      all_good = false;
    }

    free(command_queue[i]->command);
    free(command_queue[i]);
  }

  free(command_queue);
  munmap(running_proc, sizeof *running_proc);
  return all_good ? EXIT_SUCCESS : EXIT_FAILURE;
}
