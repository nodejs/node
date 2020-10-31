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
select scope, track.name as track_name, ts, dur, gpu_slice.name as slice_name,
    key, string_value as value
from gpu_track
left join track using (id)
left join gpu_slice on gpu_track.id=gpu_slice.track_id
left join args using (arg_set_id)
order by ts, slice_name, key
