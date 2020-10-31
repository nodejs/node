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
  track.name as track,
  process.name as process,
  thread.name as thread,
  thread_process.name as thread_process,
  slice.ts,
  slice.dur,
  slice.category,
  slice.name,
  slice.arg_set_id
from slice
left join track on slice.track_id = track.id
left join process_track on slice.track_id = process_track.id
left join process on process_track.upid = process.upid
left join thread_track on slice.track_id = thread_track.id
left join thread on thread_track.utid = thread.utid
left join process thread_process on thread.upid = thread_process.upid
order by ts asc;
