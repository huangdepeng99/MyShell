/* 
 * variablelib.h
 */
/* $begin variablelib.h */
#ifndef __VARIABLELIB_H__
#define __VARIABLELIB_H__


typedef struct variable
{
	struct variable *next;
	char *name;
	char *value;
} variable;


extern char * get_value_by_name (char * name);
extern void delete_variable (char * name);
extern void print_variable_list (void);
extern variable * get_variable (char * name);
extern void add_variable (char * name, char * value);


#endif /* __VARIABLELIB_H__ */
/* $end variablelib.h */