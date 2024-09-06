#pragma once

// Can include any other headers as needed
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

typedef struct
{

    // This will be given by the tester function this is the process command to be scheduled
    char *command;

    // Temporary parameters for your usage can modify them as you wish
    bool finished; // If the process is finished safely
    bool error;    // If an error occurs during execution
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    uint64_t burst_time;
    pid_t pid;
    bool timeed;
    int process_id;
    int priority;

} Process;

// Function prototypes
void FCFS(Process p[], int n);
void RoundRobin(Process p[], int n, int quantum);
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime);

// Assignment-2 Operating Systems
// Name: Shreyansh Jain
// Entry No.: 2021MT10230
// Implentation of Offline Scheduling Algorithms in C

pid_t current_pid;

void handle_alarm(int sig)
{
    // Signal handler for SIGALRM
    if (current_pid != -1)
    {
        kill(current_pid, SIGSTOP);
    }
}

void processing(Process *p, int *n, int *finished_processes, int *priority_count, int priority, int quantum0, int quantum1, int quantum2, struct itimerval *timer, struct itimerval *zero_timer, uint64_t *prev_boost_time, uint64_t boostTime, uint64_t init_time)
{
    struct timespec time;
    for (int i = 0; i < *n; i++)
    {
        if (p[i].priority != priority || p[i].finished || p[i].error)
        {
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &time);
        p[i].start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
        if (*prev_boost_time + boostTime < p[i].start_time)
        {
            for (int j = 0; j < *n; j++)
            {
                if (!p[j].error && !p[j].finished && p[j].priority != 0)
                {
                    priority_count[p[j].priority]--;
                    p[j].priority = 0;
                    priority_count[0]++;
                }
            }
            *prev_boost_time = p[i].start_time;
        }
        if (p[i].timeed == false)
        {
            p[i].response_time = p[i].start_time - init_time;
            p[i].timeed = true;
            p[i].waiting_time = p[i].response_time;
        }
        else
        {
            p[i].waiting_time += p[i].start_time - p[i].completion_time;
        }

        if (p[i].pid != -1)
        {
            kill(p[i].pid, SIGCONT);
        }
        else
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                // tokenizing the command
                char *args[100];
                char *command_copy = strdup(p[i].command);
                char *token = strtok(command_copy, " ");
                int j = 0;
                while (token != NULL)
                {
                    args[j++] = strdup(token);
                    token = strtok(NULL, " ");
                }
                args[j] = NULL;
                free(command_copy);
                execvp(args[0], args);
                exit(1);
            }
            else if (pid > 0)
            {
                p[i].pid = pid;
            }
            else
            {
                printf("Error in forking the process\n");
            }
        }

        current_pid = p[i].pid;

        // Setting the timer
        setitimer(ITIMER_REAL, timer, NULL);

        // Waiting for the process to complete
        int status;
        waitpid(p[i].pid, &status, WUNTRACED);

        // Disabling the timer
        setitimer(ITIMER_REAL, zero_timer, NULL);

        current_pid = -1;

        clock_gettime(CLOCK_MONOTONIC, &time);
        p[i].completion_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
        p[i].burst_time += p[i].completion_time - p[i].start_time;
        if (WIFSTOPPED(status))
        {
            // Process stopped due to alarm
            if (p[i].priority != 2)
            {
                priority_count[p[i].priority++]--;
                priority_count[p[i].priority]++;
            }
        }
        else
        {
            // Process completed successfully
            p[i].turnaround_time = p[i].completion_time - init_time;
            priority_count[p[i].priority]--;
            finished_processes++;
            if (WEXITSTATUS(status))
            {
                // Process did not complete successfully
                p[i].error = true;
                p[i].finished = false;
            }
            else
            {
                // Process completed successfully
                p[i].finished = true;
                p[i].error = false;
            }

            // Write in CSV file with finished and error as 'Yes' or 'No'
            FILE *f = fopen("result_offline_MLFQ.csv", "a");
            fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p[i].command, p[i].finished ? "Yes" : "No", p[i].error ? "Yes" : "No", p[i].burst_time, p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
            fclose(f);
        }

        //<Command>|<Start Time of the context>|<End Time of the context>
        printf("%s|%llu|%llu\n", p[i].command, p[i].start_time - init_time, p[i].completion_time - init_time);
    }
}

void FCFS(Process p[], int n)
{
    // First Come First Serve Scheduling Algorithm

    // Creating the CSV file
    FILE *f = fopen("result_offline_FCFS.csv", "w");
    fprintf(f, "Command,Finished,Error,Burst Time in milliseconds,Turnaround Time in milliseconds,Waiting Time in milliseconds,Response Time in milliseconds\n");
    fclose(f);

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    // Initial time
    uint64_t init_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;

    for (int i = 0; i < n; i++)
    {
        p[i].process_id = i + 1;
        clock_gettime(CLOCK_MONOTONIC, &time);
        p[i].start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
        // Forking the process
        pid_t pid = fork();
        if (pid == 0)
        {
            // tokenizing the command
            char *args[100];
            char *command_copy = strdup(p[i].command);
            char *token = strtok(command_copy, " ");
            int j = 0;
            while (token != NULL)
            {
                args[j++] = strdup(token);
                token = strtok(NULL, " ");
            }
            args[j] = NULL;
            free(command_copy);
            execvp(args[0], args);
            exit(1);
        }
        else if (pid > 0)
        {
            // Waiting for the process to complete
            int status;
            waitpid(pid, &status, 0);
            clock_gettime(CLOCK_MONOTONIC, &time);
            p[i].completion_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
            p[i].burst_time = p[i].completion_time - p[i].start_time;
            p[i].response_time = p[i].start_time - init_time;
            p[i].waiting_time = p[i].response_time;
            p[i].turnaround_time = p[i].completion_time - init_time;
            if (WEXITSTATUS(status))
            {
                // Process did not complete successfully
                p[i].error = true;
                p[i].finished = false;
            }
            else
            {
                // Process completed successfully
                p[i].finished = true;
                p[i].error = false;
            }

            //<Command>|<Start Time of the context>|<End Time of the context>
            printf("%s|%llu|%llu\n", p[i].command, p[i].start_time - init_time, p[i].completion_time - init_time);
            // Write in CSV file with finished and error as 'Yes' or 'No'
            FILE *f = fopen("result_offline_FCFS.csv", "a");
            fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p[i].command, p[i].finished ? "Yes" : "No", p[i].error ? "Yes" : "No", p[i].burst_time, p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
            fclose(f);
        }
        else
        {
            // Error in forking the process
            printf("Error in forking the process\n");
        }
    }

    return;
}

void RoundRobin(Process p[], int n, int quantum)
{
    // Round Robin Scheduling Algorithm

    // Signal handler for SIGALRM
    signal(SIGALRM, handle_alarm);

    // Setting the timer for the quantum milliseconds
    struct itimerval timer;
    timer.it_value.tv_sec = quantum / 1000;           // seconds
    timer.it_value.tv_usec = (quantum % 1000) * 1000; // microseconds
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    struct itimerval zero_timer;
    zero_timer.it_value.tv_sec = 0;
    zero_timer.it_value.tv_usec = 0;
    zero_timer.it_interval.tv_sec = 0;
    zero_timer.it_interval.tv_usec = 0;

    // Creating the CSV file
    FILE *f = fopen("result_offline_RR.csv", "w");
    fprintf(f, "Command,Finished,Error,Burst Time in milliseconds,Turnaround Time in milliseconds,Waiting Time in milliseconds,Response Time in milliseconds\n");
    fclose(f);

    for (int i = 0; i < n; i++)
    {
        p[i].process_id = i + 1;
        p[i].timeed = false;
        p[i].waiting_time = 0;
        p[i].burst_time = 0;
        p[i].completion_time = 0;
        p[i].error = false;
        p[i].finished = false;
        p[i].pid = -1;
    }

    int finished_processes = 0;

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    // Initial time
    uint64_t init_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;

    while (finished_processes < n)
    {
        for (int i = 0; i < n; i++)
        {
            if (p[i].finished || p[i].error)
            {
                continue;
            }

            clock_gettime(CLOCK_MONOTONIC, &time);
            p[i].start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
            if (p[i].timeed == false)
            {
                p[i].response_time = p[i].start_time - init_time;
                p[i].timeed = true;
                p[i].waiting_time = p[i].response_time;
            }
            else
            {
                p[i].waiting_time += p[i].start_time - p[i].completion_time;
            }

            if (p[i].pid != -1)
            {
                kill(p[i].pid, SIGCONT);
            }
            else
            {
                pid_t pid = fork();
                if (pid == 0)
                {
                    // tokenizing the command
                    char *args[100];
                    char *command_copy = strdup(p[i].command);
                    char *token = strtok(command_copy, " ");
                    int j = 0;
                    while (token != NULL)
                    {
                        args[j++] = strdup(token);
                        token = strtok(NULL, " ");
                    }
                    args[j] = NULL;
                    free(command_copy);
                    execvp(args[0], args);
                    exit(1);
                }
                else if (pid > 0)
                {
                    p[i].pid = pid;
                }
                else
                {
                    printf("Error in forking the process\n");
                }
            }

            current_pid = p[i].pid;

            // Setting the timer
            setitimer(ITIMER_REAL, &timer, NULL);

            // Waiting for the process to complete
            int status;
            waitpid(p[i].pid, &status, WUNTRACED);

            // Disabling the timer
            setitimer(ITIMER_REAL, &zero_timer, NULL);

            current_pid = -1;

            clock_gettime(CLOCK_MONOTONIC, &time);
            p[i].completion_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
            p[i].burst_time += p[i].completion_time - p[i].start_time;
            if (WIFSTOPPED(status))
            {
                // Process stopped due to alarm
            }
            else
            {
                // Process completed successfully
                p[i].turnaround_time = p[i].completion_time - init_time;
                finished_processes++;
                if (WEXITSTATUS(status))
                {
                    // Process did not complete successfully
                    p[i].error = true;
                    p[i].finished = false;
                }
                else
                {
                    // Process completed successfully
                    p[i].finished = true;
                    p[i].error = false;
                }

                // Write in CSV file with finished and error as 'Yes' or 'No'
                FILE *f = fopen("result_offline_RR.csv", "a");
                fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p[i].command, p[i].finished ? "Yes" : "No", p[i].error ? "Yes" : "No", p[i].burst_time, p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
                fclose(f);
            }

            //<Command>|<Start Time of the context>|<End Time of the context>
            printf("%s|%llu|%llu\n", p[i].command, p[i].start_time - init_time, p[i].completion_time - init_time);
        }
    }
    return;
}

void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime)
{
    // Multi-Level Feedback Queue Scheduling Algorithm

    // Set up signal handlers
    signal(SIGALRM, handle_alarm);

    // Setting the timer for the quantum milliseconds
    struct itimerval timer0;
    timer0.it_value.tv_sec = quantum0 / 1000;           // seconds
    timer0.it_value.tv_usec = (quantum0 % 1000) * 1000; // microseconds
    timer0.it_interval.tv_sec = 0;
    timer0.it_interval.tv_usec = 0;

    struct itimerval timer1;
    timer1.it_value.tv_sec = quantum1 / 1000;
    timer1.it_value.tv_usec = (quantum1 % 1000) * 1000;
    timer1.it_interval.tv_sec = 0;
    timer1.it_interval.tv_usec = 0;

    struct itimerval timer2;
    timer2.it_value.tv_sec = quantum2 / 1000;
    timer2.it_value.tv_usec = (quantum2 % 1000) * 1000;
    timer2.it_interval.tv_sec = 0;
    timer2.it_interval.tv_usec = 0;

    struct itimerval zero_timer;
    zero_timer.it_value.tv_sec = 0;
    zero_timer.it_value.tv_usec = 0;
    zero_timer.it_interval.tv_sec = 0;
    zero_timer.it_interval.tv_usec = 0;

    // Creating the CSV file
    FILE *f = fopen("result_offline_MLFQ.csv", "w");
    fprintf(f, "Command,Finished,Error,Burst Time in milliseconds,Turnaround Time in milliseconds,Waiting Time in milliseconds,Response Time in milliseconds\n");
    fclose(f);

    for (int i = 0; i < n; i++)
    {
        p[i].process_id = i + 1;
        p[i].timeed = false;
        p[i].waiting_time = 0;
        p[i].burst_time = 0;
        p[i].completion_time = 0;
        p[i].error = false;
        p[i].finished = false;
        p[i].priority = 0;
        p[i].pid = -1;
    }

    int finished_processes = 0;
    int priority_count[3] = {n, 0, 0};

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    // Initial time
    uint64_t init_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
    uint64_t prev_boost_time = init_time;

    while (finished_processes < n)
    {
        if (priority_count[0])
        {
            processing(p, &n, &finished_processes, priority_count, 0, quantum0, quantum1, quantum2, &timer0, &zero_timer, &prev_boost_time, boostTime, init_time);
        }
        else if (priority_count[1])
        {
            processing(p, &n, &finished_processes, priority_count, 1, quantum0, quantum1, quantum2, &timer1, &zero_timer, &prev_boost_time, boostTime, init_time);
        }
        else if (priority_count[2])
        {
            processing(p, &n, &finished_processes, priority_count, 2, quantum0, quantum1, quantum2, &timer2, &zero_timer, &prev_boost_time, boostTime, init_time);
        }
        else
        {
            break;
        }
    }
    return;
}
