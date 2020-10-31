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

CREATE VIEW unsymbolized_frames_view AS
SELECT UnsymbolizedFrames_Frame(
    'module', spm.name,
    'build_id', spm.build_id,
    'address', spf.rel_pc
) AS frame_proto
FROM stack_profile_frame spf
JOIN stack_profile_mapping spm
ON spf.mapping = spm.id
WHERE spm.build_id != ''
AND (spf.symbol_set_id == 0 OR spf.symbol_set_id IS NULL);

CREATE VIEW unsymbolized_frames_output AS
SELECT UnsymbolizedFrames(
  'frames',
  (SELECT RepeatedField(frame_proto) FROM unsymbolized_frames_view)
);
