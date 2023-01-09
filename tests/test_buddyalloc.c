#include "libmalloc/bitmap_alloc.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

int main(int argc, char **args)
{
    const int memory_map_capacity = 32;
    const int heap_size = 1 << 20;
    const int cache_capacity = 20;
    memory_region_t arr[memory_map_capacity];
    unsigned long heap_cache[cache_capacity];

    memory_map_t memory_map = {
        .array = arr,
        .capacity = memory_map_capacity,
        .size = 0
    };

    bitmap_heap_descriptor_t heap = {
        .bitmap = malloc(heap_size / 4),
        .block_size = 1,
        .cache = heap_cache,
        .cache_capacity = cache_capacity
    };

    memmap_insert_region(&memory_map, 0, heap_size, M_AVAILABLE);
    initialize_physical_heap(&heap, &memory_map);

    unsigned long *reserved_blocks = malloc(sizeof(unsigned long) * heap_size);
    for(int i = 0; i < heap_size; i++)
    {
        reserved_blocks[i] = reserve_region(&heap, 1);
    }

    /*for(int i = 0; i < heap_size; i++)
    {
        for(int j = i + 1; j < heap_size; j++)
        {
            assert(reserved_blocks[i] != reserved_blocks[j]);
        }
    }*/

    for(int i = 0; i < heap_size; i++)
    {
        free_region(&heap, reserved_blocks[i], 1);
    }

    assert(heap.free_block_count == heap_size);
}