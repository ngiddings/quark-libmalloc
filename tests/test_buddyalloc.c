#include "libmalloc/buddy_alloc.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct memblock_t
{
    unsigned long size;
    unsigned long location;
} memblock_t;

void print_heap(FILE *file, buddy_descriptor_t *heap)
{
    unsigned long count = 1UL << heap->max_kval;
    fprintf(file, "heap {\n");
    fprintf(file, "\t.block_size = %lu\n", heap->block_size);
    fprintf(file, "\t.max_kval = %lu\n", heap->max_kval);
    fprintf(file, "\t.offset = %lu\n", heap->offset);
    fprintf(file, "\t.block_map = {\n");
    for(unsigned int i = 0; i < count; i++)
    {
        unsigned long linkf_diff = heap->block_map[i].linkf - heap->block_map;
        unsigned long linkb_diff = heap->block_map[i].linkb - heap->block_map;
        fprintf(file, "\t\t%u: {.tag = %s, .kval = %lu, .linkf = %li, .linkb = %li}\n",
            i,
            heap->block_map[i].tag ? "BLOCK_FREE" : "BLOCK_RESERVED",
            heap->block_map[i].kval,
            linkf_diff < count ? linkf_diff : -1,
            linkb_diff < count ? linkb_diff : -1);
    }
    fprintf(file, "\t}\n");
    fprintf(file, "\t.avail = {\n");
    for(unsigned int i = 0; i <= heap->max_kval; i++)
    {
        fprintf(file, "\t\t%u: {.linkf = %lu, .linkb = %lu}\n",
            i,
            heap->avail[i].linkf - heap->block_map,
            heap->avail[i].linkb - heap->block_map);
    }
    fprintf(file, "\t}\n}\n");
}

int main(int argc, char **argv)
{
    unsigned long mem_size;
    unsigned long block_size;
    sscanf(argv[1], "%lu", &mem_size);
    sscanf(argv[2], "%lu", &block_size);

    FILE *out = fopen("debug_output.txt", "w");
    if(out == NULL)
    {
        fprintf(stderr, "Failed to open debug output.\n");
        return 0;
    }

    memory_map_t memory_map = {
        .array = malloc(sizeof(memory_region_t) * 16),
        .capacity = 16,
        .size = 0
    };

    buddy_descriptor_t heap = {
        .avail = malloc(sizeof(buddy_block_t) * 20),
        .block_map = malloc(buddy_map_size(&memory_map, block_size)),
        .block_size = block_size,
        .mmap = NULL,
        .offset = 0
    };

    memmap_insert_region(&memory_map, 0, mem_size, M_AVAILABLE);
    if(buddy_alloc_init(&heap, &memory_map))
    {
        fprintf(stderr, "Failed to initialize buddy allocator.\n");
        return 0;
    }

    print_heap(out, &heap);

    memblock_t *blocks = calloc(mem_size / block_size, sizeof(memblock_t));

    for(int i = 0; i < 1024; i++)
    {
        int index = rand() % (mem_size / block_size);
        if(blocks[index].size == 0)
        {
            blocks[index].size = 1;
            blocks[index].location = buddy_reserve(&heap, 1);
            fprintf(out, "RESERVED %lu\n", blocks[index].location / heap.block_size);
        }
        else
        {
            buddy_free(&heap, blocks[index].location);
            blocks[index].size = 0;
            fprintf(out, "FREED %lu\n", blocks[index].location / heap.block_size);
        }
        print_heap(out, &heap);
    }

    print_heap(out, &heap);

    return 0;
}