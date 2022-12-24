#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "utils.h"
#include "job_control.h"

const char *prog_dir[2] = {"/usr/bin/", "/bin/"};
const char *builtin_cmd[5] = {"bg", "cd", "fg", "jobs", "kill"};
void (*const builtin_func[5])(job_list_t *, char **) = {bg, cd, fg, jobs, kill_job};

void parse_args(char *line, char *args[][MAX_ARGS], int arg_count[], int *cmd_count) {
    char new_arg[ARG_LEN];
    int i = 0, j = 0, k = 0, in_quote = 0, in_escape = 0, next_cmd = 0;
    while (line[i] != '\0') {
        // The commands before an ampersand sign (&) have to be executed in the background.
        // Therefore, we split the command line into multiple commands.
        // Only the last command can be executed in the foreground if there is no ampersand sign at the end.
        if (line[i] == '&') {
            if (j > 0) {
                if (new_arg[j - 1] == '\n')
                    new_arg[--j] = '\0';
                else
                    new_arg[j] = '\0';
                if (j > 0) {
                    args[next_cmd][k] = malloc(sizeof(char) * (j + 1));
                    strcpy(args[next_cmd][k], new_arg);
                    k++;
                }
            }
            arg_count[next_cmd] = k;
            args[next_cmd][k] = NULL;
            next_cmd++;
            j = 0;
            k = 0;
        }
        else if (line[i] == '"') {
            if (in_quote) {
                in_quote = 0;
                new_arg[j] = '\0';
                args[next_cmd][k] = malloc(sizeof(char) * (j + 1));
                strcpy(args[next_cmd][k], new_arg);
                k++;
                j = 0;
            } else {
                in_quote = 1;
            }
        } 
        else if (line[i] == '\\') in_escape = 1;
        else if (line[i] == ' ') {
            if (in_quote || in_escape) {
                new_arg[j++] = line[i];
                in_escape = in_escape ? 0 : in_escape;
                in_quote = in_quote ? 0 : in_quote;
            } 
            else {
                if (j > 0) {
                    new_arg[j] = '\0';
                    args[next_cmd][k] = malloc(sizeof(char) * (j + 1));
                    strcpy(args[next_cmd][k], new_arg);
                    k++;
                    j = 0;
                }
            }
        } 
        else {
            new_arg[j] = line[i];
            j++;
        }
        i++;
    }
    if (j > 0) {
        if (new_arg[j - 1] == '\n')
            new_arg[--j] = '\0';
        else
            new_arg[j] = '\0';
        if (strlen(new_arg) > 0) {
            args[next_cmd][k] = malloc(sizeof(char) * (j + 1));
            strcpy(args[next_cmd][k], new_arg);
            k++;
        }
    }
    arg_count[next_cmd] = k;
    args[next_cmd][k] = NULL;
    *cmd_count = next_cmd + 1;
}

void bg(job_list_t *job_list, char *args[]) {
    if (args[1] == NULL) {
        printf("bg: no job specified\n");
        return;
    }
    if (args[1][0] != '%') {
        printf("bg: invalid job id\n");
        return;
    }
    int pgid = atoi(args[1] + 1);
    job_t *job = get_job_by_id(job_list, pgid);
    if (job == NULL) {
        fprintf(stderr, "bg: job not found: %d\n", pgid);
        return;
    }
    if (job->state != BACKGROUND) {
        job->state = BACKGROUND;
        // Append an ampersand sign (&) to the cmd.
        char *cmd = malloc(sizeof(char) * (strlen(job->cmd) + 2));
        strcpy(cmd, job->cmd);
        strcat(cmd, "&");
        free(job->cmd);
        job->cmd = cmd;
        killpg(job->pid, SIGCONT);
        printf("[%d] %d\n", job->pgid, job->pid);
    }
}

void cd(job_list_t *job_list, char *args[]) {
    char *new_path = args[1];
    if (args[1] == NULL)
        new_path = getenv("HOME");
    if (chdir(new_path) == -1)
        fprintf(stderr, "cd: %s: No such file or directory\n", new_path);
    else {
        char *pwd = malloc(sizeof(char) * PATH_LEN);
        getcwd(pwd, PATH_LEN);
        setenv("PWD", pwd, 1);
        free(pwd);
    }
}

void fg(job_list_t *job_list, char *args[]) {
    if (args[1] == NULL) {
        printf("fg: no job specified\n");
        return;
    }
    if (args[1][0] != '%') {
        printf("fg: invalid job id\n");
        return;
    }
    int pgid = atoi(args[1] + 1);
    job_t *job = get_job_by_id(job_list, pgid);
    if (job == NULL) {
        fprintf(stderr, "fg: job not found: %d\n", pgid);
        return;
    }
    if (job->state != FOREGROUND) {
        job->state = FOREGROUND;
        // Check if the cmd ends with an ampersand sign (&). Remove it if it does.
        if (job->cmd[strlen(job->cmd) - 1] == '&') job->cmd[strlen(job->cmd) - 1] = '\0';
        // Associate the job with the current terminal.
        tcsetpgrp(STDIN_FILENO, job->pid);
        killpg(job->pid, SIGCONT);
        int status;
        waitpid(job->pid, &status, WUNTRACED);
        // Restore the terminal to the shell.
        tcsetpgrp(STDIN_FILENO, getpid());
        enum job_status j_status = get_status(status);
        if (j_status == SUSPENDED) {
            printf("\n");
            // If the job is suspended, change its state to STOPPED.
            block_signal(SIGCHLD, 1);
            job->state = STOPPED;
            block_signal(SIGCHLD, 0);
        }
        // If the job is signaled or exited, delete it from the job list.
        else if (j_status == SIGNALED || j_status == EXITED) {
            if (j_status == SIGNALED) printf("\n[%d] %d terminated by signal %d\n", job->pgid, job->pid, status);
            // Delete the job from the job list.
            block_signal(SIGCHLD, 1);
            delete_job(job_list, job->pid);
            block_signal(SIGCHLD, 0);
        }
    }
}

void jobs(job_list_t *job_list, char *args[]) {
    print_job_list(job_list);
}

void kill_job(job_list_t *job_list, char *args[]) {
    if (args[1] == NULL) {
        printf("kill: no job specified\n");
        return;
    }
    if (args[1][0] != '%') {
        printf("kill: invalid job id\n");
        return;
    }
    int pgid = atoi(args[1] + 1);
    job_t *job = get_job_by_id(job_list, pgid);
    if (job == NULL) {
        fprintf(stderr, "kill: job not found: %d\n", pgid);
        return;
    }
    killpg(job->pid, SIGTERM);
    sleep(1);
}