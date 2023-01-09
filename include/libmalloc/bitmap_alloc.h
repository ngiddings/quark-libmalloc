#ifndef _LIBMALLOC_BUDDYALLOC_H
#define _LIBMALLOC_BUDDYALLOC_H

#include "memmap.h"
#include "common.h"

/**
 * @brief 
 * 
 */
typedef struct
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

} bitmap_heap_descriptor_t;

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
unsigned long reserve_region(bitmap_heap_descriptor_t *heap, unsigned long size);

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
unsigned long bitmap_size(const memory_map_t *map, unsigned long block_size);

/**
 * @brief Builds the heap's internal structures according to the memory
 * layout provided in `map`. Assumes that the layout in `map` refers to the
 * caller's virtual address space, and utilizes some of the memory marked as
 * 'available' to store the heap's internal structures.
 * 
 * A callback function `mmap` may be provided, which will be used to map the
 * space required by the heap to store its internal bitmaps. If `mmap` is NULL,
 * this function assumes that all space within the heap is already mapped.
 * 
 * There are several requirements for the initial state of the `heap` structure:
 * 
 * - The `cache` field must be defined, and point to an array of unsigned longs
 * of sufficient size.
 * 
 * - The `block_size` field must be set to the desired smallest unit of allocation.
 * 
 * @param heap A pointer to the structure describing the heap
 * @param map A pointer to the structure providing an initial memory layout
 * @param mmap A callback function used to map memory in the virtual address space
 * @return int 
 */
int initialize_virtual_heap(bitmap_heap_descriptor_t *heap, const memory_map_t *map, 
    int (*mmap)(void *location, unsigned long size));

/**
 * @brief Builds the heap's internal structures according to the memory
 * layout provided in `map`. Assumes that physical memory space is being alocated,
 * and therefore does not make assumptions about the caller's address space or
 * attempt to utilize the memory inside the heap. The caller is responsible for
 * providing space to store the heap's internal structures.
 * 
 * There are several requirements for the initial state of the `heap` structure:
 * 
 * - The `bitmap` field must be defined, and sufficient memory reserved at that
 * location to contain the heap's bitmap.
 * 
 * - The `cache` field must be defined, and point to an array of unsigned longs
 * of sufficient size.
 * 
 * - The `cache_capacity` field must be set to the size of the array pointed to
 * by `cache`
 * 
 * - The `block_size` field must be set to the desired smallest unit of allocation.
 * 
 * @param heap A pointer to the structure describing the heap
 * @param map A pointer to the structure providing an initial memory layout
 * @return int 0 upon success; nonzero upon failure
 */
int initialize_physical_heap(bitmap_heap_descriptor_t *heap, const memory_map_t *map);

#endif