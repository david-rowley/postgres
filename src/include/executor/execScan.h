/*-------------------------------------------------------------------------
 * execScan.h
 *		Inline-able support functions for Scan nodes
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/include/executor/execScan.h
 *-------------------------------------------------------------------------
 */

#ifndef EXECASYNC_H
#define EXECASYNC_H

#include "miscadmin.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"

/*
 * ExecScanGetEPQTuple -- substitutes a test tuple for EvalPlanQual recheck.
 *
 * Must only be called if the Scan is running under EvalPlanQual().
 */
static pg_attribute_always_inline TupleTableSlot *
ExecScanGetEPQTuple(ScanState *node,
					EPQState *epqstate,
					ExecScanRecheckMtd recheckMtd)
{
	Assert(epqstate != NULL);

	{
		/*
		 * We are inside an EvalPlanQual recheck.  Return the test tuple if
		 * one is available, after rechecking any access-method-specific
		 * conditions.
		 */
		Index		scanrelid = ((Scan *) node->ps.plan)->scanrelid;

		if (scanrelid == 0)
		{
			/*
			 * This is a ForeignScan or CustomScan which has pushed down a
			 * join to the remote side.  The recheck method is responsible not
			 * only for rechecking the scan/join quals but also for storing
			 * the correct tuple in the slot.
			 */

			TupleTableSlot *slot = node->ss_ScanTupleSlot;

			if (!(*recheckMtd) (node, slot))
				ExecClearTuple(slot);	/* would not be returned by scan */
			return slot;
		}
		else if (epqstate->relsubs_done[scanrelid - 1])
		{
			/*
			 * Return empty slot, as either there is no EPQ tuple for this rel
			 * or we already returned it.
			 */

			TupleTableSlot *slot = node->ss_ScanTupleSlot;

			return ExecClearTuple(slot);
		}
		else if (epqstate->relsubs_slot[scanrelid - 1] != NULL)
		{
			/*
			 * Return replacement tuple provided by the EPQ caller.
			 */

			TupleTableSlot *slot = epqstate->relsubs_slot[scanrelid - 1];

			Assert(epqstate->relsubs_rowmark[scanrelid - 1] == NULL);

			/* Mark to remember that we shouldn't return it again */
			epqstate->relsubs_done[scanrelid - 1] = true;

			/* Return empty slot if we haven't got a test tuple */
			if (TupIsNull(slot))
				return NULL;

			/* Check if it meets the access-method conditions */
			if (!(*recheckMtd) (node, slot))
				return ExecClearTuple(slot);	/* would not be returned by
												 * scan */
			return slot;
		}
		else if (epqstate->relsubs_rowmark[scanrelid - 1] != NULL)
		{
			/*
			 * Fetch and return replacement tuple using a non-locking rowmark.
			 */

			TupleTableSlot *slot = node->ss_ScanTupleSlot;

			/* Mark to remember that we shouldn't return more */
			epqstate->relsubs_done[scanrelid - 1] = true;

			if (!EvalPlanQualFetchRowMark(epqstate, scanrelid, slot))
				return NULL;

			/* Return empty slot if we haven't got a test tuple */
			if (TupIsNull(slot))
				return NULL;

			/* Check if it meets the access-method conditions */
			if (!(*recheckMtd) (node, slot))
				return ExecClearTuple(slot);	/* would not be returned by
												 * scan */
			return slot;
		}
	}

	Assert(false);
	return NULL;
}

/*
 * Fetches tuples using the access method callback until one is found that
 * safisfies the 'qual'.
 */
static pg_attribute_always_inline TupleTableSlot *
ExecScanWithQualNoProj(ScanState *node,
					   ExecScanAccessMtd accessMtd,	/* function returning a tuple */
					   ExecScanRecheckMtd recheckMtd,
					   EPQState *epqstate,
					   ExprState *qual)
{
	ExprContext *econtext = node->ps.ps_ExprContext;

	Assert(qual != NULL);

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * get a tuple from the access method.  Loop until we obtain a tuple that
	 * passes the qualification.
	 */
	for (;;)
	{
		TupleTableSlot *slot;

		CHECK_FOR_INTERRUPTS();

		/* interrupt checks are in ExecScanFetch() when it's used */
		if (epqstate == NULL)
		{
			slot = (*accessMtd) (node);
		}
		else
			slot = ExecScanGetEPQTuple(node, epqstate, recheckMtd);

		/*
		 * if the slot returned by the accessMtd contains NULL, then it means
		 * there is nothing more to scan so we just return an empty slot,
		 * being careful to use the projection result slot so it has correct
		 * tupleDesc.
		 */
		if (TupIsNull(slot))
			return slot;

		/*
		 * place the current tuple into the expr context
		 */
		econtext->ecxt_scantuple = slot;

		/*
		 * check that the current tuple satisfies the qual-clause
		 *
		 * check for non-null qual here to avoid a function call to ExecQual()
		 * when the qual is null ... saves only a few cycles, but they add up
		 * ...
		 */
		if (ExecQual(qual, econtext))
		{
			/*
			 * Found a satisfactory scan tuple.
			 *
			 * Here, we aren't projecting, so just return scan tuple.
			 */
			return slot;
		}
		else
			InstrCountFiltered1(node, 1);

		/*
		 * Tuple fails qual, so free per-tuple memory and try again.
		 */
		ResetExprContext(econtext);
	}
}

/*
 * Fetches the next tuple using the access method callback and returns the
 * tuple obtained by projecting using the 'projInfo'.
 */
static pg_attribute_always_inline TupleTableSlot *
ExecScanWithProjNoQual(ScanState *node,
					   ExecScanAccessMtd accessMtd,	/* function returning a tuple */
					   ExecScanRecheckMtd recheckMtd,
					   EPQState *epqstate,
					   ProjectionInfo *projInfo)
{
	ExprContext *econtext = node->ps.ps_ExprContext;
	TupleTableSlot *slot;

	Assert(projInfo != NULL);

	CHECK_FOR_INTERRUPTS();

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);


	/* interrupt checks are in ExecScanFetch() when it's used */
	if (epqstate == NULL)
	{
		slot = (*accessMtd) (node);
	}
	else
		slot = ExecScanGetEPQTuple(node, epqstate, recheckMtd);

	/*
	 * if the slot returned by the accessMtd contains NULL, then it means
	 * there is nothing more to scan so we just return an empty slot,
	 * being careful to use the projection result slot so it has correct
	 * tupleDesc.
	 */
	if (TupIsNull(slot))
		return ExecClearTuple(projInfo->pi_state.resultslot);

	/*
	 * place the current tuple into the expr context
	 */
	econtext->ecxt_scantuple = slot;

	/*
	 * Form a projection tuple, store it in the result tuple slot
	 * and return it.
	 */
	return ExecProject(projInfo);
}

/*
 * Fetches tuples using the access method callback until one is found that
 * safisfies the 'qual' and returns the tuple obtained by projecting using the
 * 'projInfo'.
 */
static pg_attribute_always_inline TupleTableSlot *
ExecScanWithQualAndProj(ScanState *node,
						 ExecScanAccessMtd accessMtd,	/* function returning a tuple */
						 ExecScanRecheckMtd recheckMtd,
						 EPQState *epqstate,
						 ExprState *qual,
						 ProjectionInfo *projInfo)
{
	ExprContext *econtext = node->ps.ps_ExprContext;

	Assert(qual != NULL);
	Assert(projInfo != NULL);

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * get a tuple from the access method.  Loop until we obtain a tuple that
	 * passes the qualification.
	 */
	for (;;)
	{
		TupleTableSlot *slot;

		CHECK_FOR_INTERRUPTS();

		/* interrupt checks are in ExecScanFetch() when it's used */
		if (epqstate == NULL)
		{
			slot = (*accessMtd) (node);
		}
		else
			slot = ExecScanGetEPQTuple(node, epqstate, recheckMtd);

		/*
		 * if the slot returned by the accessMtd contains NULL, then it means
		 * there is nothing more to scan so we just return an empty slot,
		 * being careful to use the projection result slot so it has correct
		 * tupleDesc.
		 */
		if (TupIsNull(slot))
			return ExecClearTuple(projInfo->pi_state.resultslot);

		/*
		 * place the current tuple into the expr context
		 */
		econtext->ecxt_scantuple = slot;

		/*
		 * check that the current tuple satisfies the qual-clause
		 *
		 * check for non-null qual here to avoid a function call to ExecQual()
		 * when the qual is null ... saves only a few cycles, but they add up
		 * ...
		 */
		if (ExecQual(qual, econtext))
		{
			/*
			 * Found a satisfactory scan tuple.
			 *
			 * Form a projection tuple, store it in the result tuple slot
			 * and return it.
			 */
			return ExecProject(projInfo);
		}
		else
			InstrCountFiltered1(node, 1);

		/*
		 * Tuple fails qual, so free per-tuple memory and try again.
		 */
		ResetExprContext(econtext);
	}
}

/* ----------------------------------------------------------------
 * ExecScanExtended
 *		Scans the relation using the given 'access method' and returns
 *		the next qualifying tuple. The tuple is optionally checked
 *		against 'qual' and, if provided, projected using 'projInfo'.
 *
 * The 'recheck method' validates an arbitrary tuple of the relation
 * against conditions enforced by the access method.
 *
 * This function is an alternative to ExecScan, used when callers
 * may omit 'qual' or 'projInfo'. The pg_attribute_always_inline
 * attribute allows the compiler to eliminate non-relevant branches
 * at compile time, avoiding run-time checks in those cases.
 *
 * Conditions:
 *	-- The AMI "cursor" is positioned at the previously returned tuple.
 *
 * Initial States:
 *	-- The relation is opened for scanning, with the "cursor"
 *	positioned before the first qualifying tuple.
 * ----------------------------------------------------------------
 */

static pg_attribute_always_inline TupleTableSlot *
ExecScanExtended(ScanState *node,
				 ExecScanAccessMtd accessMtd,	/* function returning a tuple */
				 ExecScanRecheckMtd recheckMtd,
				 EPQState *epqstate,
				 ExprState *qual,
				 ProjectionInfo *projInfo)
{
	if (qual != NULL && projInfo != NULL)
		return ExecScanWithQualAndProj(node, accessMtd, recheckMtd, epqstate, qual, projInfo);
	else if (qual != NULL)
		return ExecScanWithQualNoProj(node, accessMtd, recheckMtd, epqstate, qual);
	else if (projInfo != NULL)
		return ExecScanWithProjNoQual(node, accessMtd, recheckMtd, epqstate, projInfo);
	/*
	 * If we have neither a qual to check nor a projection to do, just skip
	 * all the overhead and return the raw scan tuple.
	 */
	else
	{
		CHECK_FOR_INTERRUPTS();
		ResetExprContext(node->ps.ps_ExprContext);
		if (epqstate == NULL)
			return (*accessMtd) (node);
		else
			return ExecScanGetEPQTuple(node, epqstate, recheckMtd);
	}

	Assert(false);
	return NULL;
}

#endif							/* EXECASYNC_H */
