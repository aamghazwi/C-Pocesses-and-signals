#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "job_control.h"

const char *job_state_str[2] = {"Running", "Stopped"};
static int job_id = 1;

void init_job_list(job_list_t *job_list) {
    job_list->first = NULL;
    job_list->size = 0;
}

job_t *add_job(job_list_t *job_list, pid_t pid, enum job_state state, char **cmd) {
    job_t *job = malloc(sizeof(job_t));
    job->pid = pid;
    job->state = state;
    job->pgid = job_id++;
    // If the job is a background job, append an ampersand to the command line.
    if (state == BACKGROUND) {
        // Concatenate all the arguments in cmd to a single string in job->cmd.
        // Then append an ampersand to the end of the string.
        int i = 0, len = 0;
        while (cmd[i] != NULL) {
            len += strlen(cmd[i]);
            i++;
        }
        job->cmd = malloc(sizeof(char) * (len + i + 2));
        i = 0;
        job->cmd[0] = '\0';
        while (cmd[i] != NULL) {
            strcat(job->cmd, cmd[i]);
            strcat(job->cmd, " ");
            i++;
        }
        strcat(job->cmd, "&");
    } 
    else {
        // Concatenate all the arguments in cmd to a single string in job->cmd.
        int i = 0, len = 0;
        while (cmd[i] != NULL) {
            len += strlen(cmd[i]);
            i++;
        }
        job->cmd = malloc(sizeof(char) * (len + i + 1));
        i = 0;
        job->cmd[0] = '\0';
        while (cmd[i] != NULL) {
            strcat(job->cmd, cmd[i]);
            strcat(job->cmd, " ");
            i++;
        }
        
    }
    job->next = job_list->first;
    job_list->first = job;
    job_list->size++;
    return job;
}

job_t *delete_job(job_list_t *job_list, pid_t pid) {
    job_t *job = job_list->first;
    job_t *prev = NULL;
    while (job != NULL) {
        if (job->pid == pid) {
            if (prev == NULL)
                job_list->first = job->next;
            else
                prev->next = job->next;
            free(job->cmd);
            free(job);
            job = NULL;
            job_list->size--;
        }
        if (job != NULL) {
            prev = job;
            job = job->next;
        }
    }
    // Make the maximum id in the job list the job_id.
    job_id = 1;
    job = job_list->first;
    while (job != NULL) {
        if (job->pgid >= job_id)
            job_id = job->pgid + 1;
        job = job->next;
    }
    return prev;
}

job_t *get_job(job_list_t *job_list, pid_t pid) {
    job_t *job = job_list->first;
    while (job != NULL) {
        if (job->pid == pid) {
            return job;
        }
        job = job->next;
    }
    return NULL;
}

job_t *get_job_by_id(job_list_t *job_list, int pgid) {
    job_t *job = job_list->first;
    while (job != NULL) {
        if (job->pgid == pgid) {
            return job;
        }
        job = job->next;
    }
    return NULL;
}

void print_job_list(job_list_t *job_list) {
    // Print the job list in sorted order of pgid.
    for (int i = 1; i < job_id; i++) {
        job_t *job = get_job_by_id(job_list, i);
        if (job != NULL) printf("[%d] %d %s %s\n", job->pgid, job->pid, job_state_str[job->state / 2], job->cmd);
    }
}

void free_job_list(job_list_t *job_list) {
    job_t *job = job_list->first;
    while (job != NULL) {
        job_t *next = job->next;
        free(job->cmd);
        free(job);
        job = next;
    }
}

void terminal_signal_handler(void (*handler)(int)) {
    signal(SIGTSTP, handler);
    signal(SIGTTIN, handler);
    signal(SIGTTOU, handler);
    signal(SIGINT, handler);
    signal(SIGQUIT, handler);
}

enum job_status get_status(int status) {
    if (WIFEXITED(status)) return EXITED;
    else if (WIFSIGNALED(status)) return SIGNALED;
    else if (WIFSTOPPED(status)) return SUSPENDED;
    else if (WIFCONTINUED(status)) return CONTINUED;
    return -1;
}

void block_signal(int signal, int block) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signal);
    sigprocmask(block ? SIG_BLOCK : SIG_UNBLOCK, &mask, NULL);
}