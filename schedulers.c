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
#include "offline_schedulers.h"

int main()
{
    Process p[10];
    p[0].command = "sleep 1";
    p[1].command = "fsdfg";
    p[2].command = "sleep 3";
    p[3].command = "ls";
    p[4].command = "echo hello";
    p[5].command = "sleep 4";
    p[6].command = "echo world";
    p[7].command = "ls";
    p[8].command = "pwd";
    p[9].command = "sleep 2";
    RoundRobin(p, 10, 1);
    return 0;
}