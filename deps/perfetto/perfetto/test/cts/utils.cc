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

#include "test/cts/utils.h"

#include <stdlib.h>
#include <sys/system_properties.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

// invokes |callback| once the target app is in the desired state
void PollRunState(bool desired_run_state,
                  base::TestTaskRunner* task_runner,
                  const std::string& name,
                  std::function<void()> callback) {
  bool app_running = IsAppRunning(name);
  if (app_running == desired_run_state) {
    callback();
    return;
  }
  task_runner->PostDelayedTask(
      [desired_run_state, task_runner, name, callback] {
        PollRunState(desired_run_state, task_runner, name, std::move(callback));
      },
      /*delay_ms=*/5);
}

}  // namespace

bool IsDebuggableBuild() {
  char buf[PROP_VALUE_MAX + 1] = {};
  int ret = __system_property_get("ro.debuggable", buf);
  PERFETTO_CHECK(ret >= 0);
  return std::string(buf) == "1";
}

bool IsUserBuild() {
  char buf[PROP_VALUE_MAX + 1] = {};
  int ret = __system_property_get("ro.build.type", buf);
  PERFETTO_CHECK(ret >= 0);
  return std::string(buf) == "user";
}

// note: cannot use gtest macros due to return type
bool IsAppRunning(const std::string& name) {
  std::string cmd = "pgrep -f ^" + name + "$";
  int retcode = system(cmd.c_str());
  PERFETTO_CHECK(retcode >= 0);
  int exit_status = WEXITSTATUS(retcode);
  if (exit_status == 0)
    return true;
  if (exit_status == 1)
    return false;
  PERFETTO_FATAL("unexpected exit status from system(pgrep): %d", exit_status);
}

int PidForProcessName(const std::string& name) {
  std::string cmd = "pgrep -f ^" + name + "$";
  FILE* fp = popen(cmd.c_str(), "re");
  if (!fp)
    return -1;

  std::string out;
  base::ReadFileStream(fp, &out);
  pclose(fp);

  char* endptr = nullptr;
  int pid = static_cast<int>(strtol(out.c_str(), &endptr, 10));
  if (*endptr != '\0' && *endptr != '\n')
    return -1;
  return pid;
}

void WaitForProcess(const std::string& process,
                    const std::string& checkpoint_name,
                    base::TestTaskRunner* task_runner,
                    uint32_t delay_ms) {
  bool desired_run_state = true;
  const auto checkpoint = task_runner->CreateCheckpoint(checkpoint_name);
  task_runner->PostDelayedTask(
      [desired_run_state, task_runner, process, checkpoint] {
        PollRunState(desired_run_state, task_runner, process,
                     std::move(checkpoint));
      },
      delay_ms);
}

void StartAppActivity(const std::string& app_name,
                      const std::string& activity_name,
                      const std::string& checkpoint_name,
                      base::TestTaskRunner* task_runner,
                      uint32_t delay_ms) {
  std::string start_cmd = "am start " + app_name + "/." + activity_name;
  int status = system(start_cmd.c_str());
  ASSERT_TRUE(status >= 0 && WEXITSTATUS(status) == 0) << "status: " << status;
  WaitForProcess(app_name, checkpoint_name, task_runner, delay_ms);
}

void StopApp(const std::string& app_name,
             const std::string& checkpoint_name,
             base::TestTaskRunner* task_runner) {
  std::string stop_cmd = "am force-stop " + app_name;
  int status = system(stop_cmd.c_str());
  ASSERT_TRUE(status >= 0 && WEXITSTATUS(status) == 0) << "status: " << status;

  bool desired_run_state = false;
  auto checkpoint = task_runner->CreateCheckpoint(checkpoint_name);
  task_runner->PostTask([desired_run_state, task_runner, app_name, checkpoint] {
    PollRunState(desired_run_state, task_runner, app_name,
                 std::move(checkpoint));
  });
}

void StopApp(const std::string& app_name) {
  std::string stop_cmd = "am force-stop " + app_name;
  system(stop_cmd.c_str());
}

}  // namespace perfetto
