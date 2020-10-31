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

-- Create a track for process OOM scores.
CREATE VIEW IF NOT EXISTS oom_score_span AS
SELECT
  ts,
  LEAD(ts, 1, (SELECT end_ts + 1 FROM trace_bounds))
    OVER(PARTITION BY track_id ORDER BY ts) - ts AS dur,
  upid,
  CAST(value AS INT) AS oom_score_val
FROM counter c JOIN process_counter_track t
  ON c.track_id = t.id
WHERE name = 'oom_score_adj' AND upid IS NOT NULL;
