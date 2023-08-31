#include "libmalloc/buddy_alloc.h"
#include "util.h"

#define BLOCK_RESERVED 0
#define BLOCK_FREE 1

static unsigned long compute_memory_size(const memory_map_t *map)
{
    // Find the last available region in the memory map.
    int map_index = map->size - 1;
    while(map->array[map_index].type != M_AVAILABLE)
    {
        map_index--;
    }

    return map->array[map_index].location + map->array[map_index].size;
}

static void insert_block(buddy_descriptor_t *heap, unsigned long index,
    unsigned long k)
{
    heap->free_block_count += 1UL << k;
    while(k < heap->max_kval)
    {
        unsigned long buddy_index = index ^ (1UL << k);
        if(heap->block_map[buddy_index].tag != BLOCK_FREE 
            || heap->block_map[buddy_index].kval != k)
        {
            break;
        }
        heap->block_map[buddy_index].linkb->linkf = heap->block_map[buddy_index].linkf;
        heap->block_map[buddy_index].linkf->linkb = heap->block_map[buddy_index].linkb;
        heap->block_map[buddy_index].tag = BLOCK_RESERVED;
        k++;
        if(buddy_index < index)
        {
            index = buddy_index;
        }
    }
    heap->block_map[index].tag = BLOCK_FREE;
    buddy_block_t *p = heap->avail[k].linkf;
    heap->block_map[index].linkf = p;
    heap->block_map[index].linkb = &heap->avail[k];
    p->linkb = &heap->block_map[index];
    heap->avail[k].linkf = &heap->block_map[index];
    heap->block_map[index].kval = k;
}

unsigned long buddy_map_size(const memory_map_t *map, unsigned long block_size)
{
    unsigned long memory_size = compute_memory_size(map);
    return 1UL << llog2(sizeof(buddy_block_t) * memory_size / block_size);
}

unsigned long buddy_reserve(buddy_descriptor_t *heap, unsigned long size)
{
    unsigned long k = llog2((size - 1) / heap->block_size + 1);
    for(unsigned long j = k; j <= heap->max_kval; j++)
    {
        if(heap->avail[j].linkf != &heap->avail[j])
        {
            buddy_block_t *block = heap->avail[j].linkb;
            heap->avail[j].linkb = block->linkb;
            heap->avail[j].linkb->linkf = &heap->avail[j];
            block->tag = BLOCK_RESERVED;
            while(j > k)
            {
                j--;
                buddy_block_t *buddy = block + (1UL << j);
                buddy->tag = BLOCK_FREE;
                buddy->kval = j;
                block->kval = j;
                buddy->linkb = &heap->avail[j];
                buddy->linkf = &heap->avail[j];
                heap->avail[j].linkb = buddy;
                heap->avail[j].linkf = buddy;
            }
            unsigned long index = block - heap->block_map;
            heap->free_block_count -= 1UL << k;
            return (unsigned long)heap->offset + index * heap->block_size;
        }
    }
    return NOMEM;
}

void buddy_free(buddy_descriptor_t *heap, unsigned long location)
{
    unsigned long index = (location - (unsigned long)heap->offset) / heap->block_size;
    unsigned long k = llog2((heap->block_size * (1UL << heap->block_map[index].kval)) / heap->block_size);
    insert_block(heap, index, k);
}

void buddy_free_size(buddy_descriptor_t *heap, unsigned long location,
    unsigned long size)
{
    unsigned long index = (location - (unsigned long)heap->offset) / heap->block_size;
    unsigned long k = llog2(size / heap->block_size);
    insert_block(heap, index, k);
}

int buddy_alloc_init(buddy_descriptor_t *heap, memory_map_t *map)
{
    heap->block_map_size = buddy_map_size(map, heap->block_size);
    heap->max_kval = llog2(heap->block_map_size / sizeof(buddy_block_t));
    heap->free_block_count = 0;
    for(int i = 0; i <= heap->max_kval; i++)
    {
        heap->avail[i].linkf = &heap->avail[i];
        heap->avail[i].linkb = &heap->avail[i];
    }

    if(heap->block_map == (buddy_block_t*)0)
    {
        int map_index = 0;
        while(map->array[map_index].type != M_AVAILABLE 
            || map->array[map_index].size < heap->block_map_size)
        {
            map_index++;
            if(map_index >= map->size)
            {
                return -1;
            }
        }

        heap->block_map = (buddy_block_t*)(heap->offset + map->array[map_index].location);
        memmap_insert_region(map, map->array[map_index].location, heap->block_map_size, M_UNAVAILABLE);
        if(heap->mmap && heap->mmap(heap->block_map, heap->block_map_size))
        {
            return -1;
        }
    }

    for(int i = 0; i < heap->block_map_size / sizeof(buddy_block_t); i++)
    {
        heap->block_map[i].tag = BLOCK_RESERVED;
        heap->block_map[i].kval = 0;
        heap->block_map[i].linkf = 0;
        heap->block_map[i].linkb = 0;
    }

    for(int i = 0; i < map->size; i++)
    {
        if(map->array[i].type != M_AVAILABLE)
        {
            continue;
        }

        unsigned long location = (map->array[i].location + heap->block_size - 1);
        location -= location % heap->block_size;
        unsigned long region_end = map->array[i].location + map->array[i].size;
        while(location + heap->block_size <= region_end)
        {
            unsigned long index = location / heap->block_size;
            unsigned long k = 0;
            insert_block(heap, index, k);
            location += heap->block_size;
            heap->free_block_count++;
        }
    }
    return 0;
}
