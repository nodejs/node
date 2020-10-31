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

#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/internal/track_event_internal.h"
#include "src/tracing/internal/tracing_muxer_impl.h"

#include <condition_variable>
#include <mutex>

namespace perfetto {

// static
void Tracing::InitializeInternal(const TracingInitArgs& args) {
  static bool was_initialized = false;
  static TracingInitArgs init_args;
  if (was_initialized) {
    // Should not be reinitialized with different args.
    PERFETTO_DCHECK(init_args == args);
    return;
  }

  // Make sure the headers and implementation files agree on the build config.
  PERFETTO_CHECK(args.dcheck_is_on_ == PERFETTO_DCHECK_IS_ON());
  internal::TracingMuxerImpl::InitializeInstance(args);
  internal::TrackRegistry::InitializeInstance();
  was_initialized = true;
  init_args = args;
}

//  static
std::unique_ptr<TracingSession> Tracing::NewTrace(BackendType backend) {
  return static_cast<internal::TracingMuxerImpl*>(internal::TracingMuxer::Get())
      ->CreateTracingSession(backend);
}

std::vector<char> TracingSession::ReadTraceBlocking() {
  std::vector<char> raw_trace;
  std::mutex mutex;
  std::condition_variable cv;
  bool all_read = false;

  ReadTrace([&mutex, &raw_trace, &all_read, &cv](ReadTraceCallbackArgs cb) {
    raw_trace.insert(raw_trace.end(), cb.data, cb.data + cb.size);
    std::unique_lock<std::mutex> lock(mutex);
    all_read = !cb.has_more;
    if (all_read)
      cv.notify_one();
  });

  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&all_read] { return all_read; });
  }
  return raw_trace;
}

}  // namespace perfetto
