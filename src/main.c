#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpc.h"
#include "debug_alloc.h"

typedef enum
{
    LVAL_ERR,
    LVAL_INTEGER,
    LVAL_DECIMAL,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_FUN
} lval_type_t;

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

#define LBUILTIN_DECL(name) lval* (name)(lenv* e, lval* a)
typedef LBUILTIN_DECL(*lbuiltin);

struct lval
{
    lval_type_t type;
    long integer;
    double decimal;
    char *err;
    char *sym;
    lbuiltin fun;
    int count;
    lval** cell;
};

struct lenv
{
    int count;
    char** syms;
    lval** vals;
};

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

char* ltype_name(lval_type_t type)
{
    switch (type)
    {
        case LVAL_ERR: return "Error";
        case LVAL_INTEGER: return "Integer";
        case LVAL_DECIMAL: return "Decimal";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        case LVAL_FUN: return "Function";
        default: return "Unknown";
    }
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

lval* lval_err(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    char tmp[512];
    vsnprintf(tmp, 511, fmt, args);
    v->err = malloc(strlen(tmp) + 1);
    strcpy(v->err, tmp);
    va_end(args);
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

lval* lval_fun(lbuiltin func)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

void lval_del(lval *v)
{
    switch (v->type)
    {
        case LVAL_DECIMAL:
        case LVAL_INTEGER:
        case LVAL_FUN:
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
                result = lval_err("% is not implemented for decimals"); // TODO: Is there a proper implementation for this?
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
        return lval_err("Operator not implemented: %s", op);
    }
    return result;
}

lval* lval_read_integer(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno == ERANGE ? lval_err("Bad number: %s", t->contents) : lval_integer(x);
}

lval* lval_read_decimal(mpc_ast_t *t)
{
    errno = 0;
    double x = strtof(t->contents, NULL);
    return errno == ERANGE ? lval_err("Bad number: %s", t->contents) : lval_decimal(x);
}

lval* lval_copy(lval* v)
{
    lval* x = malloc(sizeof(lval));

    x->type = v->type;

    switch (v->type)
    {
        case LVAL_INTEGER:
            x->integer = v->integer;
            break;
        case LVAL_DECIMAL:
            x->decimal = v->decimal;
            break;
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval) * v->count);
            for (int i = 0; i < v->count; i++)
            {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
        default:
            free(x);
            x = lval_err("Copy not implemented for %s", ltype_name(v->type));
            break;
    }
    return x;
}

lval* lval_add(lval* v, lval* new_cell)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = new_cell;
    return v;
}

lenv* lenv_new(void)
{
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e)
{
    for (int i = 0; i < e->count; i++)
    {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k)
{
    lval* result = NULL;
    for (int i = 0; i < e->count; i++)
    {
        if (strcmp(e->syms[i], k->sym) == 0)
        {
            result = lval_copy(e->vals[i]);
            break;
        }
    }
    if (result == NULL)
    {
        result = lval_err("Unbound symbol '%s'", k->sym);
    }
    return result;
}

char* lenv_get_name(lenv* e, lval* v)
{
    for (int i = 0; i < e->count; i++)
    {
        if (v->type == LVAL_FUN && e->vals[i]->type == LVAL_FUN && v->fun == e->vals[i]->fun)
        {
            return e->syms[i];
        }
    }
    return "(Unknown)";
}

void lenv_put(lenv* e, lval* k, lval* v)
{
    for (int i = 0; i < e->count; i++)
    {
        if (strcmp(e->syms[i], k->sym) == 0)
        {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    e->count++;
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    e->vals = realloc(e->vals, sizeof(lval) * e->count);
    int last = e->count - 1;
    e->vals[last] = lval_copy(v);
    e->syms[last] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[last], k->sym);
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

void lval_print(lenv* e, lval* v);

void lval_expr_print(lenv* e, lval* v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        lval_print(e, v->cell[i]);
        if (i != (v->count - 1))
        {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lenv* e, lval* v)
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
        lval_expr_print(e, v, '(', ')');
    }
    else if (v->type == LVAL_QEXPR)
    {
        lval_expr_print(e, v, '{', '}');
    }
    else if (v->type == LVAL_FUN)
    {
        char* s = lenv_get_name(e, v);
        printf("<function: %s>", s);
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

void lval_println(lenv* e, lval* v)
{
    lval_print(e, v);
    putchar('\n');
}

void lenv_list(lenv* e)
{
    for (int i = 0; i < e->count; i++)
    {
        fore_color(13);
        printf("%s = ", e->syms[i]);
        lval_println(e, e->vals[i]);
    }
}

lval* lval_pop(lval* v, int i)
{
    if (v->count == 0)
    {
        return lval_err("Popping a value from empty expression");
    }
    if (i < 0 || i >= v->count)
    {
        return lval_err("Trying to access out of bounds at %i (count %i)", i, v->count);
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

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_ARG_COUNT(a, c1, n1) \
    LASSERT(a, a->count == c1, "Function '%s' takes %i arguments but %i was given.", n1, c1, a->count)

#define LASSERT_ARG_MIN(a, cmin, n1) \
    LASSERT(a, a->count >= cmin, "Function '%s' takes at least %i arguments but %i was given.", n1, cmin, a->count)

#define LASSERT_ARG_TYPE(a, pos, t1, n1) \
    LASSERT(a, a->cell[pos]->type == t1, "Function '%s' expected %s at position %i but got %s.", n1, ltype_name(t1), pos, ltype_name(a->cell[pos]->type))

#define LASSERT_ARG_NOT_EMPTY(a, pos, t1, n1) \
    LASSERT(a, a->cell[pos]->count != 0, "Function '%s' expected a non-empty %s at position %i.", n1, ltype_name(t1), pos)

LBUILTIN_DECL(builtin_def)
{
    LASSERT_ARG_MIN(a, 2, "def");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "def");
    //LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Expected %s at argument 0 got %s.", ltype_name(LVAL_QEXPR), ltype_name(a->cell[0]->type));
    lval *k = lval_pop(a, 0);
    if (k->count != a->count)
    {
        lval* err = lval_err("Trying to define %i values for %i symbols.", a->count, k->count);
        lval_del(k);
        lval_del(a);
        return err;
    }
    for (int i = 0; i < k->count; i++)
    {
        if (k->cell[i]->type != LVAL_SYM)
        {
            lval* err = lval_err("Function 'define' expected %s at position %i got %s.", ltype_name(LVAL_SYM), i, ltype_name(k->cell[i]->type));
            lval_del(k);
            lval_del(a);
            return err;
        }
    }
    for (int i = 0; i < k->count; i++)
    {
        lenv_put(e, k->cell[i], a->cell[i]);
    }
    lval_del(k);
    lval_del(a);
    return lval_sexpr(); // Empty ()
}

LBUILTIN_DECL(builtin_head)
{
    LASSERT_ARG_COUNT(a, 1, "head");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "head");
    LASSERT_ARG_NOT_EMPTY(a, 0, LVAL_QEXPR, "head");

    lval *v = lval_take(a, 0);
    while (v->count > 1)
    {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

LBUILTIN_DECL(builtin_tail)
{
    LASSERT_ARG_COUNT(a, 1, "tail");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "tail");
    LASSERT_ARG_NOT_EMPTY(a, 0, LVAL_QEXPR, "tail");

    lval *v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

LBUILTIN_DECL(builtin_list)
{
    a->type = LVAL_QEXPR;
    return a;
}

LBUILTIN_DECL(builtin_get_env)
{
    lval* q = lval_qexpr();
    for (int i = 0; i < e->count; i++)
    {
        q = lval_add(q, lval_symbol(e->syms[i]));
    }
    return q;
}

lval* lval_eval(lval* e, lval* v);

LBUILTIN_DECL(builtin_eval)
{
    LASSERT_ARG_COUNT(a, 1, "eval");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "eval");
    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

LBUILTIN_DECL(builtin_join)
{
    for (int i = 0; i < a->count; i++)
    {
        LASSERT_ARG_TYPE(a, i, LVAL_QEXPR, "join");
    }

    lval* x = lval_pop(a, 0);
    while (a->count > 0)
    {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

LBUILTIN_DECL(builtin_cons)
{
    LASSERT_ARG_COUNT(a, 2, "cons");
    LASSERT_ARG_TYPE(a, 1, LVAL_QEXPR, "cons");

    lval* x = lval_pop(a, 0);
    lval* q = lval_qexpr();
    q = lval_add(q, x);
    lval* p = lval_take(a, 0);
    return lval_join(q, p);
}

LBUILTIN_DECL(builtin_len)
{
    LASSERT_ARG_COUNT(a, 1, "len");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "len");

    lval* x = lval_take(a, 0);
    lval* l = lval_integer(x->count);
    lval_del(x);
    return l;
}

LBUILTIN_DECL(builtin_init)
{
    LASSERT_ARG_COUNT(a, 1, "init");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "init");
    LASSERT_ARG_NOT_EMPTY(a, 0, LVAL_QEXPR, "init");

    lval* x = lval_take(a, 0);
    lval* y = lval_pop(x, x->count - 1);
    lval_del(y);
    return x;
}

lval* builtin_op(lval* v, char* sym)
{
    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type != LVAL_INTEGER && v->cell[i]->type != LVAL_DECIMAL)
        {
            lval *err = lval_err("Function '%s' got invalid operand type at position %i: %s", sym, i, ltype_name(v->cell[i]->type));
            lval_del(v);
            return err;
        }
    }

    lval* x = lval_pop(v, 0);
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

void lenv_add_builtin(lenv *e, char* name, lbuiltin func)
{
    lval* sym = lval_symbol(name);
    lval* fun = lval_fun(func);
    lenv_put(e, sym, fun);
    lval_del(sym);
    lval_del(fun);
}


#define LBUILTIN_DECL_OP(builtin_name, op) LBUILTIN_DECL(builtin_name) { return builtin_op(a, op); }

LBUILTIN_DECL_OP(builtin_add, "+");
LBUILTIN_DECL_OP(builtin_sub, "-");
LBUILTIN_DECL_OP(builtin_mul, "*");
LBUILTIN_DECL_OP(builtin_div, "/");
LBUILTIN_DECL_OP(builtin_mod, "%");
LBUILTIN_DECL_OP(builtin_pow, "^");
LBUILTIN_DECL_OP(builtin_min, "min");
LBUILTIN_DECL_OP(builtin_max, "max");

void lenv_add_builtins(lenv* e)
{
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "get_env", builtin_get_env);
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "init", builtin_init);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);
    lenv_add_builtin(e, "min", builtin_min);
    lenv_add_builtin(e, "max", builtin_max);
}

lval* lval_eval_sexpr(lval* e, lval* v)
{
    if (v->count == 0)
    {
        return v;
    }

    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type == LVAL_ERR)
        {
            return lval_take(v, i);
        }
    }

    if (v->count == 1)
    {
        lval* u = lval_take(v, 0);
        return u;
    }

    lval* f = lval_pop(v, 0);

    if (f->type != LVAL_FUN)
    {
        lval* err = lval_err("Expected %s, got %s", ltype_name(LVAL_FUN), ltype_name(f->type));
        lval_del(f);
        lval_del(v);
        return err;
    }

    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* e, lval* v)
{
    if (v->type == LVAL_SEXPR)
    {
        return lval_eval_sexpr(e, v);
    }
    else if (v->type == LVAL_SYM)
    {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
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
        "symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\%\\^=<>!&]+/ ; "
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

    debug_check("lenv_new");

    lval* e = lenv_new();
    lenv_add_builtins(e);

    debug_check("Builtins loaded");

    while (1)
    {
        char* input = readline("Felispy> ");
        add_history(input);

        if (strcmp(input, "exit") == 0)
        {
            free(input);
            break;
        }

        if (strcmp(input, "list") == 0)
        {
            fore_color(12);
            lenv_list(e);
            continue;
        }

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
            v = lval_eval(e, v);
            lval_println(e, v);
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

        debug_check("REPL");
    }

    lenv_del(e);
    debug_check("lenv_del");

#ifdef _WIN32
    SetConsoleTextAttribute(hOutput, csbInfo.wAttributes);
#endif

    mpc_cleanup(8, Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
