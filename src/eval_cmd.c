/* 
 * eval_cmd.c
 */
/* begin eval_cmd.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include "myshell.h"
#include "historylib.h"
#include "variablelib.h"
#include "wrapper.h"


static
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


static
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
            if((rv = get_hist(hist_index)) == (char *) -1){ /* failed */
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


static
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


static
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
                username = getlogin();  /* If return NULL ? */

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


static
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
            if(next_c == '{'){  /* Form 1 : ${var_name} */
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
            }else{  /* Form 2 : $var_name */
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
static
int
is_redirect (char * arg, int n)
{
    int rv = 0;
    if(n == 1){ /* n == 1 */
        if(arg[0] == '<')
            rv = 1;
        else if(arg[0] == '>')
            rv = 2;
    }else if(n == 2){   /* n == 2 */
        if(arg[0] == '0' && arg[1] == '<')
            rv = 1;
        else if(arg[0] == '1' && arg[1] == '>')
            rv = 2;
        else if(arg[0] == '2' && arg[1] == '>')
            rv = 3;
        else if(arg[0] == '>' && arg[1] == '>')
            rv = 4;
    }else{  /* n == 3 */
        if(arg[0] == '1' && arg[1] == '>' && arg[2] == '>')
            rv = 4;
        else if(arg[0] == '2' && arg[1] == '>' && arg[2] == '>')
            rv = 5;
    }
    
    return rv;
}


static
void
record_redirect (int flag, process * ps, char * filename)
{
    int index;
    int is_append;

    switch (flag) {
        case 1: { index = 0; is_append = 0; break; }
        case 2: { index = 1; is_append = 0; break; }
        case 3: { index = 2; is_append = 0; break; }
        case 4: { index = 1; is_append = 1; break; }
        case 5: { index = 2; is_append = 1; break; }
        default: assert(0);
    }

    if(((ps -> io_re)[index]).dest == NULL){
        ((ps -> io_re)[index]).dest = filename;
    }else{
        free(((ps -> io_re)[index]).dest);
        ((ps -> io_re)[index]).dest = filename;
    }
    ((ps -> io_re)[index]).is_append = is_append;
}


static
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


/* int eval_cmd (char * cmdline) : 
 *   1.history expand; 2.add history entry, add job entry;
 *   3.tilde expand; 4.variable expand; 5.add process entry.
 */
int
eval_cmd (char * cmdline)
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


/* end eval_cmd.c */