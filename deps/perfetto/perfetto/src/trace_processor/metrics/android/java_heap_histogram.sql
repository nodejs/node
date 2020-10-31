--
-- Copyright 2020 The Android Open Source Project
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

CREATE VIEW IF NOT EXISTS java_heap_histogram_output AS
WITH
-- Base histogram table
heap_obj_histograms AS (
  SELECT
    o.upid,
    o.graph_sample_ts,
    IFNULL(c.deobfuscated_name, c.name) AS type_name,
    COUNT(1) obj_count,
    SUM(CASE o.reachable WHEN TRUE THEN 1 ELSE 0 END) reachable_obj_count
  FROM heap_graph_object o JOIN heap_graph_class c ON o.type_id = c.id
  GROUP BY 1, 2, 3
),
-- Group by to build the repeated field by upid, ts
heap_obj_histogram_count_protos AS (
  SELECT
    upid,
    graph_sample_ts,
    RepeatedField(JavaHeapHistogram_TypeCount(
      'type_name', type_name,
      'obj_count', obj_count,
      'reachable_obj_count', reachable_obj_count
    )) AS count_protos
  FROM heap_obj_histograms
  GROUP BY 1, 2
),
-- Group by to build the repeated field by upid
heap_obj_histogram_sample_protos AS (
  SELECT
    upid,
    RepeatedField(JavaHeapHistogram_Sample(
      'ts', graph_sample_ts,
      'type_count', count_protos
    )) AS sample_protos
  FROM heap_obj_histogram_count_protos
  GROUP BY 1
)
SELECT JavaHeapHistogram(
  'instance_stats', RepeatedField(JavaHeapHistogram_InstanceStats(
    'upid', upid,
    'process', process_metadata.metadata,
    'samples', sample_protos
  )))
FROM heap_obj_histogram_sample_protos JOIN process_metadata USING (upid);
