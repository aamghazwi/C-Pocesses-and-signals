#include <sys/types.h>

#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

enum job_state {
    FOREGROUND, BACKGROUND, STOPPED
};
enum job_status {
    SUSPENDED, CONTINUED, EXITED, SIGNALED
};
extern const char *job_state_str[2];

typedef struct _ {
    // Process ID.
    pid_t pid;
    // Job state.
    enum job_state state;
    // Job ID.
    int pgid;
    // Command line.
    char *cmd;
    // Pointer to the next job.
    struct _ *next;
} job_t;

typedef struct {
    // Pointer to the first job.
    job_t *first;
    // Size of the job list.
    int size;
} job_list_t;

/*
 * Function: init_job_list
 * -----------------------
 *   Initialize the job list.
 *
 *   job_list: the job list
 */
void init_job_list(job_list_t *job_list);

/*
 * Function: add_job
 * -----------------
 *   Add a job to the job list.
 *
 *   job_list: the job list
 *   pid: the process ID
 *   state: the job state
 *   cmd: the command line
 * 
 *   returns: a pointer to the added job
 */
job_t *add_job(job_list_t *job_list, pid_t pid, enum job_state state, char **cmd);

/*
 * Function: delete_job
 * --------------------
 *   Delete a job from the job list.
 *
 *   job_list: the job list
 *   pid: the process ID
 * 
 *   returns: a pointer to the previous job before the deleted job
 */
job_t *delete_job(job_list_t *job_list, pid_t pid);

/*
 * Function: get_job
 * -----------------
 *   Get a job from the job list.
 *
 *   job_list: the job list
 *   pid: the process ID
 *
 *   return: the job
 */
job_t *get_job(job_list_t *job_list, pid_t pid);

/*
 * Function: get_job_by_id
 * -----------------------
 *   Get a job from the job list by its ID.
 *
 *   job_list: the job list
 *   pgid: the job ID
 *
 *   return: the job
 */
job_t *get_job_by_id(job_list_t *job_list, int pgid);

/*
 * Function: print_job_list
 * ------------------------
 *   Print the job list.
 *
 *   job_list: the job list
 */
void print_job_list(job_list_t *job_list);

/*
 * Function: free_job_list
 * -----------------------
 *    Free the job list.
 * 
 *    job_list: the job list
 */
void free_job_list(job_list_t *job_list);

/*
 * Function: terminal_signal_handler
 * ---------------------------------
 *    Handle the SIGTSTP, SIGTTIN, SIGTTOU, SIGINT, and SIGQUIT signals.
 * 
 *    handler: the signal handler
 */
void terminal_signal_handler(void (*handler)(int));

/*
 * Function: get_status
 * --------------------
 *    Get the status of a process given its exit status.
 * 
 *    status: the exit status
 */
enum job_status get_status(int status);

/*
 * Function: block_signal
 * -----------------------
 *    Block a signal.
 * 
 *    signal: the signal to be blocked or unblocked.
 *    block: whether to block the signal
 */
void block_signal(int signal, int block);

#endif