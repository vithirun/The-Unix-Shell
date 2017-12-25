#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_INPUT_CMD_SIZE	128
#define MAX_PWD_SIZE	512
#define MAX_BG_COUNT	20

char error_message[30] = "An error has occurred\n";

int main(int argc, char *argv[])
{
	int ret, i, j, fork_ret;
	int fd_in, fd_out;
	int pipe_des[2];
	int out_pos, in_pos, pipe_pos;
	int out_redir, in_redir, flag_pipe, flag_bg;
	char input_cmd[MAX_INPUT_CMD_SIZE * 5];
	int history = 1;
	char *args[MAX_INPUT_CMD_SIZE];
	int bg[MAX_BG_COUNT];
	int bg_count = 0;

	if (argc != 1) {
		write(STDERR_FILENO, error_message, strlen(error_message));
		exit(1);
	}

beg:
	while(1) {
		printf("mysh (%d)> ",history);
		fflush(stdout);
		history++;

		memset(input_cmd, '\0', MAX_INPUT_CMD_SIZE * 5);
		fgets(input_cmd, MAX_INPUT_CMD_SIZE * 5, stdin);
		char *input_str = input_cmd;
		//printf("Input cmd: %s\n",input_cmd);
		if(strlen(input_cmd) > MAX_INPUT_CMD_SIZE) {
			write(STDERR_FILENO, error_message, strlen(error_message));
			continue;
		}

		for(i=0;i < strlen(input_cmd); i++) {
			/* Replace tabs with white spaces */
			if(input_cmd[i] == 32) {
				input_cmd[i] = '\t';
			}

			/* Remove new-line */
			if(input_cmd[i] == '\n') {
				input_cmd[i] = '\0';
				break;
			}
		}

		/* Tokenize input cmd based on whitespace */
		i = 0;
		while((args[i] = strtok_r(input_str, "	", &input_str))) {
			i++;
		}
		args[i] = NULL;

		if(args[0] == NULL) {
			history--;
			continue;
		}

		if(strcmp(args[0], "exit") == 0) {
			for(j = 0; j < bg_count; j++)
				kill(bg[j % MAX_BG_COUNT], 1);			
			exit(0);
		}

		if(strcmp(args[0], "cd") == 0) {
			if(args[1] == NULL) {
				args[1] = getenv("HOME");
			}
			ret = chdir(args[1]);
			if (ret) {
				write(STDERR_FILENO, error_message, strlen(error_message));
			}
			continue;
		}
	
		if(strcmp(args[0], "pwd") == 0) {
			if (args[1] == NULL) {
				char buffer[MAX_PWD_SIZE];
				printf("%s\n",getcwd(buffer, sizeof(buffer)));
			} else 
				write(STDERR_FILENO, error_message, strlen(error_message));
			continue;
		}

		out_redir = 0;
		in_redir = 0;
		flag_pipe = 0;
		flag_bg = 0;

		for (j = 0; j < i; j++) {
			if (strcmp(args[j],">") == 0) {
				if (args[j+1] == NULL) {
					write(STDERR_FILENO, error_message, strlen(error_message));
					goto beg;
				}
				if (args[j+2] != NULL) {
					if (!((strcmp(args[j+2],"&") == 0) || (strcmp(args[j+2],"<") == 0))) {
						write(STDERR_FILENO, error_message, strlen(error_message));
						goto beg;
					}
				}
				fd_out = open(args[j+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
				out_pos = j;
				out_redir = 1;
			}
			else if(strcmp(args[j], "<") == 0) {
				if (args[j+1] == NULL) {
					write(STDERR_FILENO, error_message, strlen(error_message));
					goto beg;
				}
				if (args[j+2] != NULL) {
					if (!((strcmp(args[j+2],"&") == 0) || (strcmp(args[j+2],">") == 0))) {
						write(STDERR_FILENO, error_message, strlen(error_message));
						goto beg;
					}
				}
				fd_in = open(args[j+1], O_RDONLY, S_IRUSR | S_IWUSR);
				if (fd_in < 0) {
					write(STDERR_FILENO, error_message, strlen(error_message));
					goto beg;
				}
				in_pos = j;
				in_redir = 1;
			}
			else if(strcmp(args[j], "|") == 0) {
				if (args[j+1] == NULL) {
					write(STDERR_FILENO, error_message, strlen(error_message));
					goto beg;
				}
				pipe_pos = j;
				flag_pipe = 1;
			}
			else if (strcmp(args[j],"&") == 0) {
				//printf("It is bg\n");
				flag_bg = 1;
				args[j] = NULL;
			}
		}
	
		fork_ret = fork();
		if (fork_ret == 0) {
			if (out_redir) {
				dup2(fd_out, 1);
				args[out_pos] = NULL;
			}
			if (in_redir) {
				dup2(fd_in, 0);
				args[in_pos] = NULL;
			}
			if (flag_pipe) {
				pipe(pipe_des);
				args[pipe_pos] = NULL;

				ret = fork();
				if (ret == 0) {
					close(pipe_des[0]);
					dup2(pipe_des[1], 1);
					ret = execvp(args[0],args);
					if (ret)
						write(STDERR_FILENO, error_message, strlen(error_message));
					break;
				}
				else if (ret > 0) {
				//	waitpid(ret, NULL, 0);
					close(pipe_des[1]);
					dup2(pipe_des[0], 0);
					ret = execvp(args[pipe_pos + 1],&args[pipe_pos + 1]);
					if (ret)
						write(STDERR_FILENO, error_message, strlen(error_message));
					break;	
				}
				else {
					write(STDERR_FILENO, error_message, strlen(error_message));
					break;
				}
			}
			ret = execvp(args[0],args);
			if (ret) {
				write(STDERR_FILENO, error_message, strlen(error_message));
			}
			break;
		}
		else if (fork_ret > 0) {
			if (flag_bg){
				bg[(bg_count++)%MAX_BG_COUNT] = fork_ret;
				signal(SIGCHLD, SIG_IGN);				
				continue;
			}
			waitpid(fork_ret, NULL, 0);
		}
		else {
			write(STDERR_FILENO, error_message, strlen(error_message));
			break;
		}
	
	}

	return 0;
}
