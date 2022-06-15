/* 
 * historylib.c
 */
/* $begin historylib.c */
#include <stdio.h>
#include <stdlib.h>

#define HIST_SIZE 500

static char * hist_list[HIST_SIZE];		/* history list */
static int    hist_pos     = 0;
static int    hist_is_full = 0;


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
print_hist_list (void)
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


/* $end historylib.c */