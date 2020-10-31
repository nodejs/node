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

-- View of Power Rail counters with ts converted from ns to ms.
CREATE VIEW power_rails_counters AS
SELECT value, ts/1000000 AS ts, name
FROM counter c
JOIN counter_track t on c.track_id = t.id
WHERE name LIKE 'power.%';

CREATE VIEW power_rails_view AS
WITH RECURSIVE name AS (SELECT DISTINCT name FROM power_rails_counters)
SELECT
  name,
  ts,
  AndroidPowerRails_PowerRails(
    'name', name,
    'energy_data', RepeatedField(
      AndroidPowerRails_EnergyData(
        'timestamp_ms', ts,
        'energy_uws', value
      )
    )
  ) AS power_rails_proto
FROM power_rails_counters
GROUP BY name
ORDER BY ts ASC;

CREATE VIEW android_powrails_output AS
SELECT AndroidPowerRails(
  'power_rails', (
    SELECT RepeatedField(power_rails_proto)
    FROM power_rails_view
  )
);
