#define ARG_MAX 100
#define BUFFER_SIZE 1000
#define HIST_SIZE 100

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Read command from stdin and return it */
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

/* Parse command buffer into argv[] and return the number of arguments */
int parse_cmd(char *cmd, char *argv[]) {
	char delim[] = " ";
	int i = 0;

	argv[i] = strtok(cmd, delim);
	
	while(argv[i] != NULL) {
		argv[++i] = strtok(NULL, delim);
	}

	if (i != 0)
		argv[++i] = NULL;

	return i;
}

/* Execute the given command in a child process */
void run_cmd(char *argv[]) {
	pid_t pid;

	if ((pid = fork()) == 0) {
		if (execv(*(argv), argv) == -1)
			fprintf(stderr, "error: %s.\n", strerror(errno));
		return;
	}
	else {
		wait(&pid);
	}
}

/* Return 1 if the given command is the built-in exit command. Otherwise, return 0. */
int chk_exit_cmd(char *cmd) {
	if (strcmp(cmd, "exit") == 0)
		exit(0);
	else
		return 0;
}

/* Return 1 if the given command is the built-in cd command. Otherwise, return 0. */
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

/* Return 1 if the given command is a built-in command. Otherwise, return 0. */
int chk_builtin_cmd(char *argv[]) {
	if (chk_exit_cmd(*argv) || chk_cd_cmd(argv))
		return 1;
	else
		return 0;
}

int main() {
	char *cmd_hist[HIST_SIZE];
	char *argv[ARG_MAX];
	size_t buffer_size = BUFFER_SIZE;
	int cmd_idx = 0;
	int argc = 0;
	
	while(1) {
		printf("$");
		
		cmd_hist[cmd_idx] = read_cmd(buffer_size);
		argc = parse_cmd(*(cmd_hist+cmd_idx), argv);

		if (argc == 0)
			continue;

		cmd_idx++;
		
		if (chk_builtin_cmd(argv))
			continue;
			
		run_cmd(argv);	  
	}
}
