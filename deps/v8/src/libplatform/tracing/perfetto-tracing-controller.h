// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_PERFETTO_TRACING_CONTROLLER_H_
#define V8_LIBPLATFORM_TRACING_PERFETTO_TRACING_CONTROLLER_H_

#include <atomic>
#include <fstream>
#include <memory>
#include <vector>

#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"

namespace perfetto {
class TraceConfig;
class TraceWriter;
class TracingService;
}  // namespace perfetto

namespace v8 {
namespace platform {
namespace tracing {

class PerfettoConsumer;
class PerfettoProducer;
class PerfettoTaskRunner;
class TraceEventListener;

// This is the top-level interface for performing tracing with perfetto. The
// user of this class should call StartTracing() to start tracing, and
// StopTracing() to stop it. To write trace events, the user can obtain a
// thread-local TraceWriter object using GetOrCreateThreadLocalWriter().
class PerfettoTracingController {
 public:
  PerfettoTracingController();

  // Blocks and sets up all required data structures for tracing. It is safe to
  // call GetOrCreateThreadLocalWriter() to obtain thread-local TraceWriters for
  // writing trace events once this call returns. Tracing output will be sent to
  // the TraceEventListeners registered via AddTraceEventListener().
  void StartTracing(const ::perfetto::TraceConfig& trace_config);

  // Blocks and finishes all existing AddTraceEvent tasks. Stops the tracing
  // thread.
  void StopTracing();

  // Register a trace event listener that will receive trace events. This can be
  // called multiple times to register multiple listeners, but must be called
  // before starting tracing.
  void AddTraceEventListener(TraceEventListener* listener);

  ~PerfettoTracingController();

  // Each thread that wants to trace should call this to get their TraceWriter.
  // PerfettoTracingController creates and owns the TraceWriter.
  ::perfetto::TraceWriter* GetOrCreateThreadLocalWriter();

 private:
  // Signals the producer_ready_semaphore_.
  void OnProducerReady();

  // PerfettoProducer is the only class allowed to call OnProducerReady().
  friend class PerfettoProducer;

  std::unique_ptr<::perfetto::TracingService> service_;
  std::unique_ptr<PerfettoProducer> producer_;
  std::unique_ptr<PerfettoConsumer> consumer_;
  std::unique_ptr<PerfettoTaskRunner> task_runner_;
  std::vector<TraceEventListener*> listeners_;
  base::Thread::LocalStorageKey writer_key_;
  // A semaphore that is signalled when StartRecording is called. StartTracing
  // waits on this semaphore to be notified when the tracing service is ready to
  // receive trace events.
  base::Semaphore producer_ready_semaphore_;
  base::Semaphore consumer_finished_semaphore_;

  DISALLOW_COPY_AND_ASSIGN(PerfettoTracingController);
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_PERFETTO_TRACING_CONTROLLER_H_
