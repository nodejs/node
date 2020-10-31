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

#ifndef INCLUDE_PERFETTO_TRACING_INTERNAL_IN_PROCESS_TRACING_BACKEND_H_
#define INCLUDE_PERFETTO_TRACING_INTERNAL_IN_PROCESS_TRACING_BACKEND_H_

#include "perfetto/base/export.h"
#include "perfetto/tracing/tracing_backend.h"

namespace perfetto {

namespace base {
class TaskRunner;
}

class Producer;
class TracingService;

namespace internal {

// A built-in implementation of TracingBackend that creates a tracing service
// instance in-process. Instantiated when the embedder calls
// Tracing::Initialize(kInProcessBackend). Solves most in-app-only tracing
// use-cases.
class PERFETTO_EXPORT InProcessTracingBackend : public TracingBackend {
 public:
  static TracingBackend* GetInstance();

  // TracingBackend implementation.
  std::unique_ptr<ProducerEndpoint> ConnectProducer(
      const ConnectProducerArgs&) override;
  std::unique_ptr<ConsumerEndpoint> ConnectConsumer(
      const ConnectConsumerArgs&) override;

 private:
  InProcessTracingBackend();
  TracingService* GetOrCreateService(base::TaskRunner*);

  std::unique_ptr<TracingService> service_;
};

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_INTERNAL_IN_PROCESS_TRACING_BACKEND_H_
