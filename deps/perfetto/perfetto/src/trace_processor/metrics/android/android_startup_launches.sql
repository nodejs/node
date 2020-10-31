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

-- The start of the launching event corresponds to the end of the AM handling
-- the startActivity intent, whereas the end corresponds to the first frame drawn.
-- Only successful app launches have a launching event.
CREATE TABLE launching_events AS
SELECT
  ts,
  dur,
  ts + dur AS ts_end,
  STR_SPLIT(s.name, ": ", 1) AS package_name
FROM slice s
JOIN process_track t ON s.track_id = t.id
JOIN process USING(upid)
WHERE s.name LIKE 'launching: %'
AND (process.name IS NULL OR process.name = 'system_server');

-- Marks the beginning of the trace and is equivalent to when the statsd launch
-- logging begins.
CREATE VIEW activity_intent_received AS
SELECT ts FROM slice
WHERE name = 'MetricsLogger:launchObserverNotifyIntentStarted';

-- Successful activity launch. The end of the 'launching' event is not related
-- to whether it actually succeeded or not.
CREATE VIEW activity_intent_launch_successful AS
SELECT ts FROM slice
WHERE name = 'MetricsLogger:launchObserverNotifyActivityLaunchFinished';

-- We partition the trace into spans based on posted activity intents.
-- We will refine these progressively in the next steps to only encompass
-- activity starts.
CREATE TABLE activity_intent_recv_spans(id INT, ts BIG INT, dur BIG INT);

INSERT INTO activity_intent_recv_spans
SELECT
  ROW_NUMBER()
    OVER(ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) AS id,
  ts,
  LEAD(ts, 1, (SELECT end_ts FROM trace_bounds)) OVER(ORDER BY ts) - ts AS dur
FROM activity_intent_received
ORDER BY ts;

-- Filter activity_intent_recv_spans, keeping only the ones that triggered
-- a launch.
CREATE VIEW launch_partitions AS
SELECT * FROM activity_intent_recv_spans AS spans
WHERE 1 = (
  SELECT COUNT(1)
  FROM launching_events
  WHERE launching_events.ts BETWEEN spans.ts AND spans.ts + spans.dur);

-- All activity launches in the trace, keyed by ID.
CREATE TABLE launches(
  ts BIG INT,
  ts_end BIG INT,
  dur BIG INT,
  id INT,
  package STRING);

-- Use the starting event package name. The finish event package name
-- is not reliable in the case of failed launches.
INSERT INTO launches
SELECT
  lpart.ts AS ts,
  launching_events.ts_end AS ts_end,
  launching_events.ts_end - lpart.ts AS dur,
  lpart.id AS id,
  package_name AS package
FROM launch_partitions AS lpart
JOIN launching_events ON
  (launching_events.ts BETWEEN lpart.ts AND lpart.ts + lpart.dur) AND
  (launching_events.ts_end BETWEEN lpart.ts AND lpart.ts + lpart.dur)
JOIN activity_intent_launch_successful AS successful
  ON successful.ts BETWEEN lpart.ts AND lpart.ts + lpart.dur;

-- Maps a launch to the corresponding set of processes that handled the
-- activity start. The vast majority of cases should be a single process.
-- However it is possible that the process dies during the activity launch
-- and is respawned.
CREATE TABLE launch_processes(launch_id INT, upid BIG INT);

-- We make the (not always correct) simplification that process == package
INSERT INTO launch_processes
SELECT launches.id, process.upid
FROM launches
  JOIN process ON launches.package = process.name
  JOIN thread ON (process.upid = thread.upid AND process.pid = thread.tid)
WHERE (process.start_ts IS NULL OR process.start_ts < launches.ts_end)
AND (thread.end_ts IS NULL OR thread.end_ts > launches.ts_end)
ORDER BY process.start_ts DESC;
