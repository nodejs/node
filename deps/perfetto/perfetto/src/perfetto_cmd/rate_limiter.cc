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

#include "src/perfetto_cmd/rate_limiter.h"

#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/utils.h"
#include "src/perfetto_cmd/perfetto_cmd.h"

namespace perfetto {
namespace {

// Every 24 hours we reset how much we've uploaded.
const uint64_t kMaxUploadResetPeriodInSeconds = 60 * 60 * 24;

// Maximum of 10mb every 24h.
const uint64_t kMaxUploadInBytes = 1024 * 1024 * 10;

// Keep track of last 10 observed sessions.
const uint64_t kMaxSessionsInHistory = 10;

}  // namespace

using PerSessionState = gen::PerfettoCmdState::PerSessionState;

RateLimiter::RateLimiter() = default;
RateLimiter::~RateLimiter() = default;

bool RateLimiter::ShouldTrace(const Args& args) {
  uint64_t now_in_s = static_cast<uint64_t>(args.current_time.count());

  // Not storing in Dropbox?
  // -> We can just trace.
  if (!args.is_dropbox)
    return true;

  // If we're tracing a user build we should only trace if the override in
  // the config is set:
  if (args.is_user_build && !args.allow_user_build_tracing) {
    PERFETTO_ELOG(
        "Guardrail: allow_user_build_tracing must be set to trace on user "
        "builds");
    return false;
  }

  // The state file is gone.
  // Maybe we're tracing for the first time or maybe something went wrong the
  // last time we tried to save the state. Either way reinitialize the state
  // file.
  if (!StateFileExists()) {
    // We can't write the empty state file?
    // -> Give up.
    if (!ClearState()) {
      PERFETTO_ELOG("Guardrail: failed to initialize guardrail state.");
      return false;
    }
  }

  bool loaded_state = LoadState(&state_);

  // Failed to load the state?
  // Current time is before either saved times?
  // Last saved trace time is before first saved trace time?
  // -> Try to save a clean state but don't trace.
  if (!loaded_state || now_in_s < state_.first_trace_timestamp() ||
      now_in_s < state_.last_trace_timestamp() ||
      state_.last_trace_timestamp() < state_.first_trace_timestamp()) {
    ClearState();
    PERFETTO_ELOG("Guardrail: state invalid, clearing it.");
    if (!args.ignore_guardrails)
      return false;
  }

  // First trace was more than 24h ago? Reset state.
  if ((now_in_s - state_.first_trace_timestamp()) >
      kMaxUploadResetPeriodInSeconds) {
    state_.set_first_trace_timestamp(0);
    state_.set_last_trace_timestamp(0);
    state_.set_total_bytes_uploaded(0);
    return true;
  }

  uint64_t max_upload_guardrail = kMaxUploadInBytes;
  if (args.max_upload_bytes_override > 0) {
    if (args.unique_session_name.empty()) {
      PERFETTO_ELOG(
          "Ignoring max_upload_per_day_bytes override as unique_session_name "
          "not set");
    } else {
      max_upload_guardrail = args.max_upload_bytes_override;
    }
  }

  uint64_t uploaded_so_far = state_.total_bytes_uploaded();
  if (!args.unique_session_name.empty()) {
    uploaded_so_far = 0;
    for (const auto& session_state : state_.session_state()) {
      if (session_state.session_name() == args.unique_session_name) {
        uploaded_so_far = session_state.total_bytes_uploaded();
        break;
      }
    }
  }

  if (uploaded_so_far > max_upload_guardrail) {
    PERFETTO_ELOG("Guardrail: Uploaded %" PRIu64
                  " in the last 24h. Limit is %" PRIu64 ".",
                  uploaded_so_far, max_upload_guardrail);
    if (!args.ignore_guardrails)
      return false;
  }

  return true;
}

bool RateLimiter::OnTraceDone(const Args& args, bool success, uint64_t bytes) {
  uint64_t now_in_s = static_cast<uint64_t>(args.current_time.count());

  // Failed to upload? Don't update the state.
  if (!success)
    return false;

  if (!args.is_dropbox)
    return true;

  // If the first trace timestamp is 0 (either because this is the
  // first time or because it was reset for being more than 24h ago).
  // -> We update it to the time of this trace.
  if (state_.first_trace_timestamp() == 0)
    state_.set_first_trace_timestamp(now_in_s);
  // Always updated the last trace timestamp.
  state_.set_last_trace_timestamp(now_in_s);

  if (args.unique_session_name.empty()) {
    // Add the amount we uploaded to the running total.
    state_.set_total_bytes_uploaded(state_.total_bytes_uploaded() + bytes);
  } else {
    PerSessionState* target_session = nullptr;
    for (PerSessionState& session : *state_.mutable_session_state()) {
      if (session.session_name() == args.unique_session_name) {
        target_session = &session;
        break;
      }
    }
    if (!target_session) {
      target_session = state_.add_session_state();
      target_session->set_session_name(args.unique_session_name);
    }
    target_session->set_total_bytes_uploaded(
        target_session->total_bytes_uploaded() + bytes);
    target_session->set_last_trace_timestamp(now_in_s);
  }

  if (!SaveState(state_)) {
    PERFETTO_ELOG("Failed to save state.");
    return false;
  }

  return true;
}

std::string RateLimiter::GetStateFilePath() const {
  return std::string(kStateDir) + "/.guardraildata";
}

bool RateLimiter::StateFileExists() {
  struct stat out;
  return stat(GetStateFilePath().c_str(), &out) != -1;
}

bool RateLimiter::ClearState() {
  gen::PerfettoCmdState zero{};
  zero.set_total_bytes_uploaded(0);
  zero.set_last_trace_timestamp(0);
  zero.set_first_trace_timestamp(0);
  bool success = SaveState(zero);
  if (!success && StateFileExists())
    remove(GetStateFilePath().c_str());
  return success;
}

bool RateLimiter::LoadState(gen::PerfettoCmdState* state) {
  base::ScopedFile in_fd(base::OpenFile(GetStateFilePath(), O_RDONLY));
  if (!in_fd)
    return false;
  std::string s;
  base::ReadFileDescriptor(in_fd.get(), &s);
  if (s.size() == 0)
    return false;
  return state->ParseFromString(s);
}

bool RateLimiter::SaveState(const gen::PerfettoCmdState& input_state) {
  gen::PerfettoCmdState state = input_state;

  // Keep only the N most recent per session states so the file doesn't
  // grow indefinitely:
  std::vector<PerSessionState>* sessions = state.mutable_session_state();
  std::sort(sessions->begin(), sessions->end(),
            [](const PerSessionState& a, const PerSessionState& b) {
              return a.last_trace_timestamp() > b.last_trace_timestamp();
            });
  if (sessions->size() > kMaxSessionsInHistory) {
    sessions->resize(kMaxSessionsInHistory);
  }

  // Rationale for 0666: the cmdline client can be executed under two
  // different Unix UIDs: shell and statsd. If we run one after the
  // other and the file has 0600 permissions, then the 2nd run won't
  // be able to read the file and will clear it, aborting the trace.
  // SELinux still prevents that anything other than the perfetto
  // executable can change the guardrail file.
  std::vector<uint8_t> buf = state.SerializeAsArray();
  base::ScopedFile out_fd(
      base::OpenFile(GetStateFilePath(), O_WRONLY | O_CREAT | O_TRUNC, 0666));
  if (!out_fd)
    return false;
  ssize_t written = base::WriteAll(out_fd.get(), buf.data(), buf.size());
  return written >= 0 && static_cast<size_t>(written) == buf.size();
}

}  // namespace perfetto
