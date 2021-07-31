# contrib/slab_bench/Makefile

MODULE_big = slab_bench
OBJS = slab_bench.o

EXTENSION = slab_bench
DATA = slab_bench--1.0.sql
PGFILEDESC = "slab_bench - slab context benchmarking functions"

REGRESS = slab_bench

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/slab_bench
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
