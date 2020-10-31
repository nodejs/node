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

SELECT RUN_METRIC('android/process_oom_score.sql');

-- Create all the views used to for LMK related stuff.
CREATE TABLE IF NOT EXISTS lmk_events AS
SELECT ref AS upid, MIN(ts) AS ts
FROM instants
WHERE name = 'mem.lmk' AND ref_type = 'upid'
GROUP BY 1;

CREATE VIEW IF NOT EXISTS lmk_by_score AS
SELECT lmk_events.upid, oom_scores.oom_score_val AS score
FROM lmk_events
LEFT JOIN oom_score_span oom_scores
  ON (lmk_events.upid = oom_scores.upid AND
      lmk_events.ts >= oom_scores.ts AND
      lmk_events.ts < oom_scores.ts + oom_scores.dur)
ORDER BY lmk_events.upid;

CREATE VIEW IF NOT EXISTS lmk_counts AS
SELECT score, COUNT(1) AS count
FROM lmk_by_score
GROUP BY score;

CREATE VIEW IF NOT EXISTS android_lmk_output AS
SELECT AndroidLmkMetric(
  'total_count', (SELECT COUNT(1) FROM lmk_events),
  'by_oom_score', (
    SELECT
      RepeatedField(AndroidLmkMetric_ByOomScore(
        'oom_score_adj', score,
        'count', count
      ))
    FROM lmk_counts
    WHERE score IS NOT NULL
  ),
  'oom_victim_count', (
    SELECT COUNT(1) FROM instants WHERE name = 'mem.oom_kill'
  )
);
