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

CREATE VIEW IF NOT EXISTS core_layout_mapping AS
SELECT
  CASE
    WHEN (
      str_value LIKE '%flame%' OR
      str_value LIKE '%coral%'
    ) THEN 'big_little_bigger'
    WHEN (
      str_value LIKE '%taimen%' OR
      str_value LIKE '%walleye%' OR
      str_value LIKE '%bonito%' OR
      str_value LIKE '%sargo%' OR
      str_value LIKE '%blueline%' OR
      str_value LIKE '%crosshatch%'
    ) THEN 'big_little'
    ELSE 'unknown'
  END AS layout
FROM metadata
WHERE name = 'android_build_fingerprint';

CREATE TABLE IF NOT EXISTS core_layout_type AS
SELECT *
FROM (
  SELECT layout from core_layout_mapping
  UNION
  SELECT 'unknown'
)
LIMIT 1;
