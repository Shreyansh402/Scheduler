
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    long long int a = 10;
    for (long long int i = 0; i < 2000000000; i++)
    {
        a++;
    }
    printf("%lld\n", a);

    return 0;
}