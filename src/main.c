#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpc.h"

typedef enum { LVAL_INTEGER, LVAL_DECIMAL, LVAL_ERR } lval_type_t;
typedef enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM } lval_err_t;

typedef struct
{
    lval_type_t type;
    union
    {
        long integer;
        double decimal;
        lval_err_t err;
    };
} lval;

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

lval lval_integer(long integer)
{
    lval l;
    l.type = LVAL_INTEGER;
    l.integer = integer;
    return l;
}

lval lval_decimal(double decimal)
{
    lval l;
    l.type = LVAL_DECIMAL;
    l.decimal = decimal;
    return l;
}

lval lval_err(int err)
{
    lval l;
    l.type = LVAL_ERR;
    l.err = err;
    return l;
}

lval eval_op(lval x, char* op, lval y)
{
    if (x.type == LVAL_ERR)
    {
        return x;
    }
    if (y.type == LVAL_ERR)
    {
        return y;
    }

    if (x.type == LVAL_INTEGER && y.type == LVAL_INTEGER)
    {
        if (strcmp(op, "+") == 0)
        {
            return lval_integer(x.integer + y.integer);
        }
        else if (strcmp(op, "-") == 0)
        {
            return lval_integer(x.integer - y.integer);
        }
        else if (strcmp(op, "*") == 0)
        {
            return lval_integer(x.integer * y.integer);
        }
        else if (strcmp(op, "/") == 0)
        {
            if (y.integer == 0)
            {
                return lval_err(LERR_DIV_ZERO);
            }
            else
            {
                return lval_integer(x.integer / y.integer);
            }
        }
        else if (strcmp(op, "%") == 0)
        {
            if (y.integer == 0)
            {
                return lval_err(LERR_DIV_ZERO);
            }
            else
            {
                return lval_integer(x.integer % y.integer);
            }
        }
        else if (strcmp(op, "^") == 0)
        {
            long result = 1;
            for (long l = 0; l < y.integer; l++)
            {
                result *= x.integer;
            }
            return lval_integer(result);
        }
        else if (strcmp(op, "min") == 0)
        {
            return lval_integer(x.integer < y.integer ? x.integer : y.integer);
        }
        else if (strcmp(op, "max") == 0)
        {
            return lval_integer(x.integer > y.integer ? x.integer : y.integer);
        }
    }
    else if (x.type == LVAL_DECIMAL && y.type == LVAL_DECIMAL)
    {
        if (strcmp(op, "+") == 0)
        {
            return lval_decimal(x.decimal + y.decimal);
        }
        else if (strcmp(op, "-") == 0)
        {
            return lval_decimal(x.decimal - y.decimal);
        }
        else if (strcmp(op, "*") == 0)
        {
            return lval_decimal(x.decimal * y.decimal);
        }
        else if (strcmp(op, "/") == 0)
        {
            if (y.decimal == 0)
            {
                return lval_err(LERR_DIV_ZERO);
            }
            else
            {
                return lval_decimal(x.decimal / y.decimal);
            }
        }
        else if (strcmp(op, "%") == 0)
        {
            if (y.decimal == 0)
            {
                return lval_err(LERR_DIV_ZERO);
            }
            else
            {
                return lval_err(LERR_BAD_OP); // TODO: Is there a proper implementation for this?
            }
        }
        else if (strcmp(op, "^") == 0)
        {
            return lval_decimal(pow(x.decimal, y.decimal));
        }
        else if (strcmp(op, "min") == 0)
        {
            return lval_decimal(x.decimal < y.decimal ? x.decimal : y.decimal);
        }
        else if (strcmp(op, "max") == 0)
        {
            return lval_decimal(x.decimal > y.decimal ? x.decimal : y.decimal);
        }
    }
    else if (x.type == LVAL_DECIMAL && y.type == LVAL_INTEGER)
    {
        return eval_op(x, op, lval_decimal((double) y.integer));
    }
    else if (x.type == LVAL_INTEGER && y.type == LVAL_DECIMAL)
    {
        return eval_op(lval_decimal((double)x.integer), op, y);
    }

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t)
{
    if (strstr(t->tag, "integer"))
    {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno == ERANGE ? lval_err(LERR_BAD_NUM) : lval_integer(x);
    }
    else if (strstr(t->tag, "decimal"))
    {
        errno = 0;
        double x = strtof(t->contents, NULL);
        return errno == ERANGE ? lval_err(LERR_BAD_NUM) : lval_decimal(x);
    }

    char* op = t->children[1]->contents;
    lval result = eval(t->children[2]);
    // unary plus / minus
    if (t->children_num == 4 && strcmp(op, "-") == 0)
    {
        lval zero = lval_integer(0);
        result = eval_op(zero, "-", result);
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

void lval_print(lval l)
{
    if (l.type == LVAL_INTEGER)
    {
#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 14);
#endif
        printf("%li", l.integer);
    }
    else if (l.type == LVAL_DECIMAL)
    {
#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 14);
#endif
        printf("%lf", l.decimal);
    }
    else if (l.type == LVAL_ERR)
    {
#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 12);
#endif
        switch (l.err)
        {
            case LERR_DIV_ZERO:
                printf("Error: division by zero");
                break;
            case LERR_BAD_OP:
                printf("Error: bad operator");
                break;
            case LERR_BAD_NUM:
                printf("Error: bad number");
                break;
            default:
                printf("Error: unknown error");
                exit(1);
                break;
        }
    }
    else
    {
#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 12);
#endif
        printf("Internal error!!!");
        exit(2);
    }
}

void lval_println(lval l)
{
    lval_print(l);
    putchar('\n');
}

int main(int argc, char** argv)
{
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    char* Grammar =
        "integer  : /-?[0-9]+/ ; "
        "decimal  : /-?[0-9]*\\.[0-9]+/ ; "
        "number   : <decimal> | <integer> ; "
        "operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\"; "
        "expr     : <number> | '(' <operator> <expr>+ ')' ; "
        "lispy    : /^/ <operator> <expr>+ /$/ ; ";


    mpca_lang(MPCA_LANG_DEFAULT, Grammar, Integer, Decimal, Number, Operator, Expr, Lispy);

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbInfo;
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutput, &csbInfo);
    SetConsoleTextAttribute(hOutput, 10);
#endif
    puts("Felispy 0.0.8 - by Felipo");
    puts("Type ctrl+C to exit.\n");

    while (1)
    {
        char* input = readline("Felispy> ");

#ifdef _WIN32
        SetConsoleTextAttribute(hOutput, 3);
#endif
        //printf("Input: %s\n", input);

        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispy, &result))
        {
            /*
            printf("Count: %d\n", count_all(result.output));
            printf("Nodes: %d\n", count_nodes(result.output));
            printf("Leaves: %d\n", count_leaves(result.output));
            printf("Max. Leaves: %d\n", max_leaves(result.output));
#ifdef _WIN32
            SetConsoleTextAttribute(hOutput, 11);
#endif
            mpc_ast_print(result.output);
            */
            lval_println(eval(result.output));

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

    mpc_cleanup(6, Integer, Decimal, Number, Operator, Expr, Lispy);
    return 0;
}