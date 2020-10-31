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

CREATE VIEW IF NOT EXISTS android_task_names_output AS
WITH
-- Process to thread name
threads_by_upid AS (
  SELECT
    upid,
    RepeatedField(name) AS thread_names
  FROM thread
  WHERE name IS NOT NULL
  GROUP BY 1
),
upid_packages AS (
  SELECT
    upid,
    RepeatedField(package_list.package_name) AS packages
  FROM process
  JOIN package_list ON process.android_appid = package_list.uid
  GROUP BY 1
)
SELECT AndroidTaskNames(
  'process', RepeatedField(
    AndroidTaskNames_Process(
      'pid', p.pid,
      'process_name', p.name,
      'thread_name', threads_by_upid.thread_names,
      'uid', p.uid,
      'uid_package_name', upid_packages.packages
    )
  )
)
FROM process p
LEFT JOIN threads_by_upid USING (upid)
LEFT JOIN upid_packages USING (upid)
WHERE upid != 0;
