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

#include "src/profiling/memory/heapprofd_producer.h"

#include <algorithm>

#include <inttypes.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/thread_task_runner.h"
#include "perfetto/ext/base/watchdog_posix.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"

namespace perfetto {
namespace profiling {
namespace {
using ::perfetto::protos::pbzero::ProfilePacket;

constexpr char kHeapprofdDataSource[] = "android.heapprofd";
constexpr size_t kUnwinderThreads = 5;

constexpr uint32_t kInitialConnectionBackoffMs = 100;
constexpr uint32_t kMaxConnectionBackoffMs = 30 * 1000;
constexpr uint32_t kGuardrailIntervalMs = 30 * 1000;

constexpr uint32_t kChildModeWatchdogPeriodMs = 10 * 1000;

constexpr uint64_t kDefaultShmemSize = 8 * 1048576;  // ~8 MB
constexpr uint64_t kMaxShmemSize = 500 * 1048576;    // ~500 MB

// Constants specified by bionic, hardcoded here for simplicity.
constexpr int kProfilingSignal = __SIGRTMIN + 4;
constexpr int kHeapprofdSignalValue = 0;

std::vector<UnwindingWorker> MakeUnwindingWorkers(HeapprofdProducer* delegate,
                                                  size_t n) {
  std::vector<UnwindingWorker> ret;
  for (size_t i = 0; i < n; ++i) {
    ret.emplace_back(delegate, base::ThreadTaskRunner::CreateAndStart());
  }
  return ret;
}

bool ConfigTargetsProcess(const HeapprofdConfig& cfg,
                          const Process& proc,
                          const std::vector<std::string>& normalized_cmdlines) {
  if (cfg.all())
    return true;

  const auto& pids = cfg.pid();
  if (std::find(pids.cbegin(), pids.cend(), static_cast<uint64_t>(proc.pid)) !=
      pids.cend()) {
    return true;
  }

  if (std::find(normalized_cmdlines.cbegin(), normalized_cmdlines.cend(),
                proc.cmdline) != normalized_cmdlines.cend()) {
    return true;
  }
  return false;
}

// Return largest n such that pow(2, n) < value.
size_t Log2LessThan(uint64_t value) {
  size_t i = 0;
  while (value) {
    i++;
    value >>= 1;
  }
  return i;
}

}  // namespace

const uint64_t LogHistogram::kMaxBucket = 0;

std::vector<std::pair<uint64_t, uint64_t>> LogHistogram::GetData() {
  std::vector<std::pair<uint64_t, uint64_t>> data;
  data.reserve(kBuckets);
  for (size_t i = 0; i < kBuckets; ++i) {
    if (i == kBuckets - 1)
      data.emplace_back(kMaxBucket, values_[i]);
    else
      data.emplace_back(1 << i, values_[i]);
  }
  return data;
}

size_t LogHistogram::GetBucket(uint64_t value) {
  if (value == 0)
    return 0;

  size_t hibit = Log2LessThan(value);
  if (hibit >= kBuckets)
    return kBuckets - 1;
  return hibit;
}

// We create kUnwinderThreads unwinding threads. Bookkeeping is done on the main
// thread.
HeapprofdProducer::HeapprofdProducer(HeapprofdMode mode,
                                     base::TaskRunner* task_runner)
    : task_runner_(task_runner),
      mode_(mode),
      unwinding_workers_(MakeUnwindingWorkers(this, kUnwinderThreads)),
      socket_delegate_(this),
      weak_factory_(this) {
  CheckDataSourceMemory();  // Kick off guardrail task.
  stat_fd_.reset(open("/proc/self/stat", O_RDONLY));
  if (!stat_fd_) {
    PERFETTO_ELOG(
        "Failed to open /proc/self/stat. Cannot accept profiles "
        "with CPU guardrails.");
  } else {
    CheckDataSourceCpu();  // Kick off guardrail task.
  }
}

HeapprofdProducer::~HeapprofdProducer() = default;

void HeapprofdProducer::SetTargetProcess(pid_t target_pid,
                                         std::string target_cmdline,
                                         base::ScopedFile inherited_socket) {
  target_process_.pid = target_pid;
  target_process_.cmdline = target_cmdline;
  inherited_fd_ = std::move(inherited_socket);
}

void HeapprofdProducer::AdoptTargetProcessSocket() {
  PERFETTO_DCHECK(mode_ == HeapprofdMode::kChild);
  auto socket = base::UnixSocket::AdoptConnected(
      std::move(inherited_fd_), &socket_delegate_, task_runner_,
      base::SockFamily::kUnix, base::SockType::kStream);

  HandleClientConnection(std::move(socket), target_process_);
}

void HeapprofdProducer::OnConnect() {
  PERFETTO_DCHECK(state_ == kConnecting);
  state_ = kConnected;
  ResetConnectionBackoff();
  PERFETTO_LOG("Connected to the service, mode [%s].",
               mode_ == HeapprofdMode::kCentral ? "central" : "child");

  DataSourceDescriptor desc;
  desc.set_name(kHeapprofdDataSource);
  desc.set_will_notify_on_stop(true);
  endpoint_->RegisterDataSource(desc);
}

void HeapprofdProducer::OnDisconnect() {
  PERFETTO_DCHECK(state_ == kConnected || state_ == kConnecting);
  PERFETTO_LOG("Disconnected from tracing service");

  // Do not attempt to reconnect if we're a process-private process, just quit.
  if (mode_ == HeapprofdMode::kChild) {
    TerminateProcess(/*exit_status=*/1);  // does not return
  }

  // Central mode - attempt to reconnect.
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

void HeapprofdProducer::ConnectWithRetries(const char* socket_name) {
  PERFETTO_DCHECK(state_ == kNotStarted);
  state_ = kNotConnected;

  ResetConnectionBackoff();
  producer_sock_name_ = socket_name;
  ConnectService();
}

void HeapprofdProducer::ConnectService() {
  SetProducerEndpoint(ProducerIPCClient::Connect(
      producer_sock_name_, this, "android.heapprofd", task_runner_));
}

void HeapprofdProducer::SetProducerEndpoint(
    std::unique_ptr<TracingService::ProducerEndpoint> endpoint) {
  PERFETTO_DCHECK(state_ == kNotConnected || state_ == kNotStarted);
  state_ = kConnecting;
  endpoint_ = std::move(endpoint);
}

void HeapprofdProducer::IncreaseConnectionBackoff() {
  connection_backoff_ms_ *= 2;
  if (connection_backoff_ms_ > kMaxConnectionBackoffMs)
    connection_backoff_ms_ = kMaxConnectionBackoffMs;
}

void HeapprofdProducer::ResetConnectionBackoff() {
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
}

void HeapprofdProducer::Restart() {
  // We lost the connection with the tracing service. At this point we need
  // to reset all the data sources. Trying to handle that manually is going to
  // be error prone. What we do here is simply destroy the instance and
  // recreate it again.

  // Child mode producer should not attempt restarts. Note that this also means
  // the rest of this method doesn't have to handle child-specific state.
  if (mode_ == HeapprofdMode::kChild)
    PERFETTO_FATAL("Attempting to restart a child mode producer.");

  HeapprofdMode mode = mode_;
  base::TaskRunner* task_runner = task_runner_;
  const char* socket_name = producer_sock_name_;

  // Invoke destructor and then the constructor again.
  this->~HeapprofdProducer();
  new (this) HeapprofdProducer(mode, task_runner);

  ConnectWithRetries(socket_name);
}

void HeapprofdProducer::ScheduleActiveDataSourceWatchdog() {
  PERFETTO_DCHECK(mode_ == HeapprofdMode::kChild);

  // Post the first check after a delay, to let the freshly forked heapprofd
  // to receive the active data sources from traced. The checks will reschedule
  // themselves from that point onwards.
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer]() {
        if (!weak_producer)
          return;
        weak_producer->ActiveDataSourceWatchdogCheck();
      },
      kChildModeWatchdogPeriodMs);
}

void HeapprofdProducer::ActiveDataSourceWatchdogCheck() {
  PERFETTO_DCHECK(mode_ == HeapprofdMode::kChild);

  // Fork mode heapprofd should be working on exactly one data source matching
  // its target process.
  if (data_sources_.empty()) {
    PERFETTO_LOG(
        "Child heapprofd exiting as it never received a data source for the "
        "target process, or somehow lost/finished the task without exiting.");
    TerminateProcess(/*exit_status=*/1);
  } else {
    // reschedule check.
    auto weak_producer = weak_factory_.GetWeakPtr();
    task_runner_->PostDelayedTask(
        [weak_producer]() {
          if (!weak_producer)
            return;
          weak_producer->ActiveDataSourceWatchdogCheck();
        },
        kChildModeWatchdogPeriodMs);
  }
}

// TODO(rsavitski): would be cleaner to shut down the event loop instead
// (letting main exit). One test-friendly approach is to supply a shutdown
// callback in the constructor.
__attribute__((noreturn)) void HeapprofdProducer::TerminateProcess(
    int exit_status) {
  PERFETTO_CHECK(mode_ == HeapprofdMode::kChild);
  exit(exit_status);
}

void HeapprofdProducer::OnTracingSetup() {}

void HeapprofdProducer::SetupDataSource(DataSourceInstanceID id,
                                        const DataSourceConfig& ds_config) {
  PERFETTO_DLOG("Setting up data source.");
  if (mode_ == HeapprofdMode::kChild && ds_config.enable_extra_guardrails()) {
    PERFETTO_ELOG("enable_extra_guardrails is not supported on user.");
    return;
  }

  HeapprofdConfig heapprofd_config;
  heapprofd_config.ParseFromString(ds_config.heapprofd_config_raw());

  if (heapprofd_config.all() && !heapprofd_config.pid().empty())
    PERFETTO_ELOG("No point setting all and pid");
  if (heapprofd_config.all() && !heapprofd_config.process_cmdline().empty())
    PERFETTO_ELOG("No point setting all and process_cmdline");

  if (ds_config.name() != kHeapprofdDataSource) {
    PERFETTO_DLOG("Invalid data source name.");
    return;
  }

  if (data_sources_.find(id) != data_sources_.end()) {
    PERFETTO_DFATAL_OR_ELOG(
        "Received duplicated data source instance id: %" PRIu64, id);
    return;
  }

  base::Optional<std::vector<std::string>> normalized_cmdlines =
      NormalizeCmdlines(heapprofd_config.process_cmdline());
  if (!normalized_cmdlines.has_value()) {
    PERFETTO_ELOG("Rejecting data source due to invalid cmdline in config.");
    return;
  }

  // Child mode is only interested in the first data source matching the
  // already-connected process.
  if (mode_ == HeapprofdMode::kChild) {
    if (!ConfigTargetsProcess(heapprofd_config, target_process_,
                              normalized_cmdlines.value())) {
      PERFETTO_DLOG("Child mode skipping setup of unrelated data source.");
      return;
    }

    if (!data_sources_.empty()) {
      PERFETTO_LOG("Child mode skipping concurrent data source.");

      // Manually write one ProfilePacket about the rejected session.
      auto buffer_id = static_cast<BufferID>(ds_config.target_buffer());
      auto trace_writer = endpoint_->CreateTraceWriter(buffer_id);
      auto trace_packet = trace_writer->NewTracePacket();
      trace_packet->set_timestamp(
          static_cast<uint64_t>(base::GetBootTimeNs().count()));
      auto profile_packet = trace_packet->set_profile_packet();
      auto process_dump = profile_packet->add_process_dumps();
      process_dump->set_pid(static_cast<uint64_t>(target_process_.pid));
      process_dump->set_rejected_concurrent(true);
      trace_packet->Finalize();
      trace_writer->Flush();
      return;
    }
  }

  base::Optional<uint64_t> start_cputime_sec;
  if (heapprofd_config.max_heapprofd_cpu_secs() > 0) {
    start_cputime_sec = GetCputimeSec();

    if (!start_cputime_sec) {
      PERFETTO_ELOG("Failed to enforce CPU guardrail. Rejecting config.");
      return;
    }
  }

  auto buffer_id = static_cast<BufferID>(ds_config.target_buffer());
  DataSource data_source(endpoint_->CreateTraceWriter(buffer_id));
  data_source.id = id;
  auto& cli_config = data_source.client_configuration;
  cli_config.interval = heapprofd_config.sampling_interval_bytes();
  cli_config.block_client = heapprofd_config.block_client();
  cli_config.disable_fork_teardown = heapprofd_config.disable_fork_teardown();
  cli_config.disable_vfork_detection =
      heapprofd_config.disable_vfork_detection();
  cli_config.block_client_timeout_us =
      heapprofd_config.block_client_timeout_us();
  data_source.config = heapprofd_config;
  data_source.normalized_cmdlines = std::move(normalized_cmdlines.value());
  data_source.stop_timeout_ms = ds_config.stop_timeout_ms();
  data_source.start_cputime_sec = start_cputime_sec;

  InterningOutputTracker::WriteFixedInterningsPacket(
      data_source.trace_writer.get());
  data_sources_.emplace(id, std::move(data_source));
  PERFETTO_DLOG("Set up data source.");

  if (mode_ == HeapprofdMode::kChild)
    AdoptTargetProcessSocket();
}

bool HeapprofdProducer::IsPidProfiled(pid_t pid) {
  for (const auto& pair : data_sources_) {
    const DataSource& ds = pair.second;
    if (ds.process_states.find(pid) != ds.process_states.cend())
      return true;
  }
  return false;
}

base::Optional<uint64_t> HeapprofdProducer::GetCputimeSec() {
  if (!stat_fd_) {
    return base::nullopt;
  }
  lseek(stat_fd_.get(), 0, SEEK_SET);
  base::ProcStat stat;
  if (!ReadProcStat(stat_fd_.get(), &stat)) {
    PERFETTO_ELOG("Failed to read stat file to enforce guardrails.");
    return base::nullopt;
  }
  return (stat.utime + stat.stime) /
         static_cast<unsigned long>(sysconf(_SC_CLK_TCK));
}

void HeapprofdProducer::SetStartupProperties(DataSource* data_source) {
  const HeapprofdConfig& heapprofd_config = data_source->config;
  if (heapprofd_config.all())
    data_source->properties.emplace_back(properties_.SetAll());

  for (std::string cmdline : data_source->normalized_cmdlines)
    data_source->properties.emplace_back(
        properties_.SetProperty(std::move(cmdline)));
}

void HeapprofdProducer::SignalRunningProcesses(DataSource* data_source) {
  const HeapprofdConfig& heapprofd_config = data_source->config;

  std::set<pid_t> pids;
  if (heapprofd_config.all())
    FindAllProfilablePids(&pids);
  for (uint64_t pid : heapprofd_config.pid())
    pids.emplace(static_cast<pid_t>(pid));

  if (!data_source->normalized_cmdlines.empty())
    FindPidsForCmdlines(data_source->normalized_cmdlines, &pids);

  if (heapprofd_config.min_anonymous_memory_kb() > 0)
    RemoveUnderAnonThreshold(heapprofd_config.min_anonymous_memory_kb(), &pids);

  for (auto pid_it = pids.cbegin(); pid_it != pids.cend();) {
    pid_t pid = *pid_it;
    if (IsPidProfiled(pid)) {
      PERFETTO_LOG("Rejecting concurrent session for %" PRIdMAX,
                   static_cast<intmax_t>(pid));
      data_source->rejected_pids.emplace(pid);
      pid_it = pids.erase(pid_it);
      continue;
    }

    PERFETTO_DLOG("Sending signal: %d (si_value: %d) to pid: %d",
                  kProfilingSignal, kHeapprofdSignalValue, pid);
    union sigval signal_value;
    signal_value.sival_int = kHeapprofdSignalValue;
    if (sigqueue(pid, kProfilingSignal, signal_value) != 0) {
      PERFETTO_DPLOG("sigqueue");
    }
    ++pid_it;
  }
  data_source->signaled_pids = std::move(pids);
}

void HeapprofdProducer::StartDataSource(DataSourceInstanceID id,
                                        const DataSourceConfig&) {
  PERFETTO_DLOG("Start DataSource");

  auto it = data_sources_.find(id);
  if (it == data_sources_.end()) {
    // This is expected in child heapprofd, where we reject uninteresting data
    // sources in SetupDataSource.
    if (mode_ == HeapprofdMode::kCentral) {
      PERFETTO_DFATAL_OR_ELOG(
          "Received invalid data source instance to start: %" PRIu64, id);
    }
    return;
  }

  DataSource& data_source = it->second;
  if (data_source.started) {
    PERFETTO_DFATAL_OR_ELOG(
        "Trying to start already started data-source: %" PRIu64, id);
    return;
  }
  const HeapprofdConfig& heapprofd_config = data_source.config;

  // Central daemon - set system properties for any targets that start later,
  // and signal already-running targets to start the profiling client.
  if (mode_ == HeapprofdMode::kCentral) {
    if (!heapprofd_config.no_startup())
      SetStartupProperties(&data_source);
    if (!heapprofd_config.no_running())
      SignalRunningProcesses(&data_source);
  }

  const auto continuous_dump_config = heapprofd_config.continuous_dump_config();
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
  data_source.started = true;
  PERFETTO_DLOG("Started DataSource");
}

UnwindingWorker& HeapprofdProducer::UnwinderForPID(pid_t pid) {
  return unwinding_workers_[static_cast<uint64_t>(pid) % kUnwinderThreads];
}

void HeapprofdProducer::StopDataSource(DataSourceInstanceID id) {
  auto it = data_sources_.find(id);
  if (it == data_sources_.end()) {
    endpoint_->NotifyDataSourceStopped(id);
    if (mode_ == HeapprofdMode::kCentral)
      PERFETTO_DFATAL_OR_ELOG(
          "Trying to stop non existing data source: %" PRIu64, id);
    return;
  }

  DataSource& data_source = it->second;
  data_source.was_stopped = true;
  ShutdownDataSource(&data_source);
}

void HeapprofdProducer::ShutdownDataSource(DataSource* data_source) {
  data_source->shutting_down = true;
  // If no processes connected, or all of them have already disconnected
  // (and have been dumped) and no PIDs have been rejected,
  // MaybeFinishDataSource can tear down the data source.
  if (MaybeFinishDataSource(data_source))
    return;

  if (!data_source->rejected_pids.empty()) {
    auto trace_packet = data_source->trace_writer->NewTracePacket();
    ProfilePacket* profile_packet = trace_packet->set_profile_packet();
    for (pid_t rejected_pid : data_source->rejected_pids) {
      ProfilePacket::ProcessHeapSamples* proto =
          profile_packet->add_process_dumps();
      proto->set_pid(static_cast<uint64_t>(rejected_pid));
      proto->set_rejected_concurrent(true);
    }
    trace_packet->Finalize();
    data_source->rejected_pids.clear();
    if (MaybeFinishDataSource(data_source))
      return;
  }

  for (const auto& pid_and_process_state : data_source->process_states) {
    pid_t pid = pid_and_process_state.first;
    UnwinderForPID(pid).PostDisconnectSocket(pid);
  }

  auto id = data_source->id;
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer, id] {
        if (!weak_producer)
          return;
        auto ds_it = weak_producer->data_sources_.find(id);
        if (ds_it != weak_producer->data_sources_.end()) {
          PERFETTO_ELOG("Final dump timed out.");
          DataSource& ds = ds_it->second;
          // Do not dump any stragglers, just trigger the Flush and tear down
          // the data source.
          ds.process_states.clear();
          ds.rejected_pids.clear();
          PERFETTO_CHECK(weak_producer->MaybeFinishDataSource(&ds));
        }
      },
      data_source->stop_timeout_ms);
}

void HeapprofdProducer::DoContinuousDump(DataSourceInstanceID id,
                                         uint32_t dump_interval) {
  if (!DumpProcessesInDataSource(id))
    return;
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer, id, dump_interval] {
        if (!weak_producer)
          return;
        weak_producer->DoContinuousDump(id, dump_interval);
      },
      dump_interval);
}

void HeapprofdProducer::DumpProcessState(DataSource* data_source,
                                         pid_t pid,
                                         ProcessState* process_state) {
  HeapTracker& heap_tracker = process_state->heap_tracker;

  bool from_startup =
      data_source->signaled_pids.find(pid) == data_source->signaled_pids.cend();
  uint64_t dump_timestamp;
  if (data_source->config.dump_at_max())
    dump_timestamp = heap_tracker.max_timestamp();
  else
    dump_timestamp = heap_tracker.committed_timestamp();
  auto new_heapsamples = [pid, from_startup, dump_timestamp, process_state,
                          data_source](
                             ProfilePacket::ProcessHeapSamples* proto) {
    proto->set_pid(static_cast<uint64_t>(pid));
    proto->set_timestamp(dump_timestamp);
    proto->set_from_startup(from_startup);
    proto->set_disconnected(process_state->disconnected);
    proto->set_buffer_overran(process_state->buffer_overran);
    proto->set_buffer_corrupted(process_state->buffer_corrupted);
    proto->set_hit_guardrail(data_source->hit_guardrail);
    auto* stats = proto->set_stats();
    stats->set_unwinding_errors(process_state->unwinding_errors);
    stats->set_heap_samples(process_state->heap_samples);
    stats->set_map_reparses(process_state->map_reparses);
    stats->set_total_unwinding_time_us(process_state->total_unwinding_time_us);
    auto* unwinding_hist = stats->set_unwinding_time_us();
    for (const auto& p : process_state->unwinding_time_us.GetData()) {
      auto* bucket = unwinding_hist->add_buckets();
      if (p.first == LogHistogram::kMaxBucket)
        bucket->set_max_bucket(true);
      else
        bucket->set_upper_limit(p.first);
      bucket->set_count(p.second);
    }
  };

  DumpState dump_state(data_source->trace_writer.get(),
                       std::move(new_heapsamples), &data_source->intern_state);

  if (process_state->page_idle_checker) {
    PageIdleChecker& page_idle_checker = *process_state->page_idle_checker;
    heap_tracker.GetAllocations([&dump_state, &page_idle_checker](
                                    uint64_t addr, uint64_t,
                                    uint64_t alloc_size,
                                    uint64_t callstack_id) {
      int64_t idle =
          page_idle_checker.OnIdlePage(addr, static_cast<size_t>(alloc_size));
      if (idle < 0) {
        PERFETTO_PLOG("OnIdlePage.");
        return;
      }
      if (idle > 0)
        dump_state.AddIdleBytes(callstack_id, static_cast<uint64_t>(idle));
    });
  }

  heap_tracker.GetCallstackAllocations(
      [&dump_state,
       &data_source](const HeapTracker::CallstackAllocations& alloc) {
        dump_state.WriteAllocation(alloc, data_source->config.dump_at_max());
      });
  if (process_state->page_idle_checker)
    process_state->page_idle_checker->MarkPagesIdle();
  dump_state.DumpCallstacks(&callsites_);
}

bool HeapprofdProducer::DumpProcessesInDataSource(DataSourceInstanceID id) {
  auto it = data_sources_.find(id);
  if (it == data_sources_.end()) {
    PERFETTO_LOG(
        "Data source not found (harmless if using continuous_dump_config).");
    return false;
  }
  DataSource& data_source = it->second;

  for (std::pair<const pid_t, ProcessState>& pid_and_process_state :
       data_source.process_states) {
    pid_t pid = pid_and_process_state.first;
    ProcessState& process_state = pid_and_process_state.second;
    DumpProcessState(&data_source, pid, &process_state);
  }

  return true;
}

void HeapprofdProducer::DumpAll() {
  for (const auto& id_and_data_source : data_sources_) {
    if (!DumpProcessesInDataSource(id_and_data_source.first))
      PERFETTO_DLOG("Failed to dump %" PRIu64, id_and_data_source.first);
  }
}

void HeapprofdProducer::Flush(FlushRequestID flush_id,
                              const DataSourceInstanceID* ids,
                              size_t num_ids) {
  size_t& flush_in_progress = flushes_in_progress_[flush_id];
  PERFETTO_DCHECK(flush_in_progress == 0);
  flush_in_progress = num_ids;
  for (size_t i = 0; i < num_ids; ++i) {
    auto it = data_sources_.find(ids[i]);
    if (it == data_sources_.end()) {
      PERFETTO_DFATAL_OR_ELOG("Trying to flush unknown data-source %" PRIu64,
                              ids[i]);
      flush_in_progress--;
      continue;
    }
    DataSource& data_source = it->second;
    auto weak_producer = weak_factory_.GetWeakPtr();

    auto callback = [weak_producer, flush_id] {
      if (weak_producer)
        // Reposting because this task runner could be on a different thread
        // than the IPC task runner.
        return weak_producer->task_runner_->PostTask([weak_producer, flush_id] {
          if (weak_producer)
            return weak_producer->FinishDataSourceFlush(flush_id);
        });
    };
    data_source.trace_writer->Flush(std::move(callback));
  }
  if (flush_in_progress == 0) {
    endpoint_->NotifyFlushComplete(flush_id);
    flushes_in_progress_.erase(flush_id);
  }
}

void HeapprofdProducer::FinishDataSourceFlush(FlushRequestID flush_id) {
  auto it = flushes_in_progress_.find(flush_id);
  if (it == flushes_in_progress_.end()) {
    PERFETTO_DFATAL_OR_ELOG("FinishDataSourceFlush id invalid: %" PRIu64,
                            flush_id);
    return;
  }
  size_t& flush_in_progress = it->second;
  if (--flush_in_progress == 0) {
    endpoint_->NotifyFlushComplete(flush_id);
    flushes_in_progress_.erase(flush_id);
  }
}

void HeapprofdProducer::SocketDelegate::OnDisconnect(base::UnixSocket* self) {
  auto it = producer_->pending_processes_.find(self->peer_pid());
  if (it == producer_->pending_processes_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Unexpected disconnect.");
    return;
  }

  if (self == it->second.sock.get())
    producer_->pending_processes_.erase(it);
}

void HeapprofdProducer::SocketDelegate::OnNewIncomingConnection(
    base::UnixSocket*,
    std::unique_ptr<base::UnixSocket> new_connection) {
  Process peer_process;
  peer_process.pid = new_connection->peer_pid();
  if (!GetCmdlineForPID(peer_process.pid, &peer_process.cmdline))
    PERFETTO_PLOG("Failed to get cmdline for %d", peer_process.pid);

  producer_->HandleClientConnection(std::move(new_connection), peer_process);
}

void HeapprofdProducer::SocketDelegate::OnDataAvailable(
    base::UnixSocket* self) {
  auto it = producer_->pending_processes_.find(self->peer_pid());
  if (it == producer_->pending_processes_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Unexpected data.");
    return;
  }

  PendingProcess& pending_process = it->second;

  base::ScopedFile fds[kHandshakeSize];
  char buf[1];
  self->Receive(buf, sizeof(buf), fds, base::ArraySize(fds));

  static_assert(kHandshakeSize == 3, "change if and else if below.");
  // We deliberately do not check for fds[kHandshakePageIdle] so we can
  // degrade gracefully on kernels that do not have the file yet.
  if (fds[kHandshakeMaps] && fds[kHandshakeMem]) {
    auto ds_it =
        producer_->data_sources_.find(pending_process.data_source_instance_id);
    if (ds_it == producer_->data_sources_.end()) {
      producer_->pending_processes_.erase(it);
      return;
    }
    DataSource& data_source = ds_it->second;

    if (data_source.shutting_down) {
      producer_->pending_processes_.erase(it);
      PERFETTO_LOG("Got handshake for DS that is shutting down. Rejecting.");
      return;
    }

    auto it_and_inserted = data_source.process_states.emplace(
        std::piecewise_construct, std::forward_as_tuple(self->peer_pid()),
        std::forward_as_tuple(&producer_->callsites_,
                              data_source.config.dump_at_max()));

    ProcessState& process_state = it_and_inserted.first->second;
    if (data_source.config.idle_allocations()) {
      if (fds[kHandshakePageIdle]) {
        process_state.page_idle_checker =
            PageIdleChecker(std::move(fds[kHandshakePageIdle]));
      } else {
        PERFETTO_ELOG(
            "Idle page tracking requested but did not receive "
            "page_idle file. Continuing without idle page tracking. Please "
            "check your kernel version.");
      }
    }

    PERFETTO_DLOG("%d: Received FDs.", self->peer_pid());
    int raw_fd = pending_process.shmem.fd();
    // TODO(fmayer): Full buffer could deadlock us here.
    if (!self->Send(&data_source.client_configuration,
                    sizeof(data_source.client_configuration), &raw_fd, 1)) {
      // If Send fails, the socket will have been Shutdown, and the raw socket
      // closed.
      producer_->pending_processes_.erase(it);
      return;
    }

    UnwindingWorker::HandoffData handoff_data;
    handoff_data.data_source_instance_id =
        pending_process.data_source_instance_id;
    handoff_data.sock = self->ReleaseSocket();
    handoff_data.maps_fd = std::move(fds[kHandshakeMaps]);
    handoff_data.mem_fd = std::move(fds[kHandshakeMem]);
    handoff_data.shmem = std::move(pending_process.shmem);
    handoff_data.client_config = data_source.client_configuration;

    producer_->UnwinderForPID(self->peer_pid())
        .PostHandoffSocket(std::move(handoff_data));
    producer_->pending_processes_.erase(it);
  } else if (fds[kHandshakeMaps] || fds[kHandshakeMem] ||
             fds[kHandshakePageIdle]) {
    PERFETTO_DFATAL_OR_ELOG("%d: Received partial FDs.", self->peer_pid());
    producer_->pending_processes_.erase(it);
  } else {
    PERFETTO_ELOG("%d: Received no FDs.", self->peer_pid());
  }
}

HeapprofdProducer::DataSource* HeapprofdProducer::GetDataSourceForProcess(
    const Process& proc) {
  for (auto& ds_id_and_datasource : data_sources_) {
    DataSource& ds = ds_id_and_datasource.second;
    if (ConfigTargetsProcess(ds.config, proc, ds.normalized_cmdlines))
      return &ds;
  }
  return nullptr;
}

void HeapprofdProducer::RecordOtherSourcesAsRejected(DataSource* active_ds,
                                                     const Process& proc) {
  for (auto& ds_id_and_datasource : data_sources_) {
    DataSource& ds = ds_id_and_datasource.second;
    if (&ds != active_ds &&
        ConfigTargetsProcess(ds.config, proc, ds.normalized_cmdlines))
      ds.rejected_pids.emplace(proc.pid);
  }
}

void HeapprofdProducer::HandleClientConnection(
    std::unique_ptr<base::UnixSocket> new_connection,
    Process process) {
  DataSource* data_source = GetDataSourceForProcess(process);
  if (!data_source) {
    PERFETTO_LOG("No data source found.");
    return;
  }
  RecordOtherSourcesAsRejected(data_source, process);

  uint64_t shmem_size = data_source->config.shmem_size_bytes();
  if (!shmem_size)
    shmem_size = kDefaultShmemSize;
  if (shmem_size > kMaxShmemSize)
    shmem_size = kMaxShmemSize;

  auto shmem = SharedRingBuffer::Create(static_cast<size_t>(shmem_size));
  if (!shmem || !shmem->is_valid()) {
    PERFETTO_LOG("Failed to create shared memory.");
    return;
  }

  pid_t peer_pid = new_connection->peer_pid();
  if (peer_pid != process.pid) {
    PERFETTO_DFATAL_OR_ELOG("Invalid PID connected.");
    return;
  }

  PendingProcess pending_process;
  pending_process.sock = std::move(new_connection);
  pending_process.data_source_instance_id = data_source->id;
  pending_process.shmem = std::move(*shmem);
  pending_processes_.emplace(peer_pid, std::move(pending_process));
}

void HeapprofdProducer::PostAllocRecord(AllocRecord alloc_rec) {
  // Once we can use C++14, this should be std::moved into the lambda instead.
  AllocRecord* raw_alloc_rec = new AllocRecord(std::move(alloc_rec));
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, raw_alloc_rec] {
    if (weak_this)
      weak_this->HandleAllocRecord(std::move(*raw_alloc_rec));
    delete raw_alloc_rec;
  });
}

void HeapprofdProducer::PostFreeRecord(FreeRecord free_rec) {
  // Once we can use C++14, this should be std::moved into the lambda instead.
  FreeRecord* raw_free_rec = new FreeRecord(std::move(free_rec));
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, raw_free_rec] {
    if (weak_this)
      weak_this->HandleFreeRecord(std::move(*raw_free_rec));
    delete raw_free_rec;
  });
}

void HeapprofdProducer::PostSocketDisconnected(DataSourceInstanceID ds_id,
                                               pid_t pid,
                                               SharedRingBuffer::Stats stats) {
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, ds_id, pid, stats] {
    if (weak_this)
      weak_this->HandleSocketDisconnected(ds_id, pid, stats);
  });
}

void HeapprofdProducer::HandleAllocRecord(AllocRecord alloc_rec) {
  const AllocMetadata& alloc_metadata = alloc_rec.alloc_metadata;
  auto it = data_sources_.find(alloc_rec.data_source_instance_id);
  if (it == data_sources_.end()) {
    PERFETTO_LOG("Invalid data source in alloc record.");
    return;
  }

  DataSource& ds = it->second;
  auto process_state_it = ds.process_states.find(alloc_rec.pid);
  if (process_state_it == ds.process_states.end()) {
    PERFETTO_LOG("Invalid PID in alloc record.");
    return;
  }

  const auto& prefixes = ds.config.skip_symbol_prefix();
  if (!prefixes.empty()) {
    for (FrameData& frame_data : alloc_rec.frames) {
      const std::string& map = frame_data.frame.map_name;
      if (std::find_if(prefixes.cbegin(), prefixes.cend(),
                       [&map](const std::string& prefix) {
                         return base::StartsWith(map, prefix);
                       }) != prefixes.cend()) {
        frame_data.frame.function_name = "FILTERED";
      }
    }
  }

  ProcessState& process_state = process_state_it->second;
  HeapTracker& heap_tracker = process_state.heap_tracker;

  if (alloc_rec.error)
    process_state.unwinding_errors++;
  if (alloc_rec.reparsed_map)
    process_state.map_reparses++;
  process_state.heap_samples++;
  process_state.unwinding_time_us.Add(alloc_rec.unwinding_time_us);
  process_state.total_unwinding_time_us += alloc_rec.unwinding_time_us;

  // abspc may no longer refer to the same functions, as we had to reparse
  // maps. Reset the cache.
  if (alloc_rec.reparsed_map)
    heap_tracker.ClearFrameCache();

  heap_tracker.RecordMalloc(alloc_rec.frames, alloc_metadata.alloc_address,
                            alloc_metadata.sample_size,
                            alloc_metadata.alloc_size,
                            alloc_metadata.sequence_number,
                            alloc_metadata.clock_monotonic_coarse_timestamp);
}

void HeapprofdProducer::HandleFreeRecord(FreeRecord free_rec) {
  const FreeBatch& free_batch = free_rec.free_batch;
  auto it = data_sources_.find(free_rec.data_source_instance_id);
  if (it == data_sources_.end()) {
    PERFETTO_LOG("Invalid data source in free record.");
    return;
  }

  DataSource& ds = it->second;
  auto process_state_it = ds.process_states.find(free_rec.pid);
  if (process_state_it == ds.process_states.end()) {
    PERFETTO_LOG("Invalid PID in free record.");
    return;
  }

  ProcessState& process_state = process_state_it->second;
  HeapTracker& heap_tracker = process_state.heap_tracker;

  const FreeBatchEntry* entries = free_batch.entries;
  uint64_t num_entries = free_batch.num_entries;
  if (num_entries > kFreeBatchSize) {
    PERFETTO_DFATAL_OR_ELOG("Malformed free page.");
    return;
  }
  for (size_t i = 0; i < num_entries; ++i) {
    const FreeBatchEntry& entry = entries[i];
    heap_tracker.RecordFree(entry.addr, entry.sequence_number,
                            free_batch.clock_monotonic_coarse_timestamp);
  }
}

bool HeapprofdProducer::MaybeFinishDataSource(DataSource* ds) {
  if (!ds->process_states.empty() || !ds->rejected_pids.empty() ||
      !ds->shutting_down) {
    return false;
  }

  bool was_stopped = ds->was_stopped;
  DataSourceInstanceID ds_id = ds->id;
  auto weak_producer = weak_factory_.GetWeakPtr();
  ds->trace_writer->Flush([weak_producer, ds_id, was_stopped] {
    if (!weak_producer)
      return;

    if (was_stopped)
      weak_producer->endpoint_->NotifyDataSourceStopped(ds_id);
    weak_producer->data_sources_.erase(ds_id);

    if (weak_producer->mode_ == HeapprofdMode::kChild) {
      // Post this as a task to allow NotifyDataSourceStopped to post tasks.
      weak_producer->task_runner_->PostTask([weak_producer] {
        if (!weak_producer)
          return;
        weak_producer->TerminateProcess(
            /*exit_status=*/0);  // does not return
      });
    }
  });
  return true;
}

void HeapprofdProducer::HandleSocketDisconnected(
    DataSourceInstanceID ds_id,
    pid_t pid,
    SharedRingBuffer::Stats stats) {
  auto it = data_sources_.find(ds_id);
  if (it == data_sources_.end())
    return;
  DataSource& ds = it->second;

  auto process_state_it = ds.process_states.find(pid);
  if (process_state_it == ds.process_states.end())
    return;
  ProcessState& process_state = process_state_it->second;
  process_state.disconnected = !ds.shutting_down;
  process_state.buffer_overran =
      stats.num_writes_overflow > 0 && !ds.config.block_client();
  process_state.buffer_corrupted =
      stats.num_writes_corrupt > 0 || stats.num_reads_corrupt > 0;

  DumpProcessState(&ds, pid, &process_state);
  ds.process_states.erase(pid);
  MaybeFinishDataSource(&ds);
}

void HeapprofdProducer::CheckDataSourceCpu() {
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer] {
        if (!weak_producer)
          return;
        weak_producer->CheckDataSourceCpu();
      },
      kGuardrailIntervalMs);

  bool any_guardrail = false;
  for (auto& id_and_ds : data_sources_) {
    DataSource& ds = id_and_ds.second;
    if (ds.config.max_heapprofd_cpu_secs() > 0)
      any_guardrail = true;
  }

  if (!any_guardrail)
    return;

  base::Optional<uint64_t> cputime_sec = GetCputimeSec();
  if (!cputime_sec) {
    PERFETTO_ELOG("Failed to get CPU time.");
    return;
  }

  for (auto& id_and_ds : data_sources_) {
    DataSource& ds = id_and_ds.second;
    uint64_t ds_max_cpu = ds.config.max_heapprofd_cpu_secs();
    if (ds_max_cpu > 0) {
      // We reject data-sources with CPU guardrails if we cannot read the
      // initial value.
      PERFETTO_CHECK(ds.start_cputime_sec);
      uint64_t cpu_diff = *cputime_sec - *ds.start_cputime_sec;
      if (*cputime_sec > *ds.start_cputime_sec && cpu_diff > ds_max_cpu) {
        PERFETTO_ELOG(
            "Exceeded data-source CPU guardrail "
            "(%" PRIu64 " > %" PRIu64 "). Shutting down.",
            cpu_diff, ds_max_cpu);
        ds.hit_guardrail = true;
        ShutdownDataSource(&ds);
      }
    }
  }
}

void HeapprofdProducer::CheckDataSourceMemory() {
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer] {
        if (!weak_producer)
          return;
        weak_producer->CheckDataSourceMemory();
      },
      kGuardrailIntervalMs);

  bool any_guardrail = false;
  for (auto& id_and_ds : data_sources_) {
    DataSource& ds = id_and_ds.second;
    if (ds.config.max_heapprofd_memory_kb() > 0)
      any_guardrail = true;
  }

  if (!any_guardrail)
    return;

  base::Optional<uint32_t> anon_and_swap = GetRssAnonAndSwap(getpid());
  if (!anon_and_swap) {
    PERFETTO_ELOG("Failed to read heapprofd memory.");
    return;
  }

  for (auto& id_and_ds : data_sources_) {
    DataSource& ds = id_and_ds.second;
    uint32_t ds_max_mem = ds.config.max_heapprofd_memory_kb();
    if (ds_max_mem > 0 && *anon_and_swap > ds_max_mem) {
      PERFETTO_ELOG("Exceeded data-source memory guardrail (%" PRIu32
                    " > %" PRIu32 "). Shutting down.",
                    *anon_and_swap, ds_max_mem);
      ds.hit_guardrail = true;
      ShutdownDataSource(&ds);
    }
  }
}

}  // namespace profiling
}  // namespace perfetto
