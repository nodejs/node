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
select o.id,
       o.type,
       o.upid,
       o.graph_sample_ts,
       o.object_id,
       o.self_size,
       o.retained_size,
       o.unique_retained_size,
       o.reference_set_id,
       o.reachable,
       c.name as type_name,
       c.deobfuscated_name as deobfuscated_type_name,
       o.root_type
from heap_graph_object o join heap_graph_class c on o.type_id = c.id
