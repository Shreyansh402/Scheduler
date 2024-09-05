// Operating Systems Lab Assignment-2
// Name: Shreyansh Jain
// Entry No.: 2021MT10230
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
// #include "offline_schedulers.h"
#include "online_schedulers.h"

int main()
{
    Process p[10];
    p[0].command = "./b.out";
    p[1].command = "fsdgs";
    p[2].command = "./c.out";
    p[3].command = "ls gewgwe";
    p[4].command = "echo hello";
    p[5].command = "./d.out";
    p[6].command = "cd ../";
    p[7].command = " ";
    p[8].command = "pwd";
    p[9].command = "echo Round Completed";
    // FCFS(p, 10);
    // RoundRobin(p, 10, 100);
    // MultiLevelFeedbackQueue(p, 10, 100, 500, 1000, 5000);

    MultiLevelFeedbackQueue(10, 500, 1000, 10000);

    return 0;
}