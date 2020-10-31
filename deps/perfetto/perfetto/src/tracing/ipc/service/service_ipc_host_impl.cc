/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/tracing/ipc/service/service_ipc_host_impl.h"

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/ipc/host.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "src/tracing/ipc/posix_shared_memory.h"
#include "src/tracing/ipc/service/consumer_ipc_service.h"
#include "src/tracing/ipc/service/producer_ipc_service.h"

namespace perfetto {

// TODO(fmayer): implement per-uid connection limit (b/69093705).

// Implements the publicly exposed factory method declared in
// include/tracing/posix_ipc/posix_service_host.h.
std::unique_ptr<ServiceIPCHost> ServiceIPCHost::CreateInstance(
    base::TaskRunner* task_runner) {
  return std::unique_ptr<ServiceIPCHost>(new ServiceIPCHostImpl(task_runner));
}

ServiceIPCHostImpl::ServiceIPCHostImpl(base::TaskRunner* task_runner)
    : task_runner_(task_runner) {}

ServiceIPCHostImpl::~ServiceIPCHostImpl() {}

bool ServiceIPCHostImpl::Start(const char* producer_socket_name,
                               const char* consumer_socket_name) {
  PERFETTO_CHECK(!svc_);  // Check if already started.

  // Initialize the IPC transport.
  producer_ipc_port_ =
      ipc::Host::CreateInstance(producer_socket_name, task_runner_);
  consumer_ipc_port_ =
      ipc::Host::CreateInstance(consumer_socket_name, task_runner_);
  return DoStart();
}

bool ServiceIPCHostImpl::Start(base::ScopedFile producer_socket_fd,
                               base::ScopedFile consumer_socket_fd) {
  PERFETTO_CHECK(!svc_);  // Check if already started.

  // Initialize the IPC transport.
  producer_ipc_port_ =
      ipc::Host::CreateInstance(std::move(producer_socket_fd), task_runner_);
  consumer_ipc_port_ =
      ipc::Host::CreateInstance(std::move(consumer_socket_fd), task_runner_);
  return DoStart();
}

bool ServiceIPCHostImpl::DoStart() {
  // Create and initialize the platform-independent tracing business logic.
  std::unique_ptr<SharedMemory::Factory> shm_factory(
      new PosixSharedMemory::Factory());
  svc_ = TracingService::CreateInstance(std::move(shm_factory), task_runner_);

  if (!producer_ipc_port_ || !consumer_ipc_port_) {
    Shutdown();
    return false;
  }

  // TODO(fmayer): add a test that destroyes the ServiceIPCHostImpl soon after
  // Start() and checks that no spurious callbacks are issued.
  bool producer_service_exposed = producer_ipc_port_->ExposeService(
      std::unique_ptr<ipc::Service>(new ProducerIPCService(svc_.get())));
  PERFETTO_CHECK(producer_service_exposed);

  bool consumer_service_exposed = consumer_ipc_port_->ExposeService(
      std::unique_ptr<ipc::Service>(new ConsumerIPCService(svc_.get())));
  PERFETTO_CHECK(consumer_service_exposed);

  return true;
}

TracingService* ServiceIPCHostImpl::service() const {
  return svc_.get();
}

void ServiceIPCHostImpl::Shutdown() {
  // TODO(primiano): add a test that causes the Shutdown() and checks that no
  // spurious callbacks are issued.
  producer_ipc_port_.reset();
  consumer_ipc_port_.reset();
  svc_.reset();
}

// Definitions for the base class ctor/dtor.
ServiceIPCHost::ServiceIPCHost() = default;
ServiceIPCHost::~ServiceIPCHost() = default;

}  // namespace perfetto
