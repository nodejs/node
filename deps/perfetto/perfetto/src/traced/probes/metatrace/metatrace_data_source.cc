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

#include "src/traced/probes/metatrace/metatrace_data_source.h"

#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/tracing/core/metatrace_writer.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

// static
const ProbesDataSource::Descriptor MetatraceDataSource::descriptor = {
    /*name*/ MetatraceWriter::kDataSourceName,
    /*flags*/ Descriptor::kFlagsNone,
};

MetatraceDataSource::MetatraceDataSource(base::TaskRunner* task_runner,
                                         TracingSessionID session_id,
                                         std::unique_ptr<TraceWriter> writer)
    : ProbesDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      trace_writer_(std::move(writer)) {}

MetatraceDataSource::~MetatraceDataSource() {
  metatrace_writer_->Disable();
}

void MetatraceDataSource::Start() {
  metatrace_writer_.reset(new MetatraceWriter());
  metatrace_writer_->Enable(task_runner_, std::move(trace_writer_),
                            metatrace::TAG_ANY);
}

// This method is also called from StopDataSource with a dummy FlushRequestID
// (zero), to ensure that the metatrace commits the events recorded during the
// final flush of the tracing session.
void MetatraceDataSource::Flush(FlushRequestID,
                                std::function<void()> callback) {
  metatrace_writer_->WriteAllAndFlushTraceWriter(std::move(callback));
}

}  // namespace perfetto
