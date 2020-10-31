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
SELECT ts
FROM counter
WHERE
  ts > 72563651549 AND
  track_id = (
    SELECT t.id
    FROM process_counter_track t
    JOIN process p USING (upid)
    WHERE
      t.name = 'Heap size (KB)'
      AND p.pid = 1204
  ) AND
  value != 17952.000000
LIMIT 20
