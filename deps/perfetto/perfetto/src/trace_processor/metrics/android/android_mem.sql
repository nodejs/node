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

SELECT RUN_METRIC('android/span_view_stats.sql', 'table_name', 'anon_rss');

SELECT RUN_METRIC('android/span_view_stats.sql', 'table_name', 'file_rss');

SELECT RUN_METRIC('android/span_view_stats.sql', 'table_name', 'swap');

SELECT RUN_METRIC('android/span_view_stats.sql', 'table_name', 'anon_and_swap');

SELECT RUN_METRIC('android/span_view_stats.sql', 'table_name', 'java_heap');

SELECT RUN_METRIC('android/mem_stats_priority_breakdown.sql', 'table_name', 'anon_rss');

SELECT RUN_METRIC('android/mem_stats_priority_breakdown.sql', 'table_name', 'file_rss');

SELECT RUN_METRIC('android/mem_stats_priority_breakdown.sql', 'table_name', 'swap');

SELECT RUN_METRIC('android/mem_stats_priority_breakdown.sql', 'table_name', 'anon_and_swap');

SELECT RUN_METRIC('android/mem_stats_priority_breakdown.sql', 'table_name', 'java_heap');

-- Find out all process + priority pairs with data to drive the joins (no outer join in sqlite).
CREATE VIEW mem_all_processes AS
SELECT DISTINCT process_name
FROM
(
  SELECT process_name FROM anon_rss_stats_proto
  UNION
  SELECT process_name FROM file_rss_stats_proto
  UNION
  SELECT process_name FROM swap_stats_proto
  UNION
  SELECT process_name FROM anon_and_swap_stats_proto
  UNION
  SELECT process_name FROM java_heap_stats_proto
);

CREATE VIEW mem_all_process_priorities AS
SELECT DISTINCT process_name, priority
FROM
(
  SELECT process_name, priority FROM anon_rss_by_priority_stats_proto
  UNION
  SELECT process_name, priority FROM file_rss_by_priority_stats_proto
  UNION
  SELECT process_name, priority FROM swap_by_priority_stats_proto
  UNION
  SELECT process_name, priority FROM anon_and_swap_by_priority_stats_proto
  UNION
  SELECT process_name, priority FROM java_heap_by_priority_stats_proto
);

CREATE VIEW process_priority_view AS
SELECT
  process_name,
  AndroidMemoryMetric_PriorityBreakdown(
    'priority', priority,
    'counters', AndroidMemoryMetric_ProcessMemoryCounters(
      'anon_rss', anon_rss_by_priority_stats_proto.proto,
      'file_rss', file_rss_by_priority_stats_proto.proto,
      'swap', swap_by_priority_stats_proto.proto,
      'anon_and_swap', anon_and_swap_by_priority_stats_proto.proto,
      'java_heap', java_heap_by_priority_stats_proto.proto
    )
  ) AS priority_breakdown_proto
FROM mem_all_process_priorities
LEFT JOIN anon_rss_by_priority_stats_proto USING (process_name, priority)
LEFT JOIN file_rss_by_priority_stats_proto USING (process_name, priority)
LEFT JOIN swap_by_priority_stats_proto USING (process_name, priority)
LEFT JOIN anon_and_swap_by_priority_stats_proto USING (process_name, priority)
LEFT JOIN java_heap_by_priority_stats_proto USING (process_name, priority);

CREATE VIEW process_metrics_view AS
SELECT
  AndroidMemoryMetric_ProcessMetrics(
    'process_name', process_name,
    'total_counters', AndroidMemoryMetric_ProcessMemoryCounters(
      'anon_rss', anon_rss_stats_proto.proto,
      'file_rss', file_rss_stats_proto.proto,
      'swap', swap_stats_proto.proto,
      'anon_and_swap', anon_and_swap_stats_proto.proto,
      'java_heap', java_heap_stats_proto.proto
    ),
    'priority_breakdown', (
      SELECT RepeatedField(priority_breakdown_proto)
      FROM process_priority_view AS ppv
      WHERE mem_all_processes.process_name = ppv.process_name
    )
  ) AS metric
FROM
  mem_all_processes
  LEFT JOIN anon_rss_stats_proto USING (process_name)
  LEFT JOIN file_rss_stats_proto USING (process_name)
  LEFT JOIN swap_stats_proto USING (process_name)
  LEFT JOIN anon_and_swap_stats_proto USING (process_name)
  LEFT JOIN java_heap_stats_proto USING (process_name);

CREATE VIEW android_mem_output AS
SELECT
  AndroidMemoryMetric(
    'process_metrics',
    (SELECT RepeatedField(metric) FROM process_metrics_view)
  );
