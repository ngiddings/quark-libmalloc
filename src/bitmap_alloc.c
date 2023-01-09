#include "libmalloc/bitmap_alloc.h"
#include "libmalloc/common.h"
#include "util.h"

/*
 * The number of bits contained in a single integer inside the heap's bitmap.
 * Should be either 32 or 64 depending on the host machine.
 */
static const int bitmap_word_size = 8 * sizeof(unsigned long);

/*
 * Sets all elements in the cache's underlying array to 0.
 */
static inline void clear_cache(bitmap_heap_descriptor_t *heap)
{
    for(int i = 0; i < heap->cache_capacity; i++)
    {
        heap->cache[i] = 0;
    }
}

/*
 * Clears all bits in the heap's bitmap.
 */
static inline void clear_bitmap(bitmap_heap_descriptor_t *heap)
{
    for(int i = 0; i < heap->bitmap_size / sizeof(*heap->bitmap); i++)
    {
        heap->bitmap[i] = 0;
    }
}

/*
 * Sets bit `index` in the heap's bitmap, marking the underlying block as
 * available.
 */
static inline void set_bit(bitmap_heap_descriptor_t *heap, int index)
{
    int bitmap_index = index / bitmap_word_size;
    int bitmap_offset = index % bitmap_word_size;
    heap->bitmap[bitmap_index] |= (unsigned long)1 << bitmap_offset;
}

/*
 * Clears bit `index` in the heap's bitmap, marking the underlying block as
 * reserved.
 */
static inline void clear_bit(bitmap_heap_descriptor_t *heap, int index)
{
    int bitmap_index = index / bitmap_word_size;
    int bitmap_offset = index % bitmap_word_size;
    heap->bitmap[bitmap_index] &= ~((unsigned long)1 << bitmap_offset);
}

/*
 * Sets bit `index` and its buddy in the heap's bitmap, marking the underlying
 * blocks as available. Operation is used while spltting a block to reserve one
 * of its child blocks.
 */
static inline void set_pair(bitmap_heap_descriptor_t *heap, int index)
{
    int bitmap_index = index / bitmap_word_size;
    int bitmap_offset = index % bitmap_word_size;
    heap->bitmap[bitmap_index] |= (unsigned long)1 << bitmap_offset;
    heap->bitmap[bitmap_index] |= (unsigned long)1 << (bitmap_offset ^ 1);
}

/*
 * Clears bit `index` and its buddy in the heap's bitmap, marking the underlying
 * blocks as reserved. Used when merging two child blocks into a single parent block.
 */
static inline void clear_pair(bitmap_heap_descriptor_t *heap, int index)
{
    int bitmap_index = index / bitmap_word_size;
    int bitmap_offset = index % bitmap_word_size;
    heap->bitmap[bitmap_index] &= ~((unsigned long)1 << bitmap_offset);
    heap->bitmap[bitmap_index] &= ~((unsigned long)1 << (bitmap_offset ^ 1));
}

/*
 * Computes the location in the cache that `index` would be stored at, if it
 * were cached.
 */
static inline int cache_location_from_index(int index)
{
    return llog2(index + 1) - llog2(bitmap_word_size) - 1;
}

/*
 * Returns a cached value from the desired location, removing that value from
 * the cache.
 */
static inline int check_cache(bitmap_heap_descriptor_t *heap, int height)
{
    unsigned long n = heap->cache[heap->height - height - llog2(bitmap_word_size)];
    heap->cache[heap->height - height - llog2(bitmap_word_size)] = 0;
    return n;
}

/*
 * If space in the cache exists, stores the provided index at the appropriate
 * location.
 */
static inline void store_cache(bitmap_heap_descriptor_t *heap, int index)
{
    int level = cache_location_from_index(index);
    if(level >= 0 && heap->cache[level] == 0)
    {
        heap->cache[level] = index;
    }
}

/*
 * If `index` is stored in the cache, remove it. Otherwise, leave the cache
 * as is.
 */
static inline void uncache(bitmap_heap_descriptor_t *heap, int index)
{
    int level = cache_location_from_index(index);
    if(level >= 0 && heap->cache[level] == index)
    {
        heap->cache[level] = 0;
    }
}

/*
 * Marks the indicated block as unavailable, and marks its children as both
 * available. Stores the right-hand child in the cache, and returns the index
 * of the left-hand child. The caller is expected to either split or allocate
 * the left-hand child.
 */
static inline int split_block(bitmap_heap_descriptor_t *heap, int index)
{
    if(index)
    {
        clear_bit(heap, index);
        index *= 2;
        set_pair(heap, index);
        store_cache(heap, index + 1);
    }
    return index;
}

/*
 * If the buddy of the indicated block is marked as available, marks the 
 * indicated block and its buddy as unavailable, and marks their parent
 * as available. If the buddy of the indicated block is cached, it kill be
 * removed from the cache.
 * 
 * If the buddy of the indicated block is marked as unavailable, this function 
 * does nothing. The block indicated by `index` is assumed to be available.
 */
static int merge_block(bitmap_heap_descriptor_t *heap, int index)
{
    while(index > 1 && (heap->bitmap[index / bitmap_word_size] & ((unsigned long)1 << ((index % bitmap_word_size) ^ 1))))
    {
        uncache(heap, index ^ 1);
        clear_pair(heap, index);
        index /= 2;
        set_bit(heap, index);
    }
    return index;
}

/*
 * Finds the index of the first available block at `height`. If no such block
 * is available, recursively searches higher blocks and splits them until an
 * appropriate block is available. Returns 0 if no available blocks of sufficient
 * size exist.
 */
static int find_free_region(bitmap_heap_descriptor_t *heap, int height)
{
    if (height > heap->height || height < 0)
    {
        return 0;
    }
    else if (height <= heap->height - ilog2(bitmap_word_size))
    {
        unsigned long cached_index = check_cache(heap, height);
        if(cached_index)
        {
            return cached_index;
        }
        unsigned long start = (1 << (heap->height - height)) / bitmap_word_size;
        unsigned long end = ((1 << (heap->height - height + 1)) / bitmap_word_size);
        for (int index = start; index < end; index++)
        {
            if (heap->bitmap[index] != 0)
            {
                return bitmap_word_size * index + __builtin_ctzl(heap->bitmap[index]);
            }
        }
    }
    else
    {
#if __SIZEOF_LONG__ == 8
        static const unsigned long bitmasks[] = {0x00000002, 0x0000000C, 0x000000F0, 0x0000FF00, 0xFFFF0000, 0xFFFFFFFF00000000};
#else
        static const unsigned long bitmasks[] = {0x00000002, 0x0000000C, 0x000000F0, 0x0000FF00, 0xFFFF0000};
#endif
        int depth = heap->height - height;
        if (heap->bitmap[0] & bitmasks[depth])
        {
            return __builtin_ctzl(heap->bitmap[0] & bitmasks[depth]);
        }
    }
    return split_block(heap, find_free_region(heap, height + 1));
}


unsigned long reserve_region(bitmap_heap_descriptor_t *heap, unsigned long size)
{
    int height = llog2(size / heap->block_size);
    int index = find_free_region(heap, height);
    if(index)
    {
        clear_bit(heap, index);
        heap->free_block_count -= 1 << height;
        return (heap->block_size << height) * (index - ((unsigned long)1 << (heap->height - height)));
    }
    else
    {
        return NOMEM;
    }
}

void free_region(bitmap_heap_descriptor_t *heap, unsigned long location, unsigned long size)
{
    int height = llog2(size / heap->block_size);
    int index = (location / (heap->block_size * ((unsigned long)1 << height))) + (1 << (heap->height - height));
    set_bit(heap, index);
    index = merge_block(heap, index);
    store_cache(heap, index);
    heap->free_block_count += 1 << height;
}

unsigned long bitmap_size(const memory_map_t *map, unsigned long block_size)
{
    // Find the last available region in the memory map.
    int map_index = map->size - 1;
    while(map->array[map_index].type != M_AVAILABLE)
    {
        map_index--;
    }

    // Take memory_size to be the last available location in the memory map.
    // Round memory_size up to nearest power of 2
    unsigned long memory_size = 1 << llog2(map->array[map_index].location + map->array[map_index].size);
    return (memory_size / block_size) / 4;
}

int initialize_virtual_heap(bitmap_heap_descriptor_t *heap, const memory_map_t *map, int (*mmap)(void *location, unsigned long size))
{
    /* Not yet implemented */
    return -1;
}

int initialize_physical_heap(bitmap_heap_descriptor_t *heap, const memory_map_t *map)
{
    // Find the last available region in the memory map.
    int map_index = map->size - 1;
    while(map->array[map_index].type != M_AVAILABLE)
    {
        map_index--;
    }

    // Take memory_size to be the last available location in the memory map.
    // Round memory_size up to nearest power of 2
    unsigned long memory_size = 1 << llog2(map->array[map_index].location + map->array[map_index].size);
    heap->bitmap_size = (memory_size / heap->block_size) / 4;
    heap->height = llog2(memory_size / heap->block_size);
    heap->free_block_count = 0;
    clear_bitmap(heap);
    for(int i = 0; i < map->size; i++)
    {
        if(map->array[i].type != M_AVAILABLE)
        {
            continue;
        }

        unsigned long location = (map->array[i].location + heap->block_size - 1) & ~(heap->block_size - 1);
        unsigned long region_end = map->array[i].location + map->array[i].size;

        while(location + heap->block_size <= region_end)
        {
            int bit_offset = (location / heap->block_size) % bitmap_word_size;
            int bitmap_index = ((1 << (heap->height - 0)) / bitmap_word_size) + (location / heap->block_size) / bitmap_word_size;
            unsigned long chunk_size = (bitmap_word_size - bit_offset) * heap->block_size;
            if(bit_offset == 0 && (region_end - location) >= chunk_size)
            {
                // Set all bits in the word
                heap->bitmap[bitmap_index] = ~0;
                heap->free_block_count += bitmap_word_size;
            }
            else if(bit_offset == 0)
            {
                // Set the first 'count' bits
                int count = (region_end - location) / heap->block_size;
                heap->bitmap[bitmap_index] |= (1 << count) - 1;
                heap->free_block_count += count;
            }
            else if((region_end - location) >= chunk_size)
            {
                // Set all bits starting at 'bit_offset'
                heap->bitmap[bitmap_index] |= ~((1 << bit_offset) - 1);
                heap->free_block_count += bitmap_word_size - bit_offset;
            }
            else
            {
                // Set all bits starting at 'bit_offset' up to 'count'
                int count = (region_end - location) / heap->block_size;
                heap->bitmap[bitmap_index] |= ((1 << count) - 1) & ~((1 << bit_offset) - 1);
                heap->free_block_count += count - bit_offset;
            }

            // Merge 'buddies' when both available
            unsigned long mask = 3;
            for(int j = 0; j < bitmap_word_size / 2; j++)
            {
                if((heap->bitmap[bitmap_index] & mask) == mask)
                {
                    merge_block(heap, bitmap_index * bitmap_word_size + j * 2);
                }
                mask <<= 2;
            }
            
            location += chunk_size;
        }
    }
    clear_cache(heap);
    return 0;
}
