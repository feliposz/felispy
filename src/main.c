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

int count_all(mpc_ast_t* node)
{
    if (node->children_num == 0)
    {
        return 1;
    }
    else if (node->children_num >= 1)
    {
        int total = 1;
        for (int i = 0; i < node->children_num; i++)
        {
            total += count_all(node->children[i]);
        }
        return total;
    }
    return 0;
}

int count_leaves(mpc_ast_t *t)
{
    if (t->children_num == 0)
    {
        return 1;
    }
    int count = 0;
    for (int i = 0; i < t->children_num; i++)
    {
        count += count_leaves(t->children[i]);
    }
    return count;
}

int max_leaves(mpc_ast_t *t)
{
    if (t->children_num == 0)
    {
        return 0;
    }
    int max = t->children_num;
    for (int i = 0; i < t->children_num; i++)
    {
        int count = max_leaves(t->children[i]);
        max = max < count ? count : max;
    }
    return max;
}

int count_nodes(mpc_ast_t *t)
{
    if (t->children_num == 0)
    {
        return 0;
    }
    int count = 1;
    for (int i = 0; i < t->children_num; i++)
    {
        count += count_nodes(t->children[i]);
    }
    return count;
}

long eval_op(long x, char* op, long y)
{
    if (strcmp(op, "+") == 0)
    {
        return x + y;
    }
    else if (strcmp(op, "-") == 0)
    {
        return x - y;
    }
    else if (strcmp(op, "*") == 0)
    {
        return x * y;
    }
    else if (strcmp(op, "/") == 0)
    {
        return x / y;
    }
    else if (strcmp(op, "%") == 0)
    {
        return x % y;
    }
    else if (strcmp(op, "^") == 0)
    {
        long result = 1;
        while (y-- > 0)
        {
            result *= x;
        }
        return result;
    }
    else if (strcmp(op, "min") == 0)
    {
        return x < y ? x : y;
    }
    else if (strcmp(op, "max") == 0)
    {
        return x > y ? x : y;
    }
    return 0;
}

long eval(mpc_ast_t* t)
{
    if (strstr(t->tag, "number"))
    {
        return atoi(t->contents);
    }

    char* op = t->children[1]->contents;
    long result = eval(t->children[2]);
    // unary plus / minus
    if (t->children_num == 4 && strcmp(op, "-") == 0)
    {
        result = -result;
    }
    else
    {
        for (int i = 3; i < t->children_num - 1; i++)
        {
            result = eval_op(result, op, eval(t->children[i]));
        }
    }

    return result;
}

int main(int argc, char** argv)
{
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

#if 1
    char* Grammar =
        "number   : /-?[0-9]+/ ; "
        "operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\"; "
        "expr     : <number> | '(' <operator> <expr>+ ')' ; "
        "lispy    : /^/ <operator> <expr>+ /$/ ; ";
#endif

#if 0
    char* Grammar =
        "number   : /-?[0-9]+\\.?[0-9]*/ ; "
        "operator : '+' | '-' | '*' | '/' | '%' | \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ; "
        "expr     : <number> | '(' <operator> <expr>+ ')' ; "
        "lispy    : /^/ <operator> <expr>+ /$/ ;  ";
#endif

#if 0
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

#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 12);
#endif
        printf("Input: %s\n", input);

        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispy, &result))
        {
            printf("Count: %d\n", count_all(result.output));
            printf("Nodes: %d\n", count_nodes(result.output));
            printf("Leaves: %d\n", count_leaves(result.output));
            printf("Max. Leaves: %d\n", max_leaves(result.output));
#ifdef _WIN32
            SetConsoleTextAttribute(hOutput, 15);
#endif
            mpc_ast_print(result.output);

#ifdef _WIN32
            SetConsoleTextAttribute(hOutput, 14);
#endif
            printf("%li\n", eval(result.output));

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

        free(input);
    }

#ifdef _WIN32
    SetConsoleTextAttribute(hOutput, csbInfo.wAttributes);
#endif

    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}