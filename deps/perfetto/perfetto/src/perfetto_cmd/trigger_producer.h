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

#ifndef SRC_PERFETTO_CMD_TRIGGER_PRODUCER_H_
#define SRC_PERFETTO_CMD_TRIGGER_PRODUCER_H_

#include <string>
#include <vector>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/forward_decls.h"
namespace perfetto {

// This is a producer that only sends the provided |triggers| to the service. It
// will never register any data sources.
class TriggerProducer : public Producer {
 public:
  TriggerProducer(base::TaskRunner* task_runner,
                  std::function<void(bool)> callback,
                  const std::vector<std::string>* const triggers);
  ~TriggerProducer() override;

  // We will call Trigger() on the |producer_endpoint_| and then
  // immediately call |callback_|.
  void OnConnect() override;
  // We have no clean up to do OnDisconnect.
  void OnDisconnect() override;

  // Unimplemented methods are below this.
  void OnTracingSetup() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override;
  void ClearIncrementalState(const DataSourceInstanceID* data_source_ids,
                             size_t num_data_sources) override;

 private:
  bool issued_callback_ = false;
  base::TaskRunner* const task_runner_;
  const std::function<void(bool)> callback_;
  const std::vector<std::string>* const triggers_;
  std::unique_ptr<TracingService::ProducerEndpoint> producer_endpoint_;
  base::WeakPtrFactory<TriggerProducer> weak_factory_;
};

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_TRIGGER_PRODUCER_H_
