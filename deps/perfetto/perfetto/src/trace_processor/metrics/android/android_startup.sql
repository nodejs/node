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

-- Create the base tables and views containing the launch spans.
SELECT RUN_METRIC('android/android_startup_launches.sql');
SELECT RUN_METRIC('android/android_task_state.sql');
SELECT RUN_METRIC('android/process_metadata.sql');
SELECT RUN_METRIC('android/hsc_startups.sql');

-- Slices for forked processes. Never present in hot starts.
-- Prefer this over process start_ts, since the process might have
-- been preforked.
CREATE VIEW zygote_fork_slice AS
SELECT slice.ts, slice.dur, STR_SPLIT(slice.name, ": ", 1) AS process_name
FROM slice WHERE name LIKE 'Start proc: %';

CREATE TABLE zygote_forks_by_id AS
SELECT
  launches.id,
  zygote_fork_slice.ts,
  zygote_fork_slice.dur
FROM zygote_fork_slice
JOIN launches
ON (launches.ts < zygote_fork_slice.ts
    AND zygote_fork_slice.ts + zygote_fork_slice.dur < launches.ts_end
    AND zygote_fork_slice.process_name = launches.package
);

CREATE VIEW launch_main_threads AS
SELECT
  launches.ts AS ts,
  launches.dur AS dur,
  launches.id AS launch_id,
  thread.utid AS utid
FROM launches
JOIN launch_processes ON launches.id = launch_processes.launch_id
JOIN process USING(upid)
JOIN thread ON (process.upid = thread.upid AND process.pid = thread.tid)
ORDER BY ts;

CREATE VIRTUAL TABLE main_thread_state
USING SPAN_JOIN(
  launch_main_threads PARTITIONED utid,
  task_state PARTITIONED utid);

CREATE VIEW launch_by_thread_state AS
SELECT launch_id, state, SUM(dur) AS dur
FROM main_thread_state
GROUP BY 1, 2;

-- Tracks all slices for the main process threads
CREATE TABLE main_process_slice AS
SELECT
  launches.id AS launch_id,
  slice.name AS name,
  AndroidStartupMetric_Slice(
    'dur_ns', SUM(slice.dur),
    'dur_ms', SUM(slice.dur) / 1e6
  ) AS slice_proto
FROM launches
JOIN launch_processes ON (launches.id = launch_processes.launch_id)
JOIN thread ON (launch_processes.upid = thread.upid)
JOIN thread_track USING (utid)
JOIN slice ON (
  slice.track_id = thread_track.id
  AND slice.ts BETWEEN launches.ts AND launches.ts + launches.dur)
WHERE slice.name IN (
  'PostFork',
  'ActivityThreadMain',
  'bindApplication',
  'activityStart',
  'activityResume',
  'Choreographer#doFrame',
  'inflate')
GROUP BY 1, 2;

CREATE VIEW to_event_protos AS
SELECT
  slice.name as slice_name,
  launch_id,
  AndroidStartupMetric_Slice(
    'dur_ns', slice.ts - l.ts,
    'dur_ms', (slice.ts - l.ts) / 1e6
  ) as slice_proto
FROM launch_main_threads l
JOIN thread_track USING (utid)
JOIN slice ON (
  slice.track_id = thread_track.id
  AND slice.ts BETWEEN l.ts AND l.ts + l.dur);

CREATE VIEW startup_view AS
SELECT
  AndroidStartupMetric_Startup(
    'startup_id', launches.id,
    'package_name', launches.package,
    'process_name', (
      SELECT name FROM process
      WHERE upid IN (
        SELECT upid FROM launch_processes
        WHERE launch_id = launches.id
        LIMIT 1
      )
    ),
    'process', (
      SELECT metadata FROM process_metadata
      WHERE upid IN (
        SELECT upid FROM launch_processes
        WHERE launch_id = launches.id
        LIMIT 1
      )
    ),
    'zygote_new_process', EXISTS(SELECT TRUE FROM zygote_forks_by_id WHERE id = launches.id),
    'activity_hosting_process_count', (
      SELECT COUNT(1) FROM launch_processes WHERE launch_id = launches.id
    ),
    'to_first_frame', AndroidStartupMetric_ToFirstFrame(
      'dur_ns', launches.dur,
      'dur_ms', launches.dur / 1e6,
      'main_thread_by_task_state', AndroidStartupMetric_TaskStateBreakdown(
        'running_dur_ns', IFNULL(
            (
            SELECT dur FROM launch_by_thread_state
            WHERE launch_id = launches.id AND state = 'running'
            ), 0),
        'runnable_dur_ns', IFNULL(
            (
            SELECT dur FROM launch_by_thread_state
            WHERE launch_id = launches.id AND state = 'runnable'
            ), 0),
        'uninterruptible_sleep_dur_ns', IFNULL(
            (
            SELECT dur FROM launch_by_thread_state
            WHERE launch_id = launches.id AND state = 'uninterruptible'
            ), 0),
        'interruptible_sleep_dur_ns', IFNULL(
            (
            SELECT dur FROM launch_by_thread_state
            WHERE launch_id = launches.id AND state = 'interruptible'
            ), 0)
      ),
      'to_post_fork', (
        SELECT slice_proto
        FROM to_event_protos
        WHERE launch_id = launches.id AND slice_name = 'PostFork'
      ),
      'to_activity_thread_main', (
        SELECT slice_proto
        FROM to_event_protos
        WHERE launch_id = launches.id AND slice_name = 'ActivityThreadMain'
      ),
      'to_bind_application', (
        SELECT slice_proto
        FROM to_event_protos
        WHERE launch_id = launches.id AND slice_name = 'bindApplication'
      ),
      'other_processes_spawned_count', (
        SELECT COUNT(1) FROM process
        WHERE (process.name IS NULL OR process.name != launches.package)
        AND process.start_ts BETWEEN launches.ts AND launches.ts + launches.dur
      ),
      'time_activity_manager', (
        SELECT AndroidStartupMetric_Slice(
          'dur_ns', launching_events.ts - launches.ts,
          'dur_ms', (launching_events.ts - launches.ts) / 1e6
        )
        FROM launching_events
        WHERE launching_events.ts BETWEEN launches.ts AND launches.ts + launches.dur
      ),
      'time_post_fork', (
        SELECT slice_proto FROM main_process_slice
        WHERE launch_id = launches.id AND name = 'PostFork'
      ),
      'time_activity_thread_main', (
        SELECT slice_proto FROM main_process_slice
        WHERE launch_id = launches.id AND name = 'ActivityThreadMain'
      ),
      'time_bind_application', (
        SELECT slice_proto FROM main_process_slice
        WHERE launch_id = launches.id AND name = 'bindApplication'
      ),
      'time_activity_start', (
        SELECT slice_proto FROM main_process_slice
        WHERE launch_id = launches.id AND name = 'activityStart'
      ),
      'time_activity_resume', (
        SELECT slice_proto FROM main_process_slice
        WHERE launch_id = launches.id AND name = 'activityResume'
      ),
      'time_choreographer', (
        SELECT slice_proto FROM main_process_slice
        WHERE launch_id = launches.id AND name = 'Choreographer#doFrame'
      ),
      'time_before_start_process', (
        SELECT AndroidStartupMetric_Slice(
          'dur_ns', ts - launches.ts,
          'dur_ms', (ts - launches.ts) / 1e6
        )
        FROM zygote_forks_by_id WHERE id = launches.id
      ),
      'time_during_start_process', (
        SELECT AndroidStartupMetric_Slice(
          'dur_ns', dur,
          'dur_ms', dur / 1e6
        )
        FROM zygote_forks_by_id WHERE id = launches.id
      )
    ),
    'hsc', (
      SELECT AndroidStartupMetric_HscMetrics(
        'full_startup', (
          SELECT AndroidStartupMetric_Slice(
            'dur_ns', hsc_based_startup_times.ts_total,
            'dur_ms', hsc_based_startup_times.ts_total / 1e6
          )
          FROM hsc_based_startup_times WHERE id = launches.id
        )
      )
    )
  ) as startup
FROM launches;

CREATE VIEW android_startup_annotations AS
SELECT
  'slice' as track_type,
  'Android App Startups' as track_name,
  l.ts as ts,
  l.dur as dur,
  l.package as slice_name
FROM launches l;

CREATE VIEW android_startup_output AS
SELECT
  AndroidStartupMetric(
    'startup', (
      SELECT RepeatedField(startup) FROM startup_view
    )
  );
