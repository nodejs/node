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

#ifndef SRC_TRACED_PROBES_METATRACE_METATRACE_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_METATRACE_METATRACE_DATA_SOURCE_H_

#include <memory>

#include "src/traced/probes/probes_data_source.h"

namespace perfetto {

class MetatraceWriter;
class TraceWriter;

namespace base {
class TaskRunner;
}

class MetatraceDataSource : public ProbesDataSource {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  MetatraceDataSource(base::TaskRunner*,
                      TracingSessionID,
                      std::unique_ptr<TraceWriter> writer);

  ~MetatraceDataSource() override;

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

 private:
  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> trace_writer_;
  std::unique_ptr<MetatraceWriter> metatrace_writer_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_METATRACE_METATRACE_DATA_SOURCE_H_
