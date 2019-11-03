#define ARG_MAX 100
#define BUFFER_SIZE 262144
#define HIST_SIZE 100

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Read command from stdin and return it */
char *read_command(size_t buffer_size) {
	char *cmd = (char *)malloc(buffer_size * sizeof(char));
	int n;
	
	if (cmd == NULL) {
		perror("Error: failed to allocate buffer.\n");
		exit(1);
	}
	
	n = getline(&cmd, &buffer_size, stdin);
	// Remove newline character
	cmd[--n] = '\0';

	return cmd;
}

/* Parse command buffer into argv[] and return the number of arguments */
int parse_command(char *cmd, char *argv[]) {
	char delim[] = " ";
	int i = 0;

	argv[i] = strtok(cmd, delim);
	
	while(argv[i] != NULL) {
		argv[++i] = strtok(NULL, delim);
	}

	argv[++i] = NULL;

	return i;
}

/* Execute the given command in a child process */
void run_command(int argc, char *argv[]) {
	if (argc > 0) {
		pid_t pid;

		if ((pid = fork()) == 0) {
			if (execv(*(argv), argv) == -1)
				fprintf(stderr, "Error: %s\n", strerror(errno));
			return;
		}
		else {
			wait(&pid);
		}
	}
}

int main() {
	char *cmd_hist[HIST_SIZE];
	char *argv[ARG_MAX];
	size_t buffer_size = BUFFER_SIZE;
	int cmd_idx = 0;
	int argc = 0;
	
	while(1) {
		printf("$");
		
		cmd_hist[cmd_idx] = read_command(buffer_size);
		argc = parse_command(*(cmd_hist+cmd_idx), argv);
		run_command(argc, argv);
		
		cmd_idx++;
	}
}
