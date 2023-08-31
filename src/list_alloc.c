#include "libmalloc/list_alloc.h"
#include <stddef.h>

#define MIN_BLOCK_SIZE (4 * sizeof(list_block_t))

static list_block_t *block_end_ptr(list_block_t *p)
{
    return (list_block_t*)((void*)p + p->size - sizeof(list_block_t));
}

static list_block_t *block_start_ptr(list_block_t *p)
{
    return (list_block_t*)((void*)p - p->size + sizeof(list_block_t));
}

static void set_block_tag(list_block_t *p, unsigned long free)
{
    p->free = free;
    block_end_ptr(p)->free = free;
}

static void set_block_size(list_block_t *p, unsigned long size)
{
    p->size = size;
    block_end_ptr(p)->size = size;
}

static void set_block_next(list_block_t *p, list_block_t *next)
{
    p->next = next;
    block_end_ptr(p)->next = next;
}

static void set_block_prev(list_block_t *p, list_block_t *prev)
{
    p->prev = prev;
    block_end_ptr(p)->prev = prev;
}

static void set_block(list_block_t *p, unsigned long tag, unsigned long size,
    list_block_t *next, list_block_t *prev)
{
    set_block_size(p, size);
    set_block_tag(p, tag);
    set_block_next(p, next);
    set_block_prev(p, prev);
}

void *list_alloc_reserve(list_alloc_descriptor_t *heap, unsigned long size)
{
    size += sizeof(unsigned long) - 1;
    size -= size % sizeof(unsigned long);
    list_block_t *p = heap->current_block;
    do
    {
        if(p->size >= (size + 2 * sizeof(list_block_t) + MIN_BLOCK_SIZE))
        {
            unsigned long new_size = p->size - size - (2 * sizeof(list_block_t));
            list_block_t *new_block = (void*)p + new_size;
            set_block(new_block, 0, size + 2 * sizeof(list_block_t), 0, 0);
            set_block_tag(p, 1);
            set_block_size(p, new_size);
            heap->current_block = p;
            return (void*)new_block + sizeof(list_block_t);
        }
        else if(p->size >= (size + 2 * sizeof(list_block_t)))
        {
            set_block_next(p->prev, p->next);
            set_block_prev(p->next, p->prev);
            heap->current_block = p->next;
            set_block(p, 0, p->size, 0, 0);
            return (void*)p + sizeof(list_block_t);
        }
        p = p->next;
    } while(p != heap->current_block);
    return NOMEM;
}

void list_alloc_free(list_alloc_descriptor_t *heap, void *p)
{
    list_block_t *block = (list_block_t*)(p - sizeof(list_block_t));

    list_block_t *lhs = block_start_ptr(block - 1);
    if(lhs->free == 1)
    {
        set_block_next(lhs->prev, lhs->next);
        set_block_prev(lhs->next, lhs->prev);
        unsigned long new_size = block->size + lhs->size;
        block = (void*)block - lhs->size;
        block->size = new_size;
    }

    list_block_t *rhs = (block_end_ptr(block) + 1);
    if(rhs->free == 1)
    {
        set_block_next(rhs->prev, rhs->next);
        set_block_prev(rhs->next, rhs->prev);
        block->size += rhs->size;
    }

    set_block(block, 1, block->size, &heap->head, heap->head.prev);
    set_block_next(heap->head.prev, block);
    set_block_prev(&heap->head, block);
    heap->current_block = block;
}

int list_alloc_init(list_alloc_descriptor_t *heap, memory_map_t *map)
{
    heap->head.free = 0;
    heap->head.size = sizeof(heap->head);
    heap->head.prev = &heap->head;
    heap->head.next = &heap->head;
    heap->current_block = &heap->head;
    for(int i = 0; i < map->size; i++)
    {
        if(map->array[i].type != M_AVAILABLE 
            || map->array[i].size < MIN_BLOCK_SIZE)
        {
            continue;
        }
        list_block_t *new_block = ((list_block_t*)map->array[i].location) + 1;
        set_block(new_block, 1, map->array[i].size - sizeof(list_block_t) * 2, &heap->head, heap->head.prev);
        set_block_next(heap->head.prev, new_block);
        set_block_prev(&heap->head, new_block);
        (new_block - 1)->free = 0;
        (block_end_ptr(new_block) + 1)->free = 0;
    }
    return 0;
}
