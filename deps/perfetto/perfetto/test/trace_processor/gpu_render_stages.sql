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
SELECT track.name AS track_name, gpu_track.description AS track_desc, ts, dur,
    gpu_slice.name AS slice_name, depth, flat_key, string_value,
    gpu_slice.context_id, render_target, render_target_name, render_pass, render_pass_name,
    command_buffer, command_buffer_name, submission_id, hw_queue_id
FROM gpu_track
LEFT JOIN track USING (id)
INNER JOIN gpu_slice ON gpu_track.id=gpu_slice.track_id
LEFT JOIN args ON gpu_slice.arg_set_id = args.arg_set_id
ORDER BY ts;
