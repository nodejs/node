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
create view v1 as select tag, count(*) from android_logs group by tag order by 2 desc limit 5

create view v2 as select tag, count(*) from android_logs group by tag order by 2 asc limit 5;

create view v3 as select tag, count(*) from android_logs where msg like '%wakelock%' group by tag;

create view v4 as select msg, 1 from android_logs limit 10;

select * from v1 union all
select '-----', 0 union all
select * from v2 union all
select '-----', 0 union all
select * from v3 union all
select '-----', 0 union all
select * from v4;