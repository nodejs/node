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

DROP VIEW IF EXISTS {{table_name}}_span;

CREATE VIEW {{table_name}}_span AS
SELECT
  ts,
  LEAD(ts, 1, (
    SELECT IFNULL(
      end_ts,
      (SELECT end_ts FROM trace_bounds)
    )
    FROM process p WHERE p.upid = t.upid) + 1
  ) OVER(PARTITION BY track_id ORDER BY ts) - ts AS dur,
  upid,
  value AS {{table_name}}_val
FROM counter c JOIN process_counter_track t
  ON t.id = c.track_id
WHERE name = '{{counter_name}}' AND upid IS NOT NULL;
