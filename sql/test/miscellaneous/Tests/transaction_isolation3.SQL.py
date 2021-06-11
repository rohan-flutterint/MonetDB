from MonetDBtesting.sqltest import SQLTestCase

with SQLTestCase() as mdb1:
    with SQLTestCase() as mdb2:
        mdb1.connect(username="monetdb", password="monetdb")
        mdb2.connect(username="monetdb", password="monetdb")

        mdb1.execute("CREATE TABLE integers (i int, j int);").assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('alter table integers add primary key (i);').assertSucceeded()
        mdb2.execute('insert into integers values (5,1),(5,2),(5,3);').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40000", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute('alter table integers alter j set not null;').assertSucceeded()
        mdb2.execute('insert into integers values (6,NULL),(7,NULL),(8,NULL);').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40000", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('create schema ups;').assertSucceeded()
        mdb1.execute('create merge table parent1(a int) PARTITION BY RANGE ON (a);').assertSucceeded()
        mdb1.execute('create table child1(c int);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("ALTER TABLE parent1 ADD TABLE child1 AS PARTITION FROM '1' TO '2';").assertSucceeded() # these merge tables are very difficult, maybe allow only 1 transaction on the system?
        mdb2.execute("alter table child1 set schema ups;").assertFailed(err_code="42000", err_message="ALTER TABLE: transaction conflict detected")
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('rollback;').assertSucceeded()

        mdb1.execute('create merge table parent2(a int) PARTITION BY RANGE ON (a);').assertSucceeded()
        mdb1.execute('create table child2(c int);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("ALTER TABLE parent2 ADD TABLE child2 AS PARTITION FROM '1' TO '2';").assertSucceeded()
        mdb2.execute("insert into child2 values (3);").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40000", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('create table x(y int, z int);').assertSucceeded()
        mdb1.execute('insert into x values (1, 1);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("create view myv(a,b) as select y, z from x;").assertSucceeded()
        mdb2.execute("alter table x drop column y;").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40000", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")
        mdb1.execute('select * from myv;').assertSucceeded().assertDataResultMatch([(1,1)])

        mdb1.execute("create table ups.no (a int, b int);").assertSucceeded()
        mdb1.execute('insert into ups.no values (2, 2);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("create function sys.another() returns table(i int) begin return select a from ups.no; end;").assertSucceeded()
        mdb2.execute("alter table ups.no drop column a;").assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40000", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")
        mdb1.execute('select * from another();').assertSucceeded().assertDataResultMatch([(2,2)])

        mdb1.execute("CREATE TABLE y (i int);").assertSucceeded()
        mdb1.execute('truncate integers;').assertSucceeded()
        mdb1.execute('insert into integers values (1,1),(2,2),(3,3);').assertSucceeded()
        mdb1.execute('start transaction;').assertSucceeded()
        mdb2.execute('start transaction;').assertSucceeded()
        mdb1.execute("alter table y add constraint nono foreign key(i) references integers(i)").assertSucceeded()
        mdb2.execute("insert into y values (4)").assertSucceeded() # violates foreign key
        mdb1.execute('commit;').assertSucceeded()
        mdb2.execute('commit;').assertFailed(err_code="40000", err_message="COMMIT: transaction is aborted because of concurrency conflicts, will ROLLBACK instead")

        mdb1.execute('start transaction;').assertSucceeded()
        mdb1.execute('drop table y;').assertSucceeded()
        mdb1.execute('drop table integers;').assertSucceeded()
        mdb1.execute('drop function another;').assertSucceeded()
        mdb1.execute('drop table ups.no;').assertSucceeded()
        mdb1.execute('drop schema ups;').assertSucceeded()
        mdb1.execute('ALTER TABLE parent1 DROP TABLE child1;').assertSucceeded()
        mdb1.execute('DROP TABLE parent1;').assertSucceeded()
        mdb1.execute('DROP TABLE child1;').assertSucceeded()
        mdb1.execute('ALTER TABLE parent2 DROP TABLE child2;').assertSucceeded()
        mdb1.execute('DROP TABLE parent2;').assertSucceeded()
        mdb1.execute('DROP TABLE child2;').assertSucceeded()
        mdb1.execute('DROP VIEW myv;').assertSucceeded()
        mdb1.execute('DROP TABLE x;').assertSucceeded()
        mdb1.execute('commit;').assertSucceeded()
