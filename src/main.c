#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <string.h>
#include <windows.h>

HANDLE hOutput;

char* readline(char* prompt)
{
    static char buffer[2048];

    SetConsoleTextAttribute(hOutput, 9);
    fputs(prompt, stdout);

    SetConsoleTextAttribute(hOutput, 7);
    fgets(buffer, 2048, stdin);
    char* result = (char*) malloc(strlen(buffer) + 1);
    strcpy(result, buffer);
    size_t last = strlen(result) - 1;
    if (result[last] == '\n')
    {
        result[last] = '\0';
    }
    return result;
}

void add_history(char *unused)
{
}

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

int main(int argc, char** argv)
{

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbInfo;
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutput, &csbInfo);
    SetConsoleTextAttribute(hOutput, 10);
#endif
    puts("Felispy 0.0.1 - by Felipo");
    puts("Type ctrl+C to exit.\n");
    int counter = 0;

    while (1)
    {
        char *input = readline("Felispy> ");
#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 15);
#endif
        printf("Result(%d): ", counter++);
#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 14);
#endif
        printf("%s\n", input);
        free(input);
    }

#ifdef _WIN32
    SetConsoleTextAttribute(hOutput, csbInfo.wAttributes);
#endif

    return 0;
}