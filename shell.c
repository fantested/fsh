#define ARG_MAX 100
#define BUFFER_SIZE 10000
#define HIST_SIZE 100

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fsh.h"

struct command {
	char *val;
	int argc;
	char *argv[ARG_MAX];
	struct command *next;
};

/*
 * Read command from stdin and return it.
 */
char *read_cmd(size_t buffer_size)
{
	char *cmd = (char *)malloc(buffer_size * sizeof(char));
	int n;

	if (cmd == NULL) {
		perror("error: failed to allocate buffer.\n");
		exit(EXIT_FAILURE);
	}

	n = getline(&cmd, &buffer_size, stdin);
	/* Remove newline character */
	cmd[--n] = '\0';

	return cmd;
}

/*
 * Parse command in buffer into a linked list and return count.
 */
struct command *parse_cmd(char *cmd, char *delim, int *cmdc)
{
	char *val = strtok(cmd, delim);
	struct command *head;
	struct command *curr;
	struct command *next;

	while (val != NULL) {
		next = (struct command *) malloc(sizeof(struct command));
		next->val = val;
		next->next = NULL;

		if (next == NULL) {
			perror("error: failed to allocate memory.\n");
			exit(EXIT_FAILURE);
		}

		if (*cmdc == 0) {
			head = next;
			curr = head;
		} else {
			curr->next = next;
			curr = next;
		}

		val = strtok(NULL, delim);
		(*cmdc)++;
	}

	return head;
}

/*
 * Execute the given commands in child processes.
 * Return 0 if sucessful and 1 if failed.
 */
int run_cmd(char **cmd_hist, struct command *cmd, int cmdc)
{
	int pipefds[(cmdc-1)*2];
	int cmdi = 0;
	pid_t pids[cmdc];
	int statusv[cmdc];
	int ret = 0;
	struct command *curr = cmd;

	create_pipe(pipefds, cmdc-1);

	while (curr) {
		if (chk_builtin_cmd(curr->argv)) {
			pids[cmdi] = -1;
		} else {
			pids[cmdi] = fork();
			if (pids[cmdi] == 0)
				run_cmd_child(curr, pipefds, cmdc, cmdi);
			else
				run_cmd_parent(pids, pipefds, statusv,
					       cmdc, cmdi);
		}

		cmdi++;
		curr = curr->next;
	}

	auto_suggest(cmd_hist, cmd, statusv);

	for (int i = 0; i < cmdc; i++)
		ret = ret || statusv[i];

	return ret;
}

/*
 * Record commands into command history after freeing existing record.
 */
void record_cmd(char **cmd_hist, struct command *cmd, int *histi)
{
	struct command *curr = cmd;

	while (curr) {
		if (cmd_hist[*histi] != NULL) {
			free(cmd_hist[*histi]);
			cmd_hist[*histi] = NULL;
		}

		cmd_hist[(*histi)++] = *(curr->argv);
		if (*histi == HIST_SIZE)
			*histi -= HIST_SIZE;

		curr = curr->next;
	}
}

/*
 * Free the memory of the command linked list.
 */
void free_cmd(struct command *cmd)
{
	struct command *curr;

	while (cmd) {
		curr = cmd;
		cmd = cmd->next;
		free(curr);
	}
}

/*
 * Parse arguments for a list of commands.
 * Return 0 if successful and 1 if failed.
 */
int parse_args(struct command *head, char *delim)
{
	struct command *curr = head;

	while (curr) {
		curr->argc = parse_arg(curr->val, curr->argv, delim);

		if (curr->argc == 0) {
			perror("error: failed to parse command.\n");
			return 1;
		}

		curr = curr->next;
	}

	return 0;
}

/*
 * Parse a command into an array of arguments.
 * Return the number of arguments.
 */
int parse_arg(char *cmd, char **argv, char *delim)
{
	int i = 0;

	argv[i] = strtok(cmd, delim);

	while (argv[i] != NULL)
		argv[++i] = strtok(NULL, delim);

	return i;
}

/*
 * Return 1 if the given command is a built-in command.
 * Otherwise, return 0.
 */
int chk_builtin_cmd(char **argv)
{
	if (chk_exit_cmd(*argv) || chk_cd_cmd(argv))
		return 1;
	else
		return 0;
}

/*
 * Return 1 if the given command is the built-in exit command.
 * Otherwise, return 0.
 */
int chk_exit_cmd(char *cmd)
{
	if (strcmp(cmd, "exit") == 0)
		exit(0);
	else
		return 0;
}

/*
 * Return 1 if the given command is the built-in cd command.
 * Otherwise, return 0.
 */
int chk_cd_cmd(char **argv)
{
	if (strcmp(*argv, "cd") == 0) {
		if (chdir(*(argv+1)) != 0)
			perror("error: failed to change directory.\n");
		return 1;
	} else {
		return 0;
	}
}

/*
 * Construct the entire pipe.
 */
void create_pipe(int *pipefds, int n)
{
	for (int i = 0; i < n; i++) {
		if (pipe(pipefds+i*2) < 0) {
			perror("error: failed to create pipe.\n");
			exit(EXIT_FAILURE);
		}
	}
}

/*
 * Close all file descriptors in the pipe.
 */
void close_pipe(int *pipefds, int n)
{
	for (int i = 0; i < 2 * n; i++)
		close(pipefds[i]);
}

/*
 * Process file descriptors for piping.
 * Return 0 if successful and 1 if failed.
 */
int proc_pipe(int *pipefds, int n, int i)
{
	/* If it's not the first command, read input from the pipe */
	if (i != 0) {
		if (dup2(pipefds[(i-1)*2], STDIN_FILENO) < 0) {
			perror("error: failed to read input from the pipe.\n");
			return 1;
		}
		close(pipefds[(i-1)*2]);
		close(pipefds[(i-1)*2+1]);
	}

	/* If it's not the last command, dump output to the pipe */
	if (i != n) {
		if (dup2(pipefds[i*2+1], STDOUT_FILENO) < 0) {
			perror("error: failed to write output to the pipe.\n");
			return 1;
		}
		close(pipefds[i*2]);
		close(pipefds[i*2+1]);
	}

	return 0;
}

/*
 * Run command in the child process.
 */
void run_cmd_child(struct command *cmd, int *pipefds, int cmdc, int cmdi)
{
	if (proc_pipe(pipefds, cmdc-1, cmdi) == 1) {
		perror("error: failed to duplicate file descriptor.\n");
		exit(EXIT_FAILURE);
	}

	execv(*(cmd->argv), cmd->argv);
	fprintf(stderr, "error: %s.\n", strerror(errno));
	exit(EXIT_FAILURE);
}

/*
 * Manager process running status and the pipe in the parent process.
 */
void run_cmd_parent(pid_t *pids, int *pipefds, int *statusv,
		    int cmdc, int cmdi)
{
	if (cmdc == 1 || cmdi == cmdc-1) {
		close_pipe(pipefds, cmdc-1);

		for (int i = 0; i < cmdc; i++) {
			int status;

			if (pids[i] == -1) {
				statusv[i] = 0;
				continue;
			}

			if (waitpid(pids[i], &status, 0) == -1) {
				perror("error: waitpid() failed.\n");
				exit(EXIT_FAILURE);
			}

			if (WIFEXITED(status)) {
				int exit_status = WEXITSTATUS(status);

				statusv[i] = exit_status;
			}
		}
	}
}

/*
 * Suggest commands to user based on command history.
 */
void auto_suggest(char **cmd_hist, struct command *cmd, int *status)
{
	struct command *curr = cmd;
	int cmdi = 0;

	while (curr) {
		if (status[cmdi++] != 0) {
			/* remove path of cmd */
			char delim[] = "/";
			char *argv[ARG_MAX];
			int argc = parse_arg(*(curr->argv), argv, delim);
			char *abs_cmd = argv[argc-1];

			for (int i = 0; i < HIST_SIZE; i++) {
				if (cmd_hist[i] != NULL
				    && strstr(cmd_hist[i], abs_cmd) != NULL) {
					fprintf(stderr,
						"error: %s: command not found. "
						"Did you mean %s?\n",
						abs_cmd,
						cmd_hist[i]);
					break;
				}
			}
		}

		curr = curr->next;
	}
}

/*
 * Main shell function.
 */
int main(void)
{
	char *cmd_hist[HIST_SIZE] = { NULL };
	char pipe_delim[] = "|";
	char arg_delim[] = " ";
	const size_t buffer_size = BUFFER_SIZE;
	int histi = 0;

	while (1) {
		printf("$");

		char *cmd_text = read_cmd(buffer_size);

		int cmdc = 0;
		struct command *cmd = parse_cmd(cmd_text, pipe_delim, &cmdc);

		if (cmdc == 0)
			continue;

		if (parse_args(cmd, arg_delim) != 0)
			continue;

		if (run_cmd(cmd_hist, cmd, cmdc) == 0)
			record_cmd(cmd_hist, cmd, &histi);

		free_cmd(cmd);
	}

	return 0;
}
