/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACED_PROBES_INITIAL_DISPLAY_STATE_INITIAL_DISPLAY_STATE_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_INITIAL_DISPLAY_STATE_INITIAL_DISPLAY_STATE_DATA_SOURCE_H_

#include <memory>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/traced/probes/probes_data_source.h"

namespace perfetto {

namespace base {
class TaskRunner;
}

class InitialDisplayStateDataSource : public ProbesDataSource {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  InitialDisplayStateDataSource(base::TaskRunner* task_runner,
                                const DataSourceConfig& ds_config,
                                TracingSessionID session_id,
                                std::unique_ptr<TraceWriter> writer);

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  // Virtual for testing.
  virtual const base::Optional<std::string> ReadProperty(
      const std::string name);

 private:
  void Tick();
  base::WeakPtr<InitialDisplayStateDataSource> GetWeakPtr() const;
  void WriteState();

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> writer_;
  uint32_t poll_period_ms_ = 0;
  base::WeakPtrFactory<InitialDisplayStateDataSource>
      weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_INITIAL_DISPLAY_STATE_INITIAL_DISPLAY_STATE_DATA_SOURCE_H_
