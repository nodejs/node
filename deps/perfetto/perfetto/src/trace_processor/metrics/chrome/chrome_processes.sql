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

-- A view of all Chrome processes.
-- TODO(skyostil): Create this table in the trace processor internally so we
-- can expose the process type.
DROP VIEW IF EXISTS chrome_process;

CREATE VIEW chrome_process AS
  SELECT *
  FROM process WHERE (
    -- Chrome package names.
    name like 'com.android.chrome%' OR
    name like 'com.chrome.beta%' OR
    name like 'com.chrome.dev%' OR
    name like 'com.chrome.canary%' OR
    name like 'com.google.android.apps.chrome%' OR
    name like 'org.chromium.chrome%' OR
    -- Chrome process descriptor names.
    name = 'Browser' OR
    name = 'Renderer' OR
    name = 'Utility' OR
    name = 'Zygote' OR
    name = 'SandboxHelper' OR
    name = 'Gpu' OR
    name = 'PpapiPlugin' OR
    name = 'PpapiBroker');

-- A view of all Chrome threads.
DROP VIEW IF EXISTS chrome_thread;

CREATE VIEW chrome_thread AS
  SELECT thread.*
  FROM thread, chrome_process
  WHERE thread.upid = chrome_process.upid;
