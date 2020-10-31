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
CREATE TABLE null_test (
  primary_key INTEGER PRIMARY KEY,
  int_nulls INTEGER,
  string_nulls STRING,
  double_nulls DOUBLE,
  start_int_nulls INTEGER,
  start_string_nulls STRING,
  start_double_nulls DOUBLE,
  all_nulls INTEGER
);

INSERT INTO null_test(
  int_nulls,
  string_nulls,
  double_nulls,
  start_int_nulls,
  start_string_nulls,
  start_double_nulls
)
VALUES
(1,     "test",   2.0,  NULL, NULL,   NULL),
(2,     NULL,     NULL, NULL, "test", NULL),
(1,     "other",  NULL, NULL, NULL,   NULL),
(4,     NULL,     NULL, NULL, NULL,   1.0),
(NULL,  "test",   1.0,  1,    NULL,   NULL)

SELECT * from null_test;
