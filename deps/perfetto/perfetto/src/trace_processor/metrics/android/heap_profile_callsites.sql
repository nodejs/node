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

CREATE VIEW memory_delta AS
SELECT upid, SUM(size) AS delta
FROM heap_profile_allocation
GROUP BY 1;

CREATE VIEW memory_total AS
SELECT upid, SUM(size) AS total
FROM heap_profile_allocation
WHERE size > 0
GROUP BY 1;

-- Join frames with symbols and mappings to get a textual representation.
CREATE TABLE symbolized_frame AS
SELECT
  frame_id,
  symbol_name,
  mapping_name,
  HASH(symbol_name, mapping_name) frame_hash,
  HeapProfileCallsites_Frame(
    'name', symbol_name,
    'mapping_name', mapping_name
  ) AS frame_proto
FROM (
  SELECT
    spf.id AS frame_id,
    IFNULL(
      (SELECT name FROM stack_profile_symbol symbol
        WHERE symbol.symbol_set_id = spf.symbol_set_id
        LIMIT 1),
      spf.name
    ) AS symbol_name,
    spm.name AS mapping_name
  FROM stack_profile_frame spf
  JOIN stack_profile_mapping spm
  ON spf.mapping = spm.id
);

-- Required to join with callsites
CREATE UNIQUE INDEX symbolized_frame_idx ON symbolized_frame(frame_id);

-- View that joins callsites with frames. Allocation-agnostic, only used to
-- generated the hashed callsites.
CREATE TABLE callsites AS
SELECT cs.id, cs.parent_id, cs.depth, sf.frame_hash
FROM stack_profile_callsite cs
JOIN symbolized_frame sf USING(frame_id);

DROP INDEX symbolized_frame_idx;

-- heapprofd-based callsite ids are based on frame addresses, whereas we want
-- to group by symbol names.
-- Create a unique ID for each subtree by traversing from the root.
-- 1 self_hash can correspond to N callsite_ids (which can then be used to join
-- with allocs).
CREATE TABLE hashed_callsites AS
WITH RECURSIVE callsite_hasher(id, self_hash, parent_hash, frame_hash) AS (
  SELECT
    cs.id,
    cs.frame_hash,
    -1,
    cs.frame_hash
  FROM callsites cs
  WHERE cs.depth = 0
  UNION ALL
  SELECT
    child.id,
    HASH(child.frame_hash, parent.self_hash),
    parent.self_hash,
    child.frame_hash
  FROM callsite_hasher parent
  JOIN callsites child
  ON parent.id = child.parent_id
)
SELECT
  self_hash,
  parent_hash,
  frame_hash,
  id callsite_id
FROM callsite_hasher;

DROP TABLE callsites;

CREATE VIEW hashed_callsite_tree AS
SELECT DISTINCT self_hash, parent_hash, frame_hash
FROM hashed_callsites;

-- Required to join with allocs
CREATE INDEX hashed_callsites_id_idx ON hashed_callsites(callsite_id);

-- Computes the allocations for each hash-based callsite.
CREATE TABLE self_allocs AS
SELECT
  hc.self_hash,
  alloc.upid,
  SUM(alloc.count) AS delta_count,
  SUM(CASE WHEN alloc.count > 0 THEN alloc.count ELSE 0 END) AS total_count,
  SUM(alloc.size) AS delta_bytes,
  SUM(CASE WHEN alloc.size > 0 THEN alloc.size ELSE 0 END) AS total_bytes
FROM hashed_callsites hc
JOIN heap_profile_allocation alloc USING (callsite_id)
GROUP BY 1, 2;

DROP INDEX hashed_callsites_id_idx;

-- For each allocation (each self_alloc), emit a row for each ancestor and
-- aggregate them by self_hash.
CREATE TABLE child_allocs AS
WITH RECURSIVE parent_traversal(
  self_hash, parent_hash, upid,
  delta_count, total_count, delta_bytes, total_bytes) AS (
  SELECT
    sa.self_hash,
    hc.parent_hash,
    sa.upid,
    sa.delta_count,
    sa.total_count,
    sa.delta_bytes,
    sa.total_bytes
  FROM self_allocs sa
  JOIN hashed_callsite_tree hc ON sa.self_hash = hc.self_hash
  UNION ALL
  SELECT
    parent.self_hash,
    parent.parent_hash,
    child.upid,
    child.delta_count,
    child.total_count,
    child.delta_bytes,
    child.total_bytes
  FROM parent_traversal child
  JOIN hashed_callsite_tree parent
  ON child.parent_hash = parent.self_hash
)
SELECT
  self_hash,
  upid,
  SUM(delta_count) AS delta_count,
  SUM(total_count) AS total_count,
  SUM(delta_bytes) AS delta_bytes,
  SUM(total_bytes) AS total_bytes
FROM parent_traversal
GROUP BY 1, 2;

CREATE VIEW self_allocs_proto AS
SELECT
  self_hash,
  upid,
  HeapProfileCallsites_Counters(
    'delta_count', delta_count, 'total_count', total_count,
    'delta_bytes', delta_bytes, 'total_bytes', total_bytes
  ) AS allocs_proto
FROM self_allocs;

CREATE VIEW child_allocs_proto AS
SELECT
  self_hash,
  upid,
  HeapProfileCallsites_Counters(
    'delta_count', delta_count, 'total_count', total_count,
    'delta_bytes', delta_bytes, 'total_bytes', total_bytes
  ) AS allocs_proto
FROM child_allocs;

-- Required to map back to the symbol.
CREATE INDEX symbolized_frame_hash_idx ON symbolized_frame(frame_hash);

CREATE TABLE process_callsite AS
SELECT
  ca.upid,
  ca.self_hash,
  tree.parent_hash,
  frame.frame_proto,
  sa.allocs_proto AS self_allocs_proto,
  ca.allocs_proto AS child_allocs_proto
FROM hashed_callsite_tree tree
JOIN (SELECT DISTINCT frame_hash, frame_proto FROM symbolized_frame) frame
  USING (frame_hash)
JOIN child_allocs_proto ca
  USING (self_hash)
LEFT JOIN self_allocs_proto sa
  USING (self_hash, upid)
ORDER BY 1, 2;

DROP INDEX symbolized_frame_hash_idx;

CREATE VIEW process_callsite_proto AS
SELECT
  upid,
  RepeatedField(HeapProfileCallsites_Callsite(
    'hash', self_hash,
    'parent_hash', parent_hash,
    'frame', frame_proto,
    'self_allocs', self_allocs_proto,
    'child_allocs', child_allocs_proto
  )) AS repeated_callsite_proto
FROM process_callsite
GROUP BY 1;

CREATE VIEW instance_stats_view AS
SELECT HeapProfileCallsites_InstanceStats(
    'pid', process.pid,
    'process_name', process.name,
    'process', process_metadata.metadata,
    'callsites', repeated_callsite_proto,
    'profile_delta_bytes', memory_delta.delta,
    'profile_total_bytes', memory_total.total
) AS instance_stats_proto
FROM process_callsite_proto
JOIN memory_total USING (upid)
JOIN memory_delta USING (upid)
JOIN process USING (upid)
JOIN process_metadata USING (upid);

CREATE VIEW heap_profile_callsites_output AS
SELECT HeapProfileCallsites(
  'instance_stats',
  (SELECT RepeatedField(instance_stats_proto) FROM instance_stats_view)
);
