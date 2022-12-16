/* alloc_bench--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION alloc_bench" to load this file. \quit

CREATE FUNCTION alloc_bench(context_type text, pattern text, nchunks bigint, block_size bigint, chunk_size bigint, nloops int, out mem_allocated bigint, out alloc_ms bigint, out free_ms bigint)
AS 'MODULE_PATHNAME', 'alloc_bench_random'
LANGUAGE C VOLATILE STRICT;

