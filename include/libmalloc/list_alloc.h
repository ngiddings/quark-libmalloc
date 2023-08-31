#ifndef _LIBMALLOC_LISTALLOC_H
#define _LIBMALLOC_LISTALLOC_H

#include "memmap.h"
#include "common.h"

typedef struct list_block_t
{
    unsigned long free;
    unsigned long size;
    struct list_block_t *prev;
    struct list_block_t *next;
} list_block_t;

typedef struct list_alloc_descriptor_t
{
    list_block_t head;
    list_block_t *current_block;
} list_alloc_descriptor_t;

void *list_alloc_reserve(list_alloc_descriptor_t *heap, unsigned long size);

void list_alloc_free(list_alloc_descriptor_t *heap, void *p);

int list_alloc_init(list_alloc_descriptor_t *heap, memory_map_t *map);

#endif