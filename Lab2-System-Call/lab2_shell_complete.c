#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/sched.h>

#define MAX_CMDLINE_LENGTH 256  /*max cmdline length in a line*/
#define MAX_CMD_LENGTH 16       /*max single cmdline length*/
#define MAX_CMD_NUM 16          /*max single cmdline length*/

#ifndef NR_TASKS
#define NR_TASKS 64
#endif

#define SHELL "/bin/sh"

static int *child_pid = NULL;
static int  max_task = NR_TASKS;

int os_popen(const char* cmd, const char type){
    int     i, pipe_fd[2], proc_fd;  
    pid_t     pid;  

    /* only allow "r" or "w" */  
    if (type != 'r' && type != 'w') {  
        printf("popen() flag error\n");
        return NULL;  
    }  

    /* record children's pid */
    if(child_pid == NULL) {
        if ((child_pid = calloc(max_task, sizeof(int))) == NULL)
            return NULL;
    }

    /* open pipe */
    if (pipe(pipe_fd) < 0) {
        printf("popen() pipe create error\n");
        return NULL;   /* errno set by pipe() */  
    }  

    /* create new process */
    if ( (pid = fork()) < 0) {
        printf("popen() fork error\n");
        return NULL;   /* errno set by fork() */  
    }
    else if (pid == 0) {                            /* child */  
        if (type == 'r') {  
            /* close the unused end of the pipe */
            close(pipe_fd[0]);  
            if (pipe_fd[1] != STDOUT_FILENO) {  
                dup2(pipe_fd[1], STDOUT_FILENO);  
                close(pipe_fd[1]);  
            }  
        } else {  
            /* close the unused end of the pipe */
            close(pipe_fd[1]);  
            if (pipe_fd[0] != STDIN_FILENO) {  
                dup2(pipe_fd[0], STDIN_FILENO);  
                close(pipe_fd[0]);  
            }  
        }  
        /*wait and close all former children*/
        for (i=0;i<max_task;i++)
            if(child_pid[i]>0)
                close(i);
        /* run command */
        execl(SHELL, "sh", "-c", cmd, (char*)0);  
    }  
                                /* parent */  
    if (type == 'r') {  
        close(pipe_fd[1]);  
        proc_fd = pipe_fd[0];
    } else {  
        close(pipe_fd[0]);  
        proc_fd = pipe_fd[1]; 
    }    
    child_pid[proc_fd] = pid;
    return proc_fd;
}

int os_pclose(const int fno) {
    int stat;
    pid_t pid;
    if (child_pid == NULL)
        return -1;
    if ((pid = child_pid[fno]) == 0)
        return -1;
    child_pid[fno] = 0;
    close(fno);
    while (waitpid(pid, &stat, 0)<0)
        if(errno != EINTR)
            return -1;
    return stat;
}

int os_system(const char* cmdstring) {
    pid_t pid;
    int stat;

    if(cmdstring == NULL) {
        printf("nothing to do\n");
        return 1;
    }

    if( (pid = fork() )<0) {
        printf("system() fork error\n");
        stat = -1;
    }

    else if(pid == 0) {
        execl(SHELL, "sh", "-c", cmdstring, (char*)0);
        _exit(127);
    }

    /* parent: wait for child running */
    else {
        while(waitpid(pid, &stat, 0)<0) {
            if(errno !=EINTR) {
                stat = -1;
                break;
            }
        }
    }

    return stat;
}

/*parse cmdline, return cmd number*/
int parseCmd(char* cmdline, char cmds[MAX_CMD_NUM][MAX_CMD_LENGTH]) {
    int i,j;
    int offset = 0;
    int cmd_num = 0;        /*at least 1 cmd, then add 1 cmd per ';'*/
    char tmp[MAX_CMD_LENGTH];
    int len = NULL;
    char *end = strchr(cmdline, ';');
    char *start = cmdline;
    while (end != NULL) {
        memcpy(cmds[cmd_num], start, end - start);
        cmds[cmd_num++][end - start] = '\0';
        
        start = end + 1;
        end = strchr(start, ';');
    };
    len = strlen(cmdline);
    if (start < cmdline + len) {
        memcpy(cmds[cmd_num], start, (cmdline + len) - start);
        cmds[cmd_num++][(cmdline + len) - start] = '\0';
    }
    return cmd_num;
}

void zeroBuff(char* buff, int size) {
    int i;
    for(i=0;i<size;i++){
        buff[i]='\0';
    }
}

int main() {
    int cmd_num;
    int i,j,fd1,fd2,count;
    pid_t pids[MAX_CMD_NUM];
    char cmdline[MAX_CMDLINE_LENGTH];
    char cmds[MAX_CMD_NUM][MAX_CMD_LENGTH];
    char *buf[4096];
    int status;
    while(1){
	    write(STDOUT_FILENO, "os shell ->", 11);
        gets(cmdline);
        cmd_num = parseCmd(cmdline, cmds);
        for(i=0;i<cmd_num;i++){
            char *div = strchr(cmds[i], '|');
            if (div) {
                char cmd1[MAX_CMD_LENGTH], cmd2[MAX_CMD_LENGTH];
                int len = div - cmds[i];
                memcpy(cmd1, cmds[i], len);
                cmd1[len] = '\0';
                len = (cmds[i] + strlen(cmds[i])) - div - 1;
                memcpy(cmd2, div + 1, len);
                cmd2[len] = '\0';
                printf("cmd1: %s\n", cmd1);
                printf("cmd2: %s\n", cmd2);
                if ((fd1 = os_popen(cmd1, 'r')) == NULL) {
                    printf("error exec: %s", cmd1);
                    exit(1);
                };
                count = read(fd1, buf, 4096);
                os_pclose(fd1);
                if ((fd2 = os_popen(cmd2, 'w')) == NULL) {
                    printf("error exec: %s", cmd2);
                    exit(1);
                };
                write(fd2, buf, count);
                os_pclose(fd2);
            }
            else {
                pids[i] = fork();
                if(pids[i]==0) {
                    printf("!!exec: %s\n", cmds[i]);
                    os_system(cmds[i]);
                    exit(0);
                };
                waitpid(pids[i], &status, 0);
            }
        }
    }
    return 0;
}
