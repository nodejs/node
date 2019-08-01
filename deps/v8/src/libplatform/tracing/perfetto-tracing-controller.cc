// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/perfetto-tracing-controller.h"

#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/tracing_service.h"
#include "src/libplatform/tracing/perfetto-consumer.h"
#include "src/libplatform/tracing/perfetto-producer.h"
#include "src/libplatform/tracing/perfetto-shared-memory.h"
#include "src/libplatform/tracing/perfetto-tasks.h"
#include "src/libplatform/tracing/trace-event-listener.h"

namespace v8 {
namespace platform {
namespace tracing {

PerfettoTracingController::PerfettoTracingController()
    : writer_key_(base::Thread::CreateThreadLocalKey()),
      producer_ready_semaphore_(0),
      consumer_finished_semaphore_(0) {}

void PerfettoTracingController::StartTracing(
    const ::perfetto::TraceConfig& trace_config) {
  DCHECK(!task_runner_);
  task_runner_ = base::make_unique<PerfettoTaskRunner>();
  // The Perfetto service expects calls on the task runner thread which is why
  // the setup below occurs in posted tasks.
  task_runner_->PostTask([&trace_config, this] {
    std::unique_ptr<::perfetto::SharedMemory::Factory> shmem_factory =
        base::make_unique<PerfettoSharedMemoryFactory>();

    service_ = ::perfetto::TracingService::CreateInstance(
        std::move(shmem_factory), task_runner_.get());
    // This allows Perfetto to recover trace events that were written by
    // TraceWriters which have not yet been deleted. This allows us to keep
    // TraceWriters alive past the end of tracing, rather than having to delete
    // them all when tracing stops which would require synchronization on every
    // trace event. Eventually we will delete TraceWriters when threads die, but
    // for now we just leak all TraceWriters.
    service_->SetSMBScrapingEnabled(true);
    producer_ = base::make_unique<PerfettoProducer>(this);
    consumer_ =
        base::make_unique<PerfettoConsumer>(&consumer_finished_semaphore_);

    for (TraceEventListener* listener : listeners_) {
      consumer_->AddTraceEventListener(listener);
    }

    producer_->set_service_endpoint(service_->ConnectProducer(
        producer_.get(), 0, "v8.perfetto-producer", 0, true));

    consumer_->set_service_endpoint(
        service_->ConnectConsumer(consumer_.get(), 0));

    // We need to wait for the OnConnected() callbacks of the producer and
    // consumer to be called.
    consumer_->service_endpoint()->EnableTracing(trace_config);
  });

  producer_ready_semaphore_.Wait();
}

void PerfettoTracingController::StopTracing() {
  // Finish all of the tasks such as existing AddTraceEvent calls. These
  // require the data structures below to work properly, so keep them alive
  // until the tasks are done.
  task_runner_->FinishImmediateTasks();

  task_runner_->PostTask([this] {
    // Trigger shared memory buffer scraping which will get all pending trace
    // events that have been written by still-living TraceWriters.
    consumer_->service_endpoint()->DisableTracing();
    // Trigger the consumer to finish. This can trigger multiple calls to
    // PerfettoConsumer::OnTraceData(), with the final call passing has_more
    // as false.
    consumer_->service_endpoint()->ReadBuffers();
  });

  // Wait until the final OnTraceData() call with has_more=false has completed.
  consumer_finished_semaphore_.Wait();

  task_runner_->PostTask([this] {
    consumer_.reset();
    producer_.reset();
    service_.reset();
  });

  // Finish the above task, and any callbacks that were triggered.
  task_runner_->FinishImmediateTasks();
  task_runner_.reset();
}

void PerfettoTracingController::AddTraceEventListener(
    TraceEventListener* listener) {
  listeners_.push_back(listener);
}

PerfettoTracingController::~PerfettoTracingController() {
  base::Thread::DeleteThreadLocalKey(writer_key_);
}

::perfetto::TraceWriter*
PerfettoTracingController::GetOrCreateThreadLocalWriter() {
  // TODO(petermarshall): Use some form of thread-local destructor so that
  // repeatedly created threads don't cause excessive leaking of TraceWriters.
  if (base::Thread::HasThreadLocal(writer_key_)) {
    return static_cast<::perfetto::TraceWriter*>(
        base::Thread::GetExistingThreadLocal(writer_key_));
  }

  // We leak the TraceWriter objects created for each thread. Perfetto has a
  // way of getting events from leaked TraceWriters and we can avoid needing a
  // lock on every trace event this way.
  std::unique_ptr<::perfetto::TraceWriter> tw = producer_->CreateTraceWriter();
  ::perfetto::TraceWriter* writer = tw.release();

  base::Thread::SetThreadLocal(writer_key_, writer);
  return writer;
}

void PerfettoTracingController::OnProducerReady() {
  producer_ready_semaphore_.Signal();
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
