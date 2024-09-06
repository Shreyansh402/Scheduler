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
#define MAX_COMMANDS 64
#define HASH_MAP_SIZE 100
#define HEAP_SIZE 100

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
    uint64_t expected_burst_time;
    int priority;
    bool timeed;
    pid_t pid;
    bool started;
    int process_id;

} Process;

typedef struct HashMapEntry
{
    char *command;
    uint64_t total_burst_time;
    int count;
    struct HashMapEntry *next;
} HashMapEntry;

typedef struct
{
    HashMapEntry *buckets[HASH_MAP_SIZE];
} HashMap;

typedef struct
{
    Process *heap[HEAP_SIZE];
    int size;
} Heap;

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

// Hash function for the HashMap
unsigned long hash(char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_MAP_SIZE;
}

// Initialize the HashMap
void initHashMap(HashMap *map)
{
    for (int i = 0; i < HASH_MAP_SIZE; i++)
    {
        map->buckets[i] = NULL;
    }
}

// Insert an entry into the HashMap
void insertHashMap(HashMap *map, char *command, uint64_t burst_time)
{
    unsigned long index = hash(command);
    HashMapEntry *entry = map->buckets[index];
    while (entry != NULL)
    {
        if (strcmp(entry->command, command) == 0)
        {
            entry->total_burst_time += burst_time;
            entry->count++;
            return;
        }
        entry = entry->next;
    }
    HashMapEntry *newEntry = (HashMapEntry *)malloc(sizeof(HashMapEntry));
    newEntry->command = strdup(command);
    newEntry->total_burst_time = burst_time;
    newEntry->count = 1;
    newEntry->next = map->buckets[index];
    map->buckets[index] = newEntry;
}

// Get the average burst time for a command from the HashMap
uint64_t getAverageBurstTime(HashMap *map, char *command)
{
    unsigned long index = hash(command);
    HashMapEntry *entry = map->buckets[index];
    while (entry != NULL)
    {
        if (strcmp(entry->command, command) == 0)
        {
            return entry->total_burst_time / entry->count;
        }
        entry = entry->next;
    }
    return 0;
}

// Initialize the heap
void initHeap(Heap *p)
{
    p->size = 0;
}

// Heapify up the heap
void heapifyUp(Heap *p, int index)
{
    while (index > 0 && p->heap[(index - 1) / 2]->expected_burst_time > p->heap[index]->expected_burst_time)
    {
        Process *temp = p->heap[(index - 1) / 2];
        p->heap[(index - 1) / 2] = p->heap[index];
        p->heap[index] = temp;
        index = (index - 1) / 2;
    }
}

// Heapify down the heap
void heapifyDown(Heap *p, int index)
{
    int left, right, smallest;
    while (2 * index + 1 < p->size)
    {
        left = 2 * index + 1;
        right = 2 * index + 2;
        smallest = left;
        if (right < p->size && p->heap[right]->expected_burst_time < p->heap[left]->expected_burst_time)
        {
            smallest = right;
        }
        if (p->heap[index]->expected_burst_time < p->heap[smallest]->expected_burst_time)
        {
            break;
        }
        Process *temp = p->heap[index];
        p->heap[index] = p->heap[smallest];
        p->heap[smallest] = temp;
        index = smallest;
    }
}

// Insert a process into the heap
void insertHeap(Heap *p, Process *process)
{
    p->heap[p->size++] = process;
    heapifyUp(p, p->size - 1);
}

// Remove from the heap
Process *removeHeap(Heap *heap)
{
    Process *process = heap->heap[0];
    heap->heap[0] = heap->heap[--heap->size];
    heapifyDown(heap, 0);
    return process;
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
bool checkForInputMLFQ(Process *p, int *n, int *priority_count, HashMap *map, int quantum0, int quantum1, int quantum2)
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
            newProcess.process_id = *n + 1;
            newProcess.timeed = false;
            newProcess.waiting_time = 0;
            newProcess.burst_time = 0;
            newProcess.completion_time = 0;
            newProcess.arrival_time = arrival_time;
            newProcess.error = false;
            newProcess.finished = false;
            newProcess.pid = -1;

            // Set the priority of the process based on the average burst time
            uint64_t avg_burst_time = getAverageBurstTime(map, newProcess.command);
            if (avg_burst_time == 0)
            {
                newProcess.priority = 1;
            }
            else if (avg_burst_time <= quantum0)
            {
                newProcess.priority = 0;
            }
            else if (avg_burst_time <= quantum1)
            {
                newProcess.priority = 1;
            }
            else
            {
                newProcess.priority = 2;
            }

            p[*n] = newProcess;
            (*n)++;
            (priority_count[newProcess.priority])++;
        }
        return true;
    }
    return false;
}

bool checkForInput(Heap *heap, HashMap *map)
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
            Process *newProcess = (Process *)malloc(sizeof(Process));
            newProcess->command = strdup(commands[i]); // Duplicate the command string
            newProcess->process_id = heap->size + 1;
            newProcess->timeed = false;
            newProcess->waiting_time = 0;
            newProcess->burst_time = 0;
            newProcess->completion_time = 0;
            newProcess->arrival_time = arrival_time;
            newProcess->error = false;
            newProcess->finished = false;
            newProcess->pid = -1;

            // Set the priority of the process based on the average burst time
            uint64_t avg_burst_time = getAverageBurstTime(map, newProcess->command);
            if (avg_burst_time == 0)
            {
                newProcess->expected_burst_time = 1000;
            }
            else
            {
                newProcess->expected_burst_time = avg_burst_time;
            }

            // Insert the new process into the heap
            insertHeap(heap, newProcess);
        }
        return true;
    }
    return false;
}

void processing(Process *p, int *n, int *priority_count, HashMap *map, int priority, int quantum0, int quantum1, int quantum2, struct itimerval *timer, struct itimerval *zero_timer, uint64_t *prev_boost_time, uint64_t boostTime, uint64_t init_time)
{
    struct timespec time;
    for (int i = 0; i < *n; i++)
    {
        if (p[i].priority != priority || p[i].finished || p[i].error)
        {
            continue;
        }
        if (checkForInputMLFQ(p, n, priority_count, map, quantum0, quantum1, quantum2))
        {
            break;
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
                insertHashMap(map, p[i].command, p[i].burst_time);
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
    Process *p = (Process *)malloc(MAX_COMMANDS * sizeof(Process));
    int n = 0;

    // Initialize the HashMap
    HashMap map;
    initHashMap(&map);

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t init_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
    uint64_t prev_boost_time = init_time;

    while (1)
    {
        checkForInputMLFQ(p, &n, priority_count, &map, quantum0, quantum1, quantum2);
        if (priority_count[0])
        {
            processing(p, &n, priority_count, &map, 0, quantum0, quantum1, quantum2, &timer0, &zero_timer, &prev_boost_time, boostTime, init_time);
        }
        else if (priority_count[1])
        {
            processing(p, &n, priority_count, &map, 1, quantum0, quantum1, quantum2, &timer1, &zero_timer, &prev_boost_time, boostTime, init_time);
        }
        else if (priority_count[2])
        {
            processing(p, &n, priority_count, &map, 2, quantum0, quantum1, quantum2, &timer2, &zero_timer, &prev_boost_time, boostTime, init_time);
        }
    }
    free(p);
    free(map.buckets);
    return;
}

void ShortestJobFirst()
{
    // Shortest Job First Scheduling Algorithm

    // Set up signal handlers
    signal(SIGALRM, handle_alarm);

    // Set stdin to non-blocking mode
    setNonBlockingInput();

    // Creating the CSV file
    FILE *f = fopen("result_online_SJF.csv", "w");
    fprintf(f, "Command,Finished,Error,Burst Time in milliseconds,Turnaround Time in milliseconds,Waiting Time in milliseconds,Response Time in milliseconds\n");
    fclose(f);

    // Initialize the Heap
    Heap p;
    initHeap(&p);

    // Initialize the HashMap
    HashMap map;
    initHashMap(&map);

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t init_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;

    while (1)
    {
        if (checkForInput(&p, &map))
        {
            continue;
        }
        if (p.size > 0)
        {
            Process currentProcess = *removeHeap(&p);
            clock_gettime(CLOCK_MONOTONIC, &time);
            currentProcess.start_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
            if (currentProcess.timeed == false)
            {
                currentProcess.response_time = currentProcess.start_time - currentProcess.arrival_time;
                currentProcess.timeed = true;
                currentProcess.waiting_time = currentProcess.response_time;
            }
            else
            {
                currentProcess.waiting_time += currentProcess.start_time - currentProcess.completion_time;
            }

            pid_t pid = fork();
            if (pid == 0)
            {
                // tokenizing the command
                char *args[100];
                char *command_copy = strdup(currentProcess.command);
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
                currentProcess.pid = pid;
                // Waiting for the process to complete
                int status;
                waitpid(currentProcess.pid, &status, 0);

                clock_gettime(CLOCK_MONOTONIC, &time);
                currentProcess.completion_time = time.tv_sec * 1000 + time.tv_nsec / 1000000;
                currentProcess.burst_time += currentProcess.completion_time - currentProcess.start_time;
                currentProcess.turnaround_time = currentProcess.completion_time - currentProcess.arrival_time;
                if (WEXITSTATUS(status))
                {
                    // Process did not complete successfully
                    currentProcess.error = true;
                    currentProcess.finished = false;
                }
                else
                {
                    // Process completed successfully
                    currentProcess.finished = true;
                    currentProcess.error = false;
                    insertHashMap(&map, currentProcess.command, currentProcess.burst_time);
                }

                //<Command>|<Start Time of the context>|<End Time of the context>
                printf("%s|%llu|%llu\n", currentProcess.command, currentProcess.start_time - init_time, currentProcess.completion_time - init_time);
                // Write in CSV file with finished and error as 'Yes' or 'No'
                FILE *f = fopen("result_online_SJF.csv", "a");
                fprintf(f, "%s,%s,%s,%llu,%llu,%llu,%llu\n", currentProcess.command, currentProcess.finished ? "Yes" : "No", currentProcess.error ? "Yes" : "No", currentProcess.burst_time, currentProcess.turnaround_time, currentProcess.waiting_time, currentProcess.response_time);
                fclose(f);
            }
            else
            {
                printf("Error in forking the process\n");
            }
        }
    }
    free(p.heap);
    free(map.buckets);
    return;
}
