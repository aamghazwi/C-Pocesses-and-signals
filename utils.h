#include "job_control.h"

#ifndef UTILS_H
#define UTILS_H

#define MAX_LINE 2048
#define ARG_LEN 128
#define MAX_ARGS 20
#define MAX_CMDS 10
#define PATH_LEN 128

extern const char *prog_dir[2];
extern const char *builtin_cmd[5];
extern void (*const builtin_func[5])(job_list_t *, char **args);

/*
 * Function: parse_args
 * -------------------
 *   Parse the command line and split it into arguments.
 *   It can handle escaped whitespace and quoted strings with spaces as well.
 *   (Even though it's not required in the assignment)
 *
 *   line: the command line
 *   args: the array of arguments
 *   arg_count: the number of arguments
 */
void parse_args(char *line, char *args[][MAX_ARGS], int arg_count[], int *cmd_count);

/*
 * Function: bg
 * ------------
 *   Run a suspended job in the background.
 * 
 *   job_list: the job list
 *   pgid: the job ID
 */
void bg(job_list_t *job_list, char **args);

/*
 * Function: cd
 * ------------
 *   Change the current working directory to the given (absolute or relative) path.
 *   If no path is given, use the value of environment variable HOME.
 *   The shell should update the environment variable PWD to the new (absolute) path.
 * 
 *   path: the path
 */
void cd(job_list_t *job_list, char **args);

/*
 * Function: fg
 * ------------
 *   Run a suspended or background job in the foreground.
 * 
 *   job_list: the job list
 *   pgid: the job ID
 */
void fg(job_list_t *job_list, char **args);

/*
 * Function: jobs
 * --------------
 *   Print the list of jobs.
 * 
 *   job_list: the job list
 */
void jobs(job_list_t *job_list, char **args);

/*
 * Function: kill_job
 * ------------------
 *   Send a SIGTERM signal to a job.
 * 
 *   job_list: the job list
 *   pgid: the job ID
 */
void kill_job(job_list_t *job_list, char **args);

#endif