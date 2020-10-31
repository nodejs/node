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

-- Spans for each thread not in a running state.
CREATE TABLE unsched_state (ts BIG INT, dur BIG INT, utid BIG INT, state STRING);

INSERT INTO unsched_state
SELECT
  ts_end AS ts,
  LEAD(ts, 1, null) OVER(PARTITION BY utid ORDER BY ts) - ts_end AS dur,
  utid,
  CASE
    WHEN INSTR(end_state, 'R') > 0 THEN 'runnable'
    WHEN INSTR(end_state, 'D') > 0 THEN 'uninterruptible'
    WHEN INSTR(end_state, 'S') > 0 THEN 'interruptible'
  END AS state
FROM sched;

-- Create a single view for the task states.
CREATE VIEW task_state AS
SELECT utid, state, ts, dur FROM unsched_state
UNION
SELECT utid, 'running', ts, dur FROM sched
ORDER BY ts;
