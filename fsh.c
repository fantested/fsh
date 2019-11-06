#define ARG_MAX 100
#define BUFFER_SIZE 1000
#define HIST_SIZE 100

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Read command from stdin and return it. */
char *read_cmd(size_t buffer_size) {
	char *cmd = (char *)malloc(buffer_size * sizeof(char));
	int n;
	
	if (cmd == NULL) {
		perror("error: failed to allocate buffer.\n");
		exit(1);
	}
	
	n = getline(&cmd, &buffer_size, stdin);
	// Remove newline character
	cmd[--n] = '\0';

	return cmd;
}

/* Parse command buffer into argv[] and return argc. */
int parse_cmd(char *cmd, char *argv[], char delim[]) {
	int i = 0;

	argv[i] = strtok(cmd, delim);
	
	while(argv[i] != NULL) {
		argv[++i] = strtok(NULL, delim);
	}

	return i;
}

/* Suggest command to user based on command history. */
void auto_suggest(char *cmd_hist[], char *cmd) {
	// remove path of cmd
	char delim[] = "/";
	char *argv[ARG_MAX];
        int argc = parse_cmd(cmd, argv, delim);
	char *abs_cmd = argv[argc-1];
 
	for (int i = 0; i < HIST_SIZE; i++) {
		if (cmd_hist[i] != NULL && strstr(cmd_hist[i], abs_cmd) != NULL) {
			fprintf(stderr,
				"error: %s: command not found. Did you mean %s?\n",
				abs_cmd,
				cmd_hist[i]);
		}
	}
}

/* Execute the given command in a child process. 
   Return 0 if sucessful, otherwise return 1. */
int run_cmd(char *argv[]) {
	pid_t pid;

	if ((pid = fork()) == 0) {
		execv(*argv, argv);
		printf("error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	else {
		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			int exit_status = WEXITSTATUS(status);
			return exit_status;
		}
	}

	return 0;
}

/* Return 1 if the given command is the built-in exit command. 
   Otherwise, return 0. */
int chk_exit_cmd(char *cmd) {
	if (strcmp(cmd, "exit") == 0)
		exit(0);
	else
		return 0;
}

/* Return 1 if the given command is the built-in cd command. 
   Otherwise, return 0. */
int chk_cd_cmd(char *argv[]) {
	if (strcmp(*argv, "cd") == 0) {
		if (chdir(*(argv+1)) != 0)
			perror("error: failed to change directory.\n");
		
		return 1;
	}
	else {
		return 0;
	}
}

/* Return 1 if the given command is a built-in command. 
   Otherwise, return 0. */
int chk_builtin_cmd(char *argv[]) {
	if (chk_exit_cmd(*argv) || chk_cd_cmd(argv))
		return 1;
	else
		return 0;
}

/* Record command into command history after freeing existing record. */
void record_cmd(char *cmd_hist[], int *cmd_idx, char *cmd) {
	if (cmd_hist[*cmd_idx] != NULL)
		free(cmd_hist[*cmd_idx]);

	cmd_hist[*cmd_idx++] = cmd;
	if (*cmd_idx == HIST_SIZE)
		*cmd_idx -= HIST_SIZE;
}

int main() {
	char *cmd;
	char *cmd_hist[HIST_SIZE] = { NULL };
	char *argv[ARG_MAX];
	char cmd_delim[] = " ";
	size_t buffer_size = BUFFER_SIZE;
	int cmd_idx = 0;
	int argc = 0;
	
	while(1) {
		printf("$");
		
		cmd = read_cmd(buffer_size);
		argc = parse_cmd(cmd, argv, cmd_delim);

		if (argc == 0)
			continue;

		if (chk_builtin_cmd(argv)) {
			record_cmd(cmd_hist, &cmd_idx, cmd);
			continue;
		}

		if (run_cmd(argv) == 0)
			record_cmd(cmd_hist, &cmd_idx, *argv);
		else
			auto_suggest(cmd_hist, *argv);
	}

	return 0;
}
