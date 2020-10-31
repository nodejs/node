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

#ifndef SRC_TRACED_SERVICE_BUILTIN_PRODUCER_H_
#define SRC_TRACED_SERVICE_BUILTIN_PRODUCER_H_

#include <map>
#include <set>
#include <string>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "src/tracing/core/metatrace_writer.h"

namespace perfetto {

// Data sources built into the tracing service daemon (traced):
// * perfetto metatrace
// * lazy heapprofd daemon starter (android only)
// * lazy traced_perf daemon starter (android only)
class BuiltinProducer : public Producer {
 public:
  BuiltinProducer(base::TaskRunner* task_runner, uint32_t lazy_stop_delay_ms);

  ~BuiltinProducer() override;
  void OnConnect() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override;
  void StopDataSource(DataSourceInstanceID) override;

  // nops:
  void OnDisconnect() override {}
  void OnTracingSetup() override {}
  void ClearIncrementalState(const DataSourceInstanceID*, size_t) override {}

  void ConnectInProcess(TracingService* svc);

  // virtual for testing
  virtual bool SetAndroidProperty(const std::string& name,
                                  const std::string& value);

 private:
  struct MetatraceState {
    // If multiple metatrace sources are enabled concurrently, only the first
    // one becomes active. But we still want to be responsive to the others'
    // flushes.
    std::map<DataSourceInstanceID, MetatraceWriter> writers;
  };

  struct LazyAndroidDaemonState {
    // Track active instances to know when to stop.
    std::set<DataSourceInstanceID> instance_ids;
    // Delay between the last matching session stopping, and the lazy system
    // property being unset (to shut down the daemon).
    uint32_t stop_delay_ms;
    uint64_t generation = 0;
  };

  void MaybeInitiateLazyStop(DataSourceInstanceID ds_id,
                             LazyAndroidDaemonState* lazy_state,
                             const char* prop_name);

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;

  MetatraceState metatrace_;
  LazyAndroidDaemonState lazy_heapprofd_;
  LazyAndroidDaemonState lazy_traced_perf_;

  base::WeakPtrFactory<BuiltinProducer> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_SERVICE_BUILTIN_PRODUCER_H_
