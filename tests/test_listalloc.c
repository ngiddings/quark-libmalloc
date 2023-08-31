#include "libmalloc/list_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

typedef struct memblock_t
{
    unsigned long size;
    void *p;
} memblock_t;

void check_block_list(void *heap, unsigned long heap_size, memblock_t *blocks, unsigned long count)
{
    for (int j = 0; j < count; j++)
    {
        if (blocks[j].size > 0)
        {
            assert(blocks[j].p >= heap);
            assert(blocks[j].p + blocks[j].size <= heap + heap_size);
            for (int k = j + 1; k < count; k++)
            {
                if (blocks[k].size > 0)
                {
                    // printf("j = (%p, %lu), k = (%p, %lu)\n", blocks[j].p, blocks[j].size, blocks[k].p, blocks[k].size);
                    assert(blocks[k].p < blocks[j].p || blocks[k].p >= blocks[j].p + blocks[j].size);
                    assert(blocks[j].p < blocks[k].p || blocks[j].p >= blocks[k].p + blocks[k].size);
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    unsigned int max_block_count = 0;
    unsigned int heap_size = 0;
    unsigned int passes = 0;

    char c;
    while((c = getopt(argc, argv, "m:b:c:")) != -1)
    {
        if(c == 'm')
        {
            sscanf(optarg, "%i", &heap_size);
            heap_size *= 1024;
        }
        else if(c == 'b')
        {
            sscanf(optarg, "%i", &max_block_count);
        }
        else if(c == 'c')
        {
            sscanf(optarg, "%i", &passes);
        }
    }

    printf("Running test with %i byte heap, %i blocks, %i passes.\n",
        heap_size, max_block_count, passes);

    void *heap = malloc(heap_size);
    memory_map_t map = {
        .array = malloc(sizeof(unsigned long) * 16),
        .capacity = 16,
        .size = 0
    };
    memmap_insert_region(&map, heap, heap_size, M_AVAILABLE);

    list_alloc_descriptor_t desc;
    list_alloc_init(&desc, &map);
    memblock_t *blocks = calloc(max_block_count, sizeof(memblock_t));

    for(int i = 0; i < passes; i++)
    {
        int index = rand() % max_block_count;
        if(blocks[index].size == 0)
        {
            unsigned long size = rand() % 65536 + 1;
            blocks[index].p = list_alloc_reserve(&desc, size);
            //printf("Reserved %lu bytes.\n", size);
            if(blocks[index].p != NOMEM)
            {
                blocks[index].size = size;
            }
            else
            {
                printf("Out of memory trying to reserve %lu bytes\n", size);
            }
        }
        else
        {
            list_alloc_free(&desc, blocks[index].p);
            //printf("Freed %p\n", blocks[index].p);
            blocks[index].size = 0;
        }
        check_block_list(heap, heap_size, blocks, max_block_count);
    }

    for(int i = 0; i < max_block_count; i++)
    {
        if(blocks[i].size != 0)
        {
            list_alloc_free(&desc, blocks[i].p);
            blocks[i].size = 0;
        }
    }

    return 0;
}