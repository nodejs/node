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
select
  counter_track.name as counter_name,
  process.name as process,
  thread.name as thread,
  thread_process.name as thread_process,
  counter_track.unit as unit,
  counter_track.source_arg_set_id as track_args,
  counter.ts,
  counter.value
from counter
left join counter_track on counter.track_id = counter_track.id
left join process_counter_track on counter.track_id = process_counter_track.id
left join process on process_counter_track.upid = process.upid
left join thread_counter_track on counter.track_id = thread_counter_track.id
left join thread on thread_counter_track.utid = thread.utid
left join process thread_process on thread.upid = thread_process.upid
order by ts asc;
