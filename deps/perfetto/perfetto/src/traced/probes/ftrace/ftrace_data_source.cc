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

#include "src/traced/probes/ftrace/ftrace_data_source.h"

#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/ftrace/ftrace_controller.h"

#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_stats.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

// static
const ProbesDataSource::Descriptor FtraceDataSource::descriptor = {
    /*name*/ "linux.ftrace",
    /*flags*/ Descriptor::kFlagsNone,
};

FtraceDataSource::FtraceDataSource(
    base::WeakPtr<FtraceController> controller_weak,
    TracingSessionID session_id,
    const FtraceConfig& config,
    std::unique_ptr<TraceWriter> writer)
    : ProbesDataSource(session_id, &descriptor),
      config_(config),
      writer_(std::move(writer)),
      controller_weak_(std::move(controller_weak)) {}

FtraceDataSource::~FtraceDataSource() {
  if (controller_weak_)
    controller_weak_->RemoveDataSource(this);
}

void FtraceDataSource::Initialize(
    FtraceConfigId config_id,
    const FtraceDataSourceConfig* parsing_config) {
  PERFETTO_CHECK(config_id);
  config_id_ = config_id;
  parsing_config_ = parsing_config;
}

void FtraceDataSource::Start() {
  FtraceController* ftrace = controller_weak_.get();
  if (!ftrace)
    return;
  PERFETTO_CHECK(config_id_);  // Must be initialized at this point.
  if (!ftrace->StartDataSource(this))
    return;
  DumpFtraceStats(&stats_before_);
}

void FtraceDataSource::DumpFtraceStats(FtraceStats* stats) {
  if (controller_weak_)
    controller_weak_->DumpFtraceStats(stats);
}

void FtraceDataSource::Flush(FlushRequestID flush_request_id,
                             std::function<void()> callback) {
  if (!controller_weak_)
    return;

  pending_flushes_[flush_request_id] = std::move(callback);

  // FtraceController will call OnFtraceFlushComplete() once the data has been
  // drained from the ftrace buffer and written into the various writers of
  // all its active data sources.
  controller_weak_->Flush(flush_request_id);
}

// Called by FtraceController after all CPUs have acked the flush or timed out.
void FtraceDataSource::OnFtraceFlushComplete(FlushRequestID flush_request_id) {
  auto it = pending_flushes_.find(flush_request_id);
  if (it == pending_flushes_.end()) {
    // This can genuinely happen in case of concurrent ftrace sessions. When a
    // FtraceDataSource issues a flush, the controller has to drain ftrace data
    // for everybody (there is only one kernel ftrace buffer for all sessions).
    // FtraceController doesn't bother to remember which FtraceDataSource did or
    // did not request a flush. Instead just boradcasts the
    // OnFtraceFlushComplete() to all of them.
    return;
  }
  auto callback = std::move(it->second);
  pending_flushes_.erase(it);
  if (writer_) {
    WriteStats();
    writer_->Flush(std::move(callback));
  }
}

void FtraceDataSource::WriteStats() {
  {
    auto before_packet = writer_->NewTracePacket();
    auto out = before_packet->set_ftrace_stats();
    out->set_phase(protos::pbzero::FtraceStats_Phase_START_OF_TRACE);
    stats_before_.Write(out);
  }
  {
    FtraceStats stats_after{};
    DumpFtraceStats(&stats_after);
    auto after_packet = writer_->NewTracePacket();
    auto out = after_packet->set_ftrace_stats();
    out->set_phase(protos::pbzero::FtraceStats_Phase_END_OF_TRACE);
    stats_after.Write(out);
  }
}

}  // namespace perfetto
