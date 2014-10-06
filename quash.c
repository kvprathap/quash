#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>

#define BMAX 100
#define TRUE 1
#define FALSE 0
static char cmd_buffer[BMAX];
static char* myargv[5];
static int myargc = 0;
static char input = '\0';
static int buff_chars = 0;
static int pos;
static char* cwd;
typedef void (*sighandler_t)(int);


typedef enum execution_mode
{
	FG=0,
	BG
}mode;

typedef enum redirection
{
	READ=0,
	WRITE
}redirection;

void shell_display()
{
	printf("\nquash -> ");
}

void signal_handler(int signo)
{
	shell_display();
	fflush(stdout);
}

void init_command()
{
	while (myargc != 0) {
                myargv[myargc] = NULL;
                myargc--;
        }
        buff_chars = 0;
}



void read_command_line()
{
	init_command();
	char *buf_ptr;
	while ((input != '\n'))
	{
		cmd_buffer[buff_chars++] = input;
		input = getchar();
	}
	cmd_buffer[buff_chars] = 0x00;
	buf_ptr = strtok(cmd_buffer, " ");
	while (buf_ptr != NULL){
		myargv[myargc] = buf_ptr;
		buf_ptr = strtok(NULL, " ");
		myargc++;
	}
}

int check_for_symbol(char* token)
{
	pos = 0;
	for (; myargv[pos] != NULL; myargv[pos++]) {
		if (token) {
			if (strcmp(token, myargv[pos]) == 0)
				return BG;
		}
		else {
			
			if (strcmp("<", myargv[pos]) == 0)
				return READ;
			else if (strcmp(">", myargv[pos]) == 0)
				return WRITE;
		}
	}
	return 0;
}

void change_directory()
{
        if (myargv[1] == NULL) {
                chdir(getenv("HOME"));
        } else {
                if (chdir(myargv[1]) == -1) {
                        printf(" %s: no such directory\n", myargv[1]);
                }
        }
}

/* implement exit,cd,jobs,kill*/
int parse_for_shell_commands()
{
	mode mode;
	redirection redirection;
	if (strcmp("exit", myargv[0]) == 0) {
                exit(EXIT_SUCCESS);
        }
        if (strcmp("cd", myargv[0]) == 0) {

                change_directory();
                return 1;
        }
	mode  = check_for_symbol("&");
	printf("mode = %d\n",mode);
	redirection = check_for_symbol('\0');
	printf("redirection = %d\n",redirection);
	/*pos + 1 has the filename*/


	return 0;
}

void execute_command (char *command[])
{
	if (execvp(*command, command) == -1)
                perror("quash");
}

void start_job()
{


}

void handle_command()
{
	if (parse_for_shell_commands() == 0) {
		//start_job(myargv);
	//	execute_command(myargv);
	}
}

void initialize()
{
        
        
                signal(SIGQUIT, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
                signal(SIGINT, SIG_IGN);
                shell_display();
                fflush(stdout);
                
}



int main(int argc, char *argv[], char *envp[])
{	
	
	initialize();
	while(TRUE) {
		input = getchar();
		switch(input) {
			case '\n':
				shell_display();
				break;
			default:
				read_command_line();
				handle_command();
				shell_display();
				break;
		}
	}
	printf("\n");
	return 0;
}
