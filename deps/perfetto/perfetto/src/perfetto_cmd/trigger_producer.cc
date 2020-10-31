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

#include "src/perfetto_cmd/trigger_producer.h"

#include <memory>

#include "perfetto/base/logging.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"

namespace perfetto {

TriggerProducer::TriggerProducer(base::TaskRunner* task_runner,
                                 std::function<void(bool)> callback,
                                 const std::vector<std::string>* const triggers)
    : task_runner_(task_runner),
      callback_(std::move(callback)),
      triggers_(triggers),
      producer_endpoint_(ProducerIPCClient::Connect(GetProducerSocket(),
                                                    this,
                                                    "perfetto_cmd_producer",
                                                    task_runner)),
      weak_factory_(this) {
  // Give the socket up to 10 seconds to attach and send the triggers before
  // reporting a failure.
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this]() {
        if (!weak_this || weak_this->issued_callback_)
          return;
        weak_this->issued_callback_ = true;
        weak_this->callback_(false);
      },
      10000);
}

TriggerProducer::~TriggerProducer() {}

void TriggerProducer::OnConnect() {
  PERFETTO_DLOG("Producer connected, sending triggers.");
  // Send activation signal.
  producer_endpoint_->ActivateTriggers(*triggers_);
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this]() {
    if (!weak_this || weak_this->issued_callback_)
      return;
    weak_this->issued_callback_ = true;
    weak_this->callback_(true);
  });
}

void TriggerProducer::OnDisconnect() {}

void TriggerProducer::OnTracingSetup() {}

void TriggerProducer::SetupDataSource(DataSourceInstanceID,
                                      const DataSourceConfig&) {
  PERFETTO_DFATAL("Attempted to SetupDataSource() on commandline producer");
}
void TriggerProducer::StartDataSource(DataSourceInstanceID,
                                      const DataSourceConfig&) {
  PERFETTO_DFATAL("Attempted to StartDataSource() on commandline producer");
}
void TriggerProducer::StopDataSource(DataSourceInstanceID) {
  PERFETTO_DFATAL("Attempted to StopDataSource() on commandline producer");
}
void TriggerProducer::Flush(FlushRequestID,
                            const DataSourceInstanceID*,
                            size_t) {
  PERFETTO_DFATAL("Attempted to Flush() on commandline producer");
}

void TriggerProducer::ClearIncrementalState(const DataSourceInstanceID*,
                                            size_t) {
  PERFETTO_DFATAL(
      "Attempted to ClearIncrementalState() on commandline producer");
}

}  // namespace perfetto
