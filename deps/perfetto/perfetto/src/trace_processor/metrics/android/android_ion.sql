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

CREATE VIEW IF NOT EXISTS ion_timeline AS
SELECT
  ts,
  LEAD(ts, 1, (SELECT end_ts FROM trace_bounds))
    OVER(PARTITION BY track_id ORDER BY ts) - ts AS dur,
  SUBSTR(name, 9) AS heap_name,
  track_id,
  value
FROM counter JOIN counter_track
  ON counter.track_id = counter_track.id
WHERE name LIKE 'mem.ion.%';

CREATE VIEW IF NOT EXISTS ion_heap_stats AS
SELECT
  heap_name,
  SUM(value * dur) / SUM(dur) AS avg_size,
  MIN(value) AS min_size,
  MAX(value) AS max_size
FROM ion_timeline
GROUP BY 1;

CREATE VIEW IF NOT EXISTS ion_raw_allocs AS
SELECT
  SUBSTR(name, 16) AS heap_name,
  ts,
  value AS instant_value,
  SUM(value) OVER win AS value
FROM counter c JOIN thread_counter_track t ON c.track_id = t.id
WHERE name LIKE 'mem.ion_change.%' AND value > 0
WINDOW win AS (
  PARTITION BY SUBSTR(name, 16) ORDER BY ts
  ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
);

CREATE VIEW IF NOT EXISTS ion_alloc_stats AS
SELECT
  heap_name,
  SUM(instant_value) AS total_alloc_size_bytes
FROM ion_raw_allocs
GROUP BY 1;

CREATE VIEW IF NOT EXISTS android_ion_annotations AS
SELECT
  'counter' AS track_type,
  printf('ION allocations (heap: %s)', heap_name) AS track_name,
  ts,
  value
FROM ion_raw_allocs;

CREATE VIEW IF NOT EXISTS android_ion_output AS
SELECT AndroidIonMetric(
  'buffer', RepeatedField(
    AndroidIonMetric_Buffer(
      'name', heap_name,
      'avg_size_bytes', avg_size,
      'min_size_bytes', min_size,
      'max_size_bytes', max_size,
      'total_alloc_size_bytes', total_alloc_size_bytes
    )
  ))
FROM ion_heap_stats JOIN ion_alloc_stats USING (heap_name);
