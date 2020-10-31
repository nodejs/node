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
SELECT
  ts,
  name,
  string_value,
  int_value
FROM (
  SELECT
    slice.arg_set_id,
    slice.track_id,
    slice.ts,
    slice.name,
    prod.string_value
  FROM slice JOIN (
    SELECT
      arg_set_id,
      string_value
    FROM args
    WHERE key = "producer_name"
  ) prod ON slice.arg_set_id = prod.arg_set_id
) slice_prod JOIN (
  SELECT
    arg_set_id,
    int_value
  FROM args
  WHERE key = "trusted_producer_uid"
) prod_uid ON prod_uid.arg_set_id = slice_prod.arg_set_id
WHERE slice_prod.track_id in (
  SELECT id FROM track WHERE name = "Trace Triggers"
)
ORDER BY ts ASC;
