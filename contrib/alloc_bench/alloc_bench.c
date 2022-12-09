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

PG_FUNCTION_INFO_V1(alloc_bench_random);
PG_FUNCTION_INFO_V1(alloc_bench_fifo);
PG_FUNCTION_INFO_V1(alloc_bench_lifo);

typedef struct Chunk {
	int		random;
	void   *ptr;
} Chunk;

static int
chunk_index_cmp(const void *a, const void *b)
{
	Chunk *ca = (Chunk *) a;
	Chunk *cb = (Chunk *) b;

	if (ca->random < cb->random)
		return -1;
	else if (ca->random > cb->random)
		return 1;

	return 0;
}

Datum
alloc_bench_random(PG_FUNCTION_ARGS)
{
	MemoryContext	cxt,
					oldcxt;
	Chunk		   *chunks;
	int64			i, j;
	char		   *context_type;
	text		   *context_type_text = PG_GETARG_TEXT_PP(0);
	int64			nallocs = PG_GETARG_INT64(1);
	int64			blockSize = PG_GETARG_INT64(2);
	int64			chunkSize = PG_GETARG_INT64(3);

	int				nloops = PG_GETARG_INT32(4);
	int				free_cnt = PG_GETARG_INT32(5);
	int				alloc_cnt = PG_GETARG_INT32(6);

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

	int				maxchunks;

	context_type = text_to_cstring(context_type_text);

	maxchunks = nallocs + nloops * Max(0, alloc_cnt - free_cnt);

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
		elog(ERROR, "context_type must be \"generation\", \"aset\" or \"slab\"");


	chunks = (Chunk *) palloc(maxchunks * sizeof(Chunk));

	/* allocate the chunks in random order */
	oldcxt = MemoryContextSwitchTo(cxt);

	gettimeofday(&start_time, NULL);

	for (i = 0; i < nallocs; i++)
		chunks[i].ptr = palloc(chunkSize);

	gettimeofday(&end_time, NULL);

	alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
				  (end_time.tv_usec - start_time.tv_usec);

	MemoryContextSwitchTo(oldcxt);

	mem_allocated = MemoryContextMemAllocated(cxt, true);

	/* do the requested number of free/alloc loops */
	for (j = 0; j < nloops; j++)
	{
		CHECK_FOR_INTERRUPTS();

		/* randomize the indexes */
		for (i = 0; i < nallocs; i++)
			chunks[i].random = random();

		qsort(chunks, nallocs, sizeof(Chunk), chunk_index_cmp);

		oldcxt = MemoryContextSwitchTo(cxt);

		gettimeofday(&start_time, NULL);

		/* free the first free_cnt chunks */
		for (i = 0; i < Min(nallocs, free_cnt); i++)
			pfree(chunks[i].ptr);

		gettimeofday(&end_time, NULL);

		nallocs -= Min(nallocs, free_cnt);

		free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					 (end_time.tv_usec - start_time.tv_usec);

		memmove(chunks, &chunks[free_cnt], nallocs * sizeof(Chunk));


		/* allocate alloc_cnt chunks at the end */
		gettimeofday(&start_time, NULL);

		/* free the first free_cnt chunks */
		for (i = 0; i < alloc_cnt; i++)
			chunks[nallocs + i].ptr = palloc(chunkSize);

		gettimeofday(&end_time, NULL);

		nallocs += alloc_cnt;

		alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					  (end_time.tv_usec - start_time.tv_usec);

		MemoryContextSwitchTo(oldcxt);

		mem_allocated = Max(mem_allocated, MemoryContextMemAllocated(cxt, true));
	}

	/* release the chunks in random order */
	for (i = 0; i < nallocs; i++)
		chunks[i].random = random();

	qsort(chunks, nallocs, sizeof(Chunk), chunk_index_cmp);

	gettimeofday(&start_time, NULL);

	for (i = 0; i < nallocs; i++)
		pfree(chunks[i].ptr);

	gettimeofday(&end_time, NULL);

	free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
				 (end_time.tv_usec - start_time.tv_usec);

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

Datum
alloc_bench_fifo(PG_FUNCTION_ARGS)
{
	MemoryContext	cxt,
					oldcxt;
	Chunk		   *chunks;
	int64			i, j;
	char		   *context_type;
	text		   *context_type_text = PG_GETARG_TEXT_PP(0);
	int64			nallocs = PG_GETARG_INT64(1);
	int64			blockSize = PG_GETARG_INT64(2);
	int64			chunkSize = PG_GETARG_INT64(3);

	int				nloops = PG_GETARG_INT32(4);
	int				free_cnt = PG_GETARG_INT32(5);
	int				alloc_cnt = PG_GETARG_INT32(6);

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

	int				maxchunks;

	context_type = text_to_cstring(context_type_text);

	maxchunks = nallocs + nloops * Max(0, alloc_cnt - free_cnt);

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
		elog(ERROR, "context_type must be \"generation\", \"aset\" or \"slab\"");

	chunks = (Chunk *) palloc(maxchunks * sizeof(Chunk));

	oldcxt = MemoryContextSwitchTo(cxt);

	gettimeofday(&start_time, NULL);

	for (i = 0; i < nallocs; i++)
		chunks[i].ptr = palloc(chunkSize);

	gettimeofday(&end_time, NULL);

	alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
				  (end_time.tv_usec - start_time.tv_usec);

	MemoryContextSwitchTo(oldcxt);

	mem_allocated = MemoryContextMemAllocated(cxt, true);


	/* do the requested number of free/alloc loops */
	for (j = 0; j < nloops; j++)
	{
		CHECK_FOR_INTERRUPTS();

		oldcxt = MemoryContextSwitchTo(cxt);

		gettimeofday(&start_time, NULL);

		/* free the first free_cnt chunks */
		for (i = 0; i < Min(nallocs, free_cnt); i++)
			pfree(chunks[i].ptr);

		gettimeofday(&end_time, NULL);

		nallocs -= Min(nallocs, free_cnt);

		free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					 (end_time.tv_usec - start_time.tv_usec);

		memmove(chunks, &chunks[free_cnt], nallocs * sizeof(Chunk));

		/* allocate alloc_cnt chunks at the end */
		gettimeofday(&start_time, NULL);

		/* free the first free_cnt chunks */
		for (i = 0; i < alloc_cnt; i++)
			chunks[nallocs + i].ptr = palloc(chunkSize);

		gettimeofday(&end_time, NULL);

		nallocs += alloc_cnt;

		alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					  (end_time.tv_usec - start_time.tv_usec);

		MemoryContextSwitchTo(oldcxt);

		mem_allocated = Max(mem_allocated, MemoryContextMemAllocated(cxt, true));
	}


	gettimeofday(&start_time, NULL);

	for (i = 0; i < nallocs; i++)
		pfree(chunks[i].ptr);

	gettimeofday(&end_time, NULL);

	free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
				 (end_time.tv_usec - start_time.tv_usec);

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

Datum
alloc_bench_lifo(PG_FUNCTION_ARGS)
{
	MemoryContext	cxt,
					oldcxt;
	Chunk		  *chunks;
	int64			i, j;
	char		   *context_type;
	text		   *context_type_text = PG_GETARG_TEXT_PP(0);
	int64			nallocs = PG_GETARG_INT64(1);
	int64			blockSize = PG_GETARG_INT64(2);
	int64			chunkSize = PG_GETARG_INT64(3);

	int				nloops = PG_GETARG_INT32(4);
	int				free_cnt = PG_GETARG_INT32(5);
	int				alloc_cnt = PG_GETARG_INT32(6);

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

	int				maxchunks;

	context_type = text_to_cstring(context_type_text);

	maxchunks = nallocs + nloops * Max(0, alloc_cnt - free_cnt);

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
		elog(ERROR, "context_type must be \"generation\", \"aset\" or \"slab\"");

	chunks = (Chunk *) palloc(maxchunks * sizeof(Chunk));

	oldcxt = MemoryContextSwitchTo(cxt);

	/* palloc benchmark */
	gettimeofday(&start_time, NULL);

	for (i = 0; i < nallocs; i++)
		chunks[i].ptr = palloc(chunkSize);

	gettimeofday(&end_time, NULL);

	alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
				  (end_time.tv_usec - start_time.tv_usec);

	MemoryContextSwitchTo(oldcxt);

	mem_allocated = MemoryContextMemAllocated(cxt, true);


	/* do the requested number of free/alloc loops */
	for (j = 0; j < nloops; j++)
	{
		CHECK_FOR_INTERRUPTS();

		oldcxt = MemoryContextSwitchTo(cxt);

		gettimeofday(&start_time, NULL);

		/* free the first free_cnt chunks */
		for (i = 1; i <= Min(nallocs, free_cnt); i++)
			pfree(chunks[nallocs - i].ptr);

		gettimeofday(&end_time, NULL);

		nallocs -= Min(nallocs, free_cnt);

		free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					 (end_time.tv_usec - start_time.tv_usec);

		/* allocate alloc_cnt chunks at the end */
		gettimeofday(&start_time, NULL);

		/* free the first free_cnt chunks */
		for (i = 0; i < alloc_cnt; i++)
			chunks[nallocs + i].ptr = palloc(chunkSize);

		gettimeofday(&end_time, NULL);

		nallocs += alloc_cnt;

		alloc_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
					  (end_time.tv_usec - start_time.tv_usec);

		MemoryContextSwitchTo(oldcxt);

		mem_allocated = Max(mem_allocated, MemoryContextMemAllocated(cxt, true));
	}


	gettimeofday(&start_time, NULL);

	for (i = (nallocs - 1); i >= 0; i--)
		pfree(chunks[i].ptr);

	gettimeofday(&end_time, NULL);

	free_time += (end_time.tv_sec - start_time.tv_sec) * 1000000L +
				 (end_time.tv_usec - start_time.tv_usec);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	values[0] = Int64GetDatum(mem_allocated);
	values[1] = Int64GetDatum(alloc_time);
	values[2] = Int64GetDatum(free_time);

	memset(nulls, 0, sizeof(nulls));

	tuple = heap_form_tuple(tupdesc, values, nulls);
	result = HeapTupleGetDatum(tuple);

	MemoryContextDelete(cxt);

	PG_RETURN_DATUM(result);
}
