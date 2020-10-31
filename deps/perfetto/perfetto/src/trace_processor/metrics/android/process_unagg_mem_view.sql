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

DROP TABLE IF EXISTS {{table_name}}_unagg_values;

CREATE TABLE {{table_name}}_unagg_values AS
SELECT
  upid,
  RepeatedField(
    AndroidMemoryUnaggregatedMetric_Value(
      'ts', ts,
      'oom_score', oom_score_val,
      'value', {{table_name}}_val
    )
  ) as metric
FROM {{table_name}}_by_oom_span
GROUP BY upid;
