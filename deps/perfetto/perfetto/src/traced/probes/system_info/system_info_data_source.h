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

#ifndef SRC_TRACED_PROBES_SYSTEM_INFO_SYSTEM_INFO_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_SYSTEM_INFO_SYSTEM_INFO_DATA_SOURCE_H_

#include <memory>

#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/traced/probes/common/cpu_freq_info.h"
#include "src/traced/probes/probes_data_source.h"

namespace perfetto {

class SystemInfoDataSource : public ProbesDataSource {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  SystemInfoDataSource(TracingSessionID,
                       std::unique_ptr<TraceWriter> writer,
                       std::unique_ptr<CpuFreqInfo> cpu_freq_info);

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  // Virtual for testing.
  virtual std::string ReadFile(std::string path);

 private:
  std::unique_ptr<TraceWriter> writer_;
  std::unique_ptr<CpuFreqInfo> cpu_freq_info_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_SYSTEM_INFO_SYSTEM_INFO_DATA_SOURCE_H_
