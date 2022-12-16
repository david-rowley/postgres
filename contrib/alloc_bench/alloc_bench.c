/*-------------------------------------------------------------------------
 *
 * alloc_bench.c
 *
 * helper functions to benchmark memory contexts with different workloads
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/time.h>

#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(alloc_bench);

typedef enum AllocationPattern {
	FIFO,
	LIFO,
	RANDOM
} AllocationPattern;

typedef struct Chunk {
	int64		index;
	void   *ptr;
} Chunk;

static int
chunk_index_cmp(const int64 *a, const int64 *b)
{
	int64 ia = *a;
	int64 ib = *b;

	if (ia < ib)
		return -1;
	else if (ia > ib)
		return 1;

	return 0;
}

Datum
alloc_bench(PG_FUNCTION_ARGS)
{
	MemoryContext	cxt,
					oldcxt;
	void		  **chunks;
	int64		   *indexes;
	int64			j;
	char		   *context_type;
	text		   *context_type_text = PG_GETARG_TEXT_PP(0);
	char		   *pattern_str;
	text		   *pattern_text = PG_GETARG_TEXT_PP(1);
	int64			nchunks = PG_GETARG_INT64(2);
	int64			blockSize = PG_GETARG_INT64(3);
	int64			chunkSize = PG_GETARG_INT64(4);

	int				nloops = PG_GETARG_INT32(5);

	struct timeval	start_time,
					end_time;
	int64			alloc_time = 0,
					free_time = 0;
	int64			mem_allocated;

	TupleDesc		tupdesc;
	Datum			result;
	HeapTuple		tuple;
	Datum			values[9];
	bool			nulls[9];
	AllocationPattern pattern;

	context_type = text_to_cstring(context_type_text);

	if (strcmp(context_type, "generation") == 0)
		cxt = GenerationContextCreate(CurrentMemoryContext,
									  "alloc_bench",
									  ALLOCSET_DEFAULT_SIZES);
	else if (strcmp(context_type, "aset") == 0)
		cxt = AllocSetContextCreate(CurrentMemoryContext,
									"alloc_bench",
									ALLOCSET_DEFAULT_SIZES);
	else if (strcmp(context_type, "slab") == 0)
		cxt = SlabContextCreate(CurrentMemoryContext,
								"alloc_bench",
								blockSize,
								chunkSize);
	else
		elog(ERROR, "%s is not a valid context type. context_type must be \"generation\", \"aset\" or \"slab\"",
			 context_type);

	pattern_str = text_to_cstring(pattern_text);

	if (strcmp(pattern_str, "fifo") == 0)
		pattern = FIFO;
	else if (strcmp(pattern_str, "lifo") == 0)
		pattern = LIFO;
	else if (strcmp(pattern_str, "random") == 0)
		pattern = RANDOM;
	else
		elog(ERROR, "%s is not a valid allocation pattern. Must be \"fifo\", \"lifo\", \"random\"",
			 pattern_str);

	chunks = (void **) palloc(nchunks * sizeof(void *));
	indexes = (int64 *) palloc(nchunks * sizeof(int64));

	/* set the indexes so according to the allocation pattern */
	switch (pattern)
	{
		case FIFO:
			for (int64 i = 0; i < nchunks; i++)
				indexes[i] = i;
			break;
		case LIFO:
			for (int64 i = 0; i < nchunks; i++)
				indexes[i] = nchunks - i;
			break;
		case RANDOM:
			for (int64 i = 0; i < nchunks; i++)
				indexes[i] = random();
			qsort(indexes, nchunks, sizeof(int64), int64_cmp);
			break;
	}

	mem_allocated = 0;

	oldcxt = MemoryContextSwitchTo(cxt);
	
	/* do the requested number of pfree/palloc loops */
	for (j = 0; j < nloops; j++)
	{
		CHECK_FOR_INTERRUPTS();

		gettimeofday(&start_time, NULL);

		for (int64 i = 0; i < nchunks; i++)
			chunks[i] = palloc(chunkSize);

		gettimeofday(&end_time, NULL);

		alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					  (end_time.tv_usec - start_time.tv_usec);

		gettimeofday(&start_time, NULL);

		for (int64 i = 0; i < nchunks; i++)
			pfree(chunks[indexes[i]]);

		gettimeofday(&end_time, NULL);

		free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					 (end_time.tv_usec - start_time.tv_usec);

		mem_allocated = Max(mem_allocated, MemoryContextMemAllocated(cxt, true));
	}

	MemoryContextSwitchTo(oldcxt);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	values[0] = Int64GetDatum(mem_allocated);
	values[1] = Int64GetDatum(alloc_time);
	values[2] = Int64GetDatum(free_time);

	memset(nulls, 0, sizeof(nulls));

	tuple = heap_form_tuple(tupdesc, values, nulls);
	result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}

