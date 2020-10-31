/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "test/fake_producer.h"

#include <mutex>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/traced/traced.h"
#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"

#include "protos/perfetto/config/test_config.gen.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

namespace {
const MaybeUnboundBufferID kStartupTargetBufferReservationId = 1;
}  // namespace

FakeProducer::FakeProducer(const std::string& name,
                           base::TaskRunner* task_runner)
    : name_(name), task_runner_(task_runner) {}
FakeProducer::~FakeProducer() = default;

void FakeProducer::Connect(const char* socket_name,
                           std::function<void()> on_connect,
                           std::function<void()> on_setup_data_source_instance,
                           std::function<void()> on_create_data_source_instance,
                           std::unique_ptr<SharedMemory> shm,
                           std::unique_ptr<SharedMemoryArbiter> shm_arbiter) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  endpoint_ = ProducerIPCClient::Connect(
      socket_name, this, "android.perfetto.FakeProducer", task_runner_,
      TracingService::ProducerSMBScrapingMode::kDefault,
      /*shared_memory_size_hint_bytes=*/0,
      /*shared_memory_page_size_hint_bytes=*/base::kPageSize, std::move(shm),
      std::move(shm_arbiter));
  on_connect_ = std::move(on_connect);
  on_setup_data_source_instance_ = std::move(on_setup_data_source_instance);
  on_create_data_source_instance_ = std::move(on_create_data_source_instance);
}

void FakeProducer::OnConnect() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  DataSourceDescriptor descriptor;
  descriptor.set_name(name_);
  endpoint_->RegisterDataSource(descriptor);
  auto on_connect_callback = std::move(on_connect_);
  auto task_runner = task_runner_;
  endpoint_->Sync([task_runner, on_connect_callback] {
    task_runner->PostTask(on_connect_callback);
  });
}

void FakeProducer::OnDisconnect() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_FATAL("Producer unexpectedly disconnected from the service");
}

void FakeProducer::SetupDataSource(DataSourceInstanceID,
                                   const DataSourceConfig&) {
  task_runner_->PostTask(on_setup_data_source_instance_);
}

void FakeProducer::StartDataSource(DataSourceInstanceID,
                                   const DataSourceConfig& source_config) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (trace_writer_) {
    // Startup tracing was already active, just bind the target buffer.
    endpoint_->MaybeSharedMemoryArbiter()->BindStartupTargetBuffer(
        kStartupTargetBufferReservationId,
        static_cast<BufferID>(source_config.target_buffer()));
  } else {
    // Common case: Start tracing now.
    trace_writer_ = endpoint_->CreateTraceWriter(
        static_cast<BufferID>(source_config.target_buffer()));
    SetupFromConfig(source_config.for_testing());
  }
  if (source_config.for_testing().send_batch_on_register()) {
    ProduceEventBatch(on_create_data_source_instance_);
  } else {
    task_runner_->PostTask(on_create_data_source_instance_);
  }
}

void FakeProducer::StopDataSource(DataSourceInstanceID) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  trace_writer_.reset();
}

// Note: this can be called on a different thread.
void FakeProducer::ProduceStartupEventBatch(
    const protos::gen::TestConfig& config,
    SharedMemoryArbiter* arbiter,
    std::function<void()> callback) {
  task_runner_->PostTask([this, config, arbiter, callback] {
    SetupFromConfig(config);

    PERFETTO_CHECK(!trace_writer_);
    trace_writer_ =
        arbiter->CreateStartupTraceWriter(kStartupTargetBufferReservationId);

    EmitEventBatchOnTaskRunner({});

    // Issue callback right after writing - cannot wait for flush yet because
    // we're not connected yet.
    callback();
  });
}

// Note: this can be called on a different thread.
void FakeProducer::ProduceEventBatch(std::function<void()> callback) {
  task_runner_->PostTask(
      [this, callback] { EmitEventBatchOnTaskRunner(callback); });
}

void FakeProducer::RegisterDataSource(const DataSourceDescriptor& desc) {
  task_runner_->PostTask([this, desc] { endpoint_->RegisterDataSource(desc); });
}

void FakeProducer::CommitData(const CommitDataRequest& req,
                              std::function<void()> callback) {
  task_runner_->PostTask(
      [this, req, callback] { endpoint_->CommitData(req, callback); });
}

void FakeProducer::Sync(std::function<void()> callback) {
  task_runner_->PostTask([this, callback] { endpoint_->Sync(callback); });
}

void FakeProducer::OnTracingSetup() {}

void FakeProducer::Flush(FlushRequestID flush_request_id,
                         const DataSourceInstanceID*,
                         size_t num_data_sources) {
  PERFETTO_DCHECK(num_data_sources > 0);
  if (trace_writer_)
    trace_writer_->Flush();
  endpoint_->NotifyFlushComplete(flush_request_id);
}

void FakeProducer::SetupFromConfig(const protos::gen::TestConfig& config) {
  rnd_engine_ = std::minstd_rand0(config.seed());
  message_count_ = config.message_count();
  message_size_ = config.message_size();
  max_messages_per_second_ = config.max_messages_per_second();
}

void FakeProducer::EmitEventBatchOnTaskRunner(std::function<void()> callback) {
  PERFETTO_CHECK(trace_writer_);
  PERFETTO_CHECK(message_size_ > 1);
  std::unique_ptr<char, base::FreeDeleter> payload(
      static_cast<char*>(malloc(message_size_)));
  memset(payload.get(), '.', message_size_);
  payload.get()[message_size_ - 1] = 0;

  base::TimeMillis start = base::GetWallTimeMs();
  int64_t iterations = 0;
  uint32_t messages_to_emit = message_count_;
  while (messages_to_emit > 0) {
    uint32_t messages_in_minibatch =
        max_messages_per_second_ == 0
            ? messages_to_emit
            : std::min(max_messages_per_second_, messages_to_emit);
    PERFETTO_DCHECK(messages_to_emit >= messages_in_minibatch);

    for (uint32_t i = 0; i < messages_in_minibatch; i++) {
      auto handle = trace_writer_->NewTracePacket();
      handle->set_for_testing()->set_seq_value(
          static_cast<uint32_t>(rnd_engine_()));
      handle->set_for_testing()->set_str(payload.get(), message_size_);
    }
    messages_to_emit -= messages_in_minibatch;
    iterations++;

    // Pause until the second boundary to make sure that we are adhering to
    // the speed limitation.
    if (max_messages_per_second_ > 0) {
      int64_t expected_time_taken = iterations * 1000;
      base::TimeMillis time_taken = base::GetWallTimeMs() - start;
      while (time_taken.count() < expected_time_taken) {
        usleep(static_cast<useconds_t>(
            (expected_time_taken - time_taken.count()) * 1000));
        time_taken = base::GetWallTimeMs() - start;
      }
    }
    trace_writer_->Flush(messages_to_emit > 0 ? [] {} : callback);
  }
}

}  // namespace perfetto
