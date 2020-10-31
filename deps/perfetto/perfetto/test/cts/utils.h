/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TEST_CTS_UTILS_H_
#define TEST_CTS_UTILS_H_

#include <string>

#include "src/base/test/test_task_runner.h"

namespace perfetto {

bool IsDebuggableBuild();
bool IsUserBuild();

bool IsAppRunning(const std::string& name);

// returns -1 if the process wasn't found
int PidForProcessName(const std::string& name);

void WaitForProcess(const std::string& process,
                    const std::string& checkpoint_name,
                    base::TestTaskRunner* task_runner,
                    uint32_t delay_ms = 1);

void StartAppActivity(const std::string& app_name,
                      const std::string& activity_name,
                      const std::string& checkpoint_name,
                      base::TestTaskRunner* task_runner,
                      uint32_t delay_ms = 1);

void StopApp(const std::string& app_name,
             const std::string& checkpoint_name,
             base::TestTaskRunner* task_runner);

void StopApp(const std::string& app_name);

}  // namespace perfetto

#endif  // TEST_CTS_UTILS_H_
