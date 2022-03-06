#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpc.h"

#ifdef _WIN32
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
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

#if 0
    char* Grammar =
        "number   : /-?[0-9]+/ ;                            "
        "operator : '+' | '-' | '*' | '/' ;                 "
        "expr     : <number> | '(' <operator> <expr>+ ')' ; "
        "lispy    : /^/ <operator> <expr>+ /$/ ;            ";
#endif

#if 0
    char* Grammar =
        "number   : /-?[0-9]+\\.?[0-9]*/ ; "
        "operator : '+' | '-' | '*' | '/' | '%' | \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ; "
        "expr     : <number> | '(' <operator> <expr>+ ')' ; "
        "lispy    : /^/ <operator> <expr>+ /$/ ;  ";
#endif

#if 1
    char* Grammar =
        "number   : /-?[0-9]+\\.?[0-9]*/ ; "
        "operator : '+' | '-' | '*' | '/' | '%' | \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ; "
        "expr     : <number> <operator> <expr> | '(' <expr> ')' <operator> <expr> | '(' <expr> ')' | <number> ; "
        "lispy    : /^/ <expr> /$/ ;  ";
#endif

    mpca_lang(MPCA_LANG_DEFAULT, Grammar, Number, Operator, Expr, Lispy);

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbInfo;
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutput, &csbInfo);
    SetConsoleTextAttribute(hOutput, 10);
#endif
    puts("Felispy 0.0.1 - by Felipo");
    puts("Type ctrl+C to exit.\n");

    while (1)
    {
        char* input = readline("Felispy> ");

        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispy, &result))
        {
#ifdef _WIN32
            SetConsoleTextAttribute(hOutput, 15);
#endif
            mpc_ast_print(result.output);
            mpc_ast_delete(result.output);
        }
        else
        {
#ifdef _WIN32
            SetConsoleTextAttribute(hOutput, 13);
#endif
            mpc_err_print(result.error);
            mpc_err_delete(result.error);
        }

#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 14);
#endif

        printf("%s\n", input);
        free(input);
    }

#ifdef _WIN32
    SetConsoleTextAttribute(hOutput, csbInfo.wAttributes);
#endif

    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}