/* 
 * myshell.h
 */
/* $begin myshell.h */
#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#include <sys/types.h>
#include <termios.h>

#define BUF_SIZE	512
#define ARGV_SIZ	10
#define DFL_PROMPT	"> "

/* Default file permissions are DEF_MODE & ~DEF_UMASK */
#define DEF_MODE	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#define DEF_UMASK	S_IWGRP | S_IWOTH


/*************
 * Job Control
 ************/
/* $begin job control */
/* I/O Redirection */
typedef struct io_redirect
{
	char *dest;
	int is_append;
} io_redirect;

/* A process is a single process. */
typedef struct process
{
	struct process *next;       /* next process in pipeline */
	char **argv;                /* for exec */
 	io_redirect io_re[3];		/* for I/O redirection */
	pid_t pid;                  /* process ID */
	char completed;             /* true if process has completed */
	char stopped;               /* true if process has stopped */
	int status;                 /* reported status value */
} process;

/* A job is a pipeline of processes. */
typedef struct job
{
	struct job *next;           /* next active job */
	char *command;              /* command line, used for messages */
	process *first_process;     /* list of processes in this job */
	pid_t jid;				 	/* job ID */
	pid_t pgid;                 /* process group ID */
	char notified;              /* true if user told about stopped job */
	struct termios tmodes;      /* saved terminal modes */
	int stdin, stdout, stderr;  /* standard i/o channels */
} job;

/* The active jobs are linked into a list. This is its head. */
extern job *first_job;

extern job *current_job;
extern pid_t job_id;

extern struct termios shell_tmodes;

extern int foreground;

extern job * find_job (pid_t jid);
extern void continue_job (job * j, int foreground);
extern void free_job (job * j);
extern void init_shell (void);
extern void format_job_info (job * j, const char * status);
extern int job_is_stopped (job * j);
extern int job_is_completed (job * j);
extern void launch_job (job *j, int foreground);
extern void update_status (void);
extern void do_job_notification (void);
/* $end job control */


/*************
 * Get Command
 ************/
/* $begin get command */
extern char * next_cmd (char * prompt, FILE * fp);
extern int cmd_is_empty (char * cmdline);
/* $end get command */


/******************
 * Evaluate Command
 *****************/
/* $begin evaluate command */
extern int eval_cmd (char * cmdline);
/* $end evaluate command */


/*****************
 * Builtin Command
 ****************/
/* $begin builtin command */
extern int builtin_cmd (job * j);
/* $end builtin command */


#endif /* __MYSHELL_H__ */
/* $end myshell.h */