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

#include "perfetto/tracing/internal/system_tracing_backend.h"

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"

namespace perfetto {
namespace internal {

// static
TracingBackend* SystemTracingBackend::GetInstance() {
  static auto* instance = new SystemTracingBackend();
  return instance;
}

SystemTracingBackend::SystemTracingBackend() {}

std::unique_ptr<ProducerEndpoint> SystemTracingBackend::ConnectProducer(
    const ConnectProducerArgs& args) {
  PERFETTO_DCHECK(args.task_runner->RunsTasksOnCurrentThread());

  auto endpoint = ProducerIPCClient::Connect(
      GetProducerSocket(), args.producer, args.producer_name, args.task_runner,
      TracingService::ProducerSMBScrapingMode::kEnabled,
      args.shmem_size_hint_bytes, args.shmem_page_size_hint_bytes);
  PERFETTO_CHECK(endpoint);
  return endpoint;
}

std::unique_ptr<ConsumerEndpoint> SystemTracingBackend::ConnectConsumer(
    const ConnectConsumerArgs&) {
  PERFETTO_FATAL(
      "Trace session creation is not supported yet when using the system "
      "tracing backend. Use the perfetto cmdline client instead to start "
      "system-wide tracing sessions");
}

}  // namespace internal
}  // namespace perfetto
