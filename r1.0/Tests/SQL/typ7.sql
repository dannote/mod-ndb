
use mod_ndb_tests;
 
CREATE TABLE typ7 (
  id int not null primary key,
  vc01 varchar(2000),
  ts timestamp not null
) engine = ndbcluster ;