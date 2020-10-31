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
-- Create so that RUN_METRIC will run without outputting any rows.
CREATE TABLE TEST_TMP AS
SELECT RUN_METRIC('android/process_metadata.sql');

DROP TABLE TEST_TMP;

SELECT upid, process_name, uid, shared_uid, package_name, version_code
FROM process_metadata_table
WHERE upid != 0;
