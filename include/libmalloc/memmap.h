#ifndef _LIBMALLOC_MEMMAP_H
#define _LIBMALLOC_MEMMAP_H

typedef enum
{
    M_AVAILABLE = 1,
    M_UNAVAILABLE = 2,
    M_DEFECTIVE = 3
} memory_type_t;

typedef struct
{
    memory_type_t type;
    unsigned long location;
    unsigned long size;
} memory_region_t;

typedef struct
{
    memory_region_t *array;
    unsigned long size;
    unsigned long capacity;
} memory_map_t;

int memmap_insert_region(memory_map_t *map, unsigned long location, unsigned long size, memory_type_t type);

#endif