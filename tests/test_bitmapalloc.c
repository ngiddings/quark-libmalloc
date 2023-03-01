#include "libmalloc/bitmap_alloc.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

typedef struct memblock_t
{
    unsigned long size;
    unsigned long location;
} memblock_t;

void print_heap_desc(bitmap_heap_descriptor_t *heap)
{
    printf("heap = {\n"
        "\t.bitmap = %p\n"
        "\t.bitmap_size = %i\n"
        "\t.cache = %p\n"
        "\t.cache_capacity = %i\n"
        "\t.block_size = %i\n"
        "\t.height = %i\n"
        "\t.free_block_count = %i\n"
        "\t.mask = %lX\n"
        "\t.block_bits = %i\n"
        "\t.blocks_in_word = %i\n"
        "\t.offset = %p\n"
        "}\n",
        heap->bitmap, 
        heap->bitmap_size, 
        heap->cache, 
        heap->cache_capacity,
        heap->block_size,
        heap->height,
        heap->free_block_count,
        heap->mask,
        heap->block_bits,
        heap->blocks_in_word,
        heap->offset);
}

void print_memory_map(memory_map_t *map)
{
    printf("map = {\n"
        "\t.size = %i\n"
        "\t.capacity = %i\n"
        "\t.array = {",
        map->size,
        map->capacity);
    for(int i = 0; i < map->size; i++)
    {
        printf("{.type = %i, .location = %lX, .size = %i} ", \
            map->array[i].type, 
            map->array[i].location, 
            map->array[i].size);
    }
    printf("}\n}\n");
}

void test_heap(unsigned long size, unsigned long block_size, unsigned long bits, int result)
{
    printf("[TEST] Bitmap allocator: memory=%lX, block_size=%lu, block_bits=%lu\n", size, block_size, bits);
    const int memory_map_capacity = 32;
    const int cache_capacity = 20;
    memory_region_t arr[memory_map_capacity];
    unsigned long heap_cache[cache_capacity];
    void *heap_data = malloc(size);

    memory_map_t memory_map = {
        .array = arr,
        .capacity = memory_map_capacity,
        .size = 0
    };

    bitmap_heap_descriptor_t heap = {
        .bitmap = NULL,
        .block_size = block_size,
        .cache = heap_cache,
        .cache_capacity = cache_capacity,
        .block_bits = bits,
        .offset = (unsigned long)heap_data
    };

    memmap_insert_region(&memory_map, 0, size, M_AVAILABLE);
    int status = initialize_heap(&heap, &memory_map, NULL);
    assert(!(!status != !result));
    if(status)
    {
        return;
    }

    print_heap_desc(&heap);
    print_memory_map(&memory_map);

    unsigned long total_blocks = heap.free_block_count;

    memblock_t *reserved_blocks = malloc(sizeof(memblock_t) * size / heap.block_size);
    unsigned long count = 0;
    while(1)
    {
        memblock_t next_block;
        next_block.size = heap.block_size * (rand() % 8 + 1);
        next_block.location = reserve_region(&heap, next_block.size);
        if(next_block.location != NOMEM)
        {
            char *s = (char*)next_block.location;
            for(int i = 0; i < next_block.size; i++)
            {
                s[i] = (char)rand();
            }
            reserved_blocks[count] = next_block;
            count++;
        }
        else
        {
            printf("\tOut of memory: %i free blocks left, %i total blocks, %i allocations. Tried to allocate %i bytes.\n", heap.free_block_count, total_blocks, count, next_block.size);
            break;
        }
    }

    for(int i = 0; i < count; i++)
    {
        assert(reserved_blocks[i].location >= heap.offset);
        assert(reserved_blocks[i].location + reserved_blocks[i].size <= heap.offset + size);
        for(int j = i + 1; j < count; j++)
        {
            assert((reserved_blocks[j].location + reserved_blocks[j].size) <= reserved_blocks[i].location
                || reserved_blocks[j].location >= (reserved_blocks[i].location + reserved_blocks[i].size));
        }
    }

    for(int i = 0; i < count; i++)
    {
        free_region(&heap, reserved_blocks[i].location, bits < 2 ? reserved_blocks[i].size : 0);
    }

    assert(heap.free_block_count == total_blocks);
    free(reserved_blocks);
    free(heap_data);
}

int main(int argc, char **args)
{
    srand(time(0));
    test_heap(32, 1, 1, 1);
    test_heap(256, 0, 1, 1);
    test_heap(256, 1, 0, 1);
    test_heap(256, 1, 3, 1);
    test_heap(256, 1, 128, 1);
    test_heap(256, 1, 4, 1);

    for(int bs = 1; bs <= 32; bs *= 2)
    {
        unsigned long heap_size = (1 << llog2(bs)) * 256;
        unsigned long n = (8 * sizeof(unsigned long)) - 1 - __builtin_clzl((unsigned long) bs);
        for(int bits = 1; bits < (8 * sizeof(unsigned long)) && (2 * bits) < (8 * (1 << n)); bits <<= 1)
        {
            test_heap(heap_size, bs, bits, 0);
        }
    }
}