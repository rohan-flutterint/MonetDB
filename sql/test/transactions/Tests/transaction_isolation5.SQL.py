from MonetDBtesting.sqltest import SQLTestCase

with SQLTestCase() as mdb1:
    mdb1.connect(username="monetdb", password="monetdb")
    mdb1.execute('START TRANSACTION;').assertSucceeded()
    mdb1.execute('SAVEPOINT mys;').assertSucceeded()
    mdb1.execute('CREATE SCHEMA ups;').assertSucceeded()
    mdb1.execute('SET SCHEMA ups;').assertSucceeded()
    mdb1.execute('ROLLBACK TO SAVEPOINT mys;').assertFailed(err_code="40000", err_message="ROLLBACK: finished successfully, but the session's schema could not be found on the current transaction")
    mdb1.execute('rollback;').assertFailed()

with SQLTestCase() as mdb1:
    mdb1.connect(username="monetdb", password="monetdb")
    mdb1.execute('START TRANSACTION;').assertSucceeded()
    mdb1.execute('SAVEPOINT mys2;').assertSucceeded()
    mdb1.execute('CREATE SCHEMA ups2;').assertSucceeded()
    mdb1.execute('SET SCHEMA ups2;').assertSucceeded()
    mdb1.execute('RELEASE SAVEPOINT mys2;').assertSucceeded()
    mdb1.execute('rollback;').assertFailed()

with SQLTestCase() as mdb1:
    with SQLTestCase() as mdb2:
        mdb1.connect(username="monetdb", password="monetdb")
        mdb2.connect(username="monetdb", password="monetdb")

        mdb1.execute('create table child1(a int);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('create merge table parent1(a int);').assertSucceeded()
        mdb1.execute('alter table parent1 add table child1;').assertSucceeded()
        mdb2.execute("insert into child1 values (1);").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertSucceeded()

        mdb1.execute('create merge table parent2(a int, b int);').assertSucceeded()
        mdb1.execute('create table child2(a int, b int);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("drop table child2;").assertSucceeded()
        mdb2.execute("ALTER TABLE parent2 ADD TABLE child2;").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40001", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('create merge table parent3(a int, b int);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("drop table parent3;").assertSucceeded()
        mdb2.execute("ALTER TABLE parent3 ADD TABLE parent2;").assertFailed(err_code="42000", err_message="ALTER TABLE: transaction conflict detected")
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('rollback;').assertSucceeded()

        mdb1.execute('CREATE ROLE myrole;').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('CREATE schema mysch AUTHORIZATION myrole;').assertSucceeded()
        mdb2.execute('DROP ROLE myrole;').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40001", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('alter table parent1 drop table child1;').assertSucceeded()
        mdb1.execute('drop table parent1;').assertSucceeded()
        mdb1.execute('drop table child1;').assertSucceeded()
        mdb1.execute('drop table parent2;').assertSucceeded()
        mdb1.execute('drop schema mysch;').assertSucceeded()
        mdb1.execute('drop role myrole;').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()

with SQLTestCase() as mdb1:
    with SQLTestCase() as mdb2:
        mdb1.connect(username="monetdb", password="monetdb")
        mdb2.connect(username="monetdb", password="monetdb")

        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('create merge table parent(a int, b int);').assertSucceeded()
        mdb1.execute('create table child1(a int, b int);').assertSucceeded()
        mdb1.execute("insert into child1 values (1,1);").assertSucceeded()
        mdb1.execute('create table child2(a int, b int);').assertSucceeded()
        mdb1.execute("insert into child2 values (2,2);").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("alter table parent add table child1;").assertSucceeded()
        mdb2.execute('alter table child1 add column data int;').assertSucceeded() # number of columns must match
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40001", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('select * from parent;').assertSucceeded().assertDataResultMatch([(1,1)])
        mdb2.execute('select * from parent;').assertSucceeded().assertDataResultMatch([(1,1)])

        mdb1.execute("alter table parent drop table child1;").assertSucceeded()
        mdb1.execute("alter table parent add table child2;").assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("alter table parent add table child1;").assertSucceeded()
        mdb2.execute('alter table child1 alter column a set not null;').assertSucceeded() # null constraints must match
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40001", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('select * from parent;').assertSucceeded().assertDataResultMatch([(1,1),(2,2)])
        mdb2.execute('select * from parent;').assertSucceeded().assertDataResultMatch([(1,1),(2,2)])

        mdb1.execute('alter table parent drop table child1;').assertSucceeded()

        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("alter table parent add table child1;").assertSucceeded()
        mdb2.execute('alter table child1 drop column b;').assertSucceeded() # number of columns must match
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40001", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('select * from parent;').assertSucceeded().assertDataResultMatch([(1,1),(2,2)])
        mdb2.execute('select * from parent;').assertSucceeded().assertDataResultMatch([(1,1),(2,2)])

        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('alter table parent drop table child1;').assertSucceeded()
        mdb1.execute('alter table parent drop table child2;').assertSucceeded()
        mdb1.execute('drop table parent;').assertSucceeded()
        mdb1.execute('drop table child1;').assertSucceeded()
        mdb1.execute('drop table child2;').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()

with SQLTestCase() as mdb1:
    with SQLTestCase() as mdb2:
        mdb1.connect(username="monetdb", password="monetdb")
        mdb2.connect(username="monetdb", password="monetdb")

        # Test different instantiations of SQL functions and views on different transactions
        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('CREATE MERGE TABLE parent(a int)').assertSucceeded()
        mdb1.execute('CREATE TABLE child1(a int)').assertSucceeded()
        mdb1.execute('INSERT INTO child1 VALUES (1),(2)').assertSucceeded()
        mdb1.execute('ALTER TABLE parent ADD table child1').assertSucceeded()
        mdb1.execute('CREATE TABLE child2(a int)').assertSucceeded()
        mdb1.execute('INSERT INTO child2 VALUES (3),(4)').assertSucceeded()
        mdb1.execute('create function myfunc() returns table(a int) begin return select a from parent; end').assertSucceeded()
        mdb1.execute('create view myview as (select a from parent)').assertSucceeded()
        mdb1.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb1.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb1.execute('commit;').assertSucceeded()
        mdb1.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb1.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb1.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('ALTER TABLE parent ADD table child2').assertSucceeded()
        mdb1.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,),(3,),(4,)])
        mdb1.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,)])
        mdb2.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,),(3,),(4,)])
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertSucceeded()
        mdb1.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,),(3,),(4,)])
        mdb2.execute('SELECT a from myfunc()').assertSucceeded().assertDataResultMatch([(1,),(2,),(3,),(4,)])
        mdb1.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,),(3,),(4,)])
        mdb2.execute('SELECT a from myview').assertSucceeded().assertDataResultMatch([(1,),(2,),(3,),(4,)])
        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('drop function myfunc;').assertSucceeded()
        mdb1.execute('drop view myview;').assertSucceeded()
        mdb1.execute('alter table parent drop table child1;').assertSucceeded()
        mdb1.execute('alter table parent drop table child2;').assertSucceeded()
        mdb1.execute('drop table parent;').assertSucceeded()
        mdb1.execute('drop table child1;').assertSucceeded()
        mdb1.execute('drop table child2;').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()

        # Test replacing functions and views with concurrent transactions
        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('create or replace function otherfunc() returns int return 1;').assertSucceeded()
        mdb1.execute('create or replace view otherview(x) as (select 1)').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb1.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(1,)])
        mdb1.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(1,)])
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(1,)])
        mdb1.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('create or replace function otherfunc() returns int return 2;').assertSucceeded()
        mdb2.execute('create or replace view otherview(x) as (select 2)').assertSucceeded()
        mdb1.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(2,)])
        mdb1.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(1,)])
        mdb2.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(2,)])
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertSucceeded()
        mdb1.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(2,)])
        mdb2.execute('SELECT otherfunc()').assertSucceeded().assertDataResultMatch([(2,)])
        mdb1.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(2,)])
        mdb2.execute('SELECT x from otherview').assertSucceeded().assertDataResultMatch([(2,)])
        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('drop function otherfunc;').assertSucceeded()
        mdb1.execute('drop view otherview;').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()

        # If one transaction changes read access of a table, disallow concurrent dml
        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('create table t0(c0 int);').assertSucceeded()
        mdb1.execute('insert into t0 values (1),(2),(3);').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('alter table t0 set read only;').assertSucceeded()
        mdb2.execute("insert into t0 values (4),(5),(6);").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40001", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")
        mdb1.execute('select c0 from t0;').assertSucceeded().assertSucceeded().assertDataResultMatch([(1,),(2,),(3,)])
        mdb2.execute('select c0 from t0;').assertSucceeded().assertSucceeded().assertDataResultMatch([(1,),(2,),(3,)])
        mdb1.execute('drop table t0;').assertSucceeded()
