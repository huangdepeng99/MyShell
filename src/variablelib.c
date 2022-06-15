/* 
 * variablelib.c
 * 
 * Note: Only local variables are supported, not environment variables.
 */
/* $begin variablelib.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "variablelib.h"
#include "wrapper.h"


/* The variables are linked into a list. This is its head. */
static variable *first_variable = NULL;


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
print_variable_list (void)
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


/* $end variablelib.c */