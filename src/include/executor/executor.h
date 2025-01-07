/*-------------------------------------------------------------------------
 *
 * executor.h
 *	  support for the POSTGRES executor module
 *
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/executor.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "executor/execdesc.h"
#include "fmgr.h"
#include "nodes/lockoptions.h"
#include "nodes/parsenodes.h"
#include "miscadmin.h"
#include "utils/memutils.h"


/*
 * The "eflags" argument to ExecutorStart and the various ExecInitNode
 * routines is a bitwise OR of the following flag bits, which tell the
 * called plan node what to expect.  Note that the flags will get modified
 * as they are passed down the plan tree, since an upper node may require
 * functionality in its subnode not demanded of the plan as a whole
 * (example: MergeJoin requires mark/restore capability in its inner input),
 * or an upper node may shield its input from some functionality requirement
 * (example: Materialize shields its input from needing to do backward scan).
 *
 * EXPLAIN_ONLY indicates that the plan tree is being initialized just so
 * EXPLAIN can print it out; it will not be run.  Hence, no side-effects
 * of startup should occur.  However, error checks (such as permission checks)
 * should be performed.
 *
 * EXPLAIN_GENERIC can only be used together with EXPLAIN_ONLY.  It indicates
 * that a generic plan is being shown using EXPLAIN (GENERIC_PLAN), which
 * means that missing parameter values must be tolerated.  Currently, the only
 * effect is to suppress execution-time partition pruning.
 *
 * REWIND indicates that the plan node should try to efficiently support
 * rescans without parameter changes.  (Nodes must support ExecReScan calls
 * in any case, but if this flag was not given, they are at liberty to do it
 * through complete recalculation.  Note that a parameter change forces a
 * full recalculation in any case.)
 *
 * BACKWARD indicates that the plan node must respect the es_direction flag.
 * When this is not passed, the plan node will only be run forwards.
 *
 * MARK indicates that the plan node must support Mark/Restore calls.
 * When this is not passed, no Mark/Restore will occur.
 *
 * SKIP_TRIGGERS tells ExecutorStart/ExecutorFinish to skip calling
 * AfterTriggerBeginQuery/AfterTriggerEndQuery.  This does not necessarily
 * mean that the plan can't queue any AFTER triggers; just that the caller
 * is responsible for there being a trigger context for them to be queued in.
 *
 * WITH_NO_DATA indicates that we are performing REFRESH MATERIALIZED VIEW
 * ... WITH NO DATA.  Currently, the only effect is to suppress errors about
 * scanning unpopulated materialized views.
 */
#define EXEC_FLAG_EXPLAIN_ONLY		0x0001	/* EXPLAIN, no ANALYZE */
#define EXEC_FLAG_EXPLAIN_GENERIC	0x0002	/* EXPLAIN (GENERIC_PLAN) */
#define EXEC_FLAG_REWIND			0x0004	/* need efficient rescan */
#define EXEC_FLAG_BACKWARD			0x0008	/* need backward scan */
#define EXEC_FLAG_MARK				0x0010	/* need mark/restore */
#define EXEC_FLAG_SKIP_TRIGGERS		0x0020	/* skip AfterTrigger setup */
#define EXEC_FLAG_WITH_NO_DATA		0x0040	/* REFRESH ... WITH NO DATA */


/* Hook for plugins to get control in ExecutorStart() */
typedef void (*ExecutorStart_hook_type) (QueryDesc *queryDesc, int eflags);
extern PGDLLIMPORT ExecutorStart_hook_type ExecutorStart_hook;

/* Hook for plugins to get control in ExecutorRun() */
typedef void (*ExecutorRun_hook_type) (QueryDesc *queryDesc,
									   ScanDirection direction,
									   uint64 count);
extern PGDLLIMPORT ExecutorRun_hook_type ExecutorRun_hook;

/* Hook for plugins to get control in ExecutorFinish() */
typedef void (*ExecutorFinish_hook_type) (QueryDesc *queryDesc);
extern PGDLLIMPORT ExecutorFinish_hook_type ExecutorFinish_hook;

/* Hook for plugins to get control in ExecutorEnd() */
typedef void (*ExecutorEnd_hook_type) (QueryDesc *queryDesc);
extern PGDLLIMPORT ExecutorEnd_hook_type ExecutorEnd_hook;

/* Hook for plugins to get control in ExecCheckPermissions() */
typedef bool (*ExecutorCheckPerms_hook_type) (List *rangeTable,
											  List *rtePermInfos,
											  bool ereport_on_violation);
extern PGDLLIMPORT ExecutorCheckPerms_hook_type ExecutorCheckPerms_hook;


/*
 * prototypes from functions in execAmi.c
 */
struct Path;					/* avoid including pathnodes.h here */

extern void ExecReScan(PlanState *node);
extern void ExecMarkPos(PlanState *node);
extern void ExecRestrPos(PlanState *node);
extern bool ExecSupportsMarkRestore(struct Path *pathnode);
extern bool ExecSupportsBackwardScan(Plan *node);
extern bool ExecMaterializesOutput(NodeTag plantype);

/*
 * prototypes from functions in execCurrent.c
 */
extern bool execCurrentOf(CurrentOfExpr *cexpr,
						  ExprContext *econtext,
						  Oid table_oid,
						  ItemPointer current_tid);

/*
 * prototypes from functions in execGrouping.c
 */
extern ExprState *execTuplesMatchPrepare(TupleDesc desc,
										 int numCols,
										 const AttrNumber *keyColIdx,
										 const Oid *eqOperators,
										 const Oid *collations,
										 PlanState *parent);
extern void execTuplesHashPrepare(int numCols,
								  const Oid *eqOperators,
								  Oid **eqFuncOids,
								  FmgrInfo **hashFunctions);
extern TupleHashTable BuildTupleHashTable(PlanState *parent,
										  TupleDesc inputDesc,
										  const TupleTableSlotOps *inputOps,
										  int numCols,
										  AttrNumber *keyColIdx,
										  const Oid *eqfuncoids,
										  FmgrInfo *hashfunctions,
										  Oid *collations,
										  long nbuckets,
										  Size additionalsize,
										  MemoryContext metacxt,
										  MemoryContext tablecxt,
										  MemoryContext tempcxt,
										  bool use_variable_hash_iv);
extern TupleHashEntry LookupTupleHashEntry(TupleHashTable hashtable,
										   TupleTableSlot *slot,
										   bool *isnew, uint32 *hash);
extern uint32 TupleHashTableHash(TupleHashTable hashtable,
								 TupleTableSlot *slot);
extern TupleHashEntry LookupTupleHashEntryHash(TupleHashTable hashtable,
											   TupleTableSlot *slot,
											   bool *isnew, uint32 hash);
extern TupleHashEntry FindTupleHashEntry(TupleHashTable hashtable,
										 TupleTableSlot *slot,
										 ExprState *eqcomp,
										 ExprState *hashexpr);
extern void ResetTupleHashTable(TupleHashTable hashtable);

/*
 * prototypes from functions in execJunk.c
 */
extern JunkFilter *ExecInitJunkFilter(List *targetList,
									  TupleTableSlot *slot);
extern JunkFilter *ExecInitJunkFilterConversion(List *targetList,
												TupleDesc cleanTupType,
												TupleTableSlot *slot);
extern AttrNumber ExecFindJunkAttribute(JunkFilter *junkfilter,
										const char *attrName);
extern AttrNumber ExecFindJunkAttributeInTlist(List *targetlist,
											   const char *attrName);
extern TupleTableSlot *ExecFilterJunk(JunkFilter *junkfilter,
									  TupleTableSlot *slot);

/*
 * ExecGetJunkAttribute
 *
 * Given a junk filter's input tuple (slot) and a junk attribute's number
 * previously found by ExecFindJunkAttribute, extract & return the value and
 * isNull flag of the attribute.
 */
#ifndef FRONTEND
static inline Datum
ExecGetJunkAttribute(TupleTableSlot *slot, AttrNumber attno, bool *isNull)
{
	Assert(attno > 0);
	return slot_getattr(slot, attno, isNull);
}
#endif

/*
 * prototypes from functions in execMain.c
 */
extern void ExecutorStart(QueryDesc *queryDesc, int eflags);
extern void standard_ExecutorStart(QueryDesc *queryDesc, int eflags);
extern void ExecutorRun(QueryDesc *queryDesc,
						ScanDirection direction, uint64 count);
extern void standard_ExecutorRun(QueryDesc *queryDesc,
								 ScanDirection direction, uint64 count);
extern void ExecutorFinish(QueryDesc *queryDesc);
extern void standard_ExecutorFinish(QueryDesc *queryDesc);
extern void ExecutorEnd(QueryDesc *queryDesc);
extern void standard_ExecutorEnd(QueryDesc *queryDesc);
extern void ExecutorRewind(QueryDesc *queryDesc);
extern bool ExecCheckPermissions(List *rangeTable,
								 List *rteperminfos, bool ereport_on_violation);
extern void CheckValidResultRel(ResultRelInfo *resultRelInfo, CmdType operation,
								List *mergeActions);
extern void InitResultRelInfo(ResultRelInfo *resultRelInfo,
							  Relation resultRelationDesc,
							  Index resultRelationIndex,
							  ResultRelInfo *partition_root_rri,
							  int instrument_options);
extern ResultRelInfo *ExecGetTriggerResultRel(EState *estate, Oid relid,
											  ResultRelInfo *rootRelInfo);
extern List *ExecGetAncestorResultRels(EState *estate, ResultRelInfo *resultRelInfo);
extern void ExecConstraints(ResultRelInfo *resultRelInfo,
							TupleTableSlot *slot, EState *estate);
extern bool ExecPartitionCheck(ResultRelInfo *resultRelInfo,
							   TupleTableSlot *slot, EState *estate, bool emitError);
extern void ExecPartitionCheckEmitError(ResultRelInfo *resultRelInfo,
										TupleTableSlot *slot, EState *estate);
extern void ExecWithCheckOptions(WCOKind kind, ResultRelInfo *resultRelInfo,
								 TupleTableSlot *slot, EState *estate);
extern char *ExecBuildSlotValueDescription(Oid reloid, TupleTableSlot *slot,
										   TupleDesc tupdesc,
										   Bitmapset *modifiedCols,
										   int maxfieldlen);
extern LockTupleMode ExecUpdateLockMode(EState *estate, ResultRelInfo *relinfo);
extern ExecRowMark *ExecFindRowMark(EState *estate, Index rti, bool missing_ok);
extern ExecAuxRowMark *ExecBuildAuxRowMark(ExecRowMark *erm, List *targetlist);
extern TupleTableSlot *EvalPlanQual(EPQState *epqstate, Relation relation,
									Index rti, TupleTableSlot *inputslot);
extern void EvalPlanQualInit(EPQState *epqstate, EState *parentestate,
							 Plan *subplan, List *auxrowmarks,
							 int epqParam, List *resultRelations);
extern void EvalPlanQualSetPlan(EPQState *epqstate,
								Plan *subplan, List *auxrowmarks);
extern TupleTableSlot *EvalPlanQualSlot(EPQState *epqstate,
										Relation relation, Index rti);

#define EvalPlanQualSetSlot(epqstate, slot)  ((epqstate)->origslot = (slot))
extern bool EvalPlanQualFetchRowMark(EPQState *epqstate, Index rti, TupleTableSlot *slot);
extern TupleTableSlot *EvalPlanQualNext(EPQState *epqstate);
extern void EvalPlanQualBegin(EPQState *epqstate);
extern void EvalPlanQualEnd(EPQState *epqstate);

/*
 * functions in execProcnode.c
 */
extern PlanState *ExecInitNode(Plan *node, EState *estate, int eflags);
extern void ExecSetExecProcNode(PlanState *node, ExecProcNodeMtd function);
extern Node *MultiExecProcNode(PlanState *node);
extern void ExecEndNode(PlanState *node);
extern void ExecShutdownNode(PlanState *node);
extern void ExecSetTupleBound(int64 tuples_needed, PlanState *child_node);


/* ----------------------------------------------------------------
 *		ExecProcNode
 *
 *		Execute the given node to return a(nother) tuple.
 * ----------------------------------------------------------------
 */
#ifndef FRONTEND
static inline TupleTableSlot *
ExecProcNode(PlanState *node)
{
	if (node->chgParam != NULL) /* something changed? */
		ExecReScan(node);		/* let ReScan handle this */

	return node->ExecProcNode(node);
}
#endif

/*
 * prototypes from functions in execExpr.c
 */
extern ExprState *ExecInitExpr(Expr *node, PlanState *parent);
extern ExprState *ExecInitExprWithParams(Expr *node, ParamListInfo ext_params);
extern ExprState *ExecInitQual(List *qual, PlanState *parent);
extern ExprState *ExecInitCheck(List *qual, PlanState *parent);
extern List *ExecInitExprList(List *nodes, PlanState *parent);
extern ExprState *ExecBuildAggTrans(AggState *aggstate, struct AggStatePerPhaseData *phase,
									bool doSort, bool doHash, bool nullcheck);
extern ExprState *ExecBuildHash32FromAttrs(TupleDesc desc,
										   const TupleTableSlotOps *ops,
										   FmgrInfo *hashfunctions,
										   Oid *collations,
										   int numCols,
										   AttrNumber *keyColIdx,
										   PlanState *parent,
										   uint32 init_value);
extern ExprState *ExecBuildHash32Expr(TupleDesc desc,
									  const TupleTableSlotOps *ops,
									  const Oid *hashfunc_oids,
									  const List *collations,
									  const List *hash_exprs,
									  const bool *opstrict, PlanState *parent,
									  uint32 init_value, bool keep_nulls);
extern ExprState *ExecBuildGroupingEqual(TupleDesc ldesc, TupleDesc rdesc,
										 const TupleTableSlotOps *lops, const TupleTableSlotOps *rops,
										 int numCols,
										 const AttrNumber *keyColIdx,
										 const Oid *eqfunctions,
										 const Oid *collations,
										 PlanState *parent);
extern ExprState *ExecBuildParamSetEqual(TupleDesc desc,
										 const TupleTableSlotOps *lops,
										 const TupleTableSlotOps *rops,
										 const Oid *eqfunctions,
										 const Oid *collations,
										 const List *param_exprs,
										 PlanState *parent);
extern ProjectionInfo *ExecBuildProjectionInfo(List *targetList,
											   ExprContext *econtext,
											   TupleTableSlot *slot,
											   PlanState *parent,
											   TupleDesc inputDesc);
extern ProjectionInfo *ExecBuildUpdateProjection(List *targetList,
												 bool evalTargetList,
												 List *targetColnos,
												 TupleDesc relDesc,
												 ExprContext *econtext,
												 TupleTableSlot *slot,
												 PlanState *parent);
extern ExprState *ExecPrepareExpr(Expr *node, EState *estate);
extern ExprState *ExecPrepareQual(List *qual, EState *estate);
extern ExprState *ExecPrepareCheck(List *qual, EState *estate);
extern List *ExecPrepareExprList(List *nodes, EState *estate);

/*
 * ExecEvalExpr
 *
 * Evaluate expression identified by "state" in the execution context
 * given by "econtext".  *isNull is set to the is-null flag for the result,
 * and the Datum value is the function result.
 *
 * The caller should already have switched into the temporary memory
 * context econtext->ecxt_per_tuple_memory.  The convenience entry point
 * ExecEvalExprSwitchContext() is provided for callers who don't prefer to
 * do the switch in an outer loop.
 */
#ifndef FRONTEND
static inline Datum
ExecEvalExpr(ExprState *state,
			 ExprContext *econtext,
			 bool *isNull)
{
	return state->evalfunc(state, econtext, isNull);
}
#endif

/*
 * ExecEvalExprSwitchContext
 *
 * Same as ExecEvalExpr, but get into the right allocation context explicitly.
 */
#ifndef FRONTEND
static inline Datum
ExecEvalExprSwitchContext(ExprState *state,
						  ExprContext *econtext,
						  bool *isNull)
{
	Datum		retDatum;
	MemoryContext oldContext;

	oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
	retDatum = state->evalfunc(state, econtext, isNull);
	MemoryContextSwitchTo(oldContext);
	return retDatum;
}
#endif

/*
 * ExecProject
 *
 * Projects a tuple based on projection info and stores it in the slot passed
 * to ExecBuildProjectionInfo().
 *
 * Note: the result is always a virtual tuple; therefore it may reference
 * the contents of the exprContext's scan tuples and/or temporary results
 * constructed in the exprContext.  If the caller wishes the result to be
 * valid longer than that data will be valid, he must call ExecMaterializeSlot
 * on the result slot.
 */
#ifndef FRONTEND
static inline TupleTableSlot *
ExecProject(ProjectionInfo *projInfo)
{
	ExprContext *econtext = projInfo->pi_exprContext;
	ExprState  *state = &projInfo->pi_state;
	TupleTableSlot *slot = state->resultslot;
	bool		isnull;

	/*
	 * Clear any former contents of the result slot.  This makes it safe for
	 * us to use the slot's Datum/isnull arrays as workspace.
	 */
	ExecClearTuple(slot);

	/* Run the expression, discarding scalar result from the last column. */
	(void) ExecEvalExprSwitchContext(state, econtext, &isnull);

	/*
	 * Successfully formed a result row.  Mark the result slot as containing a
	 * valid virtual tuple (inlined version of ExecStoreVirtualTuple()).
	 */
	slot->tts_flags &= ~TTS_FLAG_EMPTY;
	slot->tts_nvalid = slot->tts_tupleDescriptor->natts;

	return slot;
}
#endif

/*
 * ExecQual - evaluate a qual prepared with ExecInitQual (possibly via
 * ExecPrepareQual).  Returns true if qual is satisfied, else false.
 *
 * Note: ExecQual used to have a third argument "resultForNull".  The
 * behavior of this function now corresponds to resultForNull == false.
 * If you want the resultForNull == true behavior, see ExecCheck.
 */
#ifndef FRONTEND
static inline bool
ExecQual(ExprState *state, ExprContext *econtext)
{
	Datum		ret;
	bool		isnull;

	/* short-circuit (here and in ExecInitQual) for empty restriction list */
	if (state == NULL)
		return true;

	/* verify that expression was compiled using ExecInitQual */
	Assert(state->flags & EEO_FLAG_IS_QUAL);

	ret = ExecEvalExprSwitchContext(state, econtext, &isnull);

	/* EEOP_QUAL should never return NULL */
	Assert(!isnull);

	return DatumGetBool(ret);
}
#endif

/*
 * ExecQualAndReset() - evaluate qual with ExecQual() and reset expression
 * context.
 */
#ifndef FRONTEND
static inline bool
ExecQualAndReset(ExprState *state, ExprContext *econtext)
{
	bool		ret = ExecQual(state, econtext);

	/* inline ResetExprContext, to avoid ordering issue in this file */
	MemoryContextReset(econtext->ecxt_per_tuple_memory);
	return ret;
}
#endif

extern bool ExecCheck(ExprState *state, ExprContext *econtext);

/*
 * prototypes from functions in execSRF.c
 */
extern SetExprState *ExecInitTableFunctionResult(Expr *expr,
												 ExprContext *econtext, PlanState *parent);
extern Tuplestorestate *ExecMakeTableFunctionResult(SetExprState *setexpr,
													ExprContext *econtext,
													MemoryContext argContext,
													TupleDesc expectedDesc,
													bool randomAccess);
extern SetExprState *ExecInitFunctionResultSet(Expr *expr,
											   ExprContext *econtext, PlanState *parent);
extern Datum ExecMakeFunctionResultSet(SetExprState *fcache,
									   ExprContext *econtext,
									   MemoryContext argContext,
									   bool *isNull,
									   ExprDoneCond *isDone);

/*
 * prototypes from functions in execScan.c
 */
typedef TupleTableSlot *(*ExecScanAccessMtd) (ScanState *node);
typedef bool (*ExecScanRecheckMtd) (ScanState *node, TupleTableSlot *slot);

extern void ExecAssignScanProjectionInfo(ScanState *node);
extern void ExecAssignScanProjectionInfoWithVarno(ScanState *node, int varno);
extern void ExecScanReScan(ScanState *node);

/*
 * prototypes from functions in execTuples.c
 */
extern void ExecInitResultTypeTL(PlanState *planstate);
extern void ExecInitResultSlot(PlanState *planstate,
							   const TupleTableSlotOps *tts_ops);
extern void ExecInitResultTupleSlotTL(PlanState *planstate,
									  const TupleTableSlotOps *tts_ops);
extern void ExecInitScanTupleSlot(EState *estate, ScanState *scanstate,
								  TupleDesc tupledesc,
								  const TupleTableSlotOps *tts_ops);
extern TupleTableSlot *ExecInitExtraTupleSlot(EState *estate,
											  TupleDesc tupledesc,
											  const TupleTableSlotOps *tts_ops);
extern TupleTableSlot *ExecInitNullTupleSlot(EState *estate, TupleDesc tupType,
											 const TupleTableSlotOps *tts_ops);
extern TupleDesc ExecTypeFromTL(List *targetList);
extern TupleDesc ExecCleanTypeFromTL(List *targetList);
extern TupleDesc ExecTypeFromExprList(List *exprList);
extern void ExecTypeSetColNames(TupleDesc typeInfo, List *namesList);
extern void UpdateChangedParamSet(PlanState *node, Bitmapset *newchg);

typedef struct TupOutputState
{
	TupleTableSlot *slot;
	DestReceiver *dest;
} TupOutputState;

extern TupOutputState *begin_tup_output_tupdesc(DestReceiver *dest,
												TupleDesc tupdesc,
												const TupleTableSlotOps *tts_ops);
extern void do_tup_output(TupOutputState *tstate, const Datum *values, const bool *isnull);
extern void do_text_output_multiline(TupOutputState *tstate, const char *txt);
extern void end_tup_output(TupOutputState *tstate);

/*
 * Write a single line of text given as a C string.
 *
 * Should only be used with a single-TEXT-attribute tupdesc.
 */
#define do_text_output_oneline(tstate, str_to_emit) \
	do { \
		Datum	values_[1]; \
		bool	isnull_[1]; \
		values_[0] = PointerGetDatum(cstring_to_text(str_to_emit)); \
		isnull_[0] = false; \
		do_tup_output(tstate, values_, isnull_); \
		pfree(DatumGetPointer(values_[0])); \
	} while (0)


/*
 * prototypes from functions in execUtils.c
 */
extern EState *CreateExecutorState(void);
extern void FreeExecutorState(EState *estate);
extern ExprContext *CreateExprContext(EState *estate);
extern ExprContext *CreateWorkExprContext(EState *estate);
extern ExprContext *CreateStandaloneExprContext(void);
extern void FreeExprContext(ExprContext *econtext, bool isCommit);
extern void ReScanExprContext(ExprContext *econtext);

#define ResetExprContext(econtext) \
	MemoryContextReset((econtext)->ecxt_per_tuple_memory)

extern ExprContext *MakePerTupleExprContext(EState *estate);

/* Get an EState's per-output-tuple exprcontext, making it if first use */
#define GetPerTupleExprContext(estate) \
	((estate)->es_per_tuple_exprcontext ? \
	 (estate)->es_per_tuple_exprcontext : \
	 MakePerTupleExprContext(estate))

#define GetPerTupleMemoryContext(estate) \
	(GetPerTupleExprContext(estate)->ecxt_per_tuple_memory)

/* Reset an EState's per-output-tuple exprcontext, if one's been created */
#define ResetPerTupleExprContext(estate) \
	do { \
		if ((estate)->es_per_tuple_exprcontext) \
			ResetExprContext((estate)->es_per_tuple_exprcontext); \
	} while (0)

extern void ExecAssignExprContext(EState *estate, PlanState *planstate);
extern TupleDesc ExecGetResultType(PlanState *planstate);
extern const TupleTableSlotOps *ExecGetResultSlotOps(PlanState *planstate,
													 bool *isfixed);
extern const TupleTableSlotOps *ExecGetCommonSlotOps(PlanState **planstates,
													 int nplans);
extern const TupleTableSlotOps *ExecGetCommonChildSlotOps(PlanState *ps);
extern void ExecAssignProjectionInfo(PlanState *planstate,
									 TupleDesc inputDesc);
extern void ExecConditionalAssignProjectionInfo(PlanState *planstate,
												TupleDesc inputDesc, int varno);
extern void ExecAssignScanType(ScanState *scanstate, TupleDesc tupDesc);
extern void ExecCreateScanSlotFromOuterPlan(EState *estate,
											ScanState *scanstate,
											const TupleTableSlotOps *tts_ops);

extern bool ExecRelationIsTargetRelation(EState *estate, Index scanrelid);

extern Relation ExecOpenScanRelation(EState *estate, Index scanrelid, int eflags);

extern void ExecInitRangeTable(EState *estate, List *rangeTable, List *permInfos);
extern void ExecCloseRangeTableRelations(EState *estate);
extern void ExecCloseResultRelations(EState *estate);

static inline RangeTblEntry *
exec_rt_fetch(Index rti, EState *estate)
{
	return (RangeTblEntry *) list_nth(estate->es_range_table, rti - 1);
}

extern Relation ExecGetRangeTableRelation(EState *estate, Index rti);
extern void ExecInitResultRelation(EState *estate, ResultRelInfo *resultRelInfo,
								   Index rti);

extern int	executor_errposition(EState *estate, int location);

extern void RegisterExprContextCallback(ExprContext *econtext,
										ExprContextCallbackFunction function,
										Datum arg);
extern void UnregisterExprContextCallback(ExprContext *econtext,
										  ExprContextCallbackFunction function,
										  Datum arg);

extern Datum GetAttributeByName(HeapTupleHeader tuple, const char *attname,
								bool *isNull);
extern Datum GetAttributeByNum(HeapTupleHeader tuple, AttrNumber attrno,
							   bool *isNull);

extern int	ExecTargetListLength(List *targetlist);
extern int	ExecCleanTargetListLength(List *targetlist);

extern TupleTableSlot *ExecGetTriggerOldSlot(EState *estate, ResultRelInfo *relInfo);
extern TupleTableSlot *ExecGetTriggerNewSlot(EState *estate, ResultRelInfo *relInfo);
extern TupleTableSlot *ExecGetReturningSlot(EState *estate, ResultRelInfo *relInfo);
extern TupleConversionMap *ExecGetChildToRootMap(ResultRelInfo *resultRelInfo);
extern TupleConversionMap *ExecGetRootToChildMap(ResultRelInfo *resultRelInfo, EState *estate);

extern Oid	ExecGetResultRelCheckAsUser(ResultRelInfo *relInfo, EState *estate);
extern Bitmapset *ExecGetInsertedCols(ResultRelInfo *relinfo, EState *estate);
extern Bitmapset *ExecGetUpdatedCols(ResultRelInfo *relinfo, EState *estate);
extern Bitmapset *ExecGetExtraUpdatedCols(ResultRelInfo *relinfo, EState *estate);
extern Bitmapset *ExecGetAllUpdatedCols(ResultRelInfo *relinfo, EState *estate);

/*
 * prototypes from functions in execIndexing.c
 */
extern void ExecOpenIndices(ResultRelInfo *resultRelInfo, bool speculative);
extern void ExecCloseIndices(ResultRelInfo *resultRelInfo);
extern List *ExecInsertIndexTuples(ResultRelInfo *resultRelInfo,
								   TupleTableSlot *slot, EState *estate,
								   bool update,
								   bool noDupErr,
								   bool *specConflict, List *arbiterIndexes,
								   bool onlySummarizing);
extern bool ExecCheckIndexConstraints(ResultRelInfo *resultRelInfo,
									  TupleTableSlot *slot,
									  EState *estate, ItemPointer conflictTid,
									  ItemPointer tupleid,
									  List *arbiterIndexes);
extern void check_exclusion_constraint(Relation heap, Relation index,
									   IndexInfo *indexInfo,
									   ItemPointer tupleid,
									   const Datum *values, const bool *isnull,
									   EState *estate, bool newIndex);

/*
 * prototypes from functions in execReplication.c
 */
extern StrategyNumber get_equal_strategy_number(Oid opclass);
extern bool RelationFindReplTupleByIndex(Relation rel, Oid idxoid,
										 LockTupleMode lockmode,
										 TupleTableSlot *searchslot,
										 TupleTableSlot *outslot);
extern bool RelationFindReplTupleSeq(Relation rel, LockTupleMode lockmode,
									 TupleTableSlot *searchslot, TupleTableSlot *outslot);

extern void ExecSimpleRelationInsert(ResultRelInfo *resultRelInfo,
									 EState *estate, TupleTableSlot *slot);
extern void ExecSimpleRelationUpdate(ResultRelInfo *resultRelInfo,
									 EState *estate, EPQState *epqstate,
									 TupleTableSlot *searchslot, TupleTableSlot *slot);
extern void ExecSimpleRelationDelete(ResultRelInfo *resultRelInfo,
									 EState *estate, EPQState *epqstate,
									 TupleTableSlot *searchslot);
extern void CheckCmdReplicaIdentity(Relation rel, CmdType cmd);

extern void CheckSubscriptionRelkind(char relkind, const char *nspname,
									 const char *relname);

/*
 * prototypes from functions in nodeModifyTable.c
 */
extern TupleTableSlot *ExecGetUpdateNewTuple(ResultRelInfo *relinfo,
											 TupleTableSlot *planSlot,
											 TupleTableSlot *oldSlot);
extern ResultRelInfo *ExecLookupResultRelByOid(ModifyTableState *node,
											   Oid resultoid,
											   bool missing_ok,
											   bool update_cache);


/*
 * inline functions for execScan.c
 */
/*
 * ExecScanFetch -- check interrupts & fetch next potential tuple
 *
 * This routine is concerned with substituting a test tuple if we are
 * inside an EvalPlanQual recheck.  If we aren't, just execute
 * the access method's next-tuple routine.
 */
static pg_attribute_always_inline TupleTableSlot *
ExecScanFetch(ScanState *node,
			  EPQState *epqstate,
			  bool hasEPQstate,
			  ExecScanAccessMtd accessMtd,
			  ExecScanRecheckMtd recheckMtd)
{
	CHECK_FOR_INTERRUPTS();

	if (hasEPQstate)
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

	/*
	 * Run the node-type-specific access method function to get the next tuple
	 */
	return (*accessMtd) (node);
}

/* ----------------------------------------------------------------
 *		ExecScanWithQualAndProjection
 *
 *		Scans the relation using the 'access method' indicated and
 *		returns the next qualifying tuple.
 *		The access method returns the next tuple and the tuple is checked
 *		against the optional 'qual'.
 *
 *		A 'recheck method' must also be provided that can check an
 *		arbitrary tuple of the relation against any qual conditions
 *		that are implemented internal to the access method.
 *
 *		The caller must pass in the EState's es_epq_active if it's non-NULL.

 *		When a non-NULL 'projInfo' is given, qualifying tuples are projected
 *		using this.
 *
 *		This function may be used as an alternative to ExecScan when
 *		callers don't have a 'qual' or don't have a 'projInfo'.  The inlining
 *		allows the compiler to eliminate the non-relevant branches, which
 *		can save having to do run-time checks on every tuple.
 *
 *		The 'hasEPQstate', 'hasQual' and 'hasProjInfo' boolean arguments must
 *		be set according to if the other corresponding parameter is NULL or
 *		not.  This isn't ideal, but it can help the compiler eliminate
 *		redundant code, as otherwise, the compiler would only be able to
 *		eliminate redundant branches when 'eqpstate', 'qual' or 'projInfo' are
 *		passed as constant NULLs.  When the calling code passes a non-null
 *		using these boolean parameters, the compiler is given the flexibility
 *		to eliminate the NULL handling branch.
 *
 *		Conditions:
 *		  -- the "cursor" maintained by the AMI is positioned at the tuple
 *			 returned previously.
 *
 *		Initial States:
 *		  -- the relation indicated is opened for scanning so that the
 *			 "cursor" is positioned before the first qualifying tuple.
 * ----------------------------------------------------------------
 */
static pg_attribute_always_inline TupleTableSlot *
ExecScanExtended(ScanState *node,
				 ExecScanAccessMtd accessMtd,	/* function returning a tuple */
				 ExecScanRecheckMtd recheckMtd,
				 EPQState *epqstate,
				 ExprState *qual,
				 ProjectionInfo *projInfo,
				 bool hasEPQstate,
				 bool hasQual,
				 bool hasProjInfo)
{
	ExprContext *econtext = node->ps.ps_ExprContext;

	Assert(hasEPQstate == (epqstate != NULL));
	Assert(hasQual == (qual != NULL));
	Assert(hasProjInfo == (projInfo != NULL));

	/* interrupt checks are in ExecScanFetch */

	/*
	 * If we have neither a qual to check nor a projection to do, just skip
	 * all the overhead and return the raw scan tuple.
	 */
	if (!hasQual && !hasProjInfo)
	{
		ResetExprContext(econtext);
		return ExecScanFetch(node, epqstate, hasEPQstate, accessMtd, recheckMtd);
	}

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

		slot = ExecScanFetch(node, epqstate, hasEPQstate, accessMtd, recheckMtd);

		/*
		 * if the slot returned by the accessMtd contains NULL, then it means
		 * there is nothing more to scan so we just return an empty slot,
		 * being careful to use the projection result slot so it has correct
		 * tupleDesc.
		 */
		if (TupIsNull(slot))
		{
			if (hasProjInfo)
				return ExecClearTuple(projInfo->pi_state.resultslot);
			else
				return slot;
		}

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
		if (!hasQual || ExecQual(qual, econtext))
		{
			/*
			 * Found a satisfactory scan tuple.
			 */
			if (hasProjInfo)
			{
				/*
				 * Form a projection tuple, store it in the result tuple slot
				 * and return it.
				 */
				return ExecProject(projInfo);
			}
			else
			{
				/*
				 * Here, we aren't projecting, so just return scan tuple.
				 */
				return slot;
			}
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
 *		ExecScan
 *
 *		Scans the relation using the 'access method' indicated and
 *		returns the next qualifying tuple.
 *		The access method returns the next tuple and ExecScan() is
 *		responsible for checking the tuple returned against the qual-clause.
 *
 *		A 'recheck method' must also be provided that can check an
 *		arbitrary tuple of the relation against any qual conditions
 *		that are implemented internal to the access method.
 *
 *		Conditions:
 *		  -- the "cursor" maintained by the AMI is positioned at the tuple
 *			 returned previously.
 *
 *		Initial States:
 *		  -- the relation indicated is opened for scanning so that the
 *			 "cursor" is positioned before the first qualifying tuple.
 * ----------------------------------------------------------------
 */
static inline TupleTableSlot *
ExecScan(ScanState *node,
		 ExecScanAccessMtd accessMtd,	/* function returning a tuple */
		 ExecScanRecheckMtd recheckMtd)

{
	EPQState *epqstate;
	ExprState  *qual;
	ProjectionInfo *projInfo;

	epqstate = node->ps.state->es_epq_active;
	qual = node->ps.qual;
	projInfo = node->ps.ps_ProjInfo;

	return ExecScanExtended(node,
							accessMtd,
							recheckMtd,
							epqstate,
							qual,
							projInfo,
							epqstate != NULL,
							qual != NULL,
							projInfo != NULL);
}

#endif							/* EXECUTOR_H  */
