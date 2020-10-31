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

DROP TABLE IF EXISTS {{table_name}}_by_priority_stats;

CREATE TABLE {{table_name}}_by_priority_stats (
  process_name TEXT,
  priority TEXT,
  min_value REAL,
  max_value REAL,
  avg_value REAL,
  PRIMARY KEY (process_name, priority)
);

INSERT INTO {{table_name}}_by_priority_stats
SELECT
  process.name AS process_name,
  CASE
    WHEN oom_score_val < -900 THEN 'native'
    WHEN oom_score_val < -800 THEN 'system'
    WHEN oom_score_val < -700 THEN 'persistent'
    WHEN oom_score_val <  0   THEN 'persistent_service'
    WHEN oom_score_val <  100 THEN 'foreground'
    WHEN oom_score_val <  200 THEN 'visible'
    WHEN oom_score_val <  300 THEN 'perceptible'
    WHEN oom_score_val <  400 THEN 'backup'
    WHEN oom_score_val <  500 THEN 'heavy_weight'
    WHEN oom_score_val <  600 THEN 'service_a'
    WHEN oom_score_val <  700 THEN 'home'
    WHEN oom_score_val <  800 THEN 'prev'
    WHEN oom_score_val <  900 THEN 'service_b'
    ELSE 'cached'
  END AS priority,
  MIN(span.{{table_name}}_val) AS min_value,
  MAX(span.{{table_name}}_val) AS max_value,
  SUM(span.{{table_name}}_val * span.dur) / SUM(span.dur) AS avg_value
FROM {{table_name}}_by_oom_span AS span JOIN process USING(upid)
WHERE process.name IS NOT NULL
GROUP BY 1, 2
ORDER BY 1, 2;

DROP VIEW IF EXISTS {{table_name}}_by_priority_stats_proto;

CREATE VIEW {{table_name}}_by_priority_stats_proto AS
SELECT
  process_name,
  priority,
  AndroidMemoryMetric_Counter(
    'min', min_value,
    'max', max_value,
    'avg', avg_value
  ) AS proto
FROM {{table_name}}_by_priority_stats;
