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

#include "src/profiling/memory/java_hprof_producer.h"

#include <signal.h>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/profiling/common/proc_utils.h"

namespace perfetto {
namespace profiling {
namespace {

constexpr int kJavaHeapprofdSignal = __SIGRTMIN + 6;
constexpr uint32_t kInitialConnectionBackoffMs = 100;
constexpr uint32_t kMaxConnectionBackoffMs = 30 * 1000;
constexpr const char* kJavaHprofDataSource = "android.java_hprof";

}  // namespace

void JavaHprofProducer::DoContinuousDump(DataSourceInstanceID id,
                                         uint32_t dump_interval) {
  auto it = data_sources_.find(id);
  if (it == data_sources_.end())
    return;
  const DataSource& ds = it->second;
  SignalDataSource(ds);
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer, id, dump_interval] {
        if (!weak_producer)
          return;
        weak_producer->DoContinuousDump(id, dump_interval);
      },
      dump_interval);
}

// static
void JavaHprofProducer::SignalDataSource(const DataSource& ds) {
  const std::set<pid_t>& pids = ds.pids;
  for (pid_t pid : pids) {
    PERFETTO_DLOG("Sending %d to %d", kJavaHeapprofdSignal, pid);
    if (kill(pid, kJavaHeapprofdSignal) != 0) {
      PERFETTO_DPLOG("kill");
    }
  }
}

void JavaHprofProducer::IncreaseConnectionBackoff() {
  connection_backoff_ms_ *= 2;
  if (connection_backoff_ms_ > kMaxConnectionBackoffMs)
    connection_backoff_ms_ = kMaxConnectionBackoffMs;
}

void JavaHprofProducer::ResetConnectionBackoff() {
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
}

void JavaHprofProducer::SetupDataSource(DataSourceInstanceID id,
                                        const DataSourceConfig& ds_config) {
  if (data_sources_.find(id) != data_sources_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Duplicate data source: %" PRIu64, id);
    return;
  }
  JavaHprofConfig config;
  config.ParseFromString(ds_config.java_hprof_config_raw());
  DataSource ds;
  ds.id = id;
  for (uint64_t pid : config.pid())
    ds.pids.emplace(static_cast<pid_t>(pid));
  base::Optional<std::vector<std::string>> normalized_cmdlines =
      NormalizeCmdlines(config.process_cmdline());
  if (!normalized_cmdlines.has_value()) {
    PERFETTO_ELOG("Rejecting data source due to invalid cmdline in config.");
    return;
  }
  FindPidsForCmdlines(normalized_cmdlines.value(), &ds.pids);
  if (config.min_anonymous_memory_kb() > 0)
    RemoveUnderAnonThreshold(config.min_anonymous_memory_kb(), &ds.pids);

  ds.config = std::move(config);
  data_sources_.emplace(id, std::move(ds));
}

void JavaHprofProducer::StartDataSource(DataSourceInstanceID id,
                                        const DataSourceConfig&) {
  auto it = data_sources_.find(id);
  if (it == data_sources_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Starting invalid data source: %" PRIu64, id);
    return;
  }
  const DataSource& ds = it->second;
  const auto continuous_dump_config = ds.config.continuous_dump_config();
  uint32_t dump_interval = continuous_dump_config.dump_interval_ms();
  if (dump_interval) {
    auto weak_producer = weak_factory_.GetWeakPtr();
    task_runner_->PostDelayedTask(
        [weak_producer, id, dump_interval] {
          if (!weak_producer)
            return;
          weak_producer->DoContinuousDump(id, dump_interval);
        },
        continuous_dump_config.dump_phase_ms());
  }
  SignalDataSource(ds);
}

void JavaHprofProducer::StopDataSource(DataSourceInstanceID id) {
  auto it = data_sources_.find(id);
  if (it == data_sources_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Stopping invalid data source: %" PRIu64, id);
    return;
  }
  data_sources_.erase(it);
}

void JavaHprofProducer::Flush(FlushRequestID flush_id,
                              const DataSourceInstanceID*,
                              size_t) {
  endpoint_->NotifyFlushComplete(flush_id);
}

void JavaHprofProducer::OnConnect() {
  PERFETTO_DCHECK(state_ == kConnecting);
  state_ = kConnected;
  ResetConnectionBackoff();
  PERFETTO_LOG("Connected to the service.");

  DataSourceDescriptor desc;
  desc.set_name(kJavaHprofDataSource);
  endpoint_->RegisterDataSource(desc);
}

void JavaHprofProducer::Restart() {
  // We lost the connection with the tracing service. At this point we need
  // to reset all the data sources. Trying to handle that manually is going to
  // be error prone. What we do here is simply destroy the instance and
  // recreate it again.
  base::TaskRunner* task_runner = task_runner_;
  const char* socket_name = producer_sock_name_;

  // Invoke destructor and then the constructor again.
  this->~JavaHprofProducer();
  new (this) JavaHprofProducer(task_runner);

  ConnectWithRetries(socket_name);
}

void JavaHprofProducer::ConnectWithRetries(const char* socket_name) {
  PERFETTO_DCHECK(state_ == kNotStarted);
  state_ = kNotConnected;

  ResetConnectionBackoff();
  producer_sock_name_ = socket_name;
  ConnectService();
}

void JavaHprofProducer::SetProducerEndpoint(
    std::unique_ptr<TracingService::ProducerEndpoint> endpoint) {
  PERFETTO_DCHECK(state_ == kNotConnected || state_ == kNotStarted);
  state_ = kConnecting;
  endpoint_ = std::move(endpoint);
}

void JavaHprofProducer::ConnectService() {
  SetProducerEndpoint(ProducerIPCClient::Connect(
      producer_sock_name_, this, "android.java_hprof", task_runner_));
}

void JavaHprofProducer::OnDisconnect() {
  PERFETTO_DCHECK(state_ == kConnected || state_ == kConnecting);
  PERFETTO_LOG("Disconnected from tracing service");

  auto weak_producer = weak_factory_.GetWeakPtr();
  if (state_ == kConnected)
    return task_runner_->PostTask([weak_producer] {
      if (!weak_producer)
        return;
      weak_producer->Restart();
    });

  state_ = kNotConnected;
  IncreaseConnectionBackoff();
  task_runner_->PostDelayedTask(
      [weak_producer] {
        if (!weak_producer)
          return;
        weak_producer->ConnectService();
      },
      connection_backoff_ms_);
}

}  // namespace profiling
}  // namespace perfetto
