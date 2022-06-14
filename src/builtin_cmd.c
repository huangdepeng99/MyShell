/* 
 * builtin_cmd.c
 * 
 * Note: 
 *   If you want to add a built-in command, you need to provide its handler function, 
 *   which has the following function prototype:
 *     int bc_do_<name> (int argc, char ** argv)
 *   The handler should return -1 on error and 1 on success.
 *   You also need to append _(<name>) to the macro FORALL_BC(_), and you also need to modify 
 *   the macro HELP_MESSAGE to make the built-in help work correctly.
 */
/* begin builtin_cmd.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "myshell.h"
#include "historylib.h"
#include "variablelib.h"
#include "wrapper.h"

typedef int (*bchandler_t)(int, char **);

typedef struct bc_entry
{
	char * name;
	bchandler_t handler;
} bc_entry;

#define FORALL_BC(_)        _(exit) _(help) _(history) _(set) _(unset) _(pwd) _(cd) _(jobs) _(fg) _(bg)
#define ADD_BC_ENTRY(NAME)  {#NAME, bc_do_##NAME},

#define HELP_MESSAGE  "Builtin command: \n" \
	                  "  exit - Exit the shell.\n" \
	                  "  help - Display information about builtin commands.\n" \
	                  "  history - Display the history list.\n" \
	                  "  set [<name>] [<value>] - 1. set : Check the names and values of all shell variables.\n" \
	                  "                           2. set <name> : Check the value of the variable named <name>.\n" \
	                  "                           3. set <name> <value> : Create a new variable with the name <name> and the \n" \
	                  "                              value <value>, or update the value of the variable named <name> to <value>.\n" \
	                  "  unset <name> - Delete the shell variable named <name>.\n" \
	                  "  pwd - Print the absolute pathname of the current working directory.\n" \
	                  "  cd <dir> - Change the current working directory to <dir>.\n" \
	                  "  jobs - Display status of jobs.\n" \
	                  "  fg <job_id> - Move job to the foreground.\n" \
	                  "  bg <job_id> - Move job to the background.\n" \
	                  "\n" \
	                  "Note: Builtin commands does not support pipelines and I/O redirection.\n"


/* begin handler */

static
int
bc_do_exit (int argc, char ** argv)
{
	if(argc > 1){
		fprintf(stderr, "exit: too many arguments\n");
		return -1;
	}

	printf("logout\n");

	exit(0);
}


static
int
bc_do_help (int argc, char ** argv)
{
	if(argc > 1){
		fprintf(stderr, "help: too many arguments\n");
		return -1;
	}

	printf(HELP_MESSAGE);

	return 1;
}


static
int
bc_do_history (int argc, char ** argv)
{
	if(argc > 1){
		fprintf(stderr, "history: too many arguments\n");
		return -1;
	}

	print_hist_list();

	return 1;
}


static
int
bc_do_set (int argc, char ** argv)
{
	if(argc > 3){
		fprintf(stderr, "set: too many arguments\n");
		return -1;
	}

	if(argc == 1){
		print_variable_list();
	}else if(argc == 2){
		char *value = get_value_by_name(argv[1]);
		if(value != NULL)
			printf("%s\n", value);
	}else if(argc == 3){
		variable *var;
		if((var = get_variable(argv[1])) != NULL){	/* update variable */
			free(var -> value);

			size_t len = strlen(argv[2]);
			char *new_value = emalloc(len + 1);
			strncpy(new_value, argv[2], len);
			new_value[len] = '\0';

			var -> value = new_value;
		}else{	/* create a new variable */
			size_t len = strlen(argv[1]);
			char *name = emalloc(len + 1);
			strncpy(name, argv[1], len);
			name[len] = '\0';

			len = strlen(argv[2]);
			char *value = emalloc(len + 1);
			strncpy(value, argv[2], len);
			value[len] = '\0';

			add_variable(name, value);
		}
	}

	return 1;
}


static
int
bc_do_unset (int argc, char ** argv)
{
	if(argc == 1){
		fprintf(stderr, "unset: missing argument\n");
		return -1;
	}

	if(argc >= 3){
		fprintf(stderr, "unset: too many arguments\n");
		return -1;
	}

	delete_variable(argv[1]);

	return 1;	
}


static
int
bc_do_pwd (int argc, char ** argv)
{
	if(argc > 1){
		fprintf(stderr, "pwd: too many arguments\n");
		return -1;
	}

	char *cwd = emalloc(BUF_SIZE);
	size_t bufspace = BUF_SIZE;

	while(getcwd(cwd, bufspace) == NULL){
		if(errno == ERANGE){
			cwd = erealloc(cwd, bufspace + BUF_SIZE);
			bufspace += BUF_SIZE;
			continue;
		}
		perror("getcwd");
		return -1;
	}

	printf("%s\n", cwd);
	free(cwd);
	return 1;
}


static
int
bc_do_cd (int argc, char ** argv)
{
	if(argc == 1){
		fprintf(stderr, "cd: missing argument\n");
		return -1;
	}

	if(argc > 2){
		fprintf(stderr, "cd: too many arguments\n");
		return -1;
	}

	if(chdir(argv[1]) == -1){
		perror("chdir");
		return -1;
	}

	return 1;
}


static
int bc_do_jobs (int argc, char ** argv)
{
	if(argc > 1){
		fprintf(stderr, "jobs: too many arguments\n");
		return -1;
	}

	job *j, *jlast, *jnext;

	/* Update status information for child processes. */
	update_status();

	jlast = NULL;
	for(j = first_job; j; j = jnext)
	{
    	jnext = j->next;

 		if(j == current_job){
 			jlast = j;
			continue;
 		}

    	if(job_is_completed(j)){
    		format_job_info(j, "Completed");
    		if(jlast)
        		jlast->next = jnext;
        	else
        		first_job = jnext;
        	free_job(j);
    	}else if(job_is_stopped(j)){
        	format_job_info(j, "Stopped");
        	j->notified = 1;
        	jlast = j;
    	}else{
        	format_job_info(j, "Running");
        	jlast = j;
    	}
    }

	return 1;
}


static
int
bc_do_fg (int argc, char ** argv)
{
	if(argc == 1){
		fprintf(stderr, "fg: missing argument\n");
		return -1;	
	}

	if(argc >= 3){
		fprintf(stderr, "fg: too many arguments\n");
		return -1;
	}

	/* if argv[1] is not a number ? */
	pid_t jid = atoi(argv[1]);

	job *j;
	if((j = find_job(jid)) == NULL){
		fprintf(stderr, "fg: %d: no such job\n", jid);
		return -1;
	}

	continue_job(j, 1);

	return 1;
}


static
int
bc_do_bg (int argc, char ** argv)
{
	if(argc == 1){
		fprintf(stderr, "bg: missing argument\n");
		return -1;
	}

	if(argc >= 3){
		fprintf(stderr, "bg: too many arguments\n");
		return -1;
	}

	/* if argv[1] is not a number ? */
	pid_t jid = atoi(argv[1]);

	job *j;
	if((j = find_job(jid)) == NULL){
		fprintf(stderr, "bg: %d: no such job\n", jid);
		return -1;
	}

	continue_job(j, 0);

	return 1;
}

/* end handler */


static
bc_entry bc_list[] = {
	FORALL_BC(ADD_BC_ENTRY)
	{NULL, NULL}
};


int
builtin_cmd (job * j)
{
	/* The case that the pointer p is NULL should be handled before the function is called, 
	 * that is, the function assumes that the pointer is p not NULL.
	 */
	int count = 1;
	process *p = j -> first_process;
	while(p -> next != NULL){
		count++;
		p = p -> next;
	}

	if(count != 1 || (p -> argv)[0] == NULL)
		return 0;	/* not a builtin command */

	int argc = 0;
	while((p -> argv)[argc] != NULL)
		argc++;

	int rv;
	bc_entry *ep = bc_list;
	while(ep -> name != NULL){
		if(strcmp((p -> argv)[0], ep -> name) == 0){
			rv = (ep -> handler)(argc, p -> argv);
			break;
		}
		++ep;
	}

	if(ep -> name == NULL)
		return 0;	/* not a builtin command */

	/* free job */
	free_job(j);

	if(first_job == current_job){
		first_job = NULL;
	}else{
		job *jp = first_job;
		while((jp -> next) != current_job)
			jp = jp -> next;
		jp -> next = NULL;
	}
	job_id--;

	return rv;	/* is a builtin command : return 1 if success, return -1 if failed */
}


/* end builtin_cmd.c */