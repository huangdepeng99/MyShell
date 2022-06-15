SHELL = /bin/bash
OBJS = main.o get_cmd.o eval_cmd.o builtin_cmd.o job_control.o historylib.o variablelib.o wrapper.o
CFLAGS = -Wall -Werror -std=c11 -O2
CC = gcc
LD = gcc

myshell: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

main.o: main.c myshell.h
	$(CC) $(CFLAGS) -c -o $@ main.c

get_cmd.o: get_cmd.c myshell.h wrapper.h
	$(CC) $(CFLAGS) -c -o $@ get_cmd.c

eval_cmd.o: eval_cmd.c myshell.h historylib.h variablelib.h wrapper.h
	$(CC) $(CFLAGS) -c -o $@ eval_cmd.c

builtin_cmd.o: builtin_cmd.c myshell.h historylib.h variablelib.h wrapper.h
	$(CC) $(CFLAGS) -c -o $@ builtin_cmd.c

job_control.o: job_control.c myshell.h
	$(CC) $(CFLAGS) -c -o $@ job_control.c

historylib.o: historylib.c
	$(CC) $(CFLAGS) -c -o $@ historylib.c

variablelib.o: variablelib.c variablelib.h wrapper.h
	$(CC) $(CFLAGS) -c -o $@ variablelib.c

wrapper.o: wrapper.c
	$(CC) $(CFLAGS) -c -o $@ wrapper.c

.PHONY: clean
clean:
	rm -f myshell *.o
