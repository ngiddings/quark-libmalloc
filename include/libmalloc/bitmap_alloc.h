#ifndef _LIBMALLOC_BUDDYALLOC_H
#define _LIBMALLOC_BUDDYALLOC_H

#include "memmap.h"
#include "common.h"

/**
 * @brief 
 * 
 */
typedef struct bitmap_heap_descriptor_t
{
    /**
     * @brief The underlying bitmap representing the availability of chunks of
     * physical memory.
     *
     */
    unsigned long *bitmap;

    /**
     * @brief Stores a list of available blocks of memory to speed up allocation.
     * 
     */
    unsigned long *cache;

    /**
     * @brief The size of the bitmap in bytes.
     *
     */
    unsigned long bitmap_size;

    /**
     * @brief The size of the array pointed to by `cache`.
     * 
     */
    unsigned long cache_capacity;

    /**
     * @brief The size in bytes of the smallest unit of allocation.
     *
     * This value should either be the size of a page on the host system, or
     * possibly some number of pages.
     *
     */
    unsigned long block_size;

    /**
     * @brief The height of the binary tree representation of the heap's memory
     * map.
     *
     */
    unsigned long height;

    /**
     * @brief The number of available blocks of memory.
     *
     * Due to memory fragmentation, it may not be possible to allocate all
     * available memory at once.
     *
     */
    unsigned long free_block_count;

    /**
     * @brief Bitmask used to isolate only those bits which indicate the
     * availability of a block. Useful when several bits are used to represent
     * the state of a single block. See `block_bits`.
     * 
     */
    unsigned long mask;

    /**
     * @brief The number of bits required to represent the state of each block.
     * 
     * If the value is greater than 1, the most significant bit describing each 
     * block shall indicate its availability. Less significant bits shall 
     * represent other aspects of the block's state. The number of trailing 
     * zeroes in `mask` shall be used to calculate this quantity.
     * 
     * This qualtity is assumed to be a power of 2, and less than or equal to
     * the number of bits in an unsigned long. Therefore, acceptable values for
     * this quantity are as follows: 1, 2, 4, 8, 16, 32[, 64].
     * 
     */
    unsigned long block_bits;

    /**
     * @brief The number of blocks described by a single unsigned long contained
     * in `bitmap`. Equal to (8 * sizeof(unsigned long)) / block_bits.
     * 
     */
    unsigned long blocks_in_word;

    /**
     * @brief Memory will be allocated relative to this location.
     * 
     */
    unsigned long offset;

    /**
     * @brief Function pointer which, if not null, will be called whenever
     * a region on the heap is allocated for the first time.
     */
    int (*mmap)(void *location, unsigned long size)

} bitmap_heap_descriptor_t;

/**
 * @brief Reads the given bit from the metadata of the memory block at 
 * `location`. 
 * 
 * `location` must have be a location previously returned by `reserve_region`, 
 * and not have been subsequently freed. `bit` must be less than the 
 * `block_bits` field in `heap`.
 * 
 * @returns nonzero if `bit` is set, 0 otherwise. If `bit` is not less than the
 * `block_bits` field in `heap`, returns nonzero.
 */
unsigned long read_bit(bitmap_heap_descriptor_t *heap, unsigned long location,
    unsigned long bit);

/**
 * @brief Writes to the given bit in the metadata of the memory block at 
 * `location`. 
 * 
 * `location` must have be a location previously returned by `reserve_region`, 
 * and not have been subsequently freed. `bit` must be less than the 
 * `block_bits` field in `heap`.
 * 
 * @returns `value` if bit was written to, nonzero otherwise
 */
int write_bit(bitmap_heap_descriptor_t *heap, unsigned long location,
    unsigned long bit, int value);

/**
 * @brief Reserves a region of memory within the heap containing at least `size`
 * bytes.
 * 
 * If `size` is not a power of two times the block size, it will be rounded up
 * to the smallest possible number which satisfies this condition. After 
 * 
 * @param heap 
 * @param size 
 * @return unsigned long 
 */
unsigned long reserve_region(bitmap_heap_descriptor_t *heap, 
    unsigned long size);

/**
 * @brief Marks the region of memory indicated by `location` and `size` as
 * available to be allocated.
 * 
 * `location` must have been previously returned by `reserve_region`. `size`
 * is expected to be a power of two times the block size.
 * 
 * @param heap 
 * @param location 
 * @param size 
 */
void free_region(bitmap_heap_descriptor_t *heap, unsigned long location, 
    unsigned long size);

/**
 * @brief Computes the amount of space required to store the heap's internal
 * bitmaps.
 * 
 * @param map A pointer to the structure providing an initial memory layout
 * @param block_size The minimum unit of allocation
 * @return unsigned long 
 */
unsigned long bitmap_size(const memory_map_t *map, unsigned long block_size, unsigned long block_bits);

/**
 * @brief Builds the heap's internal structures according to the memory
 * layout provided in `map`. All locations in `map` are relative to the `offset`
 * field in `heap`.
 * 
 * A callback function `mmap` may be provided, which will be used to map the
 * space required by the heap to store its internal bitmaps. If `mmap` is NULL,
 * this function assumes that all space within the heap is already mapped.
 * 
 * There are several requirements for the initial state of the `heap` structure:
 * 
 * - The `bitmap` field may point to a pre-allocated region of memory which will
 * be used to store the heap's internal bitmap. If this field is NULL, part of
 * the heap will be used to store the bitmap, and `mmap`, if not NULL, will be
 * called to map that region of memory.
 * 
 * - The `cache` field may point to an array of unsigned longs of sufficient 
 * size, which will be used to speed up memory allocation. If this field is
 * NULL, caching will not be performed.
 * 
 * - The `cache_capacity` field must be set to the size of the array pointed to
 * by `cache`.
 * 
 * - The `block_size` field must be set to the desired smallest unit of 
 * allocation.
 * 
 * - The `block_bits` field must be set to the number of bits required to store
 * a block's metadata.
 * 
 * - The `offset` field must be set to the first location to allocate memory
 * from. Locations in `map` will be interpreted as relative to `offset`.
 * 
 * @param heap A pointer to the structure describing the heap
 * @param map A pointer to the structure providing an initial memory layout
 * space
 * @return int 0 upon success, nonzero upon failure.
 */
int initialize_heap(bitmap_heap_descriptor_t *heap, memory_map_t *map);

#endif