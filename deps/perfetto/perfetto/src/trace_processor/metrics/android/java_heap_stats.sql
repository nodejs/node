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

SELECT RUN_METRIC('android/process_metadata.sql');
SELECT RUN_METRIC('android/process_mem.sql');

CREATE VIEW java_heap_stats_output AS
WITH
-- Base view
base_stats AS (
  SELECT
    upid,
    graph_sample_ts,
    SUM(self_size) AS total_size,
    COUNT(1) AS total_obj_count,
    SUM(CASE reachable WHEN TRUE THEN self_size ELSE 0 END) AS reachable_size,
    SUM(CASE reachable WHEN TRUE THEN 1 ELSE 0 END) AS reachable_obj_count
  FROM heap_graph_object
  GROUP BY 1, 2
),
-- Group by upid
heap_graph_sample_protos AS (
  SELECT
    base_stats.upid,
    RepeatedField(JavaHeapStats_Sample(
      'ts', graph_sample_ts,
      'heap_size', total_size,
      'obj_count', total_obj_count,
      'reachable_heap_size', reachable_size,
      'reachable_obj_count', reachable_obj_count,
      'anon_rss_and_swap_size', CAST(anon_and_swap_val AS INTEGER)
    )) sample_protos
  FROM base_stats
  LEFT JOIN anon_and_swap_span ON
    base_stats.upid = anon_and_swap_span.upid
    AND anon_and_swap_span.ts <= base_stats.graph_sample_ts
    AND base_stats.graph_sample_ts < anon_and_swap_span.ts + anon_and_swap_span.dur
  GROUP BY 1
)
SELECT JavaHeapStats(
  'instance_stats', RepeatedField(JavaHeapStats_InstanceStats(
    'upid', upid,
    'process', process_metadata.metadata,
    'samples', sample_protos
  )))
FROM heap_graph_sample_protos JOIN process_metadata USING (upid);
