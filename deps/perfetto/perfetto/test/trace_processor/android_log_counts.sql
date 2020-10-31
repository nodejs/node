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
select count(*) as cnt from android_logs union all
select count(*) as cnt from android_logs where prio = 3 union all
select count(*) as cnt from android_logs where prio > 4 union all
select count(*) as cnt from android_logs where tag = 'screen_toggled' union all
select count(*) as cnt from android_logs where tag like '%_pss' union all
select count(*) as cnt from android_logs where msg like '%i2c_write%' union all
select count(*) as cnt from android_logs where ts >= 1510113924391 and ts < 1512610021879;