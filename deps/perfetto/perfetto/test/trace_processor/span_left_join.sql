--
-- Copyright 2019 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
create table t1(
  ts BIG INT,
  dur BIG INT,
  part BIG INT,
  a BIG INT,
  PRIMARY KEY (part, ts)
) without rowid;

create table t2(
  ts BIG INT,
  dur BIG INT,
  part BIG INT,
  b BIG INT,
  PRIMARY KEY (part, ts)
) without rowid;

-- Insert some rows into t2 which are in part 0 and 1 but before t1's rows.
INSERT INTO t2(ts, dur, part, b)
VALUES
(0, 100, 0, 111),
(100, 200, 0, 222),
(0, 50, 1, 333);

-- Then insert some rows into t1 in part 1, 3, 4 and 5.
INSERT INTO t1(ts, dur, part, a)
VALUES
(100, 400, 1, 111),
(500, 50, 1, 222),
(600, 100, 1, 333),
(500, 100, 3, 444),
(100, 100, 4, 555),
(200, 50, 4, 666),
(250, 50, 4, 777),
(100, 100, 5, 888);

-- Insert a row into t2 which should be split up by t1's first row.
INSERT INTO t2(ts, dur, part, b) VALUES (50, 200, 1, 444);

-- Insert a row into t2 should should be completely covered by t1's first row.
INSERT INTO t2(ts, dur, part, b) VALUES (300, 100, 1, 555);

-- Insert a row into t2 which should span between t1's first and second rows.
INSERT INTO t2(ts, dur, part, b) VALUES (400, 250, 1, 666);

-- Insert a row into t2 in partition 2.
INSERT INTO t2(ts, dur, part, b) VALUES (100, 1000, 2, 777);

-- Insert a row into t2 before t1's first row in partition 4.
INSERT INTO t2(ts, dur, part, b) VALUES (50, 50, 4, 888);

-- Insert a row into t2 which perfectly matches the second row in partition 4.
INSERT INTO t2(ts, dur, part, b) VALUES (200, 50, 4, 999);

-- Insert a row into t2 which intersects the first row of partition 5.
INSERT INTO t2(ts, dur, part, b) VALUES (125, 50, 5, 1111);

-- Insert a row into t2 which intersects the first row of partition 5.
INSERT INTO t2(ts, dur, part, b) VALUES (190, 20, 5, 2222);

create virtual table sp using span_left_join(t1 PARTITIONED part,
                                             t2 PARTITIONED part);

select * from sp;
