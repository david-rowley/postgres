# contrib/alloc_bench/Makefile

MODULE_big = alloc_bench
OBJS = alloc_bench.o

EXTENSION = alloc_bench
DATA = alloc_bench--1.0.sql
PGFILEDESC = "alloc_bench - memory context benchmarking functions"

REGRESS = alloc_bench

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/alloc_bench
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
