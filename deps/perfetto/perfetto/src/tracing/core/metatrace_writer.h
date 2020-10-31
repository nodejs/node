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

#ifndef SRC_TRACING_CORE_METATRACE_WRITER_H_
#define SRC_TRACING_CORE_METATRACE_WRITER_H_

#include <functional>
#include <memory>

#include "perfetto/ext/base/metatrace.h"
#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/base/weak_ptr.h"

namespace perfetto {

namespace base {
class TaskRunner;
}

class TraceWriter;

// Complements the base::metatrace infrastructure.
// It hooks a callback to metatrace::Enable() and writes metatrace events into
// a TraceWriter whenever the metatrace ring buffer is half full.
// It is safe to create and attempt to start multiple instances of this class,
// however only the first one will succeed because the metatrace framework
// doesn't support multiple instances.
// This class is defined here (instead of directly in src/probes/) so it can
// be reused by other components (e.g. heapprofd).
class MetatraceWriter {
 public:
  static constexpr char kDataSourceName[] = "perfetto.metatrace";

  MetatraceWriter();
  ~MetatraceWriter();

  MetatraceWriter(const MetatraceWriter&) = delete;
  MetatraceWriter& operator=(const MetatraceWriter&) = delete;
  MetatraceWriter(MetatraceWriter&&) = delete;
  MetatraceWriter& operator=(MetatraceWriter&&) = delete;

  void Enable(base::TaskRunner*, std::unique_ptr<TraceWriter>, uint32_t tags);
  void Disable();
  void WriteAllAndFlushTraceWriter(std::function<void()> callback);

 private:
  void WriteAllAvailableEvents();

  bool started_ = false;
  base::TaskRunner* task_runner_ = nullptr;
  std::unique_ptr<TraceWriter> trace_writer_;
  PERFETTO_THREAD_CHECKER(thread_checker_)
  base::WeakPtrFactory<MetatraceWriter> weak_ptr_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_METATRACE_WRITER_H_
