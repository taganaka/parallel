#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>

#define VERSION "0.1"
#define MAX_CMD_LEN 1024
#define INITIAL_QUEUE_CAPACITY 100

typedef struct {
  char *command;
  pid_t pid;
} command_t;

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

  size_t number_of_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  size_t number_of_commands = 0;
  size_t q_capacity = INITIAL_QUEUE_CAPACITY;
  command_t **q = malloc(q_capacity * sizeof **q);
  size_t *running_proc = mmap(NULL, sizeof *running_proc, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  *running_proc = 0;
  if (q == NULL){
    perror("malloc");
    return EXIT_FAILURE;
  }

  char command[MAX_CMD_LEN];
  while (fgets(command, MAX_CMD_LEN, fp) != NULL) {
    if (trim(command) == 0)
      continue;

    q[number_of_commands] = malloc(sizeof(command_t));
    command_t *cmd = q[number_of_commands];
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
      q = realloc(q, q_capacity * sizeof **q);
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
      int stat = system(q[i]->command);
      (*running_proc)--;
      munmap(running_proc, sizeof *running_proc);
      exit(WEXITSTATUS(stat));
    } else {
      (*running_proc)++;
      q[i]->pid = pid;
    }

    while(*running_proc >= number_of_cpus){
      sleep(1);
    }
  }

  for (size_t i = 0; i < number_of_commands; i++){
    int status;
    waitpid(q[i]->pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      fprintf(stderr, "Command [%s] with PID %d ret status != 0, exit status: %d\n", q[i]->command, q[i]->pid, WEXITSTATUS(status));
    free(q[i]->command);
    free(q[i]);
  }

  free(q);
  munmap(running_proc, sizeof *running_proc);
  return EXIT_SUCCESS;
}
