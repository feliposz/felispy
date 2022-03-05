#include <stdio.h>

void hello(int n)
{
    while (n-- > 0)
    {
        puts("Hello world!");
    }
}

int main(int argc, char **argv)
{
    hello(5);
    return 0;
}
