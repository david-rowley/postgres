/* slab_bench--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION slab_bench" to load this file. \quit

CREATE FUNCTION slab_bench_random(nallocs bigint, block_size bigint, alloc_size bigint, loops int, free_cnt int, alloc_cnt int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'slab_bench_random'
LANGUAGE C VOLATILE STRICT;

CREATE FUNCTION slab_bench_fifo(nallocs bigint, block_size bigint, alloc_size bigint, loops int, free_cnt int, alloc_cnt int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'slab_bench_fifo'
LANGUAGE C VOLATILE STRICT;

CREATE FUNCTION slab_bench_lifo(nallocs bigint, block_size bigint, alloc_size bigint, loops int, free_cnt int, alloc_cnt int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'slab_bench_lifo'
LANGUAGE C VOLATILE STRICT;
