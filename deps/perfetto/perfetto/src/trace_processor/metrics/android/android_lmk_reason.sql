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

SELECT RUN_METRIC('android/android_ion.sql');
SELECT RUN_METRIC('android/android_lmk.sql');
SELECT RUN_METRIC('android/process_mem.sql');
SELECT RUN_METRIC('android/process_metadata.sql');

CREATE VIEW IF NOT EXISTS android_lmk_reason_ooms AS
SELECT
lmk_events.ts,
oom_score_span.upid,
oom_score_val
FROM lmk_events
JOIN oom_score_span
WHERE lmk_events.ts
  BETWEEN oom_score_span.ts
  AND oom_score_span.ts + MAX(oom_score_span.dur - 1, 0);

CREATE VIEW IF NOT EXISTS android_lmk_reason_output AS
WITH
lmk_ion_sizes AS (
  SELECT
  lmk_events.ts,
  CAST(ion_timeline.value as int) system_ion_heap_size
  FROM lmk_events
  JOIN ion_timeline
  WHERE ion_timeline.heap_name = 'system'
  AND lmk_events.ts
    BETWEEN ion_timeline.ts
    AND ion_timeline.ts + MAX(ion_timeline.dur - 1, 0)
),
lmk_process_sizes AS (
  SELECT
  lmk_events.ts,
  rss_and_swap_span.upid,
  rss_and_swap_val
  FROM lmk_events
  JOIN rss_and_swap_span
  WHERE lmk_events.ts
    BETWEEN rss_and_swap_span.ts
    AND rss_and_swap_span.ts + MAX(rss_and_swap_span.dur - 1, 0)
),
lmk_process_sizes_output AS (
  SELECT ts, RepeatedField(AndroidLmkReasonMetric_Process(
    'process', metadata,
    'oom_score_adj', oom_score_val,
    'size', rss_and_swap_val
  )) processes
  FROM lmk_process_sizes
  JOIN process_metadata USING (upid)
  LEFT JOIN android_lmk_reason_ooms USING (ts, upid)
  GROUP BY ts
)
SELECT AndroidLmkReasonMetric(
  'lmks', (
    SELECT RepeatedField(AndroidLmkReasonMetric_Lmk(
      'oom_score_adj', oom_score_val,
      'system_ion_heap_size', system_ion_heap_size,
      'processes', processes
    ))
    FROM lmk_events
    LEFT JOIN android_lmk_reason_ooms USING (ts, upid)
    LEFT JOIN lmk_ion_sizes USING (ts)
    LEFT JOIN lmk_process_sizes_output USING (ts)
    WHERE oom_score_val IS NOT NULL
    ORDER BY ts
  )
);
