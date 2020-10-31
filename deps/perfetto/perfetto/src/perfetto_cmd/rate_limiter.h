/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_PERFETTO_CMD_RATE_LIMITER_H_
#define SRC_PERFETTO_CMD_RATE_LIMITER_H_

#include "perfetto/base/time.h"
#include "src/perfetto_cmd/perfetto_cmd_state.gen.h"

namespace perfetto {

class RateLimiter {
 public:
  struct Args {
    bool is_user_build = false;
    bool is_dropbox = false;
    bool ignore_guardrails = false;
    bool allow_user_build_tracing = false;
    base::TimeSeconds current_time = base::TimeSeconds(0);
    uint64_t max_upload_bytes_override = 0;
    std::string unique_session_name = "";
  };

  RateLimiter();
  virtual ~RateLimiter();

  bool ShouldTrace(const Args& args);
  bool OnTraceDone(const Args& args, bool success, uint64_t bytes);

  bool ClearState();

  // virtual for testing.
  virtual bool LoadState(gen::PerfettoCmdState* state);

  // virtual for testing.
  virtual bool SaveState(const gen::PerfettoCmdState& state);

  bool StateFileExists();
  virtual std::string GetStateFilePath() const;

 private:
  gen::PerfettoCmdState state_{};
};

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_RATE_LIMITER_H_
