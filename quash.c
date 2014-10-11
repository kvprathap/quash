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
#define SUSPENDED 'S'
#define WAITING_INPUT 'W'

#define PATH "set path="

#define BY_PROCESS_ID 1
#define BY_JOB_ID 2
#define BY_JOB_STATUS 3
static char cmd_buffer[BMAX];
static char* myargv[15];	//tokenised commad buffer
static int myargc = 0;
static char input = '\0';
static int buff_chars = 0;
static int pos;			//position of < > | &
char *input_command;
int status;
int fdout,fdin;
static int QUASH_TERMINAL;
static int num_active_jobs = 0;
static char* currentDirectory;
static pid_t QUASH_PID;
static pid_t QUASH_PGID;
static int QUASH_TERMINAL, QUASH_IS_INTERACTIVE;
static struct termios QUASH_TMODES;

/* Job information */

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
/*Declaration  for ForeGround and BackGround*/
typedef enum execution_mode
{
	FG=0,
	BG
}mode;

/*Declaration for < > |*/
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

/* Delete job from the queue */
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

/* Tokenise the command buffer */

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

/* Check for BG FG and redirection symbols and updates its token position*/
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


/* Executes builtin shell commands like exit,cd,jobs,kill,set*/
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
	
	if (strcmp("set", myargv[0]) == 0) { // Killing a job

                int temp = set();
                
                return 1;
        }

	/*TODO:Set*/
    	return 0;


	
} 
/*Sets the PATH and HOME*/
int set(){
int i=0;int temp;
int length=0;
char *path = myargv[1];
input_command = (char *)malloc(sizeof(char)*100);
for(i=0;i<100;i++){
input_command[i] = '\0';
i++;
}
length = strlen(myargv[1]);

if((strncmp(path,"HOME=",5))==0){
	strncpy(input_command,path+5,(length-5));
	printf("Now input command is : %s",input_command);
	//scanf("%d",&temp);
	if((setenv("HOME",input_command,1))==-1){
		printf("could not set the given path to HOME");
	}
}
if((strncmp(path,"PATH=",5))==0){
//	strncpy(input_command,myargv[1]+5,length-5);
strncpy(input_command,path+5,(length-5));
	if((setenv("PATH",input_command,1))==-1){
		printf("could not set the given path to PATH");
	}
}

return 1;
}



void putjobforeground(t_job* job, int continuejob)
{
        job->status = FG;
        tcsetpgrp(QUASH_TERMINAL, job->pgid);
        if (continuejob) {
                if (kill(-job->pgid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");
        }

        waitjob(job);
        tcsetpgrp(QUASH_TERMINAL, QUASH_PGID);
}

void putjobbackground(t_job* job, int continuejob)
{
        if (job == NULL)
                return;

        if (continuejob && job->status != WAITING_INPUT)
                job->status = WAITING_INPUT;
        if (continuejob)
                if (kill(-job->pgid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");

        tcsetpgrp(QUASH_TERMINAL, QUASH_PGID);
}

t_job* insertjob(pid_t pid, pid_t pgid, char* name, char* descriptor,
                 int status)
{
        usleep(10000);
        t_job *newjob = malloc(sizeof(t_job));

        newjob->name = (char*) malloc(sizeof(name));
        newjob->name = strcpy(newjob->name, name);
        newjob->pid = pid;
        newjob->pgid = pgid;
        newjob->status = status;
        newjob->descriptor = (char*) malloc(sizeof(descriptor));
        newjob->descriptor = strcpy(newjob->descriptor, descriptor);
        newjob->next = NULL;

        if (jobslist == NULL) {
                num_active_jobs++;
                newjob->id = num_active_jobs;
                return newjob;
        } else {
                t_job *auxNode = jobslist;
                while (auxNode->next != NULL) {
                        auxNode = auxNode->next;
                }
                newjob->id = auxNode->id + 1;
                auxNode->next = newjob;
                num_active_jobs++;
                return jobslist;
        }
}
/* Search for executables in the directories specified by PATH 
 * environment variablei and execute them
*/
void execute_command (char *command[])
{
		close(fdin);
		if (execvp(*command, command) == -1)
                	perror("quash");
}

int changejobStatus(int pid, int status)
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
/*Signal handler of chils */
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
                                changejobStatus(pid, WAITING_INPUT);
                                printf("\n[%d]+   suspended [wants input]\t   %s\n",
                                       num_active_jobs, job->name);
                        } else {
                                tcsetpgrp(QUASH_TERMINAL, job->pgid);
                                changejobStatus(pid, SUSPENDED);
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

/* Initialization routine for Quash Shell */
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

/* Execute one level pipe */
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

/*If the command is not builtin shell command 
**this function is called to process other commands
*/ 
void start_job(char *command[], char *file)
{
	char* filename = NULL;
	pid_t pid;
	mode mode = 0;
	/*FG/BG*/
	mode = check_for_symbol("&");
	pid = fork();
	char* s;
	if (pid == 0) {
		signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGCHLD, &signalHandler_child);
                signal(SIGTTIN, SIG_DFL);
                usleep(20000);
		/*Setting terminal FG/BG jobs*/
		setpgrp();
		if (mode == FG)
			tcsetpgrp(QUASH_TERMINAL, getpid());
		else
			 printf("[1] %d\n",(int) getpid());

		/*PIPE*/
		if ( (check_for_symbol('\0')) == PIPE)
		{
			command[pos] = '\0';
			execute_pipe(command,&command[pos+1]);
			exit (0);

		}
		/*WRITE*/
		else if ( (check_for_symbol('\0')) == WRITE)
		{
			filename = command[pos + 1];
			command[pos] = '\0';
			fdout = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
			dup2(fdout, STDOUT_FILENO);
          		execute_command(command);
          		close(fdout);
          		exit(0);
        	}

		/*READ*/
		else if( (check_for_symbol('\0')) == READ) {
			FILE* fp;
			char* argsv[10];
			char *input;
			char* s = NULL;
			filename = command[pos + 1];
                        command[pos] = '\0';
			/*Read the commands from file and executes*/
			if(strcmp(myargv[0],"quash") == 0){
				fp = fopen(filename, "r");
				if(fp == NULL)
					printf("error\n");
				while (fgets(input, 1024, fp) != NULL) {
				 	s = strchr(input, '\n');
					if (s != NULL) {
						input[strlen(input)-1] = '\0';
					}
				argsv[0] = input;
                        	execute_command(argsv);
				}
				fclose(fp);
			}
			/*else normal read*/
			else {

                        	fdin = open(filename, O_RDONLY);
				dup2(fdin, STDIN_FILENO);
                        	execute_command(command);
                        	close(fdin);
          			exit(0);
			}
                        exit(0);

		}
			
		else {
          		execute_command(command);
			exit(0);
		}
	}
	else {
		/*Parent to put the job in FG/BG*/
		setpgid(pid, pid);
		jobslist = insertjob(pid, pid, *(command), file, (int)mode);
		t_job *job = getjob(pid, BY_PROCESS_ID);
		if (mode == FG) {
			putjobforeground(job, FALSE);
		}
		else
			putjobbackground(job, FALSE);
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
