#ifndef _DEBUG_ALLOC_H_
#define _DEBUG_ALLOC_H_

#include <stdlib.h>

/* debug_alloc.h

A very simple way to find out if memory is being allocated and freed accordingly
Normal calls to malloc/realloc and free are replaced by proxy calls that keep track of pointers in use.
Implementation is inefficient (no hash table, just a plain array).
*/

#define DEBUG_MAX_COUNT 4096
int debug_alloc_count = 0;
void* debug_alloc_list[DEBUG_MAX_COUNT];

void* debug_malloc(size_t size)
{
    void *p = malloc(size);
    debug_alloc_list[debug_alloc_count++] = p;
    if (debug_alloc_count > DEBUG_MAX_COUNT)
    {
        int *x = 0;
        *x = 0; // Trigger an exception!!!
    }
    return p;
}

void* debug_realloc(void *old_p, size_t size)
{
    void *p = realloc(old_p, size);
    if (p != old_p)
    {
        for (int i = 0; i < debug_alloc_count; i++)
        {
            if (debug_alloc_list[i] == old_p)
            {
                debug_alloc_list[i] = p;
                break;
            }
        }
    }
    return p;
}

void debug_free(void *p)
{
    for (int i = 0; i < debug_alloc_count; i++)
    {
        if (debug_alloc_list[i] == p)
        {
            debug_alloc_list[i] = NULL;
            break;
        }
    }
    free(p);
}

void debug_check()
{
    int freed = 0;
    int used = 0;
    for (int i = 0; i < debug_alloc_count; i++)
    {
        if (debug_alloc_list[i] == NULL)
        {
            freed++;
        }
        else
        {
            used++;
        }
    }
    printf("DEBUG - freed: %i, used: %i\n", freed, used);
}

#define malloc(size) debug_malloc(size)
#define realloc(p, size) debug_realloc(p, size)
#define free(p) debug_free(p)

#endif
