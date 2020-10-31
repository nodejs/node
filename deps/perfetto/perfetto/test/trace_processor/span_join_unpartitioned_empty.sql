--
-- Copyright 2020 The Android Open Source Project
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

-- Model that the first table is empty and the second is has some data.
create table t1(
  ts BIG INT,
  dur BIG INT,
  PRIMARY KEY (ts, dur)
) without rowid;

create table t2(
  ts BIG INT,
  dur BIG INT,
  PRIMARY KEY (ts, dur)
) without rowid;

INSERT INTO t2(ts, dur)
VALUES
(1, 2),
(5, 0),
(1, 1);

create virtual table sp using span_join(t1, t2);

select ts, dur from sp;
