#ifndef _LIBMALLOC_COMMON_H
#define _LIBMALLOC_COMMON_H

/*
 * '0' may possibly refer to a valid memory location for the heap to reserve in
 * some circumstances; as a result, NULL is an inappropriate value to use to
 * represent the failure to allocate space for the purposes of this library.
 * NOMEM shall be returned by any malloc-like function upon failure, rather than
 * NULL.
 */
#define NOMEM ~0

#endif