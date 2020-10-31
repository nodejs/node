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

#ifndef SRC_PROFILING_MEMORY_HEAPPROFD_PRODUCER_H_
#define SRC_PROFILING_MEMORY_HEAPPROFD_PRODUCER_H_

#include <array>
#include <functional>
#include <map>
#include <vector>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/unix_task_runner.h"

#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/data_source_config.h"

#include "src/profiling/common/interning_output.h"
#include "src/profiling/common/proc_utils.h"
#include "src/profiling/memory/bookkeeping.h"
#include "src/profiling/memory/bookkeeping_dump.h"
#include "src/profiling/memory/page_idle_checker.h"
#include "src/profiling/memory/system_property.h"
#include "src/profiling/memory/unwinding.h"

#include "protos/perfetto/config/profiling/heapprofd_config.gen.h"

namespace perfetto {
namespace profiling {

using HeapprofdConfig = protos::gen::HeapprofdConfig;

struct Process {
  pid_t pid;
  std::string cmdline;
};

class LogHistogram {
 public:
  static const uint64_t kMaxBucket;
  static constexpr size_t kBuckets = 20;

  void Add(uint64_t value) { values_[GetBucket(value)]++; }
  std::vector<std::pair<uint64_t, uint64_t>> GetData();

 private:
  size_t GetBucket(uint64_t value);

  std::array<uint64_t, kBuckets> values_ = {};
};

// TODO(rsavitski): central daemon can do less work if it knows that the global
// operating mode is fork-based, as it then will not be interacting with the
// clients. This can be implemented as an additional mode here.
enum class HeapprofdMode { kCentral, kChild };

// Heap profiling producer. Can be instantiated in two modes, central and
// child (also referred to as fork mode).
//
// The central mode producer is instantiated by the system heapprofd daemon. Its
// primary responsibility is activating profiling (via system properties and
// signals) in targets identified by profiling configs. On debug platform
// builds, the central producer can also handle the out-of-process unwinding &
// writing of the profiles for all client processes.
//
// An alternative model is where the central heapprofd triggers the profiling in
// the target process, but the latter fork-execs a private heapprofd binary to
// handle unwinding only for that process. The forked heapprofd instantiates
// this producer in the "child" mode. In this scenario, the profiled process
// never talks to the system daemon.
//
// TODO(fmayer||rsavitski): cover interesting invariants/structure of the
// implementation (e.g. number of data sources in child mode), including
// threading structure.
class HeapprofdProducer : public Producer, public UnwindingWorker::Delegate {
 public:
  friend class SocketDelegate;

  // TODO(fmayer): Split into two delegates for the listening socket in kCentral
  // and for the per-client sockets to make this easier to understand?
  // Alternatively, find a better name for this.
  class SocketDelegate : public base::UnixSocket::EventListener {
   public:
    SocketDelegate(HeapprofdProducer* producer) : producer_(producer) {}

    void OnDisconnect(base::UnixSocket* self) override;
    void OnNewIncomingConnection(
        base::UnixSocket* self,
        std::unique_ptr<base::UnixSocket> new_connection) override;
    void OnDataAvailable(base::UnixSocket* self) override;

   private:
    HeapprofdProducer* producer_;
  };

  HeapprofdProducer(HeapprofdMode mode, base::TaskRunner* task_runner);
  ~HeapprofdProducer() override;

  // Producer Impl:
  void OnConnect() override;
  void OnDisconnect() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void OnTracingSetup() override;
  void Flush(FlushRequestID,
             const DataSourceInstanceID* data_source_ids,
             size_t num_data_sources) override;
  void ClearIncrementalState(const DataSourceInstanceID* /*data_source_ids*/,
                             size_t /*num_data_sources*/) override {}

  // TODO(fmayer): Refactor once/if we have generic reconnect logic.
  void ConnectWithRetries(const char* socket_name);
  void DumpAll();

  // UnwindingWorker::Delegate impl:
  void PostAllocRecord(AllocRecord) override;
  void PostFreeRecord(FreeRecord) override;
  void PostSocketDisconnected(DataSourceInstanceID,
                              pid_t,
                              SharedRingBuffer::Stats) override;

  void HandleAllocRecord(AllocRecord);
  void HandleFreeRecord(FreeRecord);
  void HandleSocketDisconnected(DataSourceInstanceID,
                                pid_t,
                                SharedRingBuffer::Stats);

  // Valid only if mode_ == kChild.
  void SetTargetProcess(pid_t target_pid,
                        std::string target_cmdline,
                        base::ScopedFile inherited_socket);
  // Valid only if mode_ == kChild. Kicks off a periodic check that the child
  // heapprofd is actively working on a data source (which should correspond to
  // the target process). The first check is delayed to let the freshly spawned
  // producer get the data sources from the tracing service (i.e. traced).
  void ScheduleActiveDataSourceWatchdog();

  // Exposed for testing.
  void SetProducerEndpoint(
      std::unique_ptr<TracingService::ProducerEndpoint> endpoint);

  base::UnixSocket::EventListener& socket_delegate() {
    return socket_delegate_;
  }

 private:
  // State of the connection to tracing service (traced).
  enum State {
    kNotStarted = 0,
    kNotConnected,
    kConnecting,
    kConnected,
  };

  struct ProcessState {
    ProcessState(GlobalCallstackTrie* callsites, bool dump_at_max_mode)
        : heap_tracker(callsites, dump_at_max_mode) {}
    bool disconnected = false;
    bool buffer_overran = false;
    bool buffer_corrupted = false;

    uint64_t heap_samples = 0;
    uint64_t map_reparses = 0;
    uint64_t unwinding_errors = 0;

    uint64_t total_unwinding_time_us = 0;
    LogHistogram unwinding_time_us;
    HeapTracker heap_tracker;

    base::Optional<PageIdleChecker> page_idle_checker;
  };

  struct DataSource {
    DataSource(std::unique_ptr<TraceWriter> tw) : trace_writer(std::move(tw)) {}

    DataSourceInstanceID id;
    std::unique_ptr<TraceWriter> trace_writer;
    HeapprofdConfig config;
    ClientConfiguration client_configuration;
    std::vector<SystemProperties::Handle> properties;
    std::set<pid_t> signaled_pids;
    std::set<pid_t> rejected_pids;
    std::map<pid_t, ProcessState> process_states;
    std::vector<std::string> normalized_cmdlines;
    InterningOutputTracker intern_state;
    bool shutting_down = false;
    bool started = false;
    bool hit_guardrail = false;
    bool was_stopped = false;
    uint32_t stop_timeout_ms;
    base::Optional<uint64_t> start_cputime_sec;
  };

  struct PendingProcess {
    std::unique_ptr<base::UnixSocket> sock;
    DataSourceInstanceID data_source_instance_id;
    SharedRingBuffer shmem;
  };

  void HandleClientConnection(std::unique_ptr<base::UnixSocket> new_connection,
                              Process process);

  void ConnectService();
  void Restart();
  void ResetConnectionBackoff();
  void IncreaseConnectionBackoff();

  base::Optional<uint64_t> GetCputimeSec();

  void CheckDataSourceMemory();
  void CheckDataSourceCpu();

  void FinishDataSourceFlush(FlushRequestID flush_id);
  bool DumpProcessesInDataSource(DataSourceInstanceID id);
  void DumpProcessState(DataSource* ds, pid_t pid, ProcessState* process);

  void DoContinuousDump(DataSourceInstanceID id, uint32_t dump_interval);

  UnwindingWorker& UnwinderForPID(pid_t);
  bool IsPidProfiled(pid_t);
  DataSource* GetDataSourceForProcess(const Process& proc);
  void RecordOtherSourcesAsRejected(DataSource* active_ds, const Process& proc);

  void SetStartupProperties(DataSource* data_source);
  void SignalRunningProcesses(DataSource* data_source);

  // Specific to mode_ == kChild
  void TerminateProcess(int exit_status);
  // Specific to mode_ == kChild
  void ActiveDataSourceWatchdogCheck();
  // Adopts the (connected) sockets inherited from the target process, invoking
  // the on-connection callback.
  // Specific to mode_ == kChild
  void AdoptTargetProcessSocket();

  void ShutdownDataSource(DataSource* ds);
  bool MaybeFinishDataSource(DataSource* ds);

  // Class state:

  // Task runner is owned by the main thread.
  base::TaskRunner* const task_runner_;
  const HeapprofdMode mode_;

  // State of connection to the tracing service.
  State state_ = kNotStarted;
  uint32_t connection_backoff_ms_ = 0;
  const char* producer_sock_name_ = nullptr;

  // Client processes that have connected, but with which we have not yet
  // finished the handshake.
  std::map<pid_t, PendingProcess> pending_processes_;

  // Must outlive data_sources_ - owns at least the shared memory referenced by
  // TraceWriters.
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;

  // Must outlive data_sources_ - HeapTracker references the trie.
  GlobalCallstackTrie callsites_;

  // Must outlive data_sources_ - DataSource can hold
  // SystemProperties::Handle-s.
  // Specific to mode_ == kCentral
  SystemProperties properties_;

  std::map<FlushRequestID, size_t> flushes_in_progress_;
  std::map<DataSourceInstanceID, DataSource> data_sources_;
  std::vector<UnwindingWorker> unwinding_workers_;

  // Specific to mode_ == kChild
  Process target_process_{base::kInvalidPid, ""};
  // This is a valid FD only between SetTargetProcess and
  // AdoptTargetProcessSocket.
  // Specific to mode_ == kChild
  base::ScopedFile inherited_fd_;

  SocketDelegate socket_delegate_;
  base::ScopedFile stat_fd_;

  base::WeakPtrFactory<HeapprofdProducer> weak_factory_;  // Keep last.
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_HEAPPROFD_PRODUCER_H_
