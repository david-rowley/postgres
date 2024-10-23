/*-------------------------------------------------------------------------
 *
 * nodeMergeUnique.c
 *	  routines to handle MergeUnique nodes.
 *
 * Portions Copyright (c) 2024, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeMergeUnique.c
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitMergeUnique		- initialize the MergeUnique node
 *		ExecMergeUnique			- retrieve the next tuple from the node
 *		ExecEndMergeUnique		- shut down the MergeUnique node
 *		ExecReScanMergeUnique	- rescan the MergeUnique node
 *
 *	 NOTES
 *		A MergeUnique node contains a list of one or more subplans.
 *		These are each expected to deliver tuples that are sorted according
 *		to a common sort key, similar to MergeUnique, however MergeUnique
 *		additionally eliminates consecutive duplicate rows.  This is cheaper
 *		to do than combining MergeUnique with a Unique node as there are cases
 *		where we already know the row from another subplan is a duplicate and
 *		needn't have another node type to determine something we already know
 *		is duplicated.
 *
 *		MergeUnique nodes don't make use of their left and right
 *		subtrees, rather they maintain a list of subplans so
 *		a typical MergeUnique node looks like this in the plan tree:
 *
 *				   ...
 *				   /
 *				MergeUnique---+------+------+--- nil
 *				/	\		  |		 |		|
 *			  nil	nil		 ...    ...    ...
 *								 subplans
 */

#include "postgres.h"

#include "executor/execPartition.h"
#include "executor/executor.h"
#include "executor/nodeMergeUnique.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"

/*
 * We have one slot for each item in the heap array.  We use SlotNumber
 * to store slot indexes.  This doesn't actually provide any formal
 * type-safety, but it makes the code more self-documenting.
 */
typedef int32 SlotNumber;

static TupleTableSlot *ExecMergeUnique(PlanState *pstate);
static int heap_compare_slots(Datum a, Datum b, void *arg);

/* ----------------------------------------------------------------
 *		ExecInitMergeUnique
 *
 *		Begin all of the subscans of the MergeUnique node.
 * ----------------------------------------------------------------
 */
MergeUniqueState *
ExecInitMergeUnique(MergeUnique *node, EState *estate, int eflags)
{
	MergeUniqueState *mergestate = makeNode(MergeUniqueState);
	PlanState **mergeplanstates;
	ListCell *lc;
	int nplans;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create new MergeUniqueState for our node
	 */
	mergestate->ps.plan = (Plan *) node;
	mergestate->ps.state = estate;
	mergestate->ps.ExecProcNode = ExecMergeUnique;

	nplans = list_length(node->mergeplans);
	mergeplanstates = (PlanState **) palloc(nplans * sizeof(PlanState *));
	mergestate->mergeplans = mergeplanstates;
	mergestate->ms_nplans = nplans;

	mergestate->ms_slots =
			(TupleTableSlot **) palloc0(sizeof(TupleTableSlot *) * nplans);
	mergestate->ms_isduplicate = (bool *) palloc0(sizeof(bool) * nplans);
	mergestate->ms_heap =
			binaryheap_allocate(nplans, heap_compare_slots, mergestate);

	/*
	 * Miscellaneous initialization
	 *
	 * MergeUnique nodes do have Result slots, which hold pointers to tuples,
	 * so we have to initialize them.  FIXME
	 */
	ExecInitResultTupleSlotTL(&mergestate->ps, &TTSOpsVirtual);

	/* node returns slots from each of its subnodes, therefore not fixed */
	mergestate->ps.resultopsset = true;
	mergestate->ps.resultopsfixed = false;

	/*
	 * call ExecInitNode on each of the valid plans to be executed and save
	 * the results into the mergeplanstates array.
	 */
	foreach(lc, node->mergeplans)
	{
		Plan *initNode = (Plan *) lfirst(lc);

		mergeplanstates[foreach_current_index(lc)] =
				ExecInitNode(initNode, estate, eflags);
	}

	mergestate->ps.ps_ProjInfo = NULL;

	/*
	 * initialize sort-key information
	 */
	mergestate->ms_nkeys = node->numCols;
	mergestate->ms_sortkeys = palloc0(sizeof(SortSupportData) * node->numCols);

	for (int i = 0; i < node->numCols; i++)
	{
		SortSupport sortKey = mergestate->ms_sortkeys + i;

		sortKey->ssup_cxt = CurrentMemoryContext;
		sortKey->ssup_collation = node->collations[i];
		sortKey->ssup_nulls_first = node->nullsFirst[i];
		sortKey->ssup_attno = node->sortColIdx[i];

		/*
		 * It isn't feasible to perform abbreviated key conversion, since
		 * tuples are pulled into mergestate's binary heap as needed.  It
		 * would likely be counter-productive to convert tuples into an
		 * abbreviated representation as they're pulled up, so opt out of that
		 * additional optimization entirely.
		 */
		sortKey->abbreviate = false;

		PrepareSortSupportFromOrderingOp(node->sortOperators[i], sortKey);
	}

	/*
	 * initialize to show we have not run the subplans yet
	 */
	mergestate->ms_initialized = false;

	return mergestate;
}

/* ----------------------------------------------------------------
 *	   ExecMergeUnique
 *
 *		Handles iteration over multiple subplans.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecMergeUnique(PlanState *pstate)
{
	MergeUniqueState *node = castNode(MergeUniqueState, pstate);
	TupleTableSlot *result;
	SlotNumber i;

	CHECK_FOR_INTERRUPTS();

	if (!node->ms_initialized)
	{
		/*
		 * First time through: pull the first tuple from each subplan,
		 * and set up the heap.
		 */
		for (SlotNumber i = 0; i < node->ms_nplans; i++)
		{
			node->ms_isduplicate[i] = false; /* not until we discover it is */
			node->ms_slots[i] = ExecProcNode(node->mergeplans[i]);
			if (!TupIsNull(node->ms_slots[i]))
				binaryheap_add_unordered(node->ms_heap, Int32GetDatum(i));
		}
		binaryheap_build(node->ms_heap);
		node->ms_initialized = true;
	}
	else
	{
		bool unique = false;

		/*
		 * Otherwise, pull the next tuple from whichever subplan we returned
		 * from last time, and reinsert the subplan index into the heap,
		 * because it might now compare differently against the existing
		 * elements of the heap.  (We could perhaps simplify the logic a bit
		 * by doing this before returning from the prior call, but it's better
		 * to not pull tuples until necessary.)
		 */
		while (!unique)
		{
			i = DatumGetInt32(binaryheap_first(node->ms_heap));

			node->ms_isduplicate[i] = false; /* not until we discover it is */
			node->ms_slots[i] = ExecProcNode(node->mergeplans[i]);
			if (!TupIsNull(node->ms_slots[i]))
				binaryheap_replace_first(node->ms_heap, Int32GetDatum(i));
			else
				(void) binaryheap_remove_first(node->ms_heap);

			if (binaryheap_empty(node->ms_heap))
				break;

			unique = !node->ms_isduplicate[i];
		}
	}

	if (binaryheap_empty(node->ms_heap))
	{
		/* All the subplans are exhausted, and so is the heap */
		result = ExecClearTuple(node->ps.ps_ResultTupleSlot);
	}
	else
	{
		bool unique = false;

		i = DatumGetInt32(binaryheap_first(node->ms_heap));

		while (!unique)
		{
			if (!node->ms_isduplicate[i])
				return node->ms_slots[i];

			binaryheap_remove_first(node->ms_heap);
			node->ms_isduplicate[i] = false; /* not until we discover it is */
			node->ms_slots[i] = ExecProcNode(node->mergeplans[i]);
			if (!TupIsNull(node->ms_slots[i]))
				binaryheap_add(node->ms_heap, Int32GetDatum(i));
			else  if (binaryheap_empty(node->ms_heap))
				break;

			unique = !node->ms_isduplicate[i];
			i = DatumGetInt32(binaryheap_first(node->ms_heap));
		}

		result = node->ms_slots[i];
	}

	return result;
}

/*
 * Compare the tuples in the two given slots.
 */
static int32
heap_compare_slots(Datum a, Datum b, void *arg)
{
	MergeUniqueState *node = (MergeUniqueState *) arg;
	SlotNumber slot1 = DatumGetInt32(a);
	SlotNumber slot2 = DatumGetInt32(b);

	TupleTableSlot *s1 = node->ms_slots[slot1];
	TupleTableSlot *s2 = node->ms_slots[slot2];
	int nkey;

	Assert(!TupIsNull(s1));
	Assert(!TupIsNull(s2));

	for (nkey = 0; nkey < node->ms_nkeys; nkey++)
	{
		SortSupport sortKey = node->ms_sortkeys + nkey;
		AttrNumber attno = sortKey->ssup_attno;
		Datum datum1, datum2;
		bool isNull1, isNull2;
		int compare;

		datum1 = slot_getattr(s1, attno, &isNull1);
		datum2 = slot_getattr(s2, attno, &isNull2);

		compare =
				ApplySortComparator(datum1, isNull1, datum2, isNull2, sortKey);
		if (compare != 0)
		{
			INVERT_COMPARE_RESULT(compare);
			return compare;
		}
	}

	node->ms_isduplicate[slot1] = true;
	return 0;
}

/* ----------------------------------------------------------------
 *		ExecEndMergeUnique
 *
 *		Shuts down the subscans of the MergeUnique node.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndMergeUnique(MergeUniqueState *node)
{
	PlanState **mergeplans;
	int nplans;
	int i;

	/*
	 * get information from the node
	 */
	mergeplans = node->mergeplans;
	nplans = node->ms_nplans;

	/*
	 * shut down each of the subscans
	 */
	for (i = 0; i < nplans; i++)
		ExecEndNode(mergeplans[i]);
}

void
ExecReScanMergeUnique(MergeUniqueState *node)
{
	for (int i = 0; i < node->ms_nplans; i++)
	{
		PlanState *subnode = node->mergeplans[i];

		/*
		 * ExecReScan doesn't know about my subplans, so I have to do
		 * changed-parameter signaling myself.
		 */
		if (node->ps.chgParam != NULL)
			UpdateChangedParamSet(subnode, node->ps.chgParam);

		/*
		 * If chgParam of subnode is not null then plan will be re-scanned by
		 * first ExecProcNode.
		 */
		if (subnode->chgParam == NULL)
			ExecReScan(subnode);
	}
	binaryheap_reset(node->ms_heap);
	node->ms_initialized = false;
}
