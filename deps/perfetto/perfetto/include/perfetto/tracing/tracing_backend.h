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

#ifndef INCLUDE_PERFETTO_TRACING_TRACING_BACKEND_H_
#define INCLUDE_PERFETTO_TRACING_TRACING_BACKEND_H_

#include <memory>
#include <string>

// The embedder can (but doesn't have to) extend the TracingBackend class and
// pass as an argument to Tracing::Initialize(kCustomBackend) to override the
// way to reach the service. This is for peculiar cases where the embedder has
// a multi-process architecture and wants to override the IPC transport. The
// real use-case for this at the time of writing is chromium (+ Mojo IPC).
// Extending this class requires depending on the full set of perfetto headers
// (not just /public/). Contact the team before doing so as the non-public
// headers are not guaranteed to be API stable.

namespace perfetto {

namespace base {
class TaskRunner;
}

// These classes are declared in headers outside of /public/.
class Consumer;
class ConsumerEndpoint;
class Producer;
class ProducerEndpoint;

class TracingBackend {
 public:
  virtual ~TracingBackend();

  // Connects a Producer instance and obtains a ProducerEndpoint, which is
  // essentially a 1:1 channel between one Producer and the Service.
  // To disconnect just destroy the returned endpoint object. It is safe to
  // destroy the Producer once Producer::OnDisconnect() has been invoked.
  struct ConnectProducerArgs {
    std::string producer_name;

    // The Producer object that will receive calls like Start/StopDataSource().
    // The caller has to guarantee that this object is valid as long as the
    // returned ProducerEndpoint is alive.
    Producer* producer = nullptr;

    // The task runner where the Producer methods will be called onto.
    // The caller has to guarantee that the passed TaskRunner is valid as long
    // as the returned ProducerEndpoint is alive.
    ::perfetto::base::TaskRunner* task_runner = nullptr;

    // These get propagated from TracingInitArgs and are optionally provided by
    // the client when calling Tracing::Initialize().
    uint32_t shmem_size_hint_bytes = 0;
    uint32_t shmem_page_size_hint_bytes = 0;
  };

  virtual std::unique_ptr<ProducerEndpoint> ConnectProducer(
      const ConnectProducerArgs&) = 0;

  // As above, for the Consumer-side.
  struct ConnectConsumerArgs {
    // The Consumer object that will receive calls like OnTracingDisabled(),
    // OnTraceData().
    Consumer* consumer{};

    // The task runner where the Consumer methods will be called onto.
    ::perfetto::base::TaskRunner* task_runner{};
  };
  virtual std::unique_ptr<ConsumerEndpoint> ConnectConsumer(
      const ConnectConsumerArgs&) = 0;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_TRACING_BACKEND_H_
