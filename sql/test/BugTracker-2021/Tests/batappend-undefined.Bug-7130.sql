start transaction;

create table t_qh ( c_f INTEGER , c_y2 INTEGER , c_i768 INTEGER , c_tqx TEXT , c_mknkhml TEXT, primary key(c_f, c_y2), unique(c_y2) );
insert into t_qh values (1,1,1,'a','a'),(2,2,2,'b','b'),(3,3,3,'c','c');

create table t_ckfystsc ( c_kvhq5p INTEGER , c_aifpl INTEGER , c_jf6 TEXT , c_f31ix TEXT NOT NULL, c_lo0zqfe TEXT , c_zv INTEGER , c_l153 INTEGER , primary key(c_zv), unique(c_zv) );
insert into t_ckfystsc values (1,1,'a','a','a',1,1),(2,2,'b','b','b',2,2),(3,3,'c','c','c',3,3);

create view t_vehkuero as select distinct abs( cast(subq_0.c1 as INTEGER)) as c0 from (select ref_0.c_f as c0, ref_0.c_f as c1, ref_0.c_i768 as c2, ref_0.c_y2 as c3 from t_qh as ref_0) 
as subq_0 where cast(nullif('zdo', 'apqonv1a') as CHAR) like 'aou%2' AND length( cast(case when subq_0.c1 = ( select subq_0.c3 as c0 from t_qh as ref_6
where (subq_0.c0 <> ( select subq_0.c0 as c0 from t_qh as ref_7)) ) then 'xk' else 'xk' end as CHAR)) <> subq_0.c0;

select 1000 in (select 2 from t_vehkuero) from (select 1 as c0 from t_ckfystsc) as subq_0;
	-- False
	-- False
	-- False

select case when subq_0.c0 in ( select ref_5.c_tqx as c0 from t_qh as ref_5 where ref_5.c_y2 in ( select ref_6.c_y2 as c0 from t_qh as ref_6 union select 2 as c0 from t_vehkuero as ref_7))
then cast(nullif(subq_0.c0, subq_0.c0) as CHAR) else cast(nullif(subq_0.c0, subq_0.c0) as CHAR) end as c0 from (select ref_0.c_lo0zqfe as c0, ref_0.c_aifpl as c1 from t_ckfystsc as ref_0) as subq_0;
	-- NULL
	-- NULL
	-- NULL

rollback;

create table t_qh ( c_f INTEGER , c_y2 INTEGER , c_i768 INTEGER , c_tqx INTEGER , primary key(c_i768), unique(c_y2) );
insert into t_qh values (1,1,1,1),(2,2,2,2),(3,3,3,3);

create view t_amy as select ref_1.c_f as c0 from t_qh as ref_0 cross join t_qh as ref_1 left outer join (select ref_2.c_i768 as c0, cast(sum( cast(ref_3.c_f as INTEGER)) as bigint) as c1, count(*) as c2 from t_qh as ref_2
left outer join t_qh as ref_3 on (ref_2.c_f < ref_2.c_f) where ref_2.c_i768 > ref_3.c_y2 group by ref_2.c_i768) as subq_0 on (ref_0.c_y2 < ref_1.c_f) where ref_0.c_f <> ( select ref_4.c_i768 as c0 from t_qh
as ref_4 cross join t_qh as ref_5 where EXISTS ( select ref_6.c_i768 as c0, ref_1.c_i768 as c1, ref_6.c_y2 as c2 from t_qh as ref_6 where subq_0.c0 < ( select subq_0.c1 as c0 from t_qh as ref_7
where (ref_5.c_f <> ref_6.c_f) or (subq_0.c1 is NULL))));

select (subq_0.c0 = case when EXISTS ( select ref_15.c0 as c6 from t_amy as ref_15) then subq_0.c1 else subq_0.c1 end) as c4 from (select ref_12.c_f as c0, 30 as c1 from t_qh as ref_12) as subq_0;
	-- error, more than one match

select (select (select 1 where subq_0.c0 = (select subq_0.c1 from t_qh)) from (select 320, 1) subq_0(c0,c1)) from (select 2) subq_0(c3);
	-- error, more than one match

drop table t_qh cascade;
