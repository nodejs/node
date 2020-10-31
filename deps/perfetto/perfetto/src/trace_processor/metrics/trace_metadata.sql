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

CREATE VIEW IF NOT EXISTS error_stats_view AS
SELECT TraceMetadata_Entry(
  'name', name,
  'idx', idx,
  'value', value) as entry
FROM stats
WHERE severity IN ('data_loss', 'error')
AND value > 0;

CREATE VIEW IF NOT EXISTS trace_metadata_output AS
SELECT TraceMetadata(
  'error_stats_entry', (SELECT RepeatedField(entry) FROM error_stats_view),
  'trace_duration_ns', (SELECT end_ts - start_ts FROM trace_bounds),
  'trace_uuid', (SELECT str_value FROM metadata WHERE name = 'trace_uuid'),
  'android_build_fingerprint', (
    SELECT str_value FROM metadata WHERE name = 'android_build_fingerprint'
  ),
  'statsd_triggering_subscription_id', (
    SELECT int_value FROM metadata
    WHERE name = 'statsd_triggering_subscription_id'
  ),
 'trace_size_bytes', (
    SELECT int_value FROM metadata
    WHERE name = 'trace_size_bytes'
  ),
  'trace_trigger', (
    SELECT RepeatedField(slice.name)
    FROM track JOIN slice ON track.id = slice.track_id
    WHERE track.name = 'Trace Triggers'
  )
);
