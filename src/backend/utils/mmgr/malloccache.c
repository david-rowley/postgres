/*-------------------------------------------------------------------------
 *
 * malloccache.c
 *	  A wrapper around malloc/free for predefined sized chunks of memory
 *
 * Portions Copyright (c) 2023, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/mmgr/malloccache.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "lib/ilist.h"
#include "utils/memutils.h"

typedef struct cached_allocation
{
	const size_t alloc_size;	/* size of allocations stored in list */
	const uint32 keep_allocs;	/* number of allocations to keep of this size */
	uint32 n_allocs;			/* number of allocations currently stored */
	void *alloc_head;			/* pointer to first allocation */
} cached_allocation;

#define ALLOCATION_CACHE_ELEMENTS 4

struct cached_allocation allocation_cache[ALLOCATION_CACHE_ELEMENTS] = {
	{ ALLOCSET_SMALL_INITSIZE, 100, 0, NULL },	/* 100 kB max */
	{ ALLOCSET_DEFAULT_INITSIZE, 100, 0, NULL }, /* 800 kB max */
	{ ALLOCSET_DEFAULT_INITSIZE * 2, 16, 0, NULL }, /* 256 kB max */
	{ 27400, 4, 0, NULL },
};

void *
malloccache_fetch(size_t size)
{
	for (int i = 0; i < ALLOCATION_CACHE_ELEMENTS; i++)
	{
		cached_allocation *cache = &allocation_cache[i];

		if (cache->alloc_size != size)
			continue;

		if (cache->alloc_head != NULL)
		{
			void *ptr = cache->alloc_head;

			Assert(cache->n_allocs > 0);

			cache->alloc_head = *(void **) cache->alloc_head;
			cache->n_allocs--;
#ifdef MALLOCCACHE_DEBUG
			fprintf(stderr, "found cached %zu n_allocs = %u\n", size, cache->n_allocs);
#endif
			return ptr;
		}

		break;
	}

#ifdef MALLOCCACHE_DEBUG
	fprintf(stderr, "malloc %zu\n", size);
#endif
	return malloc(size);
}

void
malloccache_release(void *ptr, size_t size)
{
	for (int i = 0; i < ALLOCATION_CACHE_ELEMENTS; i++)
	{
		cached_allocation *cache = &allocation_cache[i];

		if (cache->alloc_size != size)
			continue;

		if (cache->n_allocs < cache->keep_allocs)
		{
			*(void **) ptr = cache->alloc_head;
			cache->alloc_head = ptr;
			cache->n_allocs++;

#ifdef MALLOCCACHE_DEBUG
			fprintf(stderr, "cached %zu n_allocs = %u\n", size, cache->n_allocs);
#endif
			return;
		}
	}

	/*
	 * Memory is not a size we care about or we have enough of that size
	 * already in the cache.
	 */
#ifdef MALLOCCACHE_DEBUG
	fprintf(stderr, "freed %zu\n", size);
#endif
	free(ptr);
}
