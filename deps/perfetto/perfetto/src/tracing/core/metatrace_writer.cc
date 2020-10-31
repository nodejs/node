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

#include "src/tracing/core/metatrace_writer.h"

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_descriptor.h"

#include "protos/perfetto/trace/perfetto/perfetto_metatrace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

// static
constexpr char MetatraceWriter::kDataSourceName[];

MetatraceWriter::MetatraceWriter() : weak_ptr_factory_(this) {}

MetatraceWriter::~MetatraceWriter() {
  Disable();
}

void MetatraceWriter::Enable(base::TaskRunner* task_runner,
                             std::unique_ptr<TraceWriter> trace_writer,
                             uint32_t tags) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (started_) {
    PERFETTO_DFATAL_OR_ELOG("Metatrace already started from this instance");
    return;
  }
  task_runner_ = task_runner;
  trace_writer_ = std::move(trace_writer);
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  bool enabled = metatrace::Enable(
      [weak_ptr] {
        if (weak_ptr)
          weak_ptr->WriteAllAvailableEvents();
      },
      task_runner, tags);
  if (!enabled)
    return;
  started_ = true;
}

void MetatraceWriter::Disable() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!started_)
    return;
  metatrace::Disable();
  started_ = false;
  trace_writer_.reset();
}

void MetatraceWriter::WriteAllAvailableEvents() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!started_)
    return;
  for (auto it = metatrace::RingBuffer::GetReadIterator(); it; ++it) {
    auto type_and_id = it->type_and_id.load(std::memory_order_acquire);
    if (type_and_id == 0)
      break;  // Stop at the first incomplete event.

    auto packet = trace_writer_->NewTracePacket();
    packet->set_timestamp(it->timestamp_ns());
    auto* evt = packet->set_perfetto_metatrace();
    uint16_t type = type_and_id & metatrace::Record::kTypeMask;
    uint16_t id = type_and_id & ~metatrace::Record::kTypeMask;
    if (type == metatrace::Record::kTypeCounter) {
      evt->set_counter_id(id);
      evt->set_counter_value(it->counter_value);
    } else {
      evt->set_event_id(id);
      evt->set_event_duration_ns(it->duration_ns);
    }

    evt->set_thread_id(static_cast<uint32_t>(it->thread_id));

    if (metatrace::RingBuffer::has_overruns())
      evt->set_has_overruns(true);
  }
  // The |it| destructor will automatically update the read index position in
  // the meta-trace ring buffer.
}

void MetatraceWriter::WriteAllAndFlushTraceWriter(
    std::function<void()> callback) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!started_)
    return;
  WriteAllAvailableEvents();
  trace_writer_->Flush(std::move(callback));
}

}  // namespace perfetto
