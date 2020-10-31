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
select
  tid,
  pid,
  thread.name as threadName,
  process.name as processName,
  total_dur as totalDur
from
  thread
  left join process using(upid)
  left join (select upid, sum(dur) as total_dur
      from sched join thread using(utid)
      group by upid
    ) using(upid)
group by utid, upid
order by total_dur desc, pid, tid
