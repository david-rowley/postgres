CREATE EXTENSION slab_bench;

\o fifo-no-loops.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_fifo(1000000, block_size, chunk_size, 0, 0, 0) x;

\o lifo-no-loops.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_lifo(1000000, block_size, chunk_size, 0, 0, 0) x;

\o random-no-loops.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_random(1000000, block_size, chunk_size, 0, 0, 0) x;


\o fifo-increase.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_fifo(1000000, block_size, chunk_size, 100, 10000, 15000) x;

\o lifo-increase.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_lifo(1000000, block_size, chunk_size, 100, 10000, 15000) x;

\o random-increase.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_random(1000000, block_size, chunk_size, 100, 10000, 15000) x;


\o fifo-decrease.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_fifo(1000000, block_size, chunk_size, 100, 10000, 5000) x;

\o lifo-decrease.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_lifo(1000000, block_size, chunk_size, 100, 10000, 5000) x;

\o random-decrease.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_random(1000000, block_size, chunk_size, 100, 10000, 5000) x;


\o fifo-cycle.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_fifo(1000, block_size, chunk_size, 10000, 1000, 1000) x;

\o lifo-cycle.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_lifo(1000, block_size, chunk_size, 10000, 1000, 1000) x;

\o random-cycle.data
select run, block_size, chunk_size, 1000000 * chunk_size, x.* from generate_series(1,5) r(run), generate_series(32,512,32) a(chunk_size), (values (1024), (2048), (4096), (8192), (16384), (32768)) AS b(block_size), lateral slab_bench_random(1000, block_size, chunk_size, 10000, 1000, 1000) x;
