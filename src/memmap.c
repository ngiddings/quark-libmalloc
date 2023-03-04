#include "libmalloc/memmap.h"
#include <stdbool.h>

static int compare_regions(memory_region_t *lhs, memory_region_t *rhs)
{
    if(lhs->location == rhs->location)
    {
        return lhs->size > rhs->size ? 1
            : (lhs->size == rhs->size ? 0
            : -1);
    }
    else
    {
        return lhs->location > rhs->location ? 1 : -1;
    }
}

static bool region_overlaps(memory_region_t *lhs, memory_region_t *rhs)
{
    if(rhs->location < lhs->location)
    {
        return rhs->location + rhs->size > lhs->location;
    }
    return rhs->location < lhs->location + lhs->size;
}

static bool region_contains(memory_region_t *lhs, memory_region_t *rhs)
{
    return (rhs->location >= lhs->location) &&
        (rhs->location + rhs->size <= lhs->location + lhs->size);
}

static void insert_map_entry(memory_map_t *map, unsigned long location, unsigned long size, unsigned int type)
{
    memory_region_t new_region = {.location = location, .size = size, .type = type};
    unsigned int i = 0;
    while(i < map->size)
    {
        if(compare_regions(&new_region, &map->array[i]) < 0)
        {
            memory_region_t buffer = new_region;
            new_region = map->array[i];
            map->array[i] = buffer;
        }
        i++;
    }
    map->array[i] = new_region;
    map->size++;
}

void remove_map_entry(memory_map_t *map, int index)
{
    if(index >= 0 && index < map->size)
    {
        for(int i = index; i < map->size - 1; i++)
        {
            map->array[i] = map->array[i + 1];
        }
        map->size--;
    }
}

static int trim_map(memory_map_t *map, int index)
{
    if(index + 1 >= map->size)
    {
        return -1;
    }
    memory_region_t *left = &map->array[index];
    memory_region_t *right = &map->array[index + 1];
    if(region_overlaps(left, right) && left->type == right->type)
    {
        left->size = (right->location + right->size > left->location + left->size ? right->location + right->size : left->location + left->size) - left->location;
        remove_map_entry(map, index + 1);
        return index;
    }
    else if(region_overlaps(left, right) && left->type < right->type && region_contains(right, left))
    {
        remove_map_entry(map, index);
        return index;
    }
    else if(region_overlaps(left, right) && left->type < right->type && left->location + left->size <= right->location + right->size)
    {
        left->size = (right->location > left->location) ? right->location - left->location : 0;
        return index + 1;
    }
    else if(region_overlaps(left, right) && left->type < right->type)
    {
        memory_region_t new_right = {
            .location = right->location + right->size,
            .size = (left->location + left->size) - (right->location + right->size),
            .type = left->type};
        left->size = (right->location > left->location) ? right->location - left->location : 0;
        if (left->size == 0)
            remove_map_entry(map, index);
        insert_map_entry(map, new_right.location, new_right.size, new_right.type);
        return index + 2;
    }
    else if(region_overlaps(left, right) && region_contains(left, right))
    {
        remove_map_entry(map, index + 1);
        return index;
    }
    else if(region_overlaps(left, right))
    {
        right->size = (right->location + right->size) - (left->location + left->size);
        right->location = left->location + left->size;
        return index + 1;
    }
    else if((left->location + left->size == right->location) && left->type == right->type)
    {
        left->size = right->location + right->size - left->location;
        remove_map_entry(map, index + 1);
        return index;
    }
    else
    {
        return index + 1;
    }
}

int memmap_insert_region(memory_map_t *map, unsigned long location, unsigned long size, memory_type_t type)
{
    if(map->size <= map->capacity - 2)
    {
        insert_map_entry(map, location, size, type);
        int i = 0;
        while(i >= 0)
        {
            i = trim_map(map, i);
        }
        return 0;
    }
    else
    {
        return -1;
    }
}