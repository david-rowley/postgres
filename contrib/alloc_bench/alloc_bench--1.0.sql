/* alloc_bench--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION alloc_bench" to load this file. \quit

CREATE FUNCTION alloc_bench_random(context_type text, nallocs bigint, block_size bigint, alloc_size bigint, loops int, free_cnt int, alloc_cnt int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'alloc_bench_random'
LANGUAGE C VOLATILE STRICT;

CREATE FUNCTION alloc_bench_fifo(context_type text, nallocs bigint, block_size bigint, alloc_size bigint, loops int, free_cnt int, alloc_cnt int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'alloc_bench_fifo'
LANGUAGE C VOLATILE STRICT;

CREATE FUNCTION alloc_bench_lifo(context_type text, nallocs bigint, block_size bigint, alloc_size bigint, loops int, free_cnt int, alloc_cnt int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'alloc_bench_lifo'
LANGUAGE C VOLATILE STRICT;
