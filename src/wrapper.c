/* 
 * wrapper.c
 */
/* begin wrapper.c */
#include <stdio.h>
#include <stdlib.h>

static
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

/* end wrapper.c */