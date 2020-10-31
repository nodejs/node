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

-- Create all the views used to aggregate CPU data.
-- View with start and end ts for each cpu frequency, per cpu.
CREATE VIEW cpu_freq_view AS
SELECT
  cpu,
  ts,
  LEAD(ts, 1, (SELECT end_ts from trace_bounds))
    OVER (PARTITION by cpu ORDER BY ts) - ts AS dur,
  CAST(value AS INT) as freq_khz
FROM counter
JOIN cpu_counter_track on counter.track_id = cpu_counter_track.id
WHERE name = 'cpufreq';

-- View that joins the cpufreq table with the slice table.
CREATE VIRTUAL TABLE cpu_freq_sched_per_thread
USING SPAN_LEFT_JOIN(sched PARTITIONED cpu, cpu_freq_view PARTITIONED cpu);
