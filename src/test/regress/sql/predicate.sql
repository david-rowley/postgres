--
-- Tests for predicate handling
--

--
-- test that restrictions that are always true are ignored
--
-- currently we only check for NullTest quals and OR clauses that include
-- NullTest quals.  We may extend it in the future.
--
create table pred_tab (a int not null, b int);

-- An IS_NOT_NULL qual in restriction clauses can be ignored if it's on a NOT
-- NULL column
explain (costs off)
select * from pred_tab t where t.a is not null;

-- On the contrary, an IS_NOT_NULL qual in restriction clauses can not be
-- ignored if it's not on a NOT NULL column
explain (costs off)
select * from pred_tab t where t.b is not null;

-- Tests for OR clauses in restriction clauses
explain (costs off)
select * from pred_tab t where t.a is not null or t.b = 1;

explain (costs off)
select * from pred_tab t where t.b is not null or t.a = 1;

-- An IS_NOT_NULL qual in join clauses can be ignored if
-- a) it's on a NOT NULL column, and
-- b) its Var is not nulled by any outer joins
explain (costs off)
select * from pred_tab t1 left join pred_tab t2 on true left join pred_tab t3 on t2.a is not null;

-- Otherwise the IS_NOT_NULL qual in join clauses cannot be ignored
explain (costs off)
select * from pred_tab t1 left join pred_tab t2 on t1.a = 1 left join pred_tab t3 on t2.a is not null;

-- Tests for OR clauses in join clauses
explain (costs off)
select * from pred_tab t1 left join pred_tab t2 on true left join pred_tab t3 on t2.a is not null or t2.b = 1;

explain (costs off)
select * from pred_tab t1 left join pred_tab t2 on t1.a = 1 left join pred_tab t3 on t2.a is not null or t2.b = 1;

drop table pred_tab;

