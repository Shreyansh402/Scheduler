#pragma once

// Can include any other headers as needed
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_COMMANDS 100

typedef struct
{
    char *command;
    bool finished;
    bool error;
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    uint64_t burst_time;
    uint64_t arrival_time;
    int priority;
    bool timeed;
    pid_t pid;
    bool started;
    int process_id;

} Process;

// Function prototypes
void ShortestJobFirst();
void ShortestRemainingTimeFirst();
void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime);

pid_t current_pid;

void handle_alarm(int sig)
{
    // Signal handler for SIGALRM
    if (current_pid != -1)
    {
        kill(current_pid, SIGSTOP);
    }
}

// Set stdin to non-blocking mode
void setNonBlockingInput()
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// Function to split input into multiple commands
int splitCommands(char *input, char *commands[])
{
    int count = 0;
    char *command = strtok(input, "\n");
    while (command != NULL && count < MAX_COMMANDS)
    {
        commands[count++] = command;
        command = strtok(NULL, "\n");
    }
    return count;
}

// Function to check for new input and add it to the array p
bool checkForInput(Process *p, int *n, int *priority_count)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t arrival_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
    if (bytesRead > 0)
    {
        buffer[bytesRead] = '\0'; // Null-terminate the input
        char *commands[MAX_COMMANDS];
        int commandCount = splitCommands(buffer, commands);
        for (int i = 0; i < commandCount; i++)
        {
            // Add new command to the array p
            Process newProcess;
            newProcess.command = strdup(commands[i]); // Duplicate the command string
            newProcess.priority = 1;                  // Middle priority
            newProcess.process_id = *n + 1;
            newProcess.timeed = false;
            newProcess.waiting_time = 0;
            newProcess.burst_time = 0;
            newProcess.completion_time = 0;
            newProcess.arrival_time = arrival_time;
            newProcess.error = false;
            newProcess.finished = false;
            newProcess.pid = -1;

            p[*n] = newProcess;
            (*n)++;
            (priority_count[1])++;
        }
        return true;
    }
    return false;
}

void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime)
{
    // Multi-Level Feedback Queue Scheduling Algorithm

    // Set up signal handlers
    signal(SIGALRM, handle_alarm);

    // Set stdin to non-blocking mode
    setNonBlockingInput();

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
    FILE *f = fopen("result_online_MLFQ.csv", "w");
    fprintf(f, "Command,Finished,Error,Burst Time in milliseconds,Turnaround Time in milliseconds,Waiting Time in milliseconds,Response Time in milliseconds\n");
    fclose(f);

    int priority_count[3] = {0, 0, 0};
    Process *p = (Process *)malloc(60 * sizeof(Process));
    int n = 0;

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t init_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
    uint64_t prev_boost_time = init_time;

    while (1)
    {
        checkForInput(p, &n, priority_count);
        if (priority_count[0])
        {
            printf("Priority 0\n");
            for (int i = 0; i < n; i++)
            {
                if (p[i].priority != 0 || p[i].finished || p[i].error)
                {
                    continue;
                }
                if (checkForInput(p, &n, priority_count))
                {
                    break;
                }

                clock_gettime(CLOCK_MONOTONIC, &time);
                p[i].start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
                if (prev_boost_time + boostTime < p[i].start_time)
                {
                    for (int j = 0; j < n; j++)
                    {
                        if (!p[j].error && !p[j].finished && p[j].priority != 0)
                        {
                            priority_count[p[j].priority]--;
                            p[j].priority = 0;
                            priority_count[0]++;
                        }
                    }
                    prev_boost_time = p[i].start_time;
                }
                if (p[i].timeed == false)
                {
                    p[i].response_time = p[i].start_time - p[i].arrival_time;
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
                setitimer(ITIMER_REAL, &timer0, NULL);

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
                    priority_count[p[i].priority++]--;
                    priority_count[p[i].priority]++;
                }
                else
                {
                    // Process completed successfully
                    p[i].turnaround_time = p[i].completion_time - p[i].arrival_time;
                    priority_count[p[i].priority]--;
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
                    FILE *f = fopen("result_online_MLFQ.csv", "a");
                    fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p[i].command, p[i].finished ? "Yes" : "No", p[i].error ? "Yes" : "No", p[i].burst_time, p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
                    fclose(f);
                }

                //<Command>|<Start Time of the context>|<End Time of the context>
                printf("%s|%llu|%llu\n", p[i].command, p[i].start_time - init_time, p[i].completion_time - init_time);
            }
        }
        else if (priority_count[1])
        {
            printf("Priority 1\n");
            for (int i = 0; i < n; i++)
            {
                if (p[i].priority != 1 || p[i].finished || p[i].error)
                {
                    continue;
                }
                if (checkForInput(p, &n, priority_count))
                {
                    break;
                }

                clock_gettime(CLOCK_MONOTONIC, &time);
                p[i].start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
                if (prev_boost_time + boostTime < p[i].start_time)
                {
                    for (int j = 0; j < n; j++)
                    {
                        if (!p[j].error && !p[j].finished && p[j].priority != 0)
                        {
                            priority_count[p[j].priority]--;
                            p[j].priority = 0;
                            priority_count[0]++;
                        }
                    }
                    prev_boost_time = p[i].start_time;
                }
                if (p[i].timeed == false)
                {
                    p[i].response_time = p[i].start_time - p[i].arrival_time;
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
                setitimer(ITIMER_REAL, &timer1, NULL);

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
                    priority_count[p[i].priority++]--;
                    priority_count[p[i].priority]++;
                }
                else
                {
                    // Process completed successfully
                    p[i].turnaround_time = p[i].completion_time - p[i].arrival_time;
                    priority_count[p[i].priority]--;
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
                    FILE *f = fopen("result_online_MLFQ.csv", "a");
                    fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p[i].command, p[i].finished ? "Yes" : "No", p[i].error ? "Yes" : "No", p[i].burst_time, p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
                    fclose(f);
                }

                //<Command>|<Start Time of the context>|<End Time of the context>
                printf("%s|%llu|%llu\n", p[i].command, p[i].start_time - init_time, p[i].completion_time - init_time);
            }
        }
        else if (priority_count[2])
        {
            printf("Priority 2\n");
            for (int i = 0; i < n; i++)
            {
                if (p[i].priority != 2 || p[i].finished || p[i].error)
                {
                    continue;
                }
                if (checkForInput(p, &n, priority_count))
                {
                    break;
                }

                clock_gettime(CLOCK_MONOTONIC, &time);
                p[i].start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
                if (prev_boost_time + boostTime < p[i].start_time)
                {
                    for (int j = 0; j < n; j++)
                    {
                        if (!p[j].error && !p[j].finished && p[j].priority != 0)
                        {
                            priority_count[p[j].priority]--;
                            p[j].priority = 0;
                            priority_count[0]++;
                        }
                    }
                    prev_boost_time = p[i].start_time;
                }
                if (p[i].timeed == false)
                {
                    p[i].response_time = p[i].start_time - p[i].arrival_time;
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
                setitimer(ITIMER_REAL, &timer2, NULL);

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
                    p[i].turnaround_time = p[i].completion_time - p[i].arrival_time;
                    priority_count[p[i].priority]--;
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
                    FILE *f = fopen("result_online_MLFQ.csv", "a");
                    fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p[i].command, p[i].finished ? "Yes" : "No", p[i].error ? "Yes" : "No", p[i].burst_time, p[i].turnaround_time, p[i].waiting_time, p[i].response_time);
                    fclose(f);
                }

                //<Command>|<Start Time of the context>|<End Time of the context>
                printf("%s|%llu|%llu\n", p[i].command, p[i].start_time - init_time, p[i].completion_time - init_time);
            }
        }
    }
    return;
}