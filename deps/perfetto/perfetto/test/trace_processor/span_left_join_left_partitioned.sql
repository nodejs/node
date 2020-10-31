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
  b BIG INT,
  PRIMARY KEY (ts)
) without rowid;

-- Then insert some rows into t1 in part 1, 3, 4 and 5.
INSERT INTO t1(ts, dur, part, a)
VALUES
(100, 400, 1, 111),
(500, 150, 1, 111),
(500, 50, 3, 222),
(500, 100, 4, 333),
(0, 1000, 5, 444);

-- Then insert some rows into t2.
INSERT INTO t2(ts, dur, b)
VALUES
(50, 200, 111),
(300, 100, 222),
(400, 250, 333);

create virtual table sp using span_left_join(t1 PARTITIONED part, t2);

select * from sp;
