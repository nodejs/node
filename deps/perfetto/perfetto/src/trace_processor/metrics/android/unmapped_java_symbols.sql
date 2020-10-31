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

CREATE TABLE IF NOT EXISTS types_per_upid AS
WITH distinct_unmapped_type_names AS (
  SELECT DISTINCT upid, type_name
  FROM (
    SELECT
      upid,
      RTRIM(REPLACE(c.name, 'java.lang.Class<', ''), '[]>') AS type_name
    FROM heap_graph_object o JOIN heap_graph_class c ON o.type_id = c.id
    WHERE c.deobfuscated_name IS NULL
  )
  WHERE type_name NOT IN ('byte', 'char', 'short', 'int', 'long', 'boolean', 'float', 'double')
  AND type_name NOT LIKE '$Proxy%'
  AND type_name NOT LIKE 'java.%'
  AND type_name NOT LIKE 'javax.%'
  AND type_name NOT LIKE 'j$.%'
  AND type_name NOT LIKE 'android.%'
  AND type_name NOT LIKE 'com.android.%'
  AND type_name NOT LIKE 'sun.%'
  AND type_name NOT LIKE 'dalvik.%'
  AND type_name NOT LIKE 'libcore.%'
  AND LENGTH(type_name) > 0
)
SELECT upid, RepeatedField(type_name) AS types
FROM distinct_unmapped_type_names GROUP BY 1;

CREATE TABLE IF NOT EXISTS fields_per_upid AS
WITH distinct_unmapped_field_names AS (
  SELECT DISTINCT o.upid, field_type_name, field_name
    FROM heap_graph_object o JOIN heap_graph_class c ON o.type_id = c.id
           JOIN heap_graph_reference USING (reference_set_id)
  WHERE c.deobfuscated_name IS NULL
  AND field_name NOT LIKE '$Proxy%'
  AND field_name NOT LIKE 'java.%'
  AND field_name NOT LIKE 'javax.%'
  AND field_name NOT LIKE 'j$.%'
  AND field_name NOT LIKE 'android.%'
  AND field_name NOT LIKE 'com.android.%'
  AND field_name NOT LIKE 'sun.%'
  AND field_name NOT LIKE 'dalvik.%'
  AND field_name NOT LIKE 'libcore.%'
  AND LENGTH(field_name) > 0
)
SELECT upid, RepeatedField(
  UnmappedJavaSymbols_Field(
    'field_name', field_name,
    'field_type_name', field_type_name)) AS fields
FROM distinct_unmapped_field_names GROUP BY 1;

CREATE VIEW IF NOT EXISTS java_symbols_per_process AS
SELECT UnmappedJavaSymbols_ProcessSymbols(
  'process_metadata', metadata,
  'type_name', types,
  'field', fields
) types
FROM types_per_upid
JOIN process_metadata USING (upid)
LEFT JOIN fields_per_upid USING (upid);

CREATE VIEW unmapped_java_symbols_output AS
SELECT UnmappedJavaSymbols(
  'process_symbols',
  (SELECT RepeatedField(types) FROM java_symbols_per_process)
);
