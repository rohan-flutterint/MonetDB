START TRANSACTION;
SET SCHEMA "sys";
CREATE ROLE "king";
CREATE USER "voc" WITH ENCRYPTED PASSWORD  'ea45cf4e124b215a28631ec7ff0bf06e82fc26b2be7a066c9594855690fb5d42438be58d6523132384a1738cb4e5139caa1f970ebdfb422d65834d9a4ef61c0e'  NAME  'VOC Explorer'  SCHEMA sys;
CREATE USER "voc2" WITH ENCRYPTED PASSWORD  'ea45cf4e124b215a28631ec7ff0bf06e82fc26b2be7a066c9594855690fb5d42438be58d6523132384a1738cb4e5139caa1f970ebdfb422d65834d9a4ef61c0e'  NAME  'VOC Explorer'  SCHEMA sys;
CREATE SCHEMA "sbar" AUTHORIZATION monetdb;
CREATE SCHEMA "sfoo" AUTHORIZATION monetdb;
ALTER USER "voc" SET SCHEMA "sfoo";
CREATE SEQUENCE "sys"."seq1" AS BIGINT  START WITH 5 INCREMENT BY 3 MINVALUE 4 MAXVALUE 10 CACHE 2 CYCLE;
CREATE SEQUENCE "sys"."seq2" AS BIGINT  START WITH 4 INCREMENT BY 3 MINVALUE 4 MAXVALUE 10 CACHE 2 CYCLE;
CREATE SEQUENCE "sys"."seq3" AS BIGINT  START WITH 4 MINVALUE 4 MAXVALUE 10 CACHE 2 CYCLE;
CREATE SEQUENCE "sys"."seq4" AS BIGINT  START WITH 1 MAXVALUE 10 CACHE 2 CYCLE;
CREATE SEQUENCE "sys"."seq5" AS BIGINT  START WITH 1 MAXVALUE 10 CACHE 2;
CREATE SEQUENCE "sys"."seq6" AS BIGINT  START WITH 1 CACHE 2;
CREATE SEQUENCE "sys"."seq7" AS BIGINT  START WITH 1;
CREATE SEQUENCE "sys"."seq8" AS BIGINT  START WITH -10 INCREMENT BY -1 MINVALUE -10 MAXVALUE -1;
CREATE SEQUENCE "sys"."seq9" AS BIGINT  START WITH 10 MINVALUE 10 MAXVALUE 10;
CREATE TABLE "sys"."test" ("s" CHARACTER LARGE OBJECT);
CREATE TABLE "sys"."bla" ("s" CHARACTER LARGE OBJECT(10));
CREATE TABLE "sys"."bob" ("ts" TIMESTAMP(3));
CREATE TABLE "sys"."fdgdf" ("tsz" TIMESTAMP(4) WITH TIME ZONE);
CREATE TABLE "sys"."yoyo" ("tsz" CHARACTER LARGE OBJECT);
CREATE TABLE "sys"."bolo" ("s" CHARACTER LARGE OBJECT NOT NULL, "v" CHARACTER LARGE OBJECT NOT NULL);
CREATE TABLE "sys"."rolo" ("v" CHARACTER LARGE OBJECT NOT NULL);
CREATE TABLE "sys"."ungolo" ("x" INTEGER, "y" INTEGER, "z" INTEGER);
CREATE TABLE "sfoo"."tfoo1" ("i" INTEGER);
CREATE TABLE "sfoo"."tfoo2" ("i" INTEGER);
CREATE TABLE "sfoo"."test" ("s" CHARACTER LARGE OBJECT);
CREATE TABLE "sys"."pfoo1" ("i" INTEGER);
CREATE TABLE "sys"."pfoo2" ("i" INTEGER);
create function "sfoo"."func1" (x int, y int) returns int begin return x + y; end;
create view "sfoo"."baz" (i) as select func1(t.x, t.y) from (values (10, 1), (20, 2)) as t(x,y);
create function "sfoo"."func2" () returns table(i integer) begin return select * from "sfoo"."baz"; end;
CREATE TABLE "sys"."lower_scorers" ("name" CHARACTER LARGE OBJECT, "first_score" INTEGER, "second_score" INTEGER);
CREATE TABLE "sys"."higher_scorers" ("name" CHARACTER LARGE OBJECT, "first_score" INTEGER, "second_score" INTEGER);
CREATE TABLE "sys"."unknown_scorers" ("name" CHARACTER LARGE OBJECT, "first_score" INTEGER, "second_score" INTEGER);
CREATE TABLE "sfoo"."foo" ("fi" INTEGER NOT NULL, "fs" CHARACTER LARGE OBJECT NOT NULL);
CREATE TABLE "sbar"."bar" ("bi" INTEGER NOT NULL, "bs" CHARACTER LARGE OBJECT NOT NULL);
CREATE REMOTE TABLE "sys"."rfoo" ("i" INTEGER) ON  'mapi:monetdb://remote.host.url:50000/dbname'  WITH USER  'bob'  ENCRYPTED PASSWORD  'f8e3183d38e6c51889582cb260ab825252f395b4ac8fb0e6b13e9a71f7c10a80d5301e4a949f2783cb0c20205f1d850f87045f4420ad2271c8fd5f0cd8944be3' ;
CREATE MERGE TABLE "sys"."scorers" ("name" CHARACTER LARGE OBJECT, "first_score" INTEGER, "second_score" INTEGER) PARTITION BY VALUES USING ("sys"."mod"("sys"."greatest"("first_score","second_score"),10));
CREATE MERGE TABLE "sys"."splitted" ("stamp" TIMESTAMP, "val" INTEGER) PARTITION BY RANGE ON ("stamp");
CREATE MERGE TABLE "sys"."m1" ("i" INTEGER);
CREATE REPLICA TABLE "sys"."rep" ("i" INTEGER);
CREATE TABLE "sys"."first_decade" ("stamp" TIMESTAMP, "val" INTEGER);
CREATE TABLE "sys"."second_decade" ("stamp" TIMESTAMP, "val" INTEGER);
CREATE TABLE "sys"."third_decade" ("stamp" TIMESTAMP, "val" INTEGER);
CREATE TABLE "sys"."p1" ("i" INTEGER);
create or replace window "sys"."stddev" (val bigint) returns double external name "sql"."stdevp";
CREATE TABLE "sys"."foo" ("i" INTEGER, "j" INTEGER);
create function "sys"."f1" () returns int begin return 10; end;
create procedure "sys"."f1" (i int) begin declare x int; end;
create procedure "sys"."f1" () begin declare x int; end;
ALTER TABLE "sys"."yoyo" ALTER COLUMN "tsz" SET DEFAULT 'BLABOLO';
ALTER TABLE "sys"."bolo" ADD CONSTRAINT "cpk" PRIMARY KEY ("s", "v");
ALTER TABLE "sys"."rolo" ADD CONSTRAINT "rolo_v_pkey" PRIMARY KEY ("v");
ALTER TABLE "sys"."ungolo" ADD CONSTRAINT "ungolo_x_y_unique" UNIQUE ("x", "y");
ALTER TABLE "sys"."ungolo" ADD CONSTRAINT "ungolo_z_unique" UNIQUE ("z");
ALTER TABLE "sfoo"."foo" ADD CONSTRAINT "foo_pk" PRIMARY KEY ("fi", "fs");
ALTER TABLE "sbar"."bar" ADD CONSTRAINT "bar_pk" PRIMARY KEY ("bi", "bs");
CREATE INDEX "ind1" ON "sys"."ungolo"(x,y);
CREATE IMPRINTS INDEX "ind2" ON "sys"."ungolo"(y,z);
CREATE ORDERED INDEX "ind3" ON "sys"."ungolo"(x,z);
ALTER TABLE "sfoo"."foo" ADD CONSTRAINT "fk_foo_to_bar" FOREIGN KEY("fi","fs") REFERENCES "sbar"."bar"("bi","bs") ON DELETE SET NULL ON UPDATE CASCADE;
ALTER TABLE "sys"."scorers"  ADD TABLE "sys"."unknown_scorers" AS PARTITION FOR NULL VALUES;
ALTER TABLE "sys"."scorers"  ADD TABLE "sys"."lower_scorers" AS PARTITION IN (0,1,2,3,4);
ALTER TABLE "sys"."scorers"  ADD TABLE "sys"."higher_scorers" AS PARTITION IN (5,6,7,8,9);
ALTER TABLE "sys"."splitted"  ADD TABLE "sys"."first_decade" AS PARTITION FROM RANGE MINVALUE TO  '2010-01-01 00:00:00.000000'  WITH NULL VALUES;
ALTER TABLE "sys"."splitted"  ADD TABLE "sys"."second_decade" AS PARTITION FROM  '2010-01-01 00:00:00.000000'  TO  '2020-01-01 00:00:00.000000' ;
ALTER TABLE "sys"."m1"  ADD TABLE "sys"."p1";
create trigger extra_insert after insert on "sfoo"."tfoo1" referencing new row as new_row for each statement insert into tfoo2(i) values (new_row.i);
COMMENT ON SCHEMA "sbar" IS  'This is a comment on a schema' ;
COMMENT ON COLUMN "sfoo"."tfoo1"."i" IS  'This is a comment on a column.' ;
COMMENT ON INDEX "sys"."ind3" IS  'This is a comment on an index.' ;
COMMENT ON SEQUENCE "sys"."seq1" IS  'This is a comment on a sequence.' ;
COMMENT ON WINDOW "sys"."stddev" IS  'This is a comment on a window function.' ;
TRUNCATE sys.privileges;
INSERT INTO sys.privileges VALUES ((SELECT t.id FROM sys.schemas s, tables t WHERE s.id = t.schema_id AND s.name || '.' || t.name = 'sys.foo' ),(SELECT id FROM auths a WHERE a.name =  'voc' ),(SELECT pc.privilege_code_id FROM privilege_codes pc WHERE pc.privilege_code_name =  'SELECT' ),(SELECT id FROM auths g WHERE g.name =  'monetdb' ),false);
INSERT INTO sys.privileges VALUES ((SELECT c.id FROM sys.schemas s, tables t, columns c WHERE s.id = t.schema_id AND t.id = c.table_id AND s.name || '.' || t.name || '.' || c.name = 'sys.foo.i' ),(SELECT id FROM auths a WHERE a.name =  'voc' ),(SELECT pc.privilege_code_id FROM privilege_codes pc WHERE pc.privilege_code_name =  'UPDATE' ),(SELECT id FROM auths g WHERE g.name =  'monetdb' ),false);
INSERT INTO sys.privileges VALUES ((SELECT fqn.id FROM fully_qualified_functions() fqn WHERE fqn.nme =  'sys.f1(INTEGER)'  AND fqn.tpe =  'FUNCTION' ),(SELECT id FROM auths a WHERE a.name =  'voc' ),(SELECT pc.privilege_code_id FROM privilege_codes pc WHERE pc.privilege_code_name =  'EXECUTE' ),(SELECT id FROM auths g WHERE g.name =  'monetdb' ),false);
INSERT INTO sys.privileges VALUES ((SELECT fqn.id FROM fully_qualified_functions() fqn WHERE fqn.nme =  'sys.f1(INTEGER)'  AND fqn.tpe =  'PROCEDURE' ),(SELECT id FROM auths a WHERE a.name =  'voc' ),(SELECT pc.privilege_code_id FROM privilege_codes pc WHERE pc.privilege_code_name =  'EXECUTE' ),(SELECT id FROM auths g WHERE g.name =  'monetdb' ),false);
INSERT INTO sys.privileges VALUES ((SELECT fqn.id FROM fully_qualified_functions() fqn WHERE fqn.nme =  'sys.f1()'  AND fqn.tpe =  'PROCEDURE' ),(SELECT id FROM auths a WHERE a.name =  'voc' ),(SELECT pc.privilege_code_id FROM privilege_codes pc WHERE pc.privilege_code_name =  'EXECUTE' ),(SELECT id FROM auths g WHERE g.name =  'monetdb' ),true);
INSERT INTO sys.privileges VALUES (0,(SELECT id FROM auths a WHERE a.name =  'voc' ),(SELECT pc.privilege_code_id FROM privilege_codes pc WHERE pc.privilege_code_name =  'UPDATE' ),(SELECT id FROM auths g WHERE g.name =  'monetdb' ),false);
COMMIT;
