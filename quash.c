#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>



typedef void (*sighandler_t)(int);
char c = '\0';

void signal_handler(int signo)
{
	printf("\nquash -> ");
	fflush(stdout);
}

int main(int argc, char *argv[], char *envp[])
{
	signal(SIGINT, SIG_IGN);
	signal(SIGINT, signal_handler);
	printf("\nquash -> ");
	while(c != EOF) {
		c = getchar();
		if(c == '\n')
			printf("quash-> ");
	}
	printf("\n");
	return 0;
}
