/*-------------------------------------------------------------------------
 *
 * deform_bench.c
 *
 * for benchmarking tuple deformation routines
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <time.h>
#include <sys/time.h>

#include "access/heapam.h"
#include "access/relscan.h"
#include "catalog/pg_am_d.h"
#include "catalog/pg_type_d.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/bitmapset.h"
#include "utils/array.h"
#include "utils/arrayaccess.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(deform_bench);
PG_FUNCTION_INFO_V1(deform_bench_select);

Datum
deform_bench(PG_FUNCTION_ARGS)
{
	Oid			tableoid = PG_GETARG_OID(0);
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(1);
	TableScanDesc scan;
	Relation	rel;
	TupleDesc	tupdesc;
	TupleTableSlot *slot;
	Datum	   *elem_datums = NULL;
	bool	   *elem_nulls = NULL;
	int			elem_count;
	int		   *attnums;
	clock_t		start,
				end;

	rel = relation_open(tableoid, AccessShareLock);

	if (rel->rd_rel->relam != HEAP_TABLE_AM_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("only heap AM is supported")));

	tupdesc = RelationGetDescr(rel);
	slot = MakeTupleTableSlot(tupdesc,
							  &TTSOpsBufferHeapTuple,
							  TTS_FLAG_OBEYS_NOT_NULL_CONSTRAINTS);
	scan = table_beginscan_strat(rel, GetActiveSnapshot(), 0, NULL, true, false);

	/*
	 * The array is used to allow callers to define how many atts to deform.
	 * e.g: '{1,10}'::int[] would deform attnum=1, then in a 2nd pass deform
	 * the remainder up to attnum=10.  Passing an element as NULL means all
	 * attnums.  This allows simulation of incremental deformation.  Generally
	 * if you're passing an array with more than 1 element, then the array
	 * should be in ascending order.  Doing something like '{10,1}' would mean
	 * we've already deformed 10 attributes and on the 2nd pass there's
	 * nothing to do since attnum=1 was already deformed in the first pass.
	 *
	 * You'll get an ERROR if you pass a number higher than the number of
	 * attributes in the table.
	 */
	deconstruct_array(array,
					  INT4OID,
					  sizeof(int32),
					  true,
					  'i',
					  &elem_datums,
					  &elem_nulls,
					  &elem_count);

	attnums = palloc_array(int, elem_count);

	for (int i = 0; i < elem_count; i++)
	{
		/* Make a NULL element mean all attributes */
		if (elem_nulls[i])
			attnums[i] = tupdesc->natts;
		else
			attnums[i] = DatumGetInt32(elem_datums[i]);
	}

	start = clock();

	while (heap_getnextslot(scan, ForwardScanDirection, slot))
	{
		CHECK_FOR_INTERRUPTS();

		/* Deform in stages according to the attnums array */
		for (int i = 0; i < elem_count; i++)
			slot_getsomeattrs(slot, attnums[i]);
	}

	end = clock();

	ExecDropSingleTupleTableSlot(slot);
	table_endscan(scan);
	relation_close(rel, AccessShareLock);


	/* Returns the number of milliseconds to run the test */
	PG_RETURN_FLOAT8((double) (end - start) / (CLOCKS_PER_SEC / 1000));
}

Datum
deform_bench_select(PG_FUNCTION_ARGS)
{
	Oid			tableoid = PG_GETARG_OID(0);
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(1);
	TableScanDesc scan;
	Relation	rel;
	TupleDesc	tupdesc;
	TupleTableSlot *slot;
	Datum	   *elem_datums = NULL;
	bool	   *elem_nulls = NULL;
	int			elem_count;
	int			i;
	int			attnum;
	int			last_attnum;
	AttrNumber *attnums;
	Bitmapset  *attrs = NULL;
	clock_t		start,
				end;

	rel = relation_open(tableoid, AccessShareLock);

	if (rel->rd_rel->relam != HEAP_TABLE_AM_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("only heap AM is supported")));

	tupdesc = RelationGetDescr(rel);
	slot = MakeTupleTableSlot(tupdesc,
							  &TTSOpsBufferHeapTuple,
							  TTS_FLAG_OBEYS_NOT_NULL_CONSTRAINTS);
	scan = table_beginscan_strat(rel, GetActiveSnapshot(), 0, NULL, true, false);

	/*
	 * The array is used to allow callers to specify which attributes to
	 * deform.  E.g: '{1,10}'::int[] would deform only attnum=1 and attnum=10.
	 *
	 * You'll get an ERROR if you pass an attnum that does not exist.  NULL
	 * elements are ignored.
	 */
	deconstruct_array(array,
					  INT4OID,
					  sizeof(int32),
					  true,
					  'i',
					  &elem_datums,
					  &elem_nulls,
					  &elem_count);

	for (i = 0; i < elem_count; i++)
	{
		/* Make a NULL element mean all attributes */
		if (elem_nulls[i])
			continue;

		attnum = DatumGetInt32(elem_datums[i]);

		if (attnum <= 0)
			elog(ERROR, "only user attributes can be deformed by deform_bench_select");
		if (attnum >= 0xffff)
			elog(ERROR, "invalid attnum %d", attnum);

		attrs = bms_add_member(attrs, attnum - 1);
	}

	attnums = palloc_array(AttrNumber, bms_num_members(attrs));

	attnum = -1;
	i = 0;
	while ((attnum = bms_next_member(attrs, attnum)) >= 0)
		attnums[i++] = (AttrNumber) attnum;

	last_attnum = attnums[i - 1];

	start = clock();

	while (heap_getnextslot(scan, ForwardScanDirection, slot))
	{
		AttrNumber attnum_map = 0;
		CHECK_FOR_INTERRUPTS();

		/* Pass in a faked up attnum_map. tts_nvalid will always be 0 */
		slot_selectattrs(slot, last_attnum, attnums, &attnum_map);
	}

	end = clock();

	ExecDropSingleTupleTableSlot(slot);
	table_endscan(scan);
	relation_close(rel, AccessShareLock);


	/* Returns the number of milliseconds to run the test */
	PG_RETURN_FLOAT8((double) (end - start) / (CLOCKS_PER_SEC / 1000));
}
