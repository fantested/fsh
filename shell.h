#ifndef FSH_H
#define FSH_H

struct command;

char *read_cmd(size_t buffer_size);
struct command *parse_cmd(char *cmd, char *delim, int *cmdc);
int run_cmd(char **cmd_hist, struct command *cmd, int cmdc);
void record_cmd(char **cmd_hist, struct command *cmd, int *histi);
void free_cmd(struct command *cmd);

int parse_args(struct command *head, char *delim);
int parse_arg(char *cmd, char **argv, char *delim);
int chk_builtin_cmd(char **argv);
int chk_exit_cmd(char *cmd);
int chk_cd_cmd(char **argv);
void create_pipe(int *pipefds, int n);
void close_pipe(int *pipefds, int n);
int proc_pipe(int *pipefds, int n, int i);
void run_cmd_child(struct command *cmd, int *pipefds, int cmdc, int cmdi);
void run_cmd_parent(pid_t *pids, int *pipefds, int *statusv,
		    int cmdc, int cmdi);
void auto_suggest(char **cmd_hist, struct command *cmd, int *statusv);

#endif
