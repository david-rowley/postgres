/*-------------------------------------------------------------------------
 *
 * nodeMergeUnique.h
 *
 *
 *
 * Portions Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * src/include/executor/nodeMergeUnique.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEMERGEUNIQUE_H
#define NODEMERGEUNIQUE_H

#include "nodes/execnodes.h"

extern MergeUniqueState *ExecInitMergeUnique(MergeUnique *node,
											 EState *estate, int eflags);
extern void ExecEndMergeUnique(MergeUniqueState *node);
extern void ExecReScanMergeUnique(MergeUniqueState *node);

#endif /* NODEMERGEUNIQUE_H */
