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

#ifndef SRC_TRACING_TEST_FAKE_PRODUCER_ENDPOINT_H_
#define SRC_TRACING_TEST_FAKE_PRODUCER_ENDPOINT_H_

#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/ext/tracing/core/tracing_service.h"

namespace perfetto {

class FakeProducerEndpoint : public TracingService::ProducerEndpoint {
 public:
  void RegisterDataSource(const DataSourceDescriptor&) override {}
  void UnregisterDataSource(const std::string&) override {}
  void RegisterTraceWriter(uint32_t, uint32_t) override {}
  void UnregisterTraceWriter(uint32_t) override {}
  void CommitData(const CommitDataRequest& req,
                  CommitDataCallback callback) override {
    last_commit_data_request = req;
    last_commit_data_callback = callback;
  }
  void NotifyFlushComplete(FlushRequestID) override {}
  void NotifyDataSourceStarted(DataSourceInstanceID) override {}
  void NotifyDataSourceStopped(DataSourceInstanceID) override {}
  void ActivateTriggers(const std::vector<std::string>&) override {}
  void Sync(std::function<void()>) override {}
  SharedMemory* shared_memory() const override { return nullptr; }
  size_t shared_buffer_page_size_kb() const override { return 0; }
  std::unique_ptr<TraceWriter> CreateTraceWriter(
      BufferID,
      BufferExhaustedPolicy) override {
    return nullptr;
  }
  SharedMemoryArbiter* MaybeSharedMemoryArbiter() override { return nullptr; }
  bool IsShmemProvidedByProducer() const override { return false; }

  CommitDataRequest last_commit_data_request;
  CommitDataCallback last_commit_data_callback;
};

}  // namespace perfetto

#endif  // SRC_TRACING_TEST_FAKE_PRODUCER_ENDPOINT_H_
