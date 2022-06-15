/* 
 * main.c
 */
/* $begin main.c */
#include <stdio.h>
#include <stdlib.h>
#include "myshell.h"


int
main (int argc, char * argv[])
{
	char *cmdline;
	char *prompt = DFL_PROMPT;

	init_shell();

	while((cmdline = next_cmd(prompt, stdin)) != NULL){
		if(!cmd_is_empty(cmdline)){
			if(eval_cmd(cmdline) == -1)
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


/* $end main.c */