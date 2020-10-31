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

SELECT RUN_METRIC('android/process_mem.sql');

SELECT RUN_METRIC('android/process_unagg_mem_view.sql',
  'table_name', 'anon_rss');

SELECT RUN_METRIC('android/process_unagg_mem_view.sql',
  'table_name', 'swap');

SELECT RUN_METRIC('android/process_unagg_mem_view.sql',
  'table_name', 'file_rss');

SELECT RUN_METRIC('android/process_unagg_mem_view.sql',
  'table_name', 'anon_and_swap');

CREATE VIEW process_unagg_metrics_view AS
SELECT
  AndroidMemoryUnaggregatedMetric_ProcessValues(
    'process_name', process.name,
    'mem_values', AndroidMemoryUnaggregatedMetric_ProcessMemoryValues(
      'anon_rss', anon_rss_unagg_values.metric,
      'swap', swap_unagg_values.metric,
      'file_rss', file_rss_unagg_values.metric,
      'anon_and_swap', anon_and_swap_unagg_values.metric
    )
  ) AS metric
FROM
  process
LEFT JOIN
  anon_rss_unagg_values USING(upid)
LEFT JOIN
  swap_unagg_values USING(upid)
LEFT JOIN
  file_rss_unagg_values USING(upid)
LEFT JOIN
  anon_and_swap_unagg_values USING(upid)
WHERE
  process.name IS NOT NULL;

CREATE VIEW android_mem_unagg_output AS
SELECT
  AndroidMemoryUnaggregatedMetric(
    'process_values',
    (SELECT RepeatedField(metric) FROM process_unagg_metrics_view)
  );
