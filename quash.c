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
//#define MAXJOB 20
#define SUSPENDED 'S'
#define WAITING_INPUT 'W'
//#define STDIN 1
//#define STDOUT 2


#define BY_PROCESS_ID 1
#define BY_JOB_ID 2
#define BY_JOB_STATUS 3
static char cmd_buffer[BMAX];
static char* myargv[15];
static int myargc = 0;
static char input = '\0';
static int buff_chars = 0;
static int pos;
//static char* cwd;
//typedef void (*sighandler_t)(int);
int status;
int fdout,fdin;
static int QUASH_TERMINAL;
static int num_active_jobs = 0;
static char* currentDirectory;
static pid_t QUASH_PID;
static pid_t QUASH_PGID;
static int QUASH_TERMINAL, QUASH_IS_INTERACTIVE;
static struct termios QUASH_TMODES;



typedef struct job {
        int id;
        char *name;
        pid_t pid;
        pid_t pgid;
        int status;
        char *descriptor;
        struct job *next;
} t_job;

static t_job* jobslist = NULL;

typedef enum execution_mode
{
	FG=0,
	BG
}mode;

typedef enum redirection
{
	EMPTY=0,
	READ,
	WRITE,
	PIPE
}redirection;

void shell_display()
{
	printf("\nquash -> ");
}

void signal_handler(int signo)
{
	fflush(stdout);
	shell_display();
}
t_job* deljob(t_job* job)
{
        usleep(10000);
        if (jobslist == NULL)
                return NULL;
        t_job* currentjob;
        t_job* beforecurrentjob;

        currentjob = jobslist->next;
        beforecurrentjob = jobslist;

        if (beforecurrentjob->pid == job->pid) {

                beforecurrentjob = beforecurrentjob->next;
                num_active_jobs--;
                return currentjob;
        }

        while (currentjob != NULL) {
                if (currentjob->pid == job->pid) {
                        num_active_jobs--;
                        beforecurrentjob->next = currentjob->next;
                }
                beforecurrentjob = currentjob;
                currentjob = currentjob->next;
        }
        return jobslist;
}


void waitjob(t_job* job)
{
        int terminationStatus; 
        while (waitpid(job->pid, &terminationStatus, WNOHANG) == 0) {
                if (job->status == SUSPENDED)
                        return;
        }
        jobslist = deljob(job);
}

t_job* getjob(int searchValue, int searchParameter)
{
        usleep(10000);
        t_job* job = jobslist;
        switch (searchParameter) {
        case BY_PROCESS_ID:
                while (job != NULL) {
                        if (job->pid == searchValue)
                                return job;
                        else
                                job = job->next;
                }
                break;
        case BY_JOB_ID:
                while (job != NULL) {
                        if (job->id == searchValue)
                                return job;
                        else
                                job = job->next;
                }
                break;
        case BY_JOB_STATUS:
                while (job != NULL) {
                        if (job->status == searchValue)
                                return job;
                        else
                                job = job->next;
                }
                break;
        default:
                return NULL;
                break;
        }
        return NULL;
}

void killjob(int jobId)
{
        t_job *job = getjob(jobId, BY_JOB_ID);
	printf("pid = %d\n",job->pid);
        kill(job->pid, SIGKILL);
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
			else if (strcmp("|", myargv[pos]) == 0)
				return PIPE;
		}
	}
	return 0;
}


void display_jobs()
{
        printf("\nActive jobs:\n");
        printf(
                "---------------------------------------------------------------------------\n");
        printf("| %7s  | %30s | %5s | %10s | %6s |\n", "job no.", "name", "pid",
               "descriptor", "status");
        printf(
                "---------------------------------------------------------------------------\n");
        t_job* job = jobslist;
        if (job == NULL) {
                printf("| %s %62s |\n", "No Jobs.", "");
        } else {
                while (job != NULL) {
                        printf("|  %7d | %30s | %5d | %10s | %6c |\n", job->id, job->name,
                               job->pid, job->descriptor, job->status);
                        job = job->next;
                }
        }
        printf(
                "---------------------------------------------------------------------------\n");
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
	if (strcmp("exit", myargv[0]) == 0) { // Implementing exit command
                exit(EXIT_SUCCESS);
        }
        if (strcmp("cd", myargv[0]) == 0) {   //Implementing cd command

                change_directory();
                return 1;
        }
	if (strcmp("quit", myargv[0]) == 0) { // Implementing quit command
                exit(EXIT_SUCCESS);
        }
	
	if (strcmp("jobs", myargv[0]) == 0) { // Printing all jobs
                display_jobs();
		return 1;
        }
	
	if (strcmp("kill", myargv[0]) == 0) { // Killing a job
                
		int jobid = atoi(myargv[1]);
		killjob(jobid);
                return 1;
        }

	/*TODO:Set*/
    	return 0;


	
} 

void putJobForeground(t_job* job, int continueJob)
{
        job->status = FG;
        tcsetpgrp(QUASH_TERMINAL, job->pgid);
        if (continueJob) {
                if (kill(-job->pgid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");
        }

        waitjob(job);
        tcsetpgrp(QUASH_TERMINAL, QUASH_PGID);
}

void putJobBackground(t_job* job, int continueJob)
{
        if (job == NULL)
                return;

        if (continueJob && job->status != WAITING_INPUT)
                job->status = WAITING_INPUT;
        if (continueJob)
                if (kill(-job->pgid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");

        tcsetpgrp(QUASH_TERMINAL, QUASH_PGID);
}

t_job* insertjob(pid_t pid, pid_t pgid, char* name, char* descriptor,
                 int status)
{
        usleep(10000);
        t_job *newJob = malloc(sizeof(t_job));

        newJob->name = (char*) malloc(sizeof(name));
        newJob->name = strcpy(newJob->name, name);
        newJob->pid = pid;
        newJob->pgid = pgid;
        newJob->status = status;
        newJob->descriptor = (char*) malloc(sizeof(descriptor));
        newJob->descriptor = strcpy(newJob->descriptor, descriptor);
        newJob->next = NULL;

        if (jobslist == NULL) {
                num_active_jobs++;
                newJob->id = num_active_jobs;
                return newJob;
        } else {
                t_job *auxNode = jobslist;
                while (auxNode->next != NULL) {
                        auxNode = auxNode->next;
                }
                newJob->id = auxNode->id + 1;
                auxNode->next = newJob;
                num_active_jobs++;
                return jobslist;
        }
}

void execute_command (char *command[])
{
		close(fdin);
		close(fdout);
		if (execvp(*command, command) == -1)
                	perror("quash");
}

int changeJobStatus(int pid, int status)
{
        usleep(10000);
        t_job *job = jobslist;
        if (job == NULL) {
                return 0;
        } else {
                int counter = 0;
                while (job != NULL) {
                        if (job->pid == pid) {
                                job->status = status;
                                return TRUE;
                        }
                        counter++;
                        job = job->next;
                }
                return FALSE;
        }
}

void signalHandler_child(int p)
{
        pid_t pid;
        int terminationStatus;
        pid = waitpid(WAIT_ANY, &terminationStatus, WUNTRACED | WNOHANG);
        if (pid > 0) {
                t_job* job = getjob(pid, BY_PROCESS_ID);
                if (job == NULL)
                        return;
                if (WIFEXITED(terminationStatus)) {
                        if (job->status == BG) {
                                printf("\n[%d]+  Done\t   %s\n", job->id, job->name);
                                jobslist = deljob(job);
                        }
                } else if (WIFSIGNALED(terminationStatus)) {
                        printf("\n[%d]+  KILLED\t   %s\n", job->id, job->name);
                        jobslist = deljob(job);
                } else if (WIFSTOPPED(terminationStatus)) {
                        if (job->status == BG) {
                                tcsetpgrp(QUASH_TERMINAL, QUASH_PGID);
                                changeJobStatus(pid, WAITING_INPUT);
                                printf("\n[%d]+   suspended [wants input]\t   %s\n",
                                       num_active_jobs, job->name);
                        } else {
                                tcsetpgrp(QUASH_TERMINAL, job->pgid);
                                changeJobStatus(pid, SUSPENDED);
                                printf("\n[%d]+   stopped\t   %s\n", num_active_jobs, job->name);
                        }
                        return;
                } else {
                        if (job->status == BG) {
                                jobslist = deljob(job);
                        }
                }
                tcsetpgrp(QUASH_TERMINAL, QUASH_PGID);
        }
}


void init()
{
        QUASH_PID = getpid();
        QUASH_TERMINAL = STDIN_FILENO;
        QUASH_IS_INTERACTIVE = isatty(QUASH_TERMINAL);

        if (QUASH_IS_INTERACTIVE) {
                while (tcgetpgrp(QUASH_TERMINAL) != (QUASH_PGID = getpgrp()))
                        kill(QUASH_PID, SIGTTIN);

                signal(SIGQUIT, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
                signal(SIGINT, SIG_IGN);
                signal(SIGCHLD, &signalHandler_child);

                setpgid(QUASH_PID, QUASH_PID);
                QUASH_PGID = getpgrp();
                if (QUASH_PID != QUASH_PGID) {
                        printf("Error, the shell is not process group leader");
                        exit(EXIT_FAILURE);
                }
                if (tcsetpgrp(QUASH_TERMINAL, QUASH_PGID) == -1)
                        tcgetattr(QUASH_TERMINAL, &QUASH_TMODES);

                currentDirectory = (char*) calloc(1024, sizeof(char));
        } else {
                printf("Could not make QUASH interactive. Exiting..\n");
                exit(EXIT_FAILURE);
        }
                shell_display();
                fflush(stdout);
}

void execute_pipe (char* command1[], char* command2[])
{
	int my_pipe[2];
	pipe(my_pipe);
	int pid;
	pid = fork();
	if(pid ==0) {
		dup2(my_pipe[1],STDOUT_FILENO);
    		close(my_pipe[0]);
    		close(my_pipe[1]);
		if (execvp(*command1, command1) == -1)
                        perror("quash");
    		exit(0);
	}
	else {
		dup2(my_pipe[0],STDIN_FILENO);
    		close(my_pipe[1]);
    		close(my_pipe[0]);
    		waitpid(pid,&status, 0);
		if (execvp(*command2, command2) == -1)
                        perror("quash");
	}	
}


void start_job(char *command[], char *file)
{
	char* filename = NULL;
	pid_t pid;
	mode mode = 0;
	redirection redirection = 0;
	mode = check_for_symbol("&");
	pid = fork();
	if (pid == 0) {
		signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGCHLD, &signalHandler_child);
                signal(SIGTTIN, SIG_DFL);
                usleep(20000);
		setpgrp();
		if (mode == FG)
			tcsetpgrp(QUASH_TERMINAL, getpid());
		else
			 printf("[1] %d\n",(int) getpid());

		redirection = check_for_symbol('\0');
		printf("redirection = %d\n",redirection);	
		if ( (check_for_symbol('\0')) == PIPE)
		{
			command[pos] = '\0';
			execute_pipe(command,&command[pos+1]);
			exit (0);

		}
		else if ( (check_for_symbol('\0')) == WRITE)
		{
			filename = command[pos + 1];
			command[pos] = '\0';
			printf("%s\n",filename); 
			fdout = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
			dup2(fdout, STDOUT_FILENO);
          		execute_command(command);
          		close(fdout);
          		exit(0);
        	}

		/*TODO: READ*/
		else if( (check_for_symbol('\0')) == READ) {
		}
			
		else {
          		execute_command(command);
			exit(0);
		}
	}
	else {
		setpgid(pid, pid);
		jobslist = insertjob(pid, pid, *(command), file, (int)mode);
		t_job *job = getjob(pid, BY_PROCESS_ID);
		if (mode == FG) {
			putJobForeground(job, FALSE);
		}
		else
			putJobBackground(job, FALSE);
	}
}

void handle_command()
{
	if (parse_for_shell_commands() == 0) {
		start_job(myargv, "STANDARD");
	}
}



int main(int argc, char *argv[], char *envp[])
{	
	
	init();
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
