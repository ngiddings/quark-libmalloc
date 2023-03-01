#include "libmalloc/bitmap_alloc.h"
#include "libmalloc/common.h"
#include "util.h"

static const int BIT_AVAIL = 0;
static const int BIT_USED = 1;

/*
 * Sets all elements in the cache's underlying array to 0.
 */
static inline void clear_cache(bitmap_heap_descriptor_t *heap)
{
    if(heap->cache == (unsigned long*)0)
    {
        return;
    }

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
static inline void set_bit(bitmap_heap_descriptor_t *heap, int index, int bit)
{
    if(bit < heap->block_bits)
    {
        int bitmap_index = index / heap->blocks_in_word;
        int bitmap_offset = index % heap->blocks_in_word;
        unsigned long mask = (unsigned long)1 << (heap->block_bits * (bitmap_offset + 1) - 1 - bit);
        heap->bitmap[bitmap_index] |= mask;
    }
}

/*
 * Clears bit `index` in the heap's bitmap, marking the underlying block as
 * reserved.
 */
static inline void clear_bit(bitmap_heap_descriptor_t *heap, int index, int bit)
{
    if(bit < heap->block_bits)
    {
        int bitmap_index = index / heap->blocks_in_word;
        int bitmap_offset = index % heap->blocks_in_word;
        unsigned long mask = ~((unsigned long)1 
            << (heap->block_bits * (bitmap_offset + 1) - 1 - bit));
        heap->bitmap[bitmap_index] &= mask;
    }
}

/*
 * Tests whether the block at bit `index` is available. If so, returns nonzero,
 * else returns 0.
 */
static inline int test_bit(bitmap_heap_descriptor_t *heap, int index, int bit)
{
    if(bit > (heap->block_bits - 1))
    {
        return 1;
    }
    unsigned long mask = ((unsigned long)1 
            << (heap->block_bits * ((index % heap->blocks_in_word) + 1) 
                - 1 
                - bit));
    return (heap->bitmap[index / heap->blocks_in_word] & mask) != 0;
}

/*
 * Sets bit `index` and its buddy in the heap's bitmap, marking the underlying
 * blocks as available. Operation is used while spltting a block to reserve one
 * of its child blocks.
 */
static inline void set_pair(bitmap_heap_descriptor_t *heap, int index, int bit)
{
    if(bit < heap->block_bits)
    {
        int bitmap_index = index / heap->blocks_in_word;
        int bitmap_offset = index % heap->blocks_in_word;

        unsigned long mask_a = (unsigned long)1 << (heap->block_bits * (bitmap_offset + 1) - 1 - bit);
        unsigned long mask_b = (unsigned long)1 << (heap->block_bits * ((bitmap_offset ^ 1) + 1) - 1 - bit);

        heap->bitmap[bitmap_index] |= mask_a;
        heap->bitmap[bitmap_index] |= mask_b;
    } 
}

/*
 * Clears bit `index` and its buddy in the heap's bitmap, marking the underlying
 * blocks as reserved. Used when merging two child blocks into a single parent 
 * block.
 */
static inline void clear_pair(bitmap_heap_descriptor_t *heap, int index, int bit)
{
    if(bit < heap->block_bits)
    {
        int bitmap_index = index / heap->blocks_in_word;
        int bitmap_offset = index % heap->blocks_in_word;

        unsigned long mask_a = ~((unsigned long)1 << (heap->block_bits * (bitmap_offset + 1) - 1 - bit));
        unsigned long mask_b = ~((unsigned long)1 << (heap->block_bits * ((bitmap_offset ^ 1) + 1) - 1 - bit));

        heap->bitmap[bitmap_index] &= mask_a;
        heap->bitmap[bitmap_index] &= mask_b;
    }
}

/*
 * Computes the location in the cache that `index` would be stored at, if it
 * were cached.
 */
static inline int cache_location_from_index(bitmap_heap_descriptor_t *heap, int index)
{
    return llog2(index + 1) - llog2(heap->blocks_in_word) - 1;
}

/*
 * Computes the location in the cache that blocks at the indicated height are
 * cached at.
 */
static inline int cache_location_from_height(bitmap_heap_descriptor_t *heap, int height)
{
    return heap->height - height - llog2(heap->blocks_in_word);
}

/*
 * Returns a cached value from the desired location, removing that value from
 * the cache.
 */
static inline int check_cache(bitmap_heap_descriptor_t *heap, int height)
{
    if(heap->cache != (unsigned long*)0)
    {
        int loc = cache_location_from_height(heap, height);
        unsigned long n = heap->cache[loc];
        heap->cache[loc] = 0;
        return n;
    }
    else
    {
        return 0;
    }
}

/*
 * If space in the cache exists, stores the provided index at the appropriate
 * location.
 */
static inline void store_cache(bitmap_heap_descriptor_t *heap, int index)
{
    if(heap->cache != (unsigned long*)0)
    {
        int level = cache_location_from_index(heap, index);
        if(level >= 0 && heap->cache[level] == 0)
        {
            heap->cache[level] = index;
        }
    }
    else
    {
        return 0;
    }
}

/*
 * If `index` is stored in the cache, remove it. Otherwise, leave the cache
 * as is.
 */
static inline void uncache(bitmap_heap_descriptor_t *heap, int index)
{
    if(heap->cache != (unsigned long*)0)
    {
        int level = cache_location_from_index(heap, index);
        if(level >= 0 && heap->cache[level] == index)
        {
            heap->cache[level] = 0;
        }
    }
    else
    {
        return 0;
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
        clear_bit(heap, index, BIT_AVAIL);
        index *= 2;
        set_pair(heap, index, BIT_AVAIL);
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
    while(index > 1 && test_bit(heap, index ^ 1, BIT_AVAIL))
    {
        uncache(heap, index ^ 1);
        clear_pair(heap, index, BIT_AVAIL);
        index /= 2;
        set_bit(heap, index, BIT_AVAIL);
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
    else if (height <= heap->height - ilog2(heap->blocks_in_word))
    {
        unsigned long cached_index = check_cache(heap, height);
        if(cached_index)
        {
            return cached_index;
        }
        unsigned long start = (1 << (heap->height - height)) / heap->blocks_in_word;
        unsigned long end = ((1 << (heap->height - height + 1)) / heap->blocks_in_word);
        for (int index = start; index < end; index++)
        {
            unsigned long avail_mask = heap->bitmap[index] & heap->mask;
            if (avail_mask != 0)
            {
                return heap->blocks_in_word * index + (__builtin_ctzl(avail_mask) / heap->block_bits);
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
        int bitmask_index = heap->height - height + llog2(heap->block_bits);
        if (heap->bitmap[0] & bitmasks[bitmask_index] & heap->mask)
        {
            return __builtin_ctzl(heap->bitmap[0] & bitmasks[bitmask_index] & heap->mask) / heap->block_bits;
        }
    }
    return split_block(heap, find_free_region(heap, height + 1));
}

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

static unsigned long generate_mask(unsigned long block_bits)
{
    unsigned long blocks_in_word = 8 * sizeof(unsigned long) / block_bits;
    unsigned long mask = 0;
    for(unsigned long i = 1; i <= blocks_in_word; i++)
    {
        mask |= 1UL << (i * block_bits - 1UL);
    }
    return mask;
}

static int construct_heap_desc(bitmap_heap_descriptor_t *heap, const memory_map_t *map)
{
    if(heap->block_bits == 0 || heap->block_bits > (8 * sizeof(*heap->bitmap)))
    {
        return -1;
    }
    else if(heap->block_size == 0)
    {
        return -1;
    }
    else if((1 << llog2(heap->block_bits)) != heap->block_bits)
    {
        return -1;
    }

    unsigned long memory_size = compute_memory_size(map);
    heap->blocks_in_word = 8 * sizeof(*heap->bitmap) / heap->block_bits;
    heap->bitmap_size = heap->block_bits * (memory_size / heap->block_size) / 4;
    heap->bitmap_size = 1 << llog2(heap->bitmap_size);
    heap->height = llog2(memory_size / heap->block_size);
    heap->free_block_count = 0;
    heap->mask = generate_mask(heap->block_bits);

    if(heap->bitmap_size <= sizeof(*heap->bitmap))
    {
        return -1;
    }
    else if(heap->bitmap_size >= memory_size && heap->bitmap == (unsigned long*)0)
    {
        return -1;
    }
    return 0;
}

static void initialize_bitmap(bitmap_heap_descriptor_t *heap, const memory_map_t *map)
{
    clear_bitmap(heap);
    for(int i = 0; i < map->size; i++)
    {
        if(map->array[i].type != M_AVAILABLE)
        {
            continue;
        }

        unsigned long location = (map->array[i].location + heap->block_size - 1);// & ~(heap->block_size - 1);
        location -= location % heap->block_size;
        unsigned long region_end = map->array[i].location + map->array[i].size;

        while(location + heap->block_size <= region_end)
        {
            int bit_offset = (location / heap->block_size) % heap->blocks_in_word;
            int bitmap_index = ((1UL << (heap->height - 0)) / heap->blocks_in_word) + (location / heap->block_size) / heap->blocks_in_word;
            unsigned long chunk_size = (heap->blocks_in_word - bit_offset) * heap->block_size;
            if(bit_offset == 0 && (region_end - location) >= chunk_size)
            {
                // Set all bits in the word
                heap->bitmap[bitmap_index] = heap->mask & ~0;
                heap->free_block_count += heap->blocks_in_word;
            }
            else if(bit_offset == 0)
            {
                // Set the first 'count' bits
                int count = (region_end - location) / heap->block_size;
                heap->bitmap[bitmap_index] |= heap->mask & ((1UL << (heap->block_bits * count)) - 1);
                heap->free_block_count += count;
            }
            else if((region_end - location) >= chunk_size)
            {
                // Set all bits starting at 'bit_offset'
                heap->bitmap[bitmap_index] |= heap->mask & ~((1UL << (heap->block_bits * bit_offset)) - 1);
                heap->free_block_count += heap->blocks_in_word - bit_offset;
            }
            else
            {
                // Set all bits starting at 'bit_offset' up to 'count'
                int count = (region_end - location) / heap->block_size;
                heap->bitmap[bitmap_index] |= heap->mask & ((1UL << (heap->block_bits * count)) - 1) & ~((1UL << (heap->block_bits * bit_offset)) - 1);
                heap->free_block_count += count - bit_offset;
            }

            // Merge 'buddies' when both available
            unsigned long mask = ((1UL << (2 * heap->block_bits)) - 1) & heap->mask;
            for(int j = 0; j < heap->blocks_in_word / 2; j++)
            {
                if((heap->bitmap[bitmap_index] & mask) == mask)
                {
                    merge_block(heap, bitmap_index * heap->blocks_in_word + j * 2);
                }
                mask <<= 2 * heap->block_bits;
            }
            
            location += chunk_size;
        }
    }
}

unsigned long reserve_region(bitmap_heap_descriptor_t *heap, unsigned long size)
{
    int height = llog2((size - 1) / heap->block_size + 1);
    int index = find_free_region(heap, height);
    if(index)
    {
        clear_bit(heap, index, BIT_AVAIL);
        set_bit(heap, index, BIT_USED);
        heap->free_block_count -= 1 << height;
        return heap->offset + (heap->block_size << height) * (index - ((unsigned long)1 << (heap->height - height)));
    }
    else
    {
        return NOMEM;
    }
}

void free_region(bitmap_heap_descriptor_t *heap, unsigned long location, unsigned long size)
{
    location -= heap->offset;
    int height = llog2(size / heap->block_size);
    int index = (location / (heap->block_size * ((unsigned long)1 << height))) + (1 << (heap->height - height));
    while(!test_bit(heap, index, BIT_USED))
    {
        height++;
        index /= 2;
    }
    set_bit(heap, index, BIT_AVAIL);
    clear_bit(heap, index, BIT_USED);
    index = merge_block(heap, index);
    store_cache(heap, index);
    heap->free_block_count += 1 << height;
}

unsigned long bitmap_size(const memory_map_t *map, unsigned long block_size, unsigned long block_bits)
{
    return 1UL << llog2((block_bits * compute_memory_size(map) / block_size) / 4);
}

int initialize_heap(bitmap_heap_descriptor_t *heap, memory_map_t *map, int (*mmap)(void *location, unsigned long size))
{
    if(construct_heap_desc(heap, map))
    {
        return -1;
    }

    if(heap->bitmap == (unsigned long*)0)
    {
        int map_index = 0;
        while(map->array[map_index].size < heap->bitmap_size)
        {
            map_index++;
            if(map_index >= map->size)
            {
                return -1;
            }
        }

        heap->bitmap = (unsigned long*)(heap->offset + map->array[map_index].location);
        memmap_insert_region(map, map->array[map_index].location, heap->bitmap_size, M_UNAVAILABLE);
        if(mmap && mmap(heap->bitmap, heap->bitmap_size))
        {
            return -1;
        }
    }
    
    initialize_bitmap(heap, map);
    clear_cache(heap);
    return 0;
}
