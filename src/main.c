#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpc.h"

//#include "debug_alloc.h"

mpc_parser_t* Comment;
mpc_parser_t* String;
mpc_parser_t* Boolean;
mpc_parser_t* Integer;
mpc_parser_t* Decimal;
mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;

typedef enum
{
    LVAL_ERR,
    LVAL_BOOLEAN,
    LVAL_INTEGER,
    LVAL_DECIMAL,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_FUN,
    LVAL_STR,
    LVAL_OK
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
    char *str;

    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    int count;
    lval** cell;
};

struct lenv
{
    lenv* par;
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
        case LVAL_BOOLEAN: return "Boolean";
        case LVAL_INTEGER: return "Integer";
        case LVAL_DECIMAL: return "Decimal";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        case LVAL_FUN: return "Function";
        case LVAL_STR: return "String";
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

lval* lval_boolean(long boolean)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_BOOLEAN;
    v->integer = boolean;
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

lval* lval_string(char *str)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_STR;
    char *s = malloc(strlen(str) + 1);
    strcpy(s, str);
    v->str = s;
    return v;
}

lval* lval_ok()
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_OK;
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

lval* lval_builtin(lbuiltin func)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    v->env = NULL;
    v->formals = NULL;
    v->body = NULL;
    return v;
}

lenv* lenv_new();

lval* lval_lambda(lval* formals, lval* body)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = NULL;
    v->env = lenv_new();
    v->formals = formals;
    v->body = body;
    return v;
}

void lenv_del(lenv *);

void lval_del(lval *v)
{
    switch (v->type)
    {
        case LVAL_DECIMAL:
        case LVAL_INTEGER:
        case LVAL_BOOLEAN:
        case LVAL_OK:
            break;

        case LVAL_FUN:
            if (v->builtin == NULL)
            {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;

        case LVAL_ERR:
            free(v->err);
            break;

        case LVAL_SYM:
            free(v->sym);
            break;

        case LVAL_STR:
            free(v->str);
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
        else if (strcmp(op, "&&") == 0)
        {
            result = lval_integer(x->integer && y->integer);
        }
        else if (strcmp(op, "||") == 0)
        {
            result = lval_integer(x->integer || y->integer);
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
                result = lval_decimal(fmod(x->decimal, y->decimal));
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
        else if (strcmp(op, "&&") == 0)
        {
            result = lval_integer(x->decimal && y->decimal);
        }
        else if (strcmp(op, "||") == 0)
        {
            result = lval_integer(x->decimal || y->decimal);
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

lval* lval_read_string(mpc_ast_t *t)
{
    size_t len = strlen(t->contents);
    t->contents[len - 1] = '\0';
    char* str = malloc(len);
    strcpy(str, t->contents + 1);
    char* unescaped = mpcf_unescape(str);
    //debug_update_ptr(str, unescaped);
    lval* x = lval_string(unescaped);
    free(unescaped);
    return x;
}

lenv* lenv_copy(lenv* e);

lval* lval_copy(lval* v)
{
    lval* x = malloc(sizeof(lval));

    x->type = v->type;

    switch (v->type)
    {
        case LVAL_BOOLEAN:
        case LVAL_INTEGER:
            x->integer = v->integer;
            break;
        case LVAL_DECIMAL:
            x->decimal = v->decimal;
            break;
        case LVAL_FUN:
            if (v->builtin == NULL)
            {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            else
            {
                x->builtin = v->builtin;
                x->env = NULL;
                x->formals = NULL;
                x->body = NULL;
            }
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_STR:
            x->str = malloc(strlen(v->str) + 1);
            strcpy(x->str, v->str);
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
    e->par = NULL;
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
    if (result == NULL && e->par != NULL)
    {
        result = lenv_get(e->par, k);
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
        if (v->type == LVAL_FUN && e->vals[i]->type == LVAL_FUN && v->builtin == e->vals[i]->builtin)
        {
            return e->syms[i];
        }
    }
    if (e->par != NULL)
    {
        return lenv_get_name(e->par, v);
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

lenv* lenv_copy(lenv* e)
{
    lenv* n = lenv_new();
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++)
    {
        n->vals[i] = lval_copy(e->vals[i]);
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
    }
    return n;
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
    else if (strstr(t->tag, "string"))
    {
        return lval_read_string(t);
    }

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0)
    {
        x = lval_sexpr();
    }
    else if (strstr(t->tag, "sexpr"))
    {
        x = lval_sexpr();
    }
    else if (strstr(t->tag, "qexpr"))
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
        if (strstr(t->children[i]->tag, "comment"))
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

void lval_str_print(lval* v)
{
    char* str = malloc(strlen(v->str) + 1);
    strcpy(str, v->str);
    char* escaped = mpcf_escape(str);
    //debug_update_ptr(str, escaped);
    printf("\"%s\"", escaped);
    free(escaped);
}

void lval_print(lenv* e, lval* v)
{
    switch (v->type)
    {
        case LVAL_OK:
            printf("ok");
            break;
        case LVAL_BOOLEAN:
            printf("%s", v->integer ? "true" : "false");
            break;
        case LVAL_INTEGER:
            printf("%li", v->integer);
            break;
        case LVAL_DECIMAL:
            printf("%lf", v->decimal);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_STR:
            lval_str_print(v);
            break;
        case LVAL_SEXPR:
            lval_expr_print(e, v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(e, v, '{', '}');
            break;
        case LVAL_FUN:
            if (v->builtin == NULL)
            {
                printf("(\\ ");
                lval_print(e, v->formals);
                putchar(' ');
                lval_print(e, v->body);
                putchar(')');
            }
            else
            {
                char* s = lenv_get_name(e, v);
                printf("<builtin: %s>", s);
            }
            break;
        case LVAL_ERR:
            fore_color(12);
            printf("Error: %s", v->err);
            break;
        default:
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

int lval_eq(lval* x, lval* y)
{
    if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER)
    {
        y->type = LVAL_DECIMAL;
        y->decimal = (double) y->integer;
    }
    else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL)
    {
        x->type = LVAL_DECIMAL;
        x->decimal = (double) x->integer;
    }

    int result;
    if (x->type != y->type)
    {
        result = 0;
    }
    else
    {
        switch (x->type)
        {
            case LVAL_DECIMAL:
                result = x->decimal == y->decimal;
                break;
            case LVAL_BOOLEAN:
            case LVAL_INTEGER:
                result = x->integer == y->integer;
                break;
            case LVAL_ERR:
                result = strcmp(x->err, y->err) == 0;
                break;
            case LVAL_SYM:
                result = strcmp(x->sym, y->sym) == 0;
                break;
            case LVAL_STR:
                result = strcmp(x->str, y->str) == 0;
                break;
            case LVAL_SEXPR:
            case LVAL_QEXPR:
                if (x->count != y->count)
                {
                    result = 0;
                }
                else
                {
                    result = 1;
                    for (int i = 0; i < x->count; i++)
                    {
                        if (!lval_eq(x->cell[i], y->cell[i]))
                        {
                            result = 0;
                            break;
                        }
                    }
                }
                break;
            case LVAL_FUN:
                if (x->builtin != NULL || y->builtin != NULL)
                {
                    result = x->builtin == y->builtin;
                }
                else
                {
                    result = lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);
                }
                break;
            default:
                result = 0;
                break;
        }
    }
    return result;
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

#define LASSERT_ARG_TYPE2(a, pos, t1, t2, n1) \
    LASSERT(a, a->cell[pos]->type == t1 || a->cell[pos]->type == t2, "Function '%s' expected %s or %s at position %i but got %s.", n1, ltype_name(t1), ltype_name(t2), pos, ltype_name(a->cell[pos]->type))

#define LASSERT_ARG_NOT_EMPTY(a, pos, t1, n1) \
    LASSERT(a, a->cell[pos]->count != 0, "Function '%s' expected a non-empty %s at position %i.", n1, ltype_name(t1), pos)

lval* builtin_var(lenv* e, lval* a, char* fun)
{
    LASSERT_ARG_MIN(a, 2, fun);
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, fun);
    lval *k = lval_pop(a, 0);
    if (k->count != a->count)
    {
        lval* err = lval_err("Function '%' got %i symbols but %i values.", k->count, a->count);
        lval_del(k);
        lval_del(a);
        return err;
    }
    for (int i = 0; i < k->count; i++)
    {
        if (k->cell[i]->type != LVAL_SYM)
        {
            lval* err = lval_err("Function '%s' expected %s at position %i got %s.", fun, ltype_name(LVAL_SYM), i, ltype_name(k->cell[i]->type));
            lval_del(k);
            lval_del(a);
            return err;
        }
    }

    // Global definition
    if (strcmp(fun, "def") == 0)
    {
        while (e->par != NULL)
        {
            e = e->par;
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

LBUILTIN_DECL(builtin_def)
{
    return builtin_var(e, a, "def");
}

LBUILTIN_DECL(builtin_put)
{
    return builtin_var(e, a, "=");
}

LBUILTIN_DECL(builtin_lambda)
{
    LASSERT_ARG_COUNT(a, 2, "\\");
    LASSERT_ARG_TYPE(a, 0, LVAL_QEXPR, "\\");
    LASSERT_ARG_TYPE(a, 1, LVAL_QEXPR, "\\");

    for (int i = 0; i < a->cell[0]->count; i++)
    {
        LASSERT(a, a->cell[0]->cell[i]->type == LVAL_SYM,
                "Cannot define non-symbol at position %d. Expected %s got %s.",
                i, ltype_name(LVAL_SYM), ltype_name(a->cell[0]->cell[i]->type));
    }

    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval* v = lval_lambda(formals, body);
    lval_del(a);
    return v;
}

LBUILTIN_DECL(builtin_head)
{
    LASSERT_ARG_COUNT(a, 1, "head");
    LASSERT_ARG_TYPE2(a, 0, LVAL_QEXPR, LVAL_STR, "head");

    if (a->cell[0]->type == LVAL_QEXPR)
    {
        LASSERT_ARG_NOT_EMPTY(a, 0, LVAL_QEXPR, "head");

        lval* v = lval_take(a, 0);
        while (v->count > 1)
        {
            lval_del(lval_pop(v, 1));
        }
        return v;
    }
    else
    {
        LASSERT(a, a->cell[0]->str[0] != '\0', "Function 'head' was given an empty string.");

        a->cell[0]->str[1] = '\0';
        lval* v = lval_string(a->cell[0]->str);
        return v;
    }    
}

LBUILTIN_DECL(builtin_tail)
{
    LASSERT_ARG_COUNT(a, 1, "tail");
    LASSERT_ARG_TYPE2(a, 0, LVAL_QEXPR, LVAL_STR, "tail");

    if (a->cell[0]->type == LVAL_QEXPR)
    {
        LASSERT_ARG_NOT_EMPTY(a, 0, LVAL_QEXPR, "tail");

        lval *v = lval_take(a, 0);
        lval_del(lval_pop(v, 0));
        return v;
    }
    else
    {
        LASSERT(a, a->cell[0]->str[0] != '\0', "Function 'tail' was given an empty string.");

        lval* v = lval_string(a->cell[0]->str + 1);
        lval_del(a);
        return v;
    }
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

lval* lval_eval(lenv* e, lval* v);

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
    LASSERT_ARG_MIN(a, 1, "join");
    LASSERT_ARG_TYPE2(a, 0, LVAL_STR, LVAL_QEXPR, "join");
    lval_type_t base_type = a->cell[0]->type;
    for (int i = 0; i < a->count; i++)
    {
        LASSERT_ARG_TYPE(a, i, base_type, "join");
    }

    if (base_type == LVAL_QEXPR)
    {
        lval* x = lval_pop(a, 0);
        while (a->count > 0)
        {
            x = lval_join(x, lval_pop(a, 0));
        }
        lval_del(a);
        return x;
    }
    else
    {
        int total_size = 0;
        for (int i = 0; i < a->count; i++)
        {
            total_size += strlen(a->cell[i]->str);
        }
        char* str = malloc(total_size + 1);
        str[0] = '\0';
        for (int i = 0; i < a->count; i++)
        {
            strcat(str, a->cell[i]->str);
        }
        lval_del(a);
        lval* x = lval_string(str);
        free(str);
        return x;
    }
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

LBUILTIN_DECL(builtin_if)
{
    LASSERT_ARG_COUNT(a, 3, "if");
    LASSERT(a, a->cell[0]->type == LVAL_BOOLEAN || a->cell[0]->type == LVAL_INTEGER || a->cell[0]->type == LVAL_DECIMAL,
            "Function 'if' got invalid type %s at position %d.", ltype_name(a->cell[0]->type), 0);
    LASSERT_ARG_TYPE(a, 1, LVAL_QEXPR, "if");
    LASSERT_ARG_TYPE(a, 2, LVAL_QEXPR, "if");

    lval* cond = a->cell[0];

    lval* result;
    if (((cond->type == LVAL_DECIMAL) && (cond->decimal != 0.0)) || ((cond->type != LVAL_DECIMAL) && cond->integer))
    {
        lval* true_val = lval_pop(a, 1);
        true_val->type = LVAL_SEXPR;
        result = lval_eval(e, true_val);
    }
    else
    {
        lval* false_val = lval_pop(a, 2);
        false_val->type = LVAL_SEXPR;
        result = lval_eval(e, false_val);
    }

    lval_del(a);
    return result;
}

LBUILTIN_DECL(builtin_load)
{
    LASSERT_ARG_COUNT(a, 1, "load");
    LASSERT_ARG_TYPE(a, 0, LVAL_STR, "load");

    char* filename = a->cell[0]->str;

    mpc_result_t r;
    if (mpc_parse_contents(filename, Lispy, &r))
    {
        lval* prog = lval_read(r.output);
        mpc_ast_delete(r.output);

        while (prog->count)
        {
            lval* expr = lval_pop(prog, 0);
            lval* x = lval_eval(e, expr);
            if (x->type == LVAL_ERR)
            {
                lval_println(e, x);
            }
            lval_del(x);
        }

        lval_del(prog);
        lval_del(a);
        return lval_ok();
    }
    else
    {
        char* err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);
        lval* err = lval_err("Could not load library: %s ", err_msg);
        free(err_msg);
        lval_del(a);
        return err;
    }   
    
}


LBUILTIN_DECL(builtin_read)
{
    LASSERT_ARG_COUNT(a, 1, "load");
    LASSERT_ARG_TYPE(a, 0, LVAL_STR, "load");

    char* expr = a->cell[0]->str;

    mpc_result_t r;
    if (mpc_parse("-", expr, Lispy, &r))
    {
        lval* prog = lval_read(r.output);
        mpc_ast_delete(r.output);
        prog->type = LVAL_QEXPR;
        return prog;
    }
    else
    {
        char* err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);
        lval* err = lval_err("Could not read expression: %s ", err_msg);
        free(err_msg);
        lval_del(a);
        return err;
    }
}

LBUILTIN_DECL(builtin_print)
{
    for (int i = 0; i < a->count; i++)
    {
        if (i > 0)
        {
            putchar(' ');
        }
        lval_print(e, a->cell[i]);
    }
    putchar('\n');

    lval_del(a);
    return lval_ok();
}

LBUILTIN_DECL(builtin_show)
{
    for (int i = 0; i < a->count; i++)
    {
        if (i > 0)
        {
            putchar(' ');
        }
        LASSERT_ARG_TYPE(a, i, LVAL_STR, "show");
        printf("%s", a->cell[i]->str);
    }
    putchar('\n');

    lval_del(a);
    return lval_ok();
}

LBUILTIN_DECL(builtin_error)
{
    LASSERT_ARG_COUNT(a, 1, "error");
    LASSERT_ARG_TYPE(a, 0, LVAL_STR, "error");
    lval* err = lval_err(a->cell[0]->str);
    lval_del(a);
    return err;
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
    if (v->count == 0)
    {
        if (strcmp(sym, "-") == 0)
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
    }

    if (strcmp(sym, "not") == 0 && v->count > 0)
    {
        lval *err = lval_err("Function 'not' got too many arguments.");
        lval_del(x);
        lval_del(v);
        return err;
    }

    while (v->count > 0)
    {
        lval* y = lval_pop(v, 0);
        x = eval_op(x, sym, y);
    }

    lval_del(v);
    return x;
}

lval* builtin_ord(lval* a, char* op)
{
    LASSERT_ARG_COUNT(a, 2, op);
    LASSERT_ARG_TYPE2(a, 0, LVAL_DECIMAL, LVAL_INTEGER, op);
    LASSERT_ARG_TYPE2(a, 1, LVAL_DECIMAL, LVAL_INTEGER, op);

    lval* result = NULL;
    lval* x = a->cell[0];
    lval* y = a->cell[1];

    if (x->type == LVAL_DECIMAL && y->type == LVAL_INTEGER)
    {
        y->type = LVAL_DECIMAL;
        y->decimal = (double) y->integer;
    }
    else if (x->type == LVAL_INTEGER && y->type == LVAL_DECIMAL)
    {
        x->type = LVAL_DECIMAL;
        x->decimal = (double) x->integer;
    }

    if (x->type == LVAL_INTEGER && y->type == LVAL_INTEGER)
    {
        if (strcmp(op, "<") == 0)
        {
            result = lval_boolean(x->integer < y->integer);
        }
        else if (strcmp(op, "<=") == 0)
        {
            result = lval_boolean(x->integer <= y->integer);
        }
        else if (strcmp(op, ">") == 0)
        {
            result = lval_boolean(x->integer > y->integer);
        }
        else if (strcmp(op, ">=") == 0)
        {
            result = lval_boolean(x->integer >= y->integer);
        }
    }
    else if (x->type == LVAL_DECIMAL && y->type == LVAL_DECIMAL)
    {
        if (strcmp(op, "<") == 0)
        {
            result = lval_boolean(x->decimal < y->decimal);
        }
        else if (strcmp(op, "<=") == 0)
        {
            result = lval_boolean(x->decimal <= y->decimal);
        }
        else if (strcmp(op, ">") == 0)
        {
            result = lval_boolean(x->decimal > y->decimal);
        }
        else if (strcmp(op, ">=") == 0)
        {
            result = lval_boolean(x->decimal >= y->decimal);
        }
    }

    if (result == NULL)
    {
        result = lval_err("Operator not implemented: %s", op);
    }

    lval_del(a);
    return result;
}

void lenv_add_builtin(lenv *e, char* name, lbuiltin func)
{
    lval* sym = lval_symbol(name);
    lval* fun = lval_builtin(func);
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
//LBUILTIN_DECL_OP(builtin_and, "&&");
//LBUILTIN_DECL_OP(builtin_or, "||");

#define LBUILTIN_DECL_ORD(builtin_name, op) LBUILTIN_DECL(builtin_name) { return builtin_ord(a, op); }
LBUILTIN_DECL_ORD(builtin_lt, "<");
LBUILTIN_DECL_ORD(builtin_le, "<=");
LBUILTIN_DECL_ORD(builtin_gt, ">");
LBUILTIN_DECL_ORD(builtin_ge, ">=");

LBUILTIN_DECL(builtin_and)
{
    LASSERT_ARG_MIN(a, 2, "&&");
    for (int i = 0; i < a->count; i++)
    {
        LASSERT(a, a->cell[i]->type == LVAL_BOOLEAN || a->cell[i]->type == LVAL_INTEGER || a->cell[i]->type == LVAL_DECIMAL,
                "Function '&&' got argument of invalid type %s at position %d.", ltype_name(a->cell[i]->type), i);
    }
    int result = 1;
    while (a->count > 0)
    {
        lval* x = lval_pop(a, 0);
        if (((x->type == LVAL_BOOLEAN || x->type == LVAL_INTEGER) && (x->integer == 0)) ||
            ((x->type == LVAL_DECIMAL) && (x->decimal == 0.0)))
        {
            lval_del(x);
            result = 0;
            break;
        }
        lval_del(x);
    }
    lval_del(a);
    return lval_boolean(result);
}

LBUILTIN_DECL(builtin_or)
{
    LASSERT_ARG_MIN(a, 2, "||");
    for (int i = 0; i < a->count; i++)
    {
        LASSERT(a, a->cell[i]->type == LVAL_BOOLEAN || a->cell[i]->type == LVAL_INTEGER || a->cell[i]->type == LVAL_DECIMAL,
                "Function '||' got argument of invalid type %s at position %d.", ltype_name(a->cell[i]->type), i);
    }
    int result = 0;
    while (a->count > 0)
    {
        lval* x = lval_pop(a, 0);
        if (((x->type == LVAL_BOOLEAN || x->type == LVAL_INTEGER) && (x->integer != 0)) ||
            ((x->type == LVAL_DECIMAL) && (x->decimal != 0.0)))
        {
            lval_del(x);
            result = 1;
            break;
        }
        lval_del(x);
    }
    lval_del(a);
    return lval_boolean(result);
}

LBUILTIN_DECL(builtin_not)
{
    LASSERT_ARG_COUNT(a, 1, "!");
    LASSERT(a, a->cell[0]->type == LVAL_BOOLEAN || a->cell[0]->type == LVAL_INTEGER || a->cell[0]->type == LVAL_DECIMAL,
            "Function '!' got argument of invalid type %s.", ltype_name(a->cell[0]->type));
    lval* x = lval_take(a, 0);
    int result = x->type == LVAL_DECIMAL ? !x->decimal : !x->integer;
    lval_del(x);
    return lval_boolean(result);
}

LBUILTIN_DECL(builtin_eq)
{
    LASSERT_ARG_COUNT(a, 2, "==");
    int result = lval_eq(a->cell[0], a->cell[1]);
    lval_del(a);
    return lval_boolean(result);
}

LBUILTIN_DECL(builtin_ne)
{
    LASSERT_ARG_COUNT(a, 2, "!=");
    int result = !lval_eq(a->cell[0], a->cell[1]);
    lval_del(a);
    return lval_boolean(result);
}


void lenv_add_builtins(lenv* e)
{
    lval* sym = lval_symbol("true");
    lval* fun = lval_boolean(1);
    lenv_put(e, sym, fun);
    lval_del(sym);
    lval_del(fun);

    sym = lval_symbol("false");
    fun = lval_boolean(0);
    lenv_put(e, sym, fun);
    lval_del(sym);
    lval_del(fun);

    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "\\", builtin_lambda);
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
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, "<", builtin_lt);
    lenv_add_builtin(e, "<=", builtin_le);
    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "&&", builtin_and);
    lenv_add_builtin(e, "||", builtin_or);
    lenv_add_builtin(e, "!", builtin_not);
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "load", builtin_load);
    lenv_add_builtin(e, "print", builtin_print);
    lenv_add_builtin(e, "error", builtin_error);
    lenv_add_builtin(e, "read", builtin_read);
    lenv_add_builtin(e, "show", builtin_show);
}

void lenv_add_library(lenv* e, mpc_parser_t* parser)
{
    char* input[] = {
        "def{ fun } (\\ {args body} {def(head args) (\\ (tail args) body)})",
        /*
        "fun {unpack f xs} {eval (join (list f) xs)}",
        "fun { pack f & xs } {f xs}",
        "def {uncurry} pack",
        "def {curry} unpack",
        "def {first} (curry (\\ {x & xs} {x}))",
        "fun {second xs} {first (tail xs)}",
        "fun {reverse f x y} {f y x}",
        "fun {combine f g & xs} {g ((curry f) xs)}",
        "fun {len l} {if (== l {}) {0} {+ 1 (len (tail l))}}",
        "fun {reverse l} {if (== l {}) {{}} {join (reverse (tail l)) (head l)}}",
        "fun {nth n l} {if (== l {}) {()} {if (== n 0) {first l} {nth (- n 1) (tail l)}}}",
        "fun {last l} {if (== l {}) {()} {if (== (tail l) {}) {first l} {last (tail l)}}}",
        "fun {and x & xs} { if (! x) {false} {if (== xs {}) {true} { curry and xs }}}",
        "fun {or x & xs} { if (x) {true} {if (== xs {}) {false} { curry or xs }}}",
        "fun {not x} { if (x) {false} {true}}",
        */
        NULL
    };

    for (int i = 0; input[i] != NULL; i++)
    {
        mpc_result_t result;
        mpc_parse("<stdin>", input[i], parser, &result);
        lval_del(lval_eval(e, lval_read(result.output)));
        mpc_ast_delete(result.output);
    }
}

lval* lval_call(lenv* e, lval* f, lval* a)
{
    if (f->builtin != NULL)
    {
        return f->builtin(e, a);
    }

    int args_given = a->count;
    int args_total = f->formals->count;

    while (a->count > 0)
    {
        if (f->formals->count == 0)
        {
            lval_del(a);
            return lval_err("Function given too many arguments. Expected %d given %d.", args_total, args_given);
        }
        lval* sym = lval_pop(f->formals, 0);
        // special case: function supports variable size argument list in the format (\ {x & xs} {...})
        // when called, the first argument is assigned to x and the rest of the arguments are assigned to xs as a list
        if (strcmp(sym->sym, "&") == 0)
        {
            if (f->formals->count != 1)
            {
                lval_del(a);
                // TODO: Shouldn't this be checked on lambda definition?
                return lval_err("Function format invalid. '&' not followed by a single symbol.");
            }

            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }
        lval* val = lval_pop(a, 0);
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }
    lval_del(a);

    // special case: function supports variable size argument list but was given none, assign empty list to rest of arguments
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0)
    {
        if (args_total - args_given != 2)
        {
            return lval_err("Function format invalid. '&' not followed by a single symbol.");
        }

        // remove &
        lval_del(lval_pop(f->formals, 0));

        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }

    if (f->formals->count == 0)
    {
        lval* tmp = lval_sexpr();
        lval_add(tmp, lval_copy(f->body));
        f->env->par = e;
        return builtin_eval(f->env, tmp);
    }
    else
    {
        // Partially applied function
        return lval_copy(f);
    }
}

lval* lval_eval_sexpr(lenv* e, lval* v)
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

    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v)
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

    Comment = mpc_new("comment");
    String = mpc_new("string");
    Boolean = mpc_new("boolean");
    Integer = mpc_new("integer");
    Decimal = mpc_new("decimal");
    Number = mpc_new("number");
    Symbol = mpc_new("symbol");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Expr = mpc_new("expr");
    Lispy = mpc_new("lispy");

    char* Grammar =
        "comment  : /;[^\\r\\n]*/ ; "
        "string   : /\"(\\\\.|[^\"])*\"/ ; "
        "boolean  : \"true\" | \"false\" ; "
        "integer  : /-?[0-9]+/ ; "
        "decimal  : /-?[0-9]*\\.[0-9]+/ ; "
        "number   : <decimal> | <integer> ; "
        "symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\%\\^=<>!&|]+/ ; "
        "sexpr    : '(' <expr>* ')' ; "
        "qexpr    : '{' <expr>* '}' ; "
        "expr     : <number> | <symbol> | <sexpr> | <qexpr> | <string> | <comment> ; "
        "lispy    : /^/ <expr>* /$/ ; ";

    mpca_lang(MPCA_LANG_DEFAULT, Grammar, Comment, String, Boolean, Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbInfo;
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutput, &csbInfo);
#endif

    fore_color(10);
    puts("Felispy 0.1.4 - by Felipo");
    puts("Type ctrl+C to exit.\n");

    //debug_check("lenv_new");

    lenv* e = lenv_new();
    lenv_add_builtins(e);
    lenv_add_library(e, Lispy);

    //debug_check("Builtins loaded");

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            //printf("%d : %s\n", i, argv[i]);
            lval* args = lval_add(lval_sexpr(), lval_string(argv[i]));
            lval* x = builtin_load(e, args);
            if (x->type == LVAL_ERR)
            {
                lval_println(e, x);
            }
            lval_del(x);
        }
    }

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
            fore_color(14);
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

        //debug_check("REPL");
    }

    lenv_del(e);
    //debug_check("lenv_del");

#ifdef _WIN32
    SetConsoleTextAttribute(hOutput, csbInfo.wAttributes);
#endif

    mpc_cleanup(11, Comment, String, Boolean, Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
