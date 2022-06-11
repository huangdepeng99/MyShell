#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>


#define DFL_PROMPT	  "> "
#define BUF_SIZE	  512
#define ARGV_SIZ	  10
#define HIST_SIZE	  500
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


/* Default file permissions are DEF_MODE & ~DEF_UMASK */
#define DEF_MODE	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#define DEF_UMASK	S_IWGRP | S_IWOTH


/* Keep track of attributes of the shell. */
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;


/* History */
char *hist_list[HIST_SIZE];	/* history list */
int hist_pos = 0;
int hist_is_full = 0;


/* Variable */
typedef struct variable
{
	struct variable *next;
	char *name;
	char *value;
} variable;

/* The variables are linked into a list. This is its head. */
variable *first_variable = NULL;


/* Job Control */
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
job *first_job = NULL;

job *current_job = NULL;
pid_t job_id = 1;


int foreground = 1;



/*********************************************************************/



/* emalloc */
void
fatal (char * s1, char * s2, int n)
{
	fprintf(stderr, "Error: %s, %s\n", s1, s2);
	exit(n);
}


void *
emalloc (size_t n)
{
	void *rv;
	if((rv = malloc(n)) == NULL)
		fatal("Out of memory", "", 1);
	return rv;
}


void *
erealloc (void * p, size_t n)
{
	void *rv;
	if((rv = realloc(p, n)) == NULL)
		fatal("realloc() failed", "", 1);
	return rv;
}



/*********************************************************************/



/* History */
char *
get_hist (int hist_index)
{
	if(hist_is_full){
		if(hist_index < 1 || hist_index > HIST_SIZE)
			return (char *) -1;
		return hist_list[(hist_pos+hist_index-1)%HIST_SIZE];
	}else{
		if(hist_pos == 0)
			return (char *) -1;
		if(hist_index < 1 || hist_index > hist_pos)
			return (char *) -1;
		return hist_list[hist_index-1];
	}
}


void
add_hist (char * hist)
{
	if(hist_is_full){
		free(hist_list[hist_pos]);
		hist_list[hist_pos++] = hist;
		if(hist_pos >= HIST_SIZE)
			hist_pos = 0;
	}else{
		hist_list[hist_pos++] = hist;
		if(hist_pos >= HIST_SIZE){
			hist_is_full = 1;
			hist_pos = 0;
		}
	}
}


void
print_hist_list ()
{
	int pos = hist_pos;
	int index = 1;
	if(hist_is_full){
		while(index <= HIST_SIZE){
			printf("%3d  %s\n", index++, hist_list[pos++]);
			if(pos >= HIST_SIZE)
				pos = 0;
		}
	}else{
		pos = 0;
		while(pos != hist_pos)
			printf("%3d  %s\n", index++, hist_list[pos++]);
	}
}



/*********************************************************************/



/* Variable */
/* Note: Only local variables are supported, not environment variables. */
char *
get_value_by_name (char * name)
{
	variable *var = first_variable;
	while(var != NULL){
		if(strcmp(name, var -> name) == 0)
			return var -> value;	/* if success */
		var = var -> next;
	}

	return NULL;	/* if failed */
}


void
delete_variable (char * name)
{
	variable *curr_var = first_variable;
	variable *prev_var = NULL;

	while(curr_var != NULL){
		if(strcmp(curr_var -> name, name) == 0){
			if(prev_var == NULL)
				first_variable = curr_var -> next;
			else
				prev_var -> next = curr_var -> next;
			free(curr_var -> name);
			free(curr_var -> value);
			free(curr_var);
			return;
		}else{
			prev_var = curr_var;
			curr_var = curr_var -> next;
		}
	}

	return;
}


void
print_variable_list ()
{
	variable *var = first_variable;

	while(var != NULL){
		printf("%s=%s\n", var -> name, var -> value);
		var = var -> next;
	}
}


variable *
get_variable (char * name)
{
	variable *var = first_variable;

	while(var != NULL){
		if(strcmp(var -> name, name) == 0)
			break;
		var = var -> next;
	}

	return var;
}


void
add_variable (char * name, char * value)
{
	variable *new_var = emalloc(sizeof(variable));
	new_var -> next = NULL;
	new_var -> name = name;
	new_var -> value = value;

	if(first_variable == NULL){
		first_variable = new_var;
	}else{
		variable *var = first_variable;
		while(var -> next != NULL)
			var = var -> next;
		var -> next = new_var;
	}
}



/*********************************************************************/



char *
next_cmd (char * prompt, FILE * fp)
{
	char *buf;
	int bufspace = 0;
	int pos = 0;
	int c;

	printf("%s", prompt);
	while((c = getc(fp)) != EOF){
		if(pos + 1 >= bufspace){
			if(bufspace == 0)
				buf = emalloc(BUF_SIZE);
			else
				buf = erealloc(buf, bufspace + BUF_SIZE);
			bufspace += BUF_SIZE;
		}

		if(c == '\n')
			break;

		buf[pos++] = c;
	}

	if(c == EOF && pos == 0)
		return NULL;

	buf[pos] = '\0';
	return buf;
}


int
cmd_is_empty (char * cmdline)
{
	char *p = cmdline;
	char c;
	while((c = *p)){
		if(isblank(c))
			p = p + 1;
		else
			return 0;
	}
	return 1;
}



/*********************************************************************/




/* Expansion */
char *
delete_extra_blank (char * cmdline)
{
	char *new_cmdline = emalloc(strlen(cmdline) + 1);

	char c;
	int cmd_pos_start = 0;
	int cmd_pos_end = 0;
	int new_cmd_pos = 0;
	while((c = cmdline[cmd_pos_start])){
		if(!isblank(c)){
			cmd_pos_end = cmd_pos_start + 1;
			char tmp_c;
			while((tmp_c = cmdline[cmd_pos_end]) != '\0' && !isblank(tmp_c))
				cmd_pos_end++;
			int substr_len = cmd_pos_end - cmd_pos_start;
			strncpy(&new_cmdline[new_cmd_pos], &cmdline[cmd_pos_start], substr_len);
			new_cmd_pos += substr_len;
			new_cmdline[new_cmd_pos++] = ' ';
			cmd_pos_start = cmd_pos_end;
		}else
			cmd_pos_start++;
	}
	new_cmdline[new_cmd_pos-1] = '\0';

	free(cmdline);
	return new_cmdline;
}


char *
history_expand (char * cmdline)
{
	char *new_cmdline;
	size_t len = strlen(cmdline) + 1;
	size_t bufspace = ((len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
	new_cmdline = emalloc(bufspace);

	char c;
	int cmd_pos_start = 0;
	int cmd_pos_end = 0;
	int new_cmd_pos = 0;
	int flag = 0;
	/* Start history expand. */
	/* If failed: 1.print error message; 2.free cmdline and new_cmdline; 3.return -1. */
	while((c = cmdline[cmd_pos_start])){
		if(c == '!' && isdigit(cmdline[cmd_pos_start+1])){
			cmd_pos_end = cmd_pos_start + 1;
			while(isdigit(cmdline[cmd_pos_end]))
				cmd_pos_end++;
			int hist_index = atoi(&cmdline[cmd_pos_start+1]);
			char *rv;
			if((rv = get_hist(hist_index)) == (char *) -1){	/* failed */
				fprintf(stderr, "Error: history expand failed\n");
				free(cmdline); free(new_cmdline);
				return (char *) -1;
			}else{
				size_t substr_len = strlen(rv);
				if(new_cmd_pos + substr_len + 1 >= bufspace){
					size_t inc_bufspace = ((substr_len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
					new_cmdline = erealloc(new_cmdline, bufspace + inc_bufspace);
					bufspace += inc_bufspace;
				}
				strncpy(&new_cmdline[new_cmd_pos], rv, substr_len);
				new_cmd_pos += substr_len;
				cmd_pos_start = cmd_pos_end;
				flag = 1;
			}
		}else{
			if(new_cmd_pos + 1 >= bufspace) {
				new_cmdline = erealloc(new_cmdline, bufspace + BUF_SIZE);
				bufspace += BUF_SIZE;
			}
			new_cmdline[new_cmd_pos++] = c;
			cmd_pos_start++;
		}
	}
	new_cmdline[new_cmd_pos] = '\0';
	/* End history expand. */
	
	/* If success. */
	if(flag)
		printf("%s\n", new_cmdline);
	free(cmdline);
	return new_cmdline;
}


void
add_job (char * command)
{
	job *new_job = emalloc(sizeof(job));

	new_job -> next = NULL;
	new_job -> command = command;
	new_job -> first_process = NULL;
	new_job -> jid = job_id++;
	new_job -> pgid = 0;
	new_job -> notified = 0;
	new_job -> tmodes = shell_tmodes;
	new_job -> stdin = STDIN_FILENO;
	new_job -> stdout = STDOUT_FILENO;
	new_job -> stderr = STDERR_FILENO;

	if(first_job == NULL){
		first_job = new_job;
	}else{
		job *j = first_job;
		while(j -> next != NULL)
			j = j -> next;
		j -> next = new_job;
	}
	current_job = new_job;
}


char *
tilde_expand (char * cmdline)
{
	char *new_cmdline;
	size_t len = strlen(cmdline) + 1;
	size_t bufspace = ((len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
	new_cmdline = emalloc(bufspace);

	char c;
	int cmd_pos_start = 0;
	int cmd_pos_end = 0;
	int new_cmd_pos = 0;
	/* Start tilde expand. */
	while((c = cmdline[cmd_pos_start])){
		if(c == '~' && (cmd_pos_start == 0 || isblank(cmdline[cmd_pos_start-1]))){
			cmd_pos_end = cmd_pos_start + 1;
			char tmp_c;
			while((tmp_c = cmdline[cmd_pos_end]) != '\0' && !isblank(tmp_c) && tmp_c != '/')
				cmd_pos_end++;

			size_t username_len = cmd_pos_end - cmd_pos_start - 1;
			char *username;
			if(username_len != 0){
				username = emalloc(username_len + 1);
				strncpy(username, &cmdline[cmd_pos_start+1], username_len);
				username[username_len] = '\0';
			}else
				username = getlogin();	/* If return NULL ? */

			struct passwd *rv;
			if((rv = getpwnam(username)) == NULL){
				if(username_len != 0)
					free(username);
				goto tilde_expand_failed;
			}else{
				size_t substr_len = strlen(rv -> pw_dir);
				if(new_cmd_pos + substr_len + 1 >= bufspace){
					size_t inc_bufspace = ((substr_len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
					new_cmdline = erealloc(new_cmdline, bufspace + inc_bufspace);
					bufspace += inc_bufspace;
				}
				strncpy(&new_cmdline[new_cmd_pos], rv -> pw_dir, substr_len);
				new_cmd_pos += substr_len;
				cmd_pos_start = cmd_pos_end;
				if(username_len != 0)
					free(username);
			}
		}else{
			tilde_expand_failed:
			if(new_cmd_pos + 1 >= bufspace){
				new_cmdline = erealloc(new_cmdline, bufspace + BUF_SIZE);
				bufspace += BUF_SIZE;
			}
			new_cmdline[new_cmd_pos++] = c;
			cmd_pos_start++;
		}
	}
	new_cmdline[new_cmd_pos] = '\0';
	/* End tilde expand. */
	
	free(cmdline);
	return new_cmdline;
}


char *
variable_expand (char * cmdline)
{
	char *new_cmdline;
	size_t len = strlen(cmdline) + 1;
	size_t bufspace = ((len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
	new_cmdline = emalloc(bufspace);

	char c;
	int cmd_pos_start = 0;
	int cmd_pos_end = 0;
	int new_cmd_pos = 0;
	/* Start variable expand. */
	while((c = cmdline[cmd_pos_start])){
		char next_c;
		if(c == '$' && (next_c = cmdline[cmd_pos_start+1]) != '\0' && !isblank(next_c)){
			if(next_c == '{'){	/* Form 1 : ${var_name} */
				cmd_pos_end = cmd_pos_start + 2;
				char tmp_c;
				while((tmp_c = cmdline[cmd_pos_end]) != '\0' && !isblank(tmp_c) && tmp_c != '}')
					cmd_pos_end++;
				if(tmp_c == '\0' || isblank(tmp_c))
					goto ordinary_character;
				cmd_pos_end++;

				size_t var_name_len = cmd_pos_end - cmd_pos_start - 3;
				if(var_name_len == 0)
					goto ordinary_character;

				char *var_name = emalloc(var_name_len + 1);
				strncpy(var_name, &cmdline[cmd_pos_start+2], var_name_len);
				var_name[var_name_len] = '\0';

				/*** 1 : The same code (begin) ***/
				char *rv;
				if((rv = get_value_by_name(var_name)) != NULL){
					size_t substr_len = strlen(rv);
					if(new_cmd_pos + substr_len + 1 >= bufspace){
						size_t inc_bufspace = ((substr_len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
						new_cmdline = erealloc(new_cmdline, bufspace + inc_bufspace);
						bufspace += inc_bufspace;
					}
					strncpy(&new_cmdline[new_cmd_pos], rv, substr_len);
					new_cmd_pos += substr_len;
				}
				cmd_pos_start = cmd_pos_end;
				free(var_name);
				/*** 1 : The same code (end) ***/
			}else{	/* Form 2 : $var_name */
				cmd_pos_end = cmd_pos_start + 2;
				char tmp_c;
				while((tmp_c = cmdline[cmd_pos_end]) != '\0' && !isblank(tmp_c))
					cmd_pos_end++;

				size_t var_name_len = cmd_pos_end - cmd_pos_start - 1;
				char *var_name = emalloc(var_name_len + 1);
				strncpy(var_name, &cmdline[cmd_pos_start+1], var_name_len);
				var_name[var_name_len] = '\0';

				/*** 1 : The same code (begin) ***/
				char *rv;
				if((rv = get_value_by_name(var_name)) != NULL){
					size_t substr_len = strlen(rv);
					if(new_cmd_pos + substr_len + 1 >= bufspace){
						size_t inc_bufspace = ((substr_len + BUF_SIZE - 1) / BUF_SIZE) * BUF_SIZE;
						new_cmdline = erealloc(new_cmdline, bufspace + inc_bufspace);
						bufspace += inc_bufspace;
					}
					strncpy(&new_cmdline[new_cmd_pos], rv, substr_len);
					new_cmd_pos += substr_len;
				}
				cmd_pos_start = cmd_pos_end;
				free(var_name);
				/*** 1 : The same code (end) ***/
			}
		}else{
			ordinary_character:
			if(new_cmd_pos + 1 >= bufspace){
				new_cmdline = erealloc(new_cmdline, bufspace + BUF_SIZE);
				bufspace += BUF_SIZE;
			}
			new_cmdline[new_cmd_pos++] = c;
			cmd_pos_start++;
		}
	}
	new_cmdline[new_cmd_pos] = '\0';
	/* End variable expand. */
	
	free(cmdline);
	return new_cmdline;
}


/* < and 0<    -    1
 * > and 1>    -    2
 * 2>          -    3
 * >> and 1>>  -    4
 * 2>>         -    5
 */
int
is_redirect (char * arg, int n)
{
	int rv = 0;
	if(n == 1){	/* n == 1 */
		if(arg[0] == '<')
			rv = 1;
		else if(arg[0] == '>')
			rv = 2;
	}else if(n == 2){	/* n == 2 */
		if(arg[0] == '0' && arg[1] == '<')
			rv = 1;
		else if(arg[0] == '1' && arg[1] == '>')
			rv = 2;
		else if(arg[0] == '2' && arg[1] == '>')
			rv = 3;
		else if(arg[0] == '>' && arg[1] == '>')
			rv = 4;
	}else{	/* n == 3 */
		if(arg[0] == '1' && arg[1] == '>' && arg[2] == '>')
			rv = 4;
		else if(arg[0] == '2' && arg[1] == '>' && arg[2] == '>')
			rv = 5;
	}
	
	return rv;
}


void
record_redirect (int flag, process * ps, char * filename)
{
	int index;
	int is_append;

	if(flag == 1){	/* flag == 1 */
		index = 0;
		is_append = 0;
	}else if(flag == 2){	/* flag == 2 */
		index = 1;
		is_append = 0;
	}else if(flag == 3){	/* flag == 3 */
		index = 2;
		is_append = 0;
	}else if(flag == 4){	/* flag == 4 */
		index = 1;
		is_append = 1;
	}else if(flag == 5){	/* flag == 5 */
		index = 2;
		is_append = 1;
	}

	if(((ps -> io_re)[index]).dest == NULL){
		((ps -> io_re)[index]).dest = filename;
	}else{
		free(((ps -> io_re)[index]).dest);
		((ps -> io_re)[index]).dest = filename;
	}
	((ps -> io_re)[index]).is_append = is_append;
}


void
add_process (char * cmdline)
{
	size_t cmd_len = strlen(cmdline);
	/* The code is not handle special cases, such as: 
	 * '&' or '& ls'.
	 */
	if(cmdline[cmd_len-1] == '&'){
		foreground = 0;
		cmdline[cmd_len-1] = '\0';
		cmd_len--;
	}else
		foreground = 1;

	int process_start = 0;
	int process_end = 0;
	int start = 0;
	int end = 0;
	/* The pipe uses an error when the left or right side of the pipe is empty, but the 
	 * shell ignores these error cases. Empty means no characters or only blank, such as: 
	 * '|' or '| ls' or 'ls |' or 'ls || sort'  or 'ls |   | sort'.
	 */
	while(cmdline[process_start]){
		process_end = process_start;
		char tmp_c;
		int count = 0;
		while((tmp_c = cmdline[process_end]) != '|' && tmp_c != '\0'){
			if(!isblank(tmp_c))
				count++;
			process_end++;
		}

		if(count == 0){
			if(tmp_c == '|')
				process_start = process_end + 1;
			else
				process_start = process_end;
			continue;
		}

		/* Malloc and initial the process. */
		process *ps = emalloc(sizeof(process));
		ps -> next = NULL;
		ps -> argv = emalloc(sizeof(char*) * ARGV_SIZ);
		size_t bufspace = ARGV_SIZ;
		int bufpos = 0;
		((ps -> io_re)[0]).dest = NULL;
		((ps -> io_re)[1]).dest = NULL;
		((ps -> io_re)[2]).dest = NULL;
		ps -> pid = -1;
		ps -> completed = 0;
		ps -> stopped = 0;

		if(current_job -> first_process == NULL)
			current_job -> first_process = ps;
		else{
			process *p = current_job -> first_process;
			while(p -> next != NULL)
				p = p -> next;
			p -> next = ps;
		}


		start = process_start;
		int flag = 0;
		while(start < process_end){
			if(!isblank(cmdline[start])){
				end = start + 1;
				while(end < process_end && !isblank(cmdline[end]))
					end++;

				size_t arg_len = end - start;
				if(flag > 0){
					char *filename = emalloc(arg_len+1);
					strncpy(filename, &cmdline[start], arg_len);
					filename[arg_len] = '\0';

					record_redirect(flag, ps, filename);
					flag = 0;
					start = end;
					continue;
				}

				if(arg_len <= 3 && (flag = is_redirect(&cmdline[start], arg_len)) > 0){
					start = end;
					continue;
				}

				char *arg = emalloc(arg_len+1);
				strncpy(arg, &cmdline[start], arg_len);
				arg[arg_len] = '\0';

				if(bufpos + 1 >= bufspace){
					ps -> argv = erealloc(ps -> argv, sizeof(char*) * (bufspace + ARGV_SIZ));
					bufspace += ARGV_SIZ;
				}
				(ps -> argv)[bufpos++] = arg;
				start = end;
			}else
				start++;
		}
		(ps -> argv)[bufpos] = NULL;

		if(tmp_c == '|')
			process_start = process_end + 1;
		else
			process_start = process_end;
	}

	free(cmdline);
}


/* int expand(char * cmdline) : 
 *   1.history expand; 2.add history entry, add job entry;
 *   3.tilde expand; 4.variable expand; 5.add process entry.
 */
int
expand (char * cmdline)
{
	char *temp_cmdline;

	temp_cmdline = delete_extra_blank(cmdline);

	if((temp_cmdline = history_expand(temp_cmdline)) == (char *) -1)
		return -1;

	size_t tmp_cmdln_len = strlen(temp_cmdline) + 1;

	add_hist(temp_cmdline);

	char *command = emalloc(tmp_cmdln_len);
	strcpy(command, temp_cmdline);
	add_job(command);

	temp_cmdline = emalloc(tmp_cmdln_len);
	strcpy(temp_cmdline, command);
	temp_cmdline = tilde_expand(temp_cmdline);

	temp_cmdline = variable_expand(temp_cmdline);

	add_process(temp_cmdline);

	return 0;
}



/*********************************************************************/



/* Builtin Command */
/* If you want to add a built-in command, you need to provide its handler function, 
 * which has the following function prototype:
 *   int bc_do_<name> (int argc, char ** argv)
 * The handler should return -1 on error and 1 on success.
 * You also need to append _(<name>) to the macro FORALL_BC(_), and you also need to modify 
 * the macro HELP_MESSAGE to make the built-in help work correctly.
 */
typedef int (*bchandler_t)(int, char **);

typedef struct bc_entry
{
	char * name;
	bchandler_t handler;
} bc_entry;

#define FORALL_BC(_)  _(exit) _(help) _(history) _(set) _(unset) _(pwd) _(cd) _(jobs) _(fg) _(bg)
#define ADD_BC_ENTRY(NAME) {#NAME, bc_do_##NAME},



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


/* Function declaration. */
void update_status (void);
int job_is_completed (job * j);
void format_job_info (job * j, const char * status);
void free_job (job * j);
int job_is_stopped (job * j);
void put_job_in_foreground (job * j, int cont);
void put_job_in_background (job * j, int cont);

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
void
mark_job_as_running (job * j)
{
	process *p;

	for(p = j->first_process; p; p = p->next)
		p->stopped = 0;
	j->notified = 0;
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



bc_entry bc_list[] = {
	FORALL_BC(ADD_BC_ENTRY)
	{NULL, NULL}
};



/* Builtin command: 
 *   exit - Exit the shell.
 *   help [<name>] - 1. help : Display information about builtin commands.
 *                   2. help <name> : Not implemented.
 *   history - Display the history list.
 *   set [<name>] [<value>] - 1. set : Check the names and values of all shell variables.
 *                            2. set <name> : Check the value of the variable named <name>.
 *                            3. set <name> <value> : Create a new variable with the name <name> and the 
 *                               value <value>, or update the value of the variable named <name> to <value>.
 *   unset <name> - Delete the shell variable named <name>.
 *   pwd - Print the absolute pathname of the current working directory.
 *   cd <dir> - Change the current working directory to <dir>.
 *   jobs - Display status of jobs.
 *   fg <job_id> - Move job to the foreground.
 *   bg <job_id> - Move job to the background.
 * 
 * Note: Builtin commands does not support pipelines and I/O redirection.
 */
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



/*********************************************************************/



/* Job Control */
/* Make sure the shell is running as the foreground job
 * before proceeding.
 */
void
init_shell ()
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


/* Store the status of the process pid that was returned by waitpid.
 * Return 0 if all went well, nonzero otherwise.
 */
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


/* Check for processes that have status information available,
 * blocking until all processes in the given job have reported.
 */
void
wait_for_job (job * j)
{
	int status;
	pid_t pid;

	do{
		pid = waitpid(WAIT_ANY, &status, WUNTRACED);	/* WAIT_ANY == -1 */
	}while(!mark_process_status(pid, status)
		   && !job_is_stopped(j)
		   && !job_is_completed(j));
}


/* Put job j in the foreground. If cont is nonzero,
 * restore the saved terminal modes and send the process group a
 * SIGCONT signal to wake it up before we block.
 */
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
void
put_job_in_background (job * j, int cont)
{
	/* Send the job a continue signal, if necessary. */
	if(cont){
		if(kill(- j->pgid, SIGCONT) < 0)
			perror("kill (SIGCONT)");
	}
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
		pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);	/* WAIT_ANY == -1 */
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



/*********************************************************************/



int
main (int argc, char * argv[])
{
	char *cmdline;
	char *prompt = DFL_PROMPT;

	init_shell();

	while((cmdline = next_cmd(prompt, stdin)) != NULL){
		if(!cmd_is_empty(cmdline)){
			if(expand(cmdline) == -1)
				continue;

			if(builtin_cmd(current_job) == 0)
				launch_job(current_job, foreground);
		}else
			free(cmdline);

		do_job_notification();
	}

	printf("logout\n");
	exit(0);
}


