#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug_alloc.h"

#include "mpc.h"

typedef enum
{
    LVAL_ERR,
    LVAL_INTEGER,
    LVAL_DECIMAL,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR
} lval_type_t;

typedef struct lval
{
    lval_type_t type;
    long integer;
    double decimal;
    char *err;
    char *sym;
    int count;
    struct lval** cell;
} lval;

#ifdef _WIN32
#include <windows.h>

HANDLE hOutput;


void fore_color(int color)
{
    SetConsoleTextAttribute(hOutput, color);
}

char* readline(char* prompt)
{
    static char buffer[2048];

    fore_color(9);
    fputs(prompt, stdout);

    fore_color(7);
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

void fore_color(int color)
{
}

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

lval* lval_integer(long integer)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_INTEGER;
    v->integer = integer;
    return v;
}

lval* lval_decimal(double decimal)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_DECIMAL;
    v->decimal = decimal;
    return v;
}

lval* lval_symbol(char *symbol)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    char *s = malloc(strlen(symbol) + 1);
    strcpy(s, symbol);
    v->sym = s;
    return v;
}

lval* lval_err(char *err)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    char *e = malloc(strlen(err) + 1);
    strcpy(e, err);
    v->err = e;
    return v;
}

lval* lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr()
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval *v)
{
    switch (v->type)
    {
        case LVAL_DECIMAL:
        case LVAL_INTEGER:
            break;

        case LVAL_ERR:
            free(v->err);
            break;

        case LVAL_SYM:
            free(v->sym);
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++)
            {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

lval* eval_op(lval* x, char* op, lval* y)
{
    lval *result = NULL;
    if (x->type == LVAL_INTEGER && y->type == LVAL_INTEGER)
    {
        if (strcmp(op, "+") == 0)
        {
            result = lval_integer(x->integer + y->integer);
        }
        else if (strcmp(op, "-") == 0)
        {
            result = lval_integer(x->integer - y->integer);
        }
        else if (strcmp(op, "*") == 0)
        {
            result = lval_integer(x->integer * y->integer);
        }
        else if (strcmp(op, "/") == 0)
        {
            if (y->integer == 0)
            {
                result = lval_err("Divison by zero");
            }
            else
            {
                result = lval_integer(x->integer / y->integer);
            }
        }
        else if (strcmp(op, "%") == 0)
        {
            if (y->integer == 0)
            {
                result = lval_err("Division by zero");
            }
            else
            {
                result = lval_integer(x->integer % y->integer);
            }
        }
        else if (strcmp(op, "^") == 0)
        {
            long z = 1;
            for (long l = 0; l < y->integer; l++)
            {
                z *= x->integer;
            }
            result = lval_integer(z);
        }
        else if (strcmp(op, "min") == 0)
        {
            result = lval_integer(x->integer < y->integer ? x->integer : y->integer);
        }
        else if (strcmp(op, "max") == 0)
        {
            result = lval_integer(x->integer > y->integer ? x->integer : y->integer);
        }
    }
    else if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL)
    {
        if (strcmp(op, "+") == 0)
        {
            result = lval_decimal(x->decimal + y->decimal);
        }
        else if (strcmp(op, "-") == 0)
        {
            result = lval_decimal(x->decimal - y->decimal);
        }
        else if (strcmp(op, "*") == 0)
        {
            result = lval_decimal(x->decimal * y->decimal);
        }
        else if (strcmp(op, "/") == 0)
        {
            if (y->decimal == 0)
            {
                result = lval_err("Division by zero");
            }
            else
            {
                result = lval_decimal(x->decimal / y->decimal);
            }
        }
        else if (strcmp(op, "%") == 0)
        {
            if (y->decimal == 0)
            {
                result = lval_err("Division by zero");
            }
            else
            {
                result = lval_err("Not implemented"); // TODO: Is there a proper implementation for this?
            }
        }
        else if (strcmp(op, "^") == 0)
        {
            result = lval_decimal(pow(x->decimal, y->decimal));
        }
        else if (strcmp(op, "min") == 0)
        {
            result = lval_decimal(x->decimal < y->decimal ? x->decimal : y->decimal);
        }
        else if (strcmp(op, "max") == 0)
        {
            result = lval_decimal(x->decimal > y->decimal ? x->decimal : y->decimal);
        }
    }
    else if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER)
    {
        y->type = LVAL_DECIMAL;
        y->decimal = (double) y->integer;
        return eval_op(x, op, y);
    }
    else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL)
    {
        x->type = LVAL_DECIMAL;
        x->decimal = (double) x->integer;
        return eval_op(x, op, y);
    }

    lval_del(x);
    lval_del(y);
    if (result == NULL)
    {
        return lval_err("Unknown symbol");
    }
    return result;
}

lval* lval_read_integer(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno == ERANGE ? lval_err("Bad number") : lval_integer(x);
}

lval* lval_read_decimal(mpc_ast_t *t)
{
    errno = 0;
    double x = strtof(t->contents, NULL);
    return errno == ERANGE ? lval_err("Bad number") : lval_decimal(x);
}

lval* lval_add(lval* v, lval* new_cell)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = new_cell;
    return v;
}

lval* lval_read(mpc_ast_t* t)
{
    if (strstr(t->tag, "integer"))
    {
        return lval_read_integer(t);
    }
    else if (strstr(t->tag, "decimal"))
    {
        return lval_read_decimal(t);
    }
    else if (strstr(t->tag, "symbol"))
    {
        return lval_symbol(t->contents);
    }

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0)
    {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr"))
    {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "qexpr"))
    {
        x = lval_qexpr();
    }

    for (int i = 0; i < t->children_num - 1; i++)
    {
        if (strcmp(t->children[i]->contents, "(") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->contents, "{") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->contents, "}") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->tag, "regex") == 0)
        {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;

}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        lval_print(v->cell[i]);
        if (i != (v->count - 1))
        {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v)
{
    fore_color(14);
    if (v->type == LVAL_INTEGER)
    {
        printf("%li", v->integer);
    }
    else if (v->type == LVAL_DECIMAL)
    {
        printf("%lf", v->decimal);
    }
    else if (v->type == LVAL_SYM)
    {
        printf("%s", v->sym);
    }
    else if (v->type == LVAL_SEXPR)
    {
        lval_expr_print(v, '(', ')');
    }
    else if (v->type == LVAL_QEXPR)
    {
        lval_expr_print(v, '{', '}');
    }
    else if (v->type == LVAL_ERR)
    {
        fore_color(12);
        printf("Error: %s", v->err);
    }
    else
    {
        fore_color(12);
        printf("Internal error!!!");
        exit(2);
    }
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

lval* lval_pop(lval* v, int i)
{
    if (v->count == 0 || i >= v->count)
    {
        return lval_err("Popping from empty expression");
    }
    lval* x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i)
{
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}


lval *lval_join(lval *x, lval *y)
{
    while (y->count > 0)
    {
        x = lval_add(x, lval_pop(y, 0));
    }
    lval_del(y);
    return x;
}

lval* lval_eval(lval* v);

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

#define LASSERT_COUNT(a, num, err) LASSERT(a, a->count == num, err)
#define LASSERT_NOT_EMPTY(a, err) LASSERT(a, a->cell[0]->count > 0, err)

lval *builtin_head(lval *a)
{
    LASSERT_COUNT(a, 1, "Expected one argument for head.");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Argument for head is not a Q-expression.");
    LASSERT_NOT_EMPTY(a, "Empty Q-expression for head.");

    lval *v = lval_take(a, 0);
    while (v->count > 1)
    {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval *builtin_tail(lval *a)
{
    LASSERT_COUNT(a, 1, "Expected one argument for tail.");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Argument for tail is not a Q-expression.");
    LASSERT_NOT_EMPTY(a, "Empty Q-expression for tail.");

    lval *v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval *builtin_list(lval *a)
{
    a->type = LVAL_QEXPR;
    return a;
}

lval *builtin_eval(lval *a)
{
    LASSERT_COUNT(a, 1, "Expected one argument for eval.");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Argument for tail is not a Q-expression.");
    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval *builtin_join(lval *a)
{
    for (int i = 0; i < a->count; i++)
    {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Argument for join is not a Q-expression.");
    }

    lval* x = lval_pop(a, 0);
    while (a->count > 0)
    {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval* builtin_cons(lval* a)
{
    LASSERT_COUNT(a, 2, "Cons takes two arguments");
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR, "Second argument for cons should be a Q-expression.");

    lval* x = lval_pop(a, 0);
    lval* q = lval_qexpr();
    q = lval_add(q, x);
    lval* p = lval_take(a, 0);
    return lval_join(q, p);
}

lval* builtin_len(lval* a)
{
    LASSERT_COUNT(a, 1, "Len takes one argument");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Argument for len should be a Q-expression.");

    lval* x = lval_take(a, 0);
    lval* l = lval_integer(x->count);
    lval_del(x);
    return l;
}

lval* builtin_init(lval* a)
{
    LASSERT_COUNT(a, 1, "Init takes one argument");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Argument for init should be a Q-expression.");
    LASSERT_NOT_EMPTY(a, "Q-expression is empty");

    lval* x = lval_take(a, 0);
    lval* y = lval_pop(x, x->count - 1);
    lval_del(y);
    return x;
}

lval* builtin_op(lval* v, char* sym)
{
    lval* x = lval_pop(v, 0);

    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type != LVAL_INTEGER && v->cell[i]->type != LVAL_DECIMAL)
        {
            lval_del(x);
            lval_del(v);
            return lval_err("Invalid operand, expected integer or decimal");
        }
    }

    if (v->count == 0 && strcmp(sym, "-") == 0)
    {
        if (x->type == LVAL_INTEGER)
        {
            x->integer = -x->integer;
        }
        else if (x->type == LVAL_DECIMAL)
        {
            x->decimal = -x->decimal;
        }
    }

    while (v->count > 0)
    {
        lval* y = lval_pop(v, 0);
        x = eval_op(x, sym, y);
    }

    lval_del(v);
    return x;
}

lval *builtin(lval *a, char *func)
{
    lval *result = NULL;
    if (strcmp(func, "list") == 0)
    {
        result = builtin_list(a);
    }
    else if (strcmp(func, "head") == 0)
    {
        result = builtin_head(a);
    }
    else if (strcmp(func, "tail") == 0)
    {
        result = builtin_tail(a);
    }
    else if (strcmp(func, "eval") == 0)
    {
        result = builtin_eval(a);
    }
    else if (strcmp(func, "join") == 0)
    {
        result = builtin_join(a);
    }
    else if (strcmp(func, "cons") == 0)
    {
        result = builtin_cons(a);
    }
    else if (strcmp(func, "len") == 0)
    {
        result = builtin_len(a);
    }
    else if (strcmp(func, "init") == 0)
    {
        result = builtin_init(a);
    }
    else if (strstr("+-*/%^ min max", func) >= 0)
    {
        result = builtin_op(a, func);
    }
    else
    {
        lval_del(a);
        result = lval_err("Unknown function");
    }
    return result;
}

lval* lval_eval_sexpr(lval* v)
{
    if (v->count == 0)
    {
        return v;
    }

    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type == LVAL_ERR)
        {
            lval* u = lval_take(v, i);
            return u;
        }
    }

    if (v->count == 1)
    {
        lval* u = lval_take(v, 0);
        return u;
    }

    lval* op = lval_pop(v, 0);

    if (op->type != LVAL_SYM)
    {
        lval_del(op);
        lval_del(v);
        return lval_err("Symbol expected on s-expression");
    }

    lval* result = builtin(v, op->sym);
    lval_del(op);
    return result;
}

lval* lval_eval(lval* v)
{
    if (v->type == LVAL_SEXPR)
    {
        return lval_eval_sexpr(v);
    }
    else
    {
        return v;
    }
}

int main(int argc, char** argv)
{
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    char* Grammar =
        "integer  : /-?[0-9]+/ ; "
        "decimal  : /-?[0-9]*\\.[0-9]+/ ; "
        "number   : <decimal> | <integer> ; "
        "symbol   : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" "
        "         | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" "
        "         | \"cons\" | \"len\" | \"init\" ; "
        "sexpr    : '(' <expr>* ')' ; "
        "qexpr    : '{' <expr>* '}' ; "
        "expr     : <number> | <symbol> | <sexpr> | <qexpr> ; "
        "lispy    : /^/ <expr>* /$/ ; ";


    mpca_lang(MPCA_LANG_DEFAULT, Grammar, Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbInfo;
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutput, &csbInfo);
#endif

    fore_color(10);
    puts("Felispy 0.1.0 - by Felipo");
    puts("Type ctrl+C to exit.\n");

    while (1)
    {
        char* input = readline("Felispy> ");

        fore_color(3);
        //printf("Input: %s\n", input);

        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispy, &result))
        {
            /*
            printf("Count: %d\n", count_all(result.output));
            printf("Nodes: %d\n", count_nodes(result.output));
            printf("Leaves: %d\n", count_leaves(result.output));
            printf("Max. Leaves: %d\n", max_leaves(result.output));
            fore_color(11);
            mpc_ast_print(result.output);
            */

            //lval* v = eval(result.output);
            lval* v = lval_read(result.output);
            //lval_println(v);
            v = lval_eval(v);
            lval_println(v);
            lval_del(v);

            mpc_ast_delete(result.output);
        }
        else
        {
            fore_color(13);
            mpc_err_print(result.error);
            mpc_err_delete(result.error);
        }

        free(input);

        debug_check();
    }

#ifdef _WIN32
    SetConsoleTextAttribute(hOutput, csbInfo.wAttributes);
#endif

    mpc_cleanup(7, Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
