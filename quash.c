#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define BMAX 100
static char cmd_buffer[BMAX];
static char *myargv[5];
static int myargc = 0;

typedef void (*sighandler_t)(int);
char c = '\0';

void shell_display()
{
	printf("\nquash -> ");
}

void signal_handler(int signo)
{
	shell_display();
	fflush(stdout);
}


void read_command_line()
{
	int blen = 0;
	char *buf_ptr;
	while ((c != '\n'))
	{
		cmd_buffer[blen++] = c;
		c = getchar();
	}
	buf_ptr = strtok(cmd_buffer, " ");
	while(buf_ptr != NULL){
		myargv[myargc] = buf_ptr;
		buf_ptr = strtok(NULL, " ");
		myargc++;
	}
}

void execute_command(char* cmd[])
{
	int error;
	pid_t pid;
	pid = fork();
	if(pid == 0) {
		error = execvp(*cmd, cmd);
		if (error < 0) {
			printf("command not found\n");
			exit(1);
		}
	}
	else
		wait(NULL);
			
}

int main(int argc, char *argv[], char *envp[])
{
	signal(SIGINT, SIG_IGN);
	signal(SIGINT, signal_handler);
	shell_display();
	while(c != EOF) {
		c = getchar();

		switch(c) {
			case '\n':
				shell_display();
				break;
			default:
				read_command_line();
				execute_command(myargv);
				shell_display();
				break;
		}
				
	}
	printf("\n");
	return 0;
}
