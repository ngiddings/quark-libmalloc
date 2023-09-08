#ifndef _LIBMALLOC_BUDDYALLOC_H
#define _LIBMALLOC_BUDDYALLOC_H

#include "memmap.h"
#include "common.h"

typedef struct buddy_block_t 
{
    struct buddy_block_t *linkb;

    struct buddy_block_t *linkf;

    unsigned long kval;

    unsigned long tag;

} buddy_block_t;

typedef struct buddy_descriptor_t
{
    /**
     * @brief An array of `buddy_block_t` structs serving as the heads of the
     * lists of available blocks. avail[k] serves as the head of the list of
     * blocks of size 2^k.
     */
    buddy_block_t *avail;

    buddy_block_t *block_map;

    unsigned long block_map_size;

    unsigned long max_kval;

    unsigned long block_size;

    unsigned long offset;

    unsigned long free_block_count;

    int (*mmap)(void *location, unsigned long size);

} buddy_descriptor_t;

unsigned long buddy_map_size(const memory_map_t *map, unsigned long block_size);

unsigned long buddy_reserve(buddy_descriptor_t *heap, unsigned long size);

unsigned long buddy_free(buddy_descriptor_t *heap, unsigned long location);

unsigned long buddy_free_size(buddy_descriptor_t *heap, unsigned long size, 
    unsigned long location);

int buddy_alloc_init(buddy_descriptor_t *heap, memory_map_t *map);

#endif