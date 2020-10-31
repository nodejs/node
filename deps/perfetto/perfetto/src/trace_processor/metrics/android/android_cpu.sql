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

-- Create all the views used to generate the Android Cpu metrics proto.
SELECT RUN_METRIC('android/android_cpu_agg.sql');
SELECT RUN_METRIC('android/cpu_info.sql');

CREATE TABLE raw_metrics_per_core AS
SELECT
  utid,
  cpu,
  (
    SELECT
      CASE
        WHEN layout = 'big_little_bigger' AND cpu < 4 THEN 'little'
        WHEN layout = 'big_little_bigger' AND cpu < 7 THEN 'big'
        WHEN layout = 'big_little_bigger' AND cpu = 7 THEN 'bigger'
        WHEN layout = 'big_little' AND cpu < 4 THEN 'little'
        WHEN layout = 'big_little' AND cpu < 8 THEN 'big'
        ELSE 'unknown'
      END
    FROM core_layout_type
  ) AS core_type,
  -- We divide by 1e3 here as dur is in ns and freq_khz in khz. In total
  -- this means we need to divide the duration by 1e9 and multiply the
  -- frequency by 1e3 then multiply again by 1e3 to get millicycles
  -- i.e. divide by 1e3 in total.
  -- We use millicycles as we want to preserve this level of precision
  -- for future calculations.
  SUM(dur * freq_khz / 1000) AS millicycles,
  CAST(SUM(dur * freq_khz / 1000000 / 1000000) AS INT) AS mcycles,
  SUM(dur) AS runtime_ns,
  MIN(freq_khz) AS min_freq_khz,
  MAX(freq_khz) AS max_freq_khz,
  -- We choose to work in micros space in both the numerator and
  -- denominator as this gives us good enough precision without risking
  -- overflows.
  CAST(SUM(dur * freq_khz / 1000) / SUM(dur / 1000) AS INT) AS avg_freq_khz
FROM cpu_freq_sched_per_thread
GROUP BY utid, cpu;

CREATE VIEW metrics_per_core_type AS
SELECT
  utid,
  core_type,
  AndroidCpuMetric_Metrics(
    'mcycles', SUM(mcycles),
    'runtime_ns', SUM(runtime_ns),
    'min_freq_khz', MIN(min_freq_khz),
    'max_freq_khz', MAX(max_freq_khz),
    -- In total here, we need to divide the denominator by 1e9 (to convert
    -- ns to s) and divide the numerator by 1e6 (to convert millicycles to
    -- kcycles). In total, this means we need to multiply the expression as
    -- a whole by 1e3.
    'avg_freq_khz', CAST((SUM(millicycles) / SUM(runtime_ns)) * 1000 AS INT)
  ) AS proto
FROM raw_metrics_per_core
GROUP BY utid, core_type;

-- Aggregate everything per thread.
CREATE VIEW core_proto_per_thread AS
SELECT
  utid,
  RepeatedField(
    AndroidCpuMetric_CoreData(
      'id', cpu,
      'metrics', AndroidCpuMetric_Metrics(
        'mcycles', mcycles,
        'runtime_ns', runtime_ns,
        'min_freq_khz', min_freq_khz,
        'max_freq_khz', max_freq_khz,
        'avg_freq_khz', avg_freq_khz
      )
    )
  ) as proto
FROM raw_metrics_per_core
GROUP BY utid;

CREATE VIEW core_type_proto_per_thread AS
SELECT
  utid,
  RepeatedField(
    AndroidCpuMetric_CoreTypeData(
      'type', core_type,
      'metrics', metrics_per_core_type.proto
    )
  ) as proto
FROM metrics_per_core_type
GROUP BY utid;

CREATE VIEW metrics_proto_per_thread AS
SELECT
  utid,
  AndroidCpuMetric_Metrics(
    'mcycles', SUM(mcycles),
    'runtime_ns', SUM(runtime_ns),
    'min_freq_khz', MIN(min_freq_khz),
    'max_freq_khz', MAX(max_freq_khz),
    -- See above for a breakdown of the maths used to compute the
    -- multiplicative factor.
    'avg_freq_khz', CAST((SUM(millicycles) / SUM(runtime_ns)) * 1000 AS INT)
  ) AS proto
FROM raw_metrics_per_core
GROUP BY utid;

-- Aggregate everything per perocess
CREATE VIEW thread_proto_per_process AS
SELECT
  upid,
  RepeatedField(
    AndroidCpuMetric_Thread(
      'name', thread.name,
      'metrics', metrics_proto_per_thread.proto,
      'core', core_proto_per_thread.proto,
      'core_type', core_type_proto_per_thread.proto
    )
  ) as proto
FROM thread
LEFT JOIN core_proto_per_thread USING (utid)
LEFT JOIN core_type_proto_per_thread USING (utid)
LEFT JOIN metrics_proto_per_thread USING(utid)
GROUP BY upid;

CREATE VIEW core_metrics_per_process AS
SELECT
  upid,
  cpu,
  AndroidCpuMetric_Metrics(
    'mcycles', SUM(mcycles),
    'runtime_ns', SUM(runtime_ns),
    'min_freq_khz', MIN(min_freq_khz),
    'max_freq_khz', MAX(max_freq_khz),
    -- In total here, we need to divide the denominator by 1e9 (to convert
    -- ns to s) and divide the numerator by 1e6 (to convert millicycles to
    -- kcycles). In total, this means we need to multiply the expression as
    -- a whole by 1e3.
    'avg_freq_khz', CAST((SUM(millicycles) / SUM(runtime_ns)) * 1000 AS INT)
  ) AS proto
FROM raw_metrics_per_core
JOIN thread USING (utid)
GROUP BY upid, cpu;

CREATE VIEW core_proto_per_process AS
SELECT
  upid,
  RepeatedField(
    AndroidCpuMetric_CoreData(
      'id', cpu,
      'metrics', core_metrics_per_process.proto
    )
  ) as proto
FROM core_metrics_per_process
GROUP BY upid;

CREATE VIEW core_type_metrics_per_process AS
SELECT
  upid,
  core_type,
  AndroidCpuMetric_Metrics(
    'mcycles', SUM(mcycles),
    'runtime_ns', SUM(runtime_ns),
    'min_freq_khz', MIN(min_freq_khz),
    'max_freq_khz', MAX(max_freq_khz),
    -- In total here, we need to divide the denominator by 1e9 (to convert
    -- ns to s) and divide the numerator by 1e6 (to convert millicycles to
    -- kcycles). In total, this means we need to multiply the expression as
    -- a whole by 1e3.
    'avg_freq_khz', CAST((SUM(millicycles) / SUM(runtime_ns)) * 1000 AS INT)
  ) AS proto
FROM raw_metrics_per_core
JOIN thread USING (utid)
GROUP BY upid, core_type;

CREATE VIEW core_type_proto_per_process AS
SELECT
  upid,
  RepeatedField(
    AndroidCpuMetric_CoreTypeData(
      'type', core_type,
      'metrics', core_type_metrics_per_process.proto
    )
  ) as proto
FROM core_type_metrics_per_process
GROUP BY upid;

CREATE VIEW metrics_proto_per_process AS
SELECT
  upid,
  AndroidCpuMetric_Metrics(
    'mcycles', SUM(mcycles),
    'runtime_ns', SUM(runtime_ns),
    'min_freq_khz', MIN(min_freq_khz),
    'max_freq_khz', MAX(max_freq_khz),
    -- See above for a breakdown of the maths used to compute the
    -- multiplicative factor.
    'avg_freq_khz', CAST((SUM(millicycles) / SUM(runtime_ns)) * 1000 AS INT)
  ) AS proto
FROM raw_metrics_per_core
JOIN thread USING (utid)
GROUP BY upid;

CREATE VIEW android_cpu_output AS
SELECT AndroidCpuMetric(
  'process_info', (
    SELECT RepeatedField(
      AndroidCpuMetric_Process(
        'name', process.name,
        'metrics', metrics_proto_per_process.proto,
        'threads', thread_proto_per_process.proto,
        'core', core_proto_per_process.proto,
        'core_type', core_type_proto_per_process.proto
      )
    )
    FROM process
    JOIN metrics_proto_per_process USING(upid)
    JOIN thread_proto_per_process USING (upid)
    JOIN core_proto_per_process USING (upid)
    JOIN core_type_proto_per_process USING (upid)
  )
);
