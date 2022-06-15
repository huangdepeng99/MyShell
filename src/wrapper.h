/* 
 * wrapper.h
 */
/* $begin wrapper.h */
#ifndef __WRAPPER_H__
#define __WRAPPER_H__


#include <stddef.h>

extern void * emalloc (size_t n);
extern void * erealloc (void * p, size_t n);


#endif /* __WRAPPER_H__ */
/* $end wrapper.h */