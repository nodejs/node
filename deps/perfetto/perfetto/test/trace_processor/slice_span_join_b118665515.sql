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
create virtual table window_8 using window;

create virtual table span_8 using span_join(sched PARTITIONED cpu, window_8);

update window_8 set window_start=81473010031230, window_dur=19684693341, quantum=10000000 where rowid = 0;

select quantum_ts as bucket, sum(dur)/cast(10000000 as float) as utilization from span_8 where cpu = 7 and utid != 0 group by quantum_ts;
