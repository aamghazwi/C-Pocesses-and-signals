#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "utils.h"
#include "job_control.h"

job_list_t job_list;

void sigchld_handler(int sig) {
    job_t *job = job_list.first;
    block_signal(SIGCHLD, 1);
    while (job != NULL) {
        int status;
        // Non-blocking waitpid call for checking state changes of child processes.
        pid_t pid = waitpid(job->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid == job->pid) {
            enum job_status job_status = get_status(status);
            if (job_status == SIGNALED || job_status == EXITED) {
                if (job_status == EXITED) printf("[%d] Done\n", job->pgid);
                else printf("[%d] %d terminated by signal %d\n", job->pgid, job->pid, status);
                // If the job is signaled or exited, delete it from the job list.
                job = delete_job(&job_list, pid);
            }
            else if (job_status == SUSPENDED) {
                // Send SIGSTOP to the process group to stop all processes in the group.
                killpg(job->pgid, SIGSTOP);
                // If the job is suspended, change its state to STOPPED.
                job->state = STOPPED;
            }
            else if (job_status == CONTINUED) {
                // Send SIGCONT to the process group to continue all processes in the group.
                killpg(job->pgid, SIGCONT);
                // If the job is continued, change its state to BACKGROUND.
                job->state = BACKGROUND;
            }
        }
        if (job != NULL) job = job->next;
    }
    block_signal(SIGCHLD, 0);
    fflush(stdout);
}

int main(int argc, char const *argv[]) {
    char input[MAX_LINE];
    int num_args[MAX_ARGS], num_cmds;
    init_job_list(&job_list);
    // Ignore terminal signals.
    terminal_signal_handler(SIG_IGN);
    // Register SIGCHLD handler.
    signal(SIGCHLD, sigchld_handler);
    while (1) {
        // Print prompt.
        printf("> ");
        fflush(stdout);
        // Read input.
        char *line = fgets(input, MAX_LINE, stdin);
        // Check if EOF (Ctrl + D) is reached.
        if (line == NULL) {
            // Reap all zombie processes.
            for (int i = 0; i < job_list.size; i++) waitpid(-1, NULL, WNOHANG);
            // free_job_list(&job_list);
            printf("\n");
            // Exit shell.
            exit(0);
        }
        // Split input into arguments.
        char *args[MAX_CMDS][MAX_ARGS];
        parse_args(input, args, num_args, &num_cmds);

        // If no commands, continue.
        if (num_cmds == 0) continue;

        for (int k = 0; k < num_cmds; k++) {
            // If no arguments, continue.
            if (num_args[k] == 0) continue;

            // Check if the command needs to be executed in the background.
            // If the next command has non-zero arguments, the current command has to be executed in the background.
            int bg_process = num_cmds == 2 || (k < num_cmds - 1 && num_args[k + 1] > 0) ? 1 : 0;

            char *cmd = args[k][0];
            // Check for exit command.
            if (strcmp(cmd, "exit") == 0) {
                // Reap all zombie processes.
                for (int i = 0; i < job_list.size; i++) waitpid(-1, NULL, WNOHANG);
                free_job_list(&job_list);
                // Exit shell.
                exit(0);
            }
            // Check if the command has a path.
            if (strchr(cmd, '/') == NULL) {
                // If not, check if it's a built-in command.
                int found = 0;
                for (int i = 0; i < 5; i++)
                    if (strcmp(cmd, builtin_cmd[i]) == 0) {
                        // If it's a built-in command, execute it.
                        builtin_func[i](&job_list, args[k]);
                        found = 1;
                        break;
                    }
                // If it's not a built-in command, check if it's in the default paths.
                if (!found) {
                    for (int i = 0; i < 2; i++) {
                        char *path = malloc(strlen(prog_dir[i]) + strlen(cmd) + 1);
                        strcpy(path, prog_dir[i]);
                        strcat(path, cmd);
                        if (access(path, F_OK) != -1) {
                            // Fork a child process to execute the command.
                            pid_t pid = fork();
                            // Child process.
                            if (pid == 0) {
                                pid = getpid();
                                // Establish child process group to avoid race (if the parent process has not done it yet).
                                setpgid(pid, pid);
                                // If it is a foreground process, associate the process group with the terminal.
                                if (!bg_process) tcsetpgrp(STDIN_FILENO, pid);
                                // Restore default terminal signals.
                                terminal_signal_handler(SIG_DFL);
                                // Execute the command.
                                execv(path, args[k]);
                                // If execv returns, it means there was an error.
                                perror("execv");
                                free(path);
                                exit(-1);
                            }
                            // Parent process.
                            else if (pid > 0) {
                                // Create a new process group for the command.
                                setpgid(pid, pid);
                                if (!bg_process) {
                                    // Add the job to the job list.
                                    block_signal(SIGCHLD, 1);
                                    job_t *job = add_job(&job_list, pid, FOREGROUND, args[k]);
                                    block_signal(SIGCHLD, 0);
                                    // Associate the process group with the terminal.
                                    tcsetpgrp(STDIN_FILENO, pid);
                                    int status;
                                    waitpid(-pid, &status, WUNTRACED);
                                    // Get the terminal back.
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
                                        delete_job(&job_list, pid);
                                        block_signal(SIGCHLD, 0);
                                    }
                                }
                                else {
                                    // Print the job pgid and the command line.
                                    block_signal(SIGCHLD, 1);
                                    job_t *job = add_job(&job_list, pid, BACKGROUND, args[k]);
                                    block_signal(SIGCHLD, 0);
                                    printf("[%d] %d\n", job->pgid, job->pid);
                                }
                                free(path);
                            }
                            // Error.
                            else {
                                perror("fork");
                                free(path);
                            }
                            found = 1;
                            break;
                        }
                        free(path);
                    }
                }
                // If it's not in the default paths, print an error.
                if (!found) printf("%s: command not found\n", cmd);
            }
            else {
                // If it has a path, check if it's a valid path.
                if (access(cmd, F_OK) != -1) {
                    // Fork a child process to execute the command.
                    pid_t pid = fork();
                    // Child process.
                    if (pid == 0) {
                        pid = getpid();
                        // Establish child process group to avoid race (if the parent has not done it yet).
                        setpgid(pid, pid);
                        // If it is a foreground process, associate the process group with the terminal.
                        if (!bg_process) tcsetpgrp(STDIN_FILENO, pid);
                        // Restore default terminal signals.
                        terminal_signal_handler(SIG_DFL);
                        // Execute the command.
                        execv(cmd, args[k]);
                        // If execv returns, it means there was an error.
                        perror("execv");
                        exit(-1);
                    }
                    // Parent process.
                    else if (pid > 0) {
                        // Create a new process group for the command.
                        setpgid(pid, pid);
                        if(!bg_process) {
                            // Add the job to the job list.
                            block_signal(SIGCHLD, 1);
                            job_t *job = add_job(&job_list, pid, FOREGROUND, args[k]);
                            block_signal(SIGCHLD, 0);
                            // Associate the process group with the terminal.
                            tcsetpgrp(STDIN_FILENO, pid);
                            int status;
                            waitpid(-pid, &status, WUNTRACED);
                            // Get the terminal back.
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
                                delete_job(&job_list, pid);
                                block_signal(SIGCHLD, 0);
                            }
                        }
                        else {
                            // Print the job pgid and the command line.
                            block_signal(SIGCHLD, 1);
                            job_t *job = add_job(&job_list, pid, BACKGROUND, args[k]);
                            block_signal(SIGCHLD, 0);
                            printf("[%d] %d\n", job->pgid, job->pid);
                        }
                    }
                    else if (pid < 0) perror("fork");
                }
                // If it's not a valid path, print an error.
                else printf("%s: command not found\n", cmd);
            }
            for (int i = 0; i < num_args[k]; i++) free(args[k][i]);
        }
    }
    return 0;
}
