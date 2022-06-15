/* 
 * get_cmd.c
 */
/* $begin get_cmd.c */
#include <stdio.h>
#include <ctype.h>
#include "myshell.h"
#include "wrapper.h"


char *
next_cmd (char * prompt, FILE * fp)
{
	char *buf = NULL;
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


/* $end get_cmd.c */