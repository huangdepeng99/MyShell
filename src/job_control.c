/* 
 * job_control.c
 */
/* $begin job_control.c */
#define _POSIX_C_SOURCE 1	/* for kill(), see the man pages KILL(2) and FEATURE_TEST_MACROS(7) */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include "myshell.h"


/* The active jobs are linked into a list. This is its head. */
job *first_job = NULL;

job *current_job = NULL;
pid_t job_id = 1;

/* Keep track of attributes of the shell. */
static pid_t shell_pgid;
struct termios shell_tmodes;
static int shell_terminal;

int foreground = 1;


/* Find the active job with the indicated jid. */
job *
find_job (pid_t jid)
{
	job *j;

	for(j = first_job; j; j = j->next)
	{
		if(j == current_job)
			continue;

		if(j->jid == jid)
			return j;
	}

	return NULL;
}


/* Mark a stopped job J as being running again. */
static
void
mark_job_as_running (job * j)
{
	process *p;

	for(p = j->first_process; p; p = p->next)
		p->stopped = 0;
	j->notified = 0;
}


/* Store the status of the process pid that was returned by waitpid.
 * Return 0 if all went well, nonzero otherwise.
 */
static
int
mark_process_status (pid_t pid, int status)
{
	job *j;
	process *p;


	if(pid > 0){
		/* Update the record for the process. */
    	for(j = first_job; j; j = j->next)
    	{
        	for(p = j->first_process; p; p = p->next)
        	{
        		if(p->pid == pid){
            		p->status = status;
            		if(WIFSTOPPED(status)){
            			p->stopped = 1;
            		}else{
                		p->completed = 1;
                		if(WIFSIGNALED(status))
                    		fprintf(stderr, "%d: Terminated by signal %d.\n",
                            		(int) pid, WTERMSIG(p->status));
                	}
            		return 0;
            	}
            }
    	}
    	fprintf(stderr, "No child process %d.\n", pid);
    	return -1;
    }else if(pid == 0 || errno == ECHILD){
    	/* No processes ready to report. */
    	return -1;
	}else{
    	/* Other weird errors. */
    	perror("waitpid");
    	return -1;
	}
}


/* Check for processes that have status information available,
 * blocking until all processes in the given job have reported.
 */
static
void
wait_for_job (job * j)
{
	int status;
	pid_t pid;

	do{
		pid = waitpid(-1, &status, WUNTRACED);
	}while(!mark_process_status(pid, status)
		   && !job_is_stopped(j)
		   && !job_is_completed(j));
}


/* Put job j in the foreground. If cont is nonzero,
 * restore the saved terminal modes and send the process group a
 * SIGCONT signal to wake it up before we block.
 */
static
void
put_job_in_foreground (job * j, int cont)
{
	/* Put the job into the foreground. */
	tcsetpgrp(shell_terminal, j->pgid);


	/* Send the job a continue signal, if necessary. */
	if(cont){
    	tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
    	if(kill(- j->pgid, SIGCONT) < 0)
    		perror ("kill (SIGCONT)");
    }


	/* Wait for it to report. */
	wait_for_job(j);

	/* Put the shell back in the foreground. */
	tcsetpgrp(shell_terminal, shell_pgid);

	/* Restore the shell’s terminal modes. */
	tcgetattr(shell_terminal, &j->tmodes);
	tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}


/* Put a job in the background. If the cont argument is true, send
 * the process group a SIGCONT signal to wake it up.
 */
static
void
put_job_in_background (job * j, int cont)
{
	/* Send the job a continue signal, if necessary. */
	if(cont){
		if(kill(- j->pgid, SIGCONT) < 0)
			perror("kill (SIGCONT)");
	}
}


/* Continue the job J. */
void
continue_job (job * j, int foreground)
{
	mark_job_as_running(j);
	if(foreground)
		put_job_in_foreground(j, 1);
	else
		put_job_in_background(j, 1);
}


void
free_job (job * j)
{
	process *p = j -> first_process;

	while(p != NULL){
		int index = 0;
		char *tmp_s;
		while((tmp_s = (p -> argv)[index]) != NULL){
			free(tmp_s);
			index++;
		}
		free(p -> argv);

		index = 0;
		while(index < 3){
			if((tmp_s = ((p -> io_re)[index]).dest) != NULL)
				free(tmp_s);
			index++;
		}

		process *pnext = p -> next;
		free(p);
		p = pnext;
	}

	free(j -> command);
	free(j);
}


/* Make sure the shell is running as the foreground job
 * before proceeding.
 */
void
init_shell (void)
{
	shell_terminal = STDIN_FILENO;

    /* Loop until we are in the foreground. */
    while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
    	kill(- shell_pgid, SIGTTIN);

    /* Ignore job-control signals. */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    /* Note: Even though the default disposition of SIGCHLD is "ignore", explicitly setting 
     * the disposition to SIG_IGN results in different treatment of zombie process children.
     * See the ERRORS and NOTES sections of the man page wait(2).
     */
    /* signal(SIGCHLD, SIG_IGN); */

    /* Put ourselves in our own process group. */
    shell_pgid = getpid();
    if(setpgid(shell_pgid, shell_pgid) < 0){
		perror("Couldn't put the shell in its own process group");
		exit(1);
	}

    /* Grab control of the terminal. */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save default terminal attributes for shell. */
    tcgetattr(shell_terminal, &shell_tmodes);
}


static
void
launch_process (process *p, pid_t pgid,
                int infile, int outfile, int errfile,
                int foreground)
{
	pid_t pid;

    /* Put the process into the process group and give the process group
       the terminal, if appropriate.
       This has to be done both by the shell and in the individual
       child processes because of potential race conditions.
     */
    pid = getpid();
    if(pgid == 0)
    	pgid = pid;
    setpgid(pid, pgid);
    if(foreground)
    	tcsetpgrp(shell_terminal, pgid);

    /* Set the handling for job control signals back to the default. */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    /* signal(SIGCHLD, SIG_DFL); */

	/* Set the standard input/output channels of the new process. */
	if(infile != STDIN_FILENO){
    	dup2(infile, STDIN_FILENO);
    	close(infile);
    }

	if(outfile != STDOUT_FILENO){
    	dup2(outfile, STDOUT_FILENO);
    	close(outfile);
    }

	if(errfile != STDERR_FILENO){
    	dup2(errfile, STDERR_FILENO);
    	close(errfile);
    }

	/* Exec the new process. make sure we exit. */
	if((p->argv)[0] == NULL)
		exit(0);
	execvp(p->argv[0], p->argv);
	perror ("execvp");
	exit (1);
}


/* Format information about job status for the user to look at. */
void
format_job_info (job * j, const char * status)
{
	fprintf(stderr, "[%ld] %ld (%s): %s\n", (long)j->jid, (long)j->pgid, status, j->command);
}


/* Return true if all processes in the job have stopped or completed. */
int
job_is_stopped (job * j)
{
	process *p;

	for(p = j->first_process; p; p = p->next)
	{
		if(!p->completed && !p->stopped)
    		return 0;
	}
	return 1;
}


/* Return true if all processes in the job have completed. */
int
job_is_completed (job * j)
{
	process *p;

	for(p = j->first_process; p; p = p->next)
	{
		if(!p->completed)
    		return 0;
	}
	return 1;
}


void
launch_job (job *j, int foreground)
{
	process *p;
	pid_t pid;
	int mypipe[2], infile, outfile, errfile;

	infile = j -> stdin;

	p = j -> first_process;
	while(p != NULL){
		/* set up pipes, if necessary */
		if(p -> next != NULL){
			if(pipe(mypipe) < 0){
				perror ("pipe");
            	exit (1);
            }
        	outfile = mypipe[1];
        }else
        	outfile = j -> stdout;


    	/* fork the child processes */
    	pid = fork();
    	if(pid == 0){	/* this is the child process */
    		/* Note: need error processing!!! */
    		if(p -> next != NULL)
    			close(mypipe[0]);	/* if return -1 ? */

	        /* process I/O redirection */
        	char *filename;
        	if((filename = ((p -> io_re)[0]).dest) != NULL){	/* standard input */
        		if(infile != j->stdin)
        			close(infile);
        		infile = open(filename, O_RDONLY);	/* if return -1 ? */
    		}

        	umask(DEF_UMASK);
        	if((filename = ((p -> io_re)[1]).dest) != NULL){	/* standard output */
        		if(((p -> io_re)[1]).is_append)
        			outfile = open(filename, O_WRONLY | O_CREAT | O_APPEND, DEF_MODE);
        		else
        			outfile = open(filename, O_WRONLY | O_CREAT | O_TRUNC, DEF_MODE);
        		if(p -> next != NULL)
        			close(mypipe[1]);
        	}

        	if((filename = ((p -> io_re)[2]).dest) != NULL){	/* standard error */
        		if(((p -> io_re)[2]).is_append)
        			errfile = open(filename, O_WRONLY | O_CREAT | O_APPEND, DEF_MODE);
        		else
        			errfile = open(filename, O_WRONLY | O_CREAT | O_TRUNC, DEF_MODE);
        	}else
        		errfile = j -> stderr;

        	/* call launch_process() */
        	launch_process(p, j->pgid, infile, outfile, errfile, foreground);
        }else if(pid < 0){	/* the fork failed */
        	perror ("fork");
        	exit (1);
        }else{	/* this is the parent process */
        	p -> pid = pid;
            if(!(j -> pgid))
            	j -> pgid = pid;
            setpgid(pid, j->pgid);
        }

    	/* clean up after pipes */
    	if(infile != j->stdin)
    		close(infile);
    	if(outfile != j->stdout)
        	close(outfile);
      	infile = mypipe[0];

      	p = p -> next;
    }

	if(foreground){
    	put_job_in_foreground(j, 0);
	}else{
		format_job_info(j, "Launched");
    	put_job_in_background(j, 0);
	}
}


/* Check for processes that have status information available,
 * without blocking.
 */
void
update_status (void)
{
	int status;
	pid_t pid;

	do{
		pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
	}while(!mark_process_status(pid, status));
}


/* Notify the user about stopped or terminated jobs.
 * Delete terminated jobs from the active job list.
 */
void
do_job_notification (void)
{
	job *j, *jlast, *jnext;

	/* Update status information for child processes. */
	update_status();

	jlast = NULL;
	for(j = first_job; j; j = jnext)
	{
    	jnext = j->next;

    	if(job_is_completed(j)){
			/* If all processes have completed, tell the user the job has
        	 * completed and delete it from the list of active jobs.
        	 */
    		format_job_info(j, "Completed");
    		if(jlast)
        		jlast->next = jnext;
        	else
        		first_job = jnext;
        	free_job(j);
    	}else if(job_is_stopped(j) && !j->notified){
      		/* Notify the user about stopped jobs,
			 * marking them so that we won’t do this more than once.
      		 */
        	format_job_info(j, "Stopped");
        	j->notified = 1;
        	jlast = j;
    	}else
    	    /* Don't say anything about jobs that are still running. */
        	jlast = j;
    }
}


/* $end builtin_cmd.c */