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

#include "src/tracing/core/tracing_service_impl.h"

#include "perfetto/base/build_config.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <regex>
#include <unordered_set>

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
#include <sys/uio.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <sys/system_properties.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#define PERFETTO_HAS_CHMOD
#include <sys/stat.h>
#endif

#include <algorithm>

#include "perfetto/base/build_config.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/metatrace.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/base/watchdog.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/observable_events.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/protozero/static_buffer.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/tracing_service_capabilities.h"
#include "perfetto/tracing/core/tracing_service_state.h"
#include "src/tracing/core/packet_stream_validator.h"
#include "src/tracing/core/shared_memory_arbiter_impl.h"
#include "src/tracing/core/trace_buffer.h"

#include "protos/perfetto/common/trace_stats.pbzero.h"
#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/perfetto/tracing_service_event.pbzero.h"
#include "protos/perfetto/trace/system_info.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/trigger.pbzero.h"

// General note: this class must assume that Producers are malicious and will
// try to crash / exploit this class. We can trust pointers because they come
// from the IPC layer, but we should never assume that that the producer calls
// come in the right order or their arguments are sane / within bounds.

namespace perfetto {

namespace {
constexpr int kMaxBuffersPerConsumer = 128;
constexpr base::TimeMillis kSnapshotsInterval(10 * 1000);
constexpr int kDefaultWriteIntoFilePeriodMs = 5000;
constexpr int kMaxConcurrentTracingSessions = 15;
constexpr int kMaxConcurrentTracingSessionsPerUid = 5;
constexpr int kMaxConcurrentTracingSessionsForStatsdUid = 10;
constexpr int64_t kMinSecondsBetweenTracesGuardrail = 5 * 60;

constexpr uint32_t kMillisPerHour = 3600000;
constexpr uint32_t kMaxTracingDurationMillis = 7 * 24 * kMillisPerHour;

// These apply only if enable_extra_guardrails is true.
constexpr uint32_t kGuardrailsMaxTracingBufferSizeKb = 128 * 1024;
constexpr uint32_t kGuardrailsMaxTracingDurationMillis = 24 * kMillisPerHour;

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) || PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
struct iovec {
  void* iov_base;  // Address
  size_t iov_len;  // Block size
};

// Simple implementation of writev. Note that this does not give the atomicity
// guarantees of a real writev, but we don't depend on these (we aren't writing
// to the same file from another thread).
ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
  ssize_t total_size = 0;
  for (int i = 0; i < iovcnt; ++i) {
    ssize_t current_size = base::WriteAll(fd, iov[i].iov_base, iov[i].iov_len);
    if (current_size != static_cast<ssize_t>(iov[i].iov_len))
      return -1;
    total_size += current_size;
  }
  return total_size;
}

#define IOV_MAX 1024  // Linux compatible limit.

// uid checking is a NOP on Windows.
uid_t getuid() {
  return 0;
}
uid_t geteuid() {
  return 0;
}
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) ||
        // PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)

// Partially encodes a CommitDataRequest in an int32 for the purposes of
// metatracing. Note that it encodes only the bottom 10 bits of the producer id
// (which is technically 16 bits wide).
//
// Format (by bit range):
// [   31 ][         30 ][             29:20 ][            19:10 ][        9:0]
// [unused][has flush id][num chunks to patch][num chunks to move][producer id]
static int32_t EncodeCommitDataRequest(ProducerID producer_id,
                                       const CommitDataRequest& req_untrusted) {
  uint32_t cmov = static_cast<uint32_t>(req_untrusted.chunks_to_move_size());
  uint32_t cpatch = static_cast<uint32_t>(req_untrusted.chunks_to_patch_size());
  uint32_t has_flush_id = req_untrusted.flush_request_id() != 0;

  uint32_t mask = (1 << 10) - 1;
  uint32_t acc = 0;
  acc |= has_flush_id << 30;
  acc |= (cpatch & mask) << 20;
  acc |= (cmov & mask) << 10;
  acc |= (producer_id & mask);
  return static_cast<int32_t>(acc);
}

void SerializeAndAppendPacket(std::vector<TracePacket>* packets,
                              std::vector<uint8_t> packet) {
  Slice slice = Slice::Allocate(packet.size());
  memcpy(slice.own_data(), packet.data(), packet.size());
  packets->emplace_back();
  packets->back().AddSlice(std::move(slice));
}

std::tuple<size_t /*shm_size*/, size_t /*page_size*/> EnsureValidShmSizes(
    size_t shm_size,
    size_t page_size) {
  // Theoretically the max page size supported by the ABI is 64KB.
  // However, the current implementation of TraceBuffer (the non-shared
  // userspace buffer where the service copies data) supports at most
  // 32K. Setting 64K "works" from the producer<>consumer viewpoint
  // but then causes the data to be discarded when copying it into
  // TraceBuffer.
  constexpr size_t kMaxPageSize = 32 * 1024;
  static_assert(kMaxPageSize <= SharedMemoryABI::kMaxPageSize, "");

  if (page_size == 0)
    page_size = TracingServiceImpl::kDefaultShmPageSize;
  if (shm_size == 0)
    shm_size = TracingServiceImpl::kDefaultShmSize;

  page_size = std::min<size_t>(page_size, kMaxPageSize);
  shm_size = std::min<size_t>(shm_size, TracingServiceImpl::kMaxShmSize);

  // Page size has to be multiple of system's page size.
  bool page_size_is_valid = page_size >= base::kPageSize;
  page_size_is_valid &= page_size % base::kPageSize == 0;

  // Only allow power of two numbers of pages, i.e. 1, 2, 4, 8 pages.
  size_t num_pages = page_size / base::kPageSize;
  page_size_is_valid &= (num_pages & (num_pages - 1)) == 0;

  if (!page_size_is_valid || shm_size < page_size ||
      shm_size % page_size != 0) {
    return std::make_tuple(TracingServiceImpl::kDefaultShmSize,
                           TracingServiceImpl::kDefaultShmPageSize);
  }
  return std::make_tuple(shm_size, page_size);
}

bool NameMatchesFilter(const std::string& name,
                       const std::vector<std::string>& name_filter,
                       const std::vector<std::string>& name_regex_filter) {
  bool filter_is_set = !name_filter.empty() || !name_regex_filter.empty();
  if (!filter_is_set)
    return true;
  bool filter_matches = std::find(name_filter.begin(), name_filter.end(),
                                  name) != name_filter.end();
  bool filter_regex_matches =
      std::find_if(name_regex_filter.begin(), name_regex_filter.end(),
                   [&](const std::string& regex) {
                     return std::regex_match(
                         name, std::regex(regex, std::regex::extended));
                   }) != name_regex_filter.end();
  return filter_matches || filter_regex_matches;
}

// Used when write_into_file == true and output_path is not empty.
base::ScopedFile CreateTraceFile(const std::string& path) {
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  static const char kBase[] = "/data/misc/perfetto-traces/";
  if (!base::StartsWith(path, kBase) || path.rfind('/') != strlen(kBase) - 1) {
    PERFETTO_ELOG("Invalid output_path %s. On Android it must be within %s.",
                  path.c_str(), kBase);
    return base::ScopedFile();
  }
#endif
  // O_CREAT | O_EXCL will fail if the file exists already.
  auto fd = base::OpenFile(path, O_RDWR | O_CREAT | O_EXCL, 0600);
  if (!fd)
    PERFETTO_PLOG("Failed to create %s", path.c_str());
#if defined(PERFETTO_HAS_CHMOD)
  // Passing 0644 directly above won't work because of umask.
  PERFETTO_CHECK(fchmod(*fd, 0644) == 0);
#endif
  return fd;
}

}  // namespace

// These constants instead are defined in the header because are used by tests.
constexpr size_t TracingServiceImpl::kDefaultShmSize;
constexpr size_t TracingServiceImpl::kDefaultShmPageSize;

constexpr size_t TracingServiceImpl::kMaxShmSize;
constexpr uint32_t TracingServiceImpl::kDataSourceStopTimeoutMs;
constexpr uint8_t TracingServiceImpl::kSyncMarker[];

// static
std::unique_ptr<TracingService> TracingService::CreateInstance(
    std::unique_ptr<SharedMemory::Factory> shm_factory,
    base::TaskRunner* task_runner) {
  return std::unique_ptr<TracingService>(
      new TracingServiceImpl(std::move(shm_factory), task_runner));
}

TracingServiceImpl::TracingServiceImpl(
    std::unique_ptr<SharedMemory::Factory> shm_factory,
    base::TaskRunner* task_runner)
    : task_runner_(task_runner),
      shm_factory_(std::move(shm_factory)),
      uid_(getuid()),
      buffer_ids_(kMaxTraceBufferID),
      weak_ptr_factory_(this) {
  PERFETTO_DCHECK(task_runner_);
}

TracingServiceImpl::~TracingServiceImpl() {
  // TODO(fmayer): handle teardown of all Producer.
}

std::unique_ptr<TracingService::ProducerEndpoint>
TracingServiceImpl::ConnectProducer(Producer* producer,
                                    uid_t uid,
                                    const std::string& producer_name,
                                    size_t shared_memory_size_hint_bytes,
                                    bool in_process,
                                    ProducerSMBScrapingMode smb_scraping_mode,
                                    size_t shared_memory_page_size_hint_bytes,
                                    std::unique_ptr<SharedMemory> shm) {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  if (lockdown_mode_ && uid != geteuid()) {
    PERFETTO_DLOG("Lockdown mode. Rejecting producer with UID %ld",
                  static_cast<unsigned long>(uid));
    return nullptr;
  }

  if (producers_.size() >= kMaxProducerID) {
    PERFETTO_DFATAL("Too many producers.");
    return nullptr;
  }
  const ProducerID id = GetNextProducerID();
  PERFETTO_DLOG("Producer %" PRIu16 " connected", id);

  bool smb_scraping_enabled = smb_scraping_enabled_;
  switch (smb_scraping_mode) {
    case ProducerSMBScrapingMode::kDefault:
      break;
    case ProducerSMBScrapingMode::kEnabled:
      smb_scraping_enabled = true;
      break;
    case ProducerSMBScrapingMode::kDisabled:
      smb_scraping_enabled = false;
      break;
  }

  std::unique_ptr<ProducerEndpointImpl> endpoint(new ProducerEndpointImpl(
      id, uid, this, task_runner_, producer, producer_name, in_process,
      smb_scraping_enabled));
  auto it_and_inserted = producers_.emplace(id, endpoint.get());
  PERFETTO_DCHECK(it_and_inserted.second);
  endpoint->shmem_size_hint_bytes_ = shared_memory_size_hint_bytes;
  endpoint->shmem_page_size_hint_bytes_ = shared_memory_page_size_hint_bytes;

  // Producer::OnConnect() should run before Producer::OnTracingSetup(). The
  // latter may be posted by SetupSharedMemory() below, so post OnConnect() now.
  task_runner_->PostTask(std::bind(&Producer::OnConnect, endpoint->producer_));

  if (shm) {
    // The producer supplied an SMB. This is used only by Chrome; in the most
    // common cases the SMB is created by the service and passed via
    // OnTracingSetup(). Verify that it is correctly sized before we attempt to
    // use it. The transport layer has to verify the integrity of the SMB (e.g.
    // ensure that the producer can't resize if after the fact).
    size_t shm_size, page_size;
    std::tie(shm_size, page_size) =
        EnsureValidShmSizes(shm->size(), endpoint->shmem_page_size_hint_bytes_);
    if (shm_size == shm->size() &&
        page_size == endpoint->shmem_page_size_hint_bytes_) {
      PERFETTO_DLOG(
          "Adopting producer-provided SMB of %zu kB for producer \"%s\"",
          shm_size / 1024, endpoint->name_.c_str());
      endpoint->SetupSharedMemory(std::move(shm), page_size,
                                  /*provided_by_producer=*/true);
    } else {
      PERFETTO_LOG(
          "Discarding incorrectly sized producer-provided SMB for producer "
          "\"%s\", falling back to service-provided SMB. Requested sizes: %zu "
          "B total, %zu B page size; suggested corrected sizes: %zu B total, "
          "%zu B page size",
          endpoint->name_.c_str(), shm->size(),
          endpoint->shmem_page_size_hint_bytes_, shm_size, page_size);
      shm.reset();
    }
  }

  return std::unique_ptr<ProducerEndpoint>(std::move(endpoint));
}

void TracingServiceImpl::DisconnectProducer(ProducerID id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Producer %" PRIu16 " disconnected", id);
  PERFETTO_DCHECK(producers_.count(id));

  // Scrape remaining chunks for this producer to ensure we don't lose data.
  if (auto* producer = GetProducer(id)) {
    for (auto& session_id_and_session : tracing_sessions_)
      ScrapeSharedMemoryBuffers(&session_id_and_session.second, producer);
  }

  for (auto it = data_sources_.begin(); it != data_sources_.end();) {
    auto next = it;
    next++;
    if (it->second.producer_id == id)
      UnregisterDataSource(id, it->second.descriptor.name());
    it = next;
  }

  producers_.erase(id);
  UpdateMemoryGuardrail();
}

TracingServiceImpl::ProducerEndpointImpl* TracingServiceImpl::GetProducer(
    ProducerID id) const {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto it = producers_.find(id);
  if (it == producers_.end())
    return nullptr;
  return it->second;
}

std::unique_ptr<TracingService::ConsumerEndpoint>
TracingServiceImpl::ConnectConsumer(Consumer* consumer, uid_t uid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Consumer %p connected", reinterpret_cast<void*>(consumer));
  std::unique_ptr<ConsumerEndpointImpl> endpoint(
      new ConsumerEndpointImpl(this, task_runner_, consumer, uid));
  auto it_and_inserted = consumers_.emplace(endpoint.get());
  PERFETTO_DCHECK(it_and_inserted.second);
  // Consumer might go away before we're able to send the connect notification,
  // if that is the case just bail out.
  auto weak_ptr = endpoint->GetWeakPtr();
  task_runner_->PostTask([weak_ptr] {
    if (!weak_ptr) {
      return;
    }
    weak_ptr->consumer_->OnConnect();
  });
  return std::unique_ptr<ConsumerEndpoint>(std::move(endpoint));
}

void TracingServiceImpl::DisconnectConsumer(ConsumerEndpointImpl* consumer) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Consumer %p disconnected", reinterpret_cast<void*>(consumer));
  PERFETTO_DCHECK(consumers_.count(consumer));

  // TODO(primiano) : Check that this is safe (what happens if there are
  // ReadBuffers() calls posted in the meantime? They need to become noop).
  if (consumer->tracing_session_id_)
    FreeBuffers(consumer->tracing_session_id_);  // Will also DisableTracing().
  consumers_.erase(consumer);

  // At this point no more pointers to |consumer| should be around.
  PERFETTO_DCHECK(!std::any_of(
      tracing_sessions_.begin(), tracing_sessions_.end(),
      [consumer](const std::pair<const TracingSessionID, TracingSession>& kv) {
        return kv.second.consumer_maybe_null == consumer;
      }));
}

bool TracingServiceImpl::DetachConsumer(ConsumerEndpointImpl* consumer,
                                        const std::string& key) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Consumer %p detached", reinterpret_cast<void*>(consumer));
  PERFETTO_DCHECK(consumers_.count(consumer));

  TracingSessionID tsid = consumer->tracing_session_id_;
  TracingSession* tracing_session;
  if (!tsid || !(tracing_session = GetTracingSession(tsid)))
    return false;

  if (GetDetachedSession(consumer->uid_, key)) {
    PERFETTO_ELOG("Another session has been detached with the same key \"%s\"",
                  key.c_str());
    return false;
  }

  PERFETTO_DCHECK(tracing_session->consumer_maybe_null == consumer);
  tracing_session->consumer_maybe_null = nullptr;
  tracing_session->detach_key = key;
  consumer->tracing_session_id_ = 0;
  return true;
}

bool TracingServiceImpl::AttachConsumer(ConsumerEndpointImpl* consumer,
                                        const std::string& key) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Consumer %p attaching to session %s",
                reinterpret_cast<void*>(consumer), key.c_str());
  PERFETTO_DCHECK(consumers_.count(consumer));

  if (consumer->tracing_session_id_) {
    PERFETTO_ELOG(
        "Cannot reattach consumer to session %s"
        " while it already attached tracing session ID %" PRIu64,
        key.c_str(), consumer->tracing_session_id_);
    return false;
  }

  auto* tracing_session = GetDetachedSession(consumer->uid_, key);
  if (!tracing_session) {
    PERFETTO_ELOG(
        "Failed to attach consumer, session '%s' not found for uid %d",
        key.c_str(), static_cast<int>(consumer->uid_));
    return false;
  }

  consumer->tracing_session_id_ = tracing_session->id;
  tracing_session->consumer_maybe_null = consumer;
  tracing_session->detach_key.clear();
  return true;
}

bool TracingServiceImpl::EnableTracing(ConsumerEndpointImpl* consumer,
                                       const TraceConfig& cfg,
                                       base::ScopedFile fd) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Enabling tracing for consumer %p",
                reinterpret_cast<void*>(consumer));
  if (cfg.lockdown_mode() == TraceConfig::LOCKDOWN_SET)
    lockdown_mode_ = true;
  if (cfg.lockdown_mode() == TraceConfig::LOCKDOWN_CLEAR)
    lockdown_mode_ = false;
  TracingSession* tracing_session =
      GetTracingSession(consumer->tracing_session_id_);
  if (tracing_session) {
    PERFETTO_DLOG(
        "A Consumer is trying to EnableTracing() but another tracing session "
        "is already active (forgot a call to FreeBuffers() ?)");
    return false;
  }

  const uint32_t max_duration_ms = cfg.enable_extra_guardrails()
                                       ? kGuardrailsMaxTracingDurationMillis
                                       : kMaxTracingDurationMillis;
  if (cfg.duration_ms() > max_duration_ms) {
    PERFETTO_ELOG("Requested too long trace (%" PRIu32 "ms  > %" PRIu32 " ms)",
                  cfg.duration_ms(), max_duration_ms);
    return false;
  }

  const bool has_trigger_config = cfg.trigger_config().trigger_mode() !=
                                  TraceConfig::TriggerConfig::UNSPECIFIED;
  if (has_trigger_config && (cfg.trigger_config().trigger_timeout_ms() == 0 ||
                             cfg.trigger_config().trigger_timeout_ms() >
                                 kGuardrailsMaxTracingDurationMillis)) {
    PERFETTO_ELOG(
        "Traces with START_TRACING triggers must provide a positive "
        "trigger_timeout_ms < 7 days (received %" PRIu32 "ms)",
        cfg.trigger_config().trigger_timeout_ms());
    return false;
  }

  if (has_trigger_config && cfg.duration_ms() != 0) {
    PERFETTO_ELOG(
        "duration_ms was set, this must not be set for traces with triggers.");
    return false;
  }

  std::unordered_set<std::string> triggers;
  for (const auto& trigger : cfg.trigger_config().triggers()) {
    if (!triggers.insert(trigger.name()).second) {
      PERFETTO_ELOG("Duplicate trigger name: %s", trigger.name().c_str());
      return false;
    }
  }

  if (cfg.enable_extra_guardrails()) {
    if (cfg.deferred_start()) {
      PERFETTO_ELOG(
          "deferred_start=true is not supported in unsupervised traces");
      return false;
    }
    uint64_t buf_size_sum = 0;
    for (const auto& buf : cfg.buffers())
      buf_size_sum += buf.size_kb();
    if (buf_size_sum > kGuardrailsMaxTracingBufferSizeKb) {
      PERFETTO_ELOG("Requested too large trace buffer (%" PRIu64
                    "kB  > %" PRIu32 " kB)",
                    buf_size_sum, kGuardrailsMaxTracingBufferSizeKb);
      return false;
    }
  }

  if (cfg.buffers_size() > kMaxBuffersPerConsumer) {
    PERFETTO_ELOG("Too many buffers configured (%d)", cfg.buffers_size());
    return false;
  }

  if (!cfg.unique_session_name().empty()) {
    const std::string& name = cfg.unique_session_name();
    for (auto& kv : tracing_sessions_) {
      if (kv.second.config.unique_session_name() == name) {
        PERFETTO_ELOG(
            "A trace with this unique session name (%s) already exists",
            name.c_str());
        return false;
      }
    }
  }

  if (cfg.enable_extra_guardrails()) {
    // unique_session_name can be empty
    const std::string& name = cfg.unique_session_name();
    int64_t now_s = base::GetBootTimeS().count();

    // Remove any entries where the time limit has passed so this map doesn't
    // grow indefinitely:
    std::map<std::string, int64_t>& sessions = session_to_last_trace_s_;
    for (auto it = sessions.cbegin(); it != sessions.cend();) {
      if (now_s - it->second > kMinSecondsBetweenTracesGuardrail) {
        it = sessions.erase(it);
      } else {
        ++it;
      }
    }

    int64_t& previous_s = session_to_last_trace_s_[name];
    if (previous_s == 0) {
      previous_s = now_s;
    } else {
      PERFETTO_ELOG(
          "A trace with unique session name \"%s\" began less than %" PRId64
          "s ago (%" PRId64 "s)",
          name.c_str(), kMinSecondsBetweenTracesGuardrail, now_s - previous_s);
      return false;
    }
  }

  const long sessions_for_uid = std::count_if(
      tracing_sessions_.begin(), tracing_sessions_.end(),
      [consumer](const decltype(tracing_sessions_)::value_type& s) {
        return s.second.consumer_uid == consumer->uid_;
      });

  int per_uid_limit = kMaxConcurrentTracingSessionsPerUid;
  if (consumer->uid_ == 1066 /* AID_STATSD*/) {
    per_uid_limit = kMaxConcurrentTracingSessionsForStatsdUid;
  }
  if (sessions_for_uid >= per_uid_limit) {
    PERFETTO_ELOG(
        "Too many concurrent tracing sesions (%ld) for uid %d limit is %d",
        sessions_for_uid, static_cast<int>(consumer->uid_), per_uid_limit);
    return false;
  }

  // TODO(primiano): This is a workaround to prevent that a producer gets stuck
  // in a state where it stalls by design by having more TraceWriterImpl
  // instances than free pages in the buffer. This is really a bug in
  // trace_probes and the way it handles stalls in the shmem buffer.
  if (tracing_sessions_.size() >= kMaxConcurrentTracingSessions) {
    PERFETTO_ELOG("Too many concurrent tracing sesions (%zu)",
                  tracing_sessions_.size());
    return false;
  }

  const TracingSessionID tsid = ++last_tracing_session_id_;
  tracing_session =
      &tracing_sessions_.emplace(tsid, TracingSession(tsid, consumer, cfg))
           .first->second;

  if (cfg.write_into_file()) {
    if (!fd ^ !cfg.output_path().empty()) {
      PERFETTO_ELOG(
          "When write_into_file==true either a FD needs to be passed or "
          "output_path must be populated (but not both)");
      tracing_sessions_.erase(tsid);
      return false;
    }
    if (!cfg.output_path().empty()) {
      fd = CreateTraceFile(cfg.output_path());
      if (!fd) {
        tracing_sessions_.erase(tsid);
        return false;
      }
    }
    tracing_session->write_into_file = std::move(fd);
    uint32_t write_period_ms = cfg.file_write_period_ms();
    if (write_period_ms == 0)
      write_period_ms = kDefaultWriteIntoFilePeriodMs;
    if (write_period_ms < min_write_period_ms_)
      write_period_ms = min_write_period_ms_;
    tracing_session->write_period_ms = write_period_ms;
    tracing_session->max_file_size_bytes = cfg.max_file_size_bytes();
    tracing_session->bytes_written_into_file = 0;
  }

  // Initialize the log buffers.
  bool did_allocate_all_buffers = true;

  // Allocate the trace buffers. Also create a map to translate a consumer
  // relative index (TraceConfig.DataSourceConfig.target_buffer) into the
  // corresponding BufferID, which is a global ID namespace for the service and
  // all producers.
  size_t total_buf_size_kb = 0;
  const size_t num_buffers = static_cast<size_t>(cfg.buffers_size());
  tracing_session->buffers_index.reserve(num_buffers);
  for (size_t i = 0; i < num_buffers; i++) {
    const TraceConfig::BufferConfig& buffer_cfg = cfg.buffers()[i];
    BufferID global_id = buffer_ids_.Allocate();
    if (!global_id) {
      did_allocate_all_buffers = false;  // We ran out of IDs.
      break;
    }
    tracing_session->buffers_index.push_back(global_id);
    const size_t buf_size_bytes = buffer_cfg.size_kb() * 1024u;
    total_buf_size_kb += buffer_cfg.size_kb();
    TraceBuffer::OverwritePolicy policy =
        buffer_cfg.fill_policy() == TraceConfig::BufferConfig::DISCARD
            ? TraceBuffer::kDiscard
            : TraceBuffer::kOverwrite;
    auto it_and_inserted = buffers_.emplace(
        global_id, TraceBuffer::Create(buf_size_bytes, policy));
    PERFETTO_DCHECK(it_and_inserted.second);  // buffers_.count(global_id) == 0.
    std::unique_ptr<TraceBuffer>& trace_buffer = it_and_inserted.first->second;
    if (!trace_buffer) {
      did_allocate_all_buffers = false;
      break;
    }
  }

  UpdateMemoryGuardrail();

  // This can happen if either:
  // - All the kMaxTraceBufferID slots are taken.
  // - OOM, or, more relistically, we exhausted virtual memory.
  // In any case, free all the previously allocated buffers and abort.
  // TODO(fmayer): add a test to cover this case, this is quite subtle.
  if (!did_allocate_all_buffers) {
    for (BufferID global_id : tracing_session->buffers_index) {
      buffer_ids_.Free(global_id);
      buffers_.erase(global_id);
    }
    tracing_sessions_.erase(tsid);
    return false;
  }

  consumer->tracing_session_id_ = tsid;

  // Setup the data sources on the producers without starting them.
  for (const TraceConfig::DataSource& cfg_data_source : cfg.data_sources()) {
    // Scan all the registered data sources with a matching name.
    auto range = data_sources_.equal_range(cfg_data_source.config().name());
    for (auto it = range.first; it != range.second; it++) {
      TraceConfig::ProducerConfig producer_config;
      for (auto& config : cfg.producers()) {
        if (GetProducer(it->second.producer_id)->name_ ==
            config.producer_name()) {
          producer_config = config;
          break;
        }
      }
      SetupDataSource(cfg_data_source, producer_config, it->second,
                      tracing_session);
    }
  }

  bool has_start_trigger = false;
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  switch (cfg.trigger_config().trigger_mode()) {
    case TraceConfig::TriggerConfig::UNSPECIFIED:
      // no triggers are specified so this isn't a trace that is using triggers.
      PERFETTO_DCHECK(!has_trigger_config);
      break;
    case TraceConfig::TriggerConfig::START_TRACING:
      // For traces which use START_TRACE triggers we need to ensure that the
      // tracing session will be cleaned up when it times out.
      has_start_trigger = true;
      task_runner_->PostDelayedTask(
          [weak_this, tsid]() {
            if (weak_this)
              weak_this->OnStartTriggersTimeout(tsid);
          },
          cfg.trigger_config().trigger_timeout_ms());
      break;
    case TraceConfig::TriggerConfig::STOP_TRACING:
      // Update the tracing_session's duration_ms to ensure that if no trigger
      // is received the session will end and be cleaned up equal to the
      // timeout.
      //
      // TODO(nuskos): Refactor this so that rather then modifying the config we
      // have a field we look at on the tracing_session.
      tracing_session->config.set_duration_ms(
          cfg.trigger_config().trigger_timeout_ms());
      break;
  }

  tracing_session->state = TracingSession::CONFIGURED;
  PERFETTO_LOG(
      "Configured tracing session %" PRIu64
      ", #sources:%zu, duration:%d ms, #buffers:%d, total "
      "buffer size:%zu KB, total sessions:%zu, uid:%d session name: \"%s\"",
      tsid, cfg.data_sources().size(), tracing_session->config.duration_ms(),
      cfg.buffers_size(), total_buf_size_kb, tracing_sessions_.size(),
      static_cast<unsigned int>(consumer->uid_),
      cfg.unique_session_name().c_str());

  // Start the data sources, unless this is a case of early setup + fast
  // triggering, either through TraceConfig.deferred_start or
  // TraceConfig.trigger_config(). If both are specified which ever one occurs
  // first will initiate the trace.
  if (!cfg.deferred_start() && !has_start_trigger)
    return StartTracing(tsid);

  return true;
}

void TracingServiceImpl::ChangeTraceConfig(ConsumerEndpointImpl* consumer,
                                           const TraceConfig& updated_cfg) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session =
      GetTracingSession(consumer->tracing_session_id_);
  PERFETTO_DCHECK(tracing_session);

  if ((tracing_session->state != TracingSession::STARTED) &&
      (tracing_session->state != TracingSession::CONFIGURED)) {
    PERFETTO_ELOG(
        "ChangeTraceConfig() was called for a tracing session which isn't "
        "running.");
    return;
  }

  // We only support updating producer_name_{,regex}_filter (and pass-through
  // configs) for now; null out any changeable fields and make sure the rest are
  // identical.
  TraceConfig new_config_copy(updated_cfg);
  for (auto& ds_cfg : *new_config_copy.mutable_data_sources()) {
    ds_cfg.clear_producer_name_filter();
    ds_cfg.clear_producer_name_regex_filter();
  }

  TraceConfig current_config_copy(tracing_session->config);
  for (auto& ds_cfg : *current_config_copy.mutable_data_sources()) {
    ds_cfg.clear_producer_name_filter();
    ds_cfg.clear_producer_name_regex_filter();
  }

  if (new_config_copy != current_config_copy) {
    PERFETTO_LOG(
        "ChangeTraceConfig() was called with a config containing unsupported "
        "changes; only adding to the producer_name_{,regex}_filter is "
        "currently supported and will have an effect.");
  }

  for (TraceConfig::DataSource& cfg_data_source :
       *tracing_session->config.mutable_data_sources()) {
    // Find the updated producer_filter in the new config.
    std::vector<std::string> new_producer_name_filter;
    std::vector<std::string> new_producer_name_regex_filter;
    bool found_data_source = false;
    for (auto it : updated_cfg.data_sources()) {
      if (cfg_data_source.config().name() == it.config().name()) {
        new_producer_name_filter = it.producer_name_filter();
        new_producer_name_regex_filter = it.producer_name_regex_filter();
        found_data_source = true;
        break;
      }
    }

    // Bail out if data source not present in the new config.
    if (!found_data_source) {
      PERFETTO_ELOG(
          "ChangeTraceConfig() called without a current data source also "
          "present in the new config: %s",
          cfg_data_source.config().name().c_str());
      continue;
    }

    // TODO(oysteine): Just replacing the filter means that if
    // there are any filter entries which were present in the original config,
    // but removed from the config passed to ChangeTraceConfig, any matching
    // producers will keep producing but newly added producers after this
    // point will never start.
    *cfg_data_source.mutable_producer_name_filter() = new_producer_name_filter;
    *cfg_data_source.mutable_producer_name_regex_filter() =
        new_producer_name_regex_filter;

    // Scan all the registered data sources with a matching name.
    auto range = data_sources_.equal_range(cfg_data_source.config().name());
    for (auto it = range.first; it != range.second; it++) {
      ProducerEndpointImpl* producer = GetProducer(it->second.producer_id);
      PERFETTO_DCHECK(producer);

      // Check if the producer name of this data source is present
      // in the name filters. We currently only support new filters, not
      // removing old ones.
      if (!NameMatchesFilter(producer->name_, new_producer_name_filter,
                             new_producer_name_regex_filter)) {
        continue;
      }

      bool already_setup = false;
      auto& ds_instances = tracing_session->data_source_instances;
      for (auto instance_it = ds_instances.begin();
           instance_it != ds_instances.end(); ++instance_it) {
        if (instance_it->first == it->second.producer_id &&
            instance_it->second.data_source_name ==
                cfg_data_source.config().name()) {
          already_setup = true;
          break;
        }
      }

      if (already_setup)
        continue;

      // If it wasn't previously setup, set it up now.
      // (The per-producer config is optional).
      TraceConfig::ProducerConfig producer_config;
      for (auto& config : tracing_session->config.producers()) {
        if (producer->name_ == config.producer_name()) {
          producer_config = config;
          break;
        }
      }

      DataSourceInstance* ds_inst = SetupDataSource(
          cfg_data_source, producer_config, it->second, tracing_session);

      if (ds_inst && tracing_session->state == TracingSession::STARTED)
        StartDataSourceInstance(producer, tracing_session, ds_inst);
    }
  }
}

bool TracingServiceImpl::StartTracing(TracingSessionID tsid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session) {
    PERFETTO_DLOG("StartTracing() failed, invalid session ID %" PRIu64, tsid);
    return false;
  }

  if (tracing_session->state != TracingSession::CONFIGURED) {
    PERFETTO_DLOG("StartTracing() failed, invalid session state: %d",
                  tracing_session->state);
    return false;
  }

  tracing_session->state = TracingSession::STARTED;

  if (!tracing_session->config.builtin_data_sources()
           .disable_clock_snapshotting()) {
    SnapshotClocks(&tracing_session->initial_clock_snapshot_,
                   /*set_root_timestamp=*/true);
  }

  // Trigger delayed task if the trace is time limited.
  const uint32_t trace_duration_ms = tracing_session->config.duration_ms();
  if (trace_duration_ms > 0) {
    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    task_runner_->PostDelayedTask(
        [weak_this, tsid] {
          // Skip entirely the flush if the trace session doesn't exist anymore.
          // This is to prevent misleading error messages to be logged.
          if (!weak_this)
            return;
          auto* tracing_session_ptr = weak_this->GetTracingSession(tsid);
          if (!tracing_session_ptr)
            return;
          // If this trace was using STOP_TRACING triggers and we've seen
          // one, then the trigger overrides the normal timeout. In this
          // case we just return and let the other task clean up this trace.
          if (tracing_session_ptr->config.trigger_config().trigger_mode() ==
                  TraceConfig::TriggerConfig::STOP_TRACING &&
              !tracing_session_ptr->received_triggers.empty())
            return;
          // In all other cases (START_TRACING or no triggers) we flush
          // after |trace_duration_ms| unconditionally.
          weak_this->FlushAndDisableTracing(tsid);
        },
        trace_duration_ms);
  }

  // Start the periodic drain tasks if we should to save the trace into a file.
  if (tracing_session->config.write_into_file()) {
    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    task_runner_->PostDelayedTask(
        [weak_this, tsid] {
          if (weak_this)
            weak_this->ReadBuffers(tsid, nullptr);
        },
        tracing_session->delay_to_next_write_period_ms());
  }

  // Start the periodic flush tasks if the config specified a flush period.
  if (tracing_session->config.flush_period_ms())
    PeriodicFlushTask(tsid, /*post_next_only=*/true);

  // Start the periodic incremental state clear tasks if the config specified a
  // period.
  if (tracing_session->config.incremental_state_config().clear_period_ms()) {
    PeriodicClearIncrementalStateTask(tsid, /*post_next_only=*/true);
  }

  for (auto& kv : tracing_session->data_source_instances) {
    ProducerID producer_id = kv.first;
    DataSourceInstance& data_source = kv.second;
    ProducerEndpointImpl* producer = GetProducer(producer_id);
    if (!producer) {
      PERFETTO_DFATAL("Producer does not exist.");
      continue;
    }
    StartDataSourceInstance(producer, tracing_session, &data_source);
  }
  return true;
}

void TracingServiceImpl::StartDataSourceInstance(
    ProducerEndpointImpl* producer,
    TracingSession* tracing_session,
    TracingServiceImpl::DataSourceInstance* instance) {
  PERFETTO_DCHECK(instance->state == DataSourceInstance::CONFIGURED);
  if (instance->will_notify_on_start) {
    instance->state = DataSourceInstance::STARTING;
  } else {
    instance->state = DataSourceInstance::STARTED;
  }
  if (tracing_session->consumer_maybe_null) {
    tracing_session->consumer_maybe_null->OnDataSourceInstanceStateChange(
        *producer, *instance);
  }
  producer->StartDataSource(instance->instance_id, instance->config);

  // If all data sources are started, notify the consumer.
  if (instance->state == DataSourceInstance::STARTED)
    MaybeNotifyAllDataSourcesStarted(tracing_session);
}

// DisableTracing just stops the data sources but doesn't free up any buffer.
// This is to allow the consumer to freeze the buffers (by stopping the trace)
// and then drain the buffers. The actual teardown of the TracingSession happens
// in FreeBuffers().
void TracingServiceImpl::DisableTracing(TracingSessionID tsid,
                                        bool disable_immediately) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session) {
    // Can happen if the consumer calls this before EnableTracing() or after
    // FreeBuffers().
    PERFETTO_DLOG("DisableTracing() failed, invalid session ID %" PRIu64, tsid);
    return;
  }

  switch (tracing_session->state) {
    // Spurious call to DisableTracing() while already disabled, nothing to do.
    case TracingSession::DISABLED:
      PERFETTO_DCHECK(tracing_session->AllDataSourceInstancesStopped());
      return;

    // This is either:
    // A) The case of a graceful DisableTracing() call followed by a call to
    //    FreeBuffers(), iff |disable_immediately| == true. In this case we want
    //    to forcefully transition in the disabled state without waiting for the
    //    outstanding acks because the buffers are going to be destroyed soon.
    // B) A spurious call, iff |disable_immediately| == false, in which case
    //    there is nothing to do.
    case TracingSession::DISABLING_WAITING_STOP_ACKS:
      PERFETTO_DCHECK(!tracing_session->AllDataSourceInstancesStopped());
      if (disable_immediately)
        DisableTracingNotifyConsumerAndFlushFile(tracing_session);
      return;

    // Continues below.
    case TracingSession::CONFIGURED:
      // If the session didn't even start there is no need to orchestrate a
      // graceful stop of data sources.
      disable_immediately = true;
      break;

    // This is the nominal case, continues below.
    case TracingSession::STARTED:
      break;
  }

  for (auto& data_source_inst : tracing_session->data_source_instances) {
    const ProducerID producer_id = data_source_inst.first;
    DataSourceInstance& instance = data_source_inst.second;
    ProducerEndpointImpl* producer = GetProducer(producer_id);
    PERFETTO_DCHECK(producer);
    PERFETTO_DCHECK(instance.state == DataSourceInstance::CONFIGURED ||
                    instance.state == DataSourceInstance::STARTING ||
                    instance.state == DataSourceInstance::STARTED);
    StopDataSourceInstance(producer, tracing_session, &instance,
                           disable_immediately);
  }

  // Either this request is flagged with |disable_immediately| or there are no
  // data sources that are requesting a final handshake. In both cases just mark
  // the session as disabled immediately, notify the consumer and flush the
  // trace file (if used).
  if (tracing_session->AllDataSourceInstancesStopped())
    return DisableTracingNotifyConsumerAndFlushFile(tracing_session);

  tracing_session->state = TracingSession::DISABLING_WAITING_STOP_ACKS;
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this, tsid] {
        if (weak_this)
          weak_this->OnDisableTracingTimeout(tsid);
      },
      tracing_session->data_source_stop_timeout_ms());

  // Deliberately NOT removing the session from |tracing_session_|, it's still
  // needed to call ReadBuffers(). FreeBuffers() will erase() the session.
}

void TracingServiceImpl::NotifyDataSourceStarted(
    ProducerID producer_id,
    DataSourceInstanceID instance_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  for (auto& kv : tracing_sessions_) {
    TracingSession& tracing_session = kv.second;
    DataSourceInstance* instance =
        tracing_session.GetDataSourceInstance(producer_id, instance_id);

    if (!instance)
      continue;

    // If the tracing session was already stopped, ignore this notification.
    if (tracing_session.state != TracingSession::STARTED)
      continue;

    if (instance->state != DataSourceInstance::STARTING) {
      PERFETTO_ELOG("Started data source instance in incorrect state: %d",
                    instance->state);
      continue;
    }

    instance->state = DataSourceInstance::STARTED;

    ProducerEndpointImpl* producer = GetProducer(producer_id);
    PERFETTO_DCHECK(producer);
    if (tracing_session.consumer_maybe_null) {
      tracing_session.consumer_maybe_null->OnDataSourceInstanceStateChange(
          *producer, *instance);
    }

    // If all data sources are started, notify the consumer.
    MaybeNotifyAllDataSourcesStarted(&tracing_session);
  }  // for (tracing_session)
}

void TracingServiceImpl::MaybeNotifyAllDataSourcesStarted(
    TracingSession* tracing_session) {
  if (!tracing_session->consumer_maybe_null)
    return;

  if (!tracing_session->AllDataSourceInstancesStarted())
    return;

  // In some rare cases, we can get in this state more than once. Consider the
  // following scenario: 3 data sources are registered -> trace starts ->
  // all 3 data sources ack -> OnAllDataSourcesStarted() is called.
  // Imagine now that a 4th data source registers while the trace is ongoing.
  // This would hit the AllDataSourceInstancesStarted() condition again.
  // In this case, however, we don't want to re-notify the consumer again.
  // That would be unexpected (even if, perhaps, technically correct) and
  // trigger bugs in the consumer.
  if (tracing_session->did_notify_all_data_source_started)
    return;

  PERFETTO_DLOG("All data sources started");
  tracing_session->did_notify_all_data_source_started = true;
  tracing_session->time_all_data_source_started = base::GetBootTimeNs();
  tracing_session->consumer_maybe_null->OnAllDataSourcesStarted();
}

void TracingServiceImpl::NotifyDataSourceStopped(
    ProducerID producer_id,
    DataSourceInstanceID instance_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  for (auto& kv : tracing_sessions_) {
    TracingSession& tracing_session = kv.second;
    DataSourceInstance* instance =
        tracing_session.GetDataSourceInstance(producer_id, instance_id);

    if (!instance)
      continue;

    if (instance->state != DataSourceInstance::STOPPING) {
      PERFETTO_ELOG("Stopped data source instance in incorrect state: %d",
                    instance->state);
      continue;
    }

    instance->state = DataSourceInstance::STOPPED;

    ProducerEndpointImpl* producer = GetProducer(producer_id);
    PERFETTO_DCHECK(producer);
    if (tracing_session.consumer_maybe_null) {
      tracing_session.consumer_maybe_null->OnDataSourceInstanceStateChange(
          *producer, *instance);
    }

    if (!tracing_session.AllDataSourceInstancesStopped())
      continue;

    if (tracing_session.state != TracingSession::DISABLING_WAITING_STOP_ACKS)
      continue;

    // All data sources acked the termination.
    DisableTracingNotifyConsumerAndFlushFile(&tracing_session);
  }  // for (tracing_session)
}

void TracingServiceImpl::ActivateTriggers(
    ProducerID producer_id,
    const std::vector<std::string>& triggers) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto* producer = GetProducer(producer_id);
  PERFETTO_DCHECK(producer);
  for (const auto& trigger_name : triggers) {
    for (auto& id_and_tracing_session : tracing_sessions_) {
      auto& tracing_session = id_and_tracing_session.second;
      TracingSessionID tsid = id_and_tracing_session.first;
      auto iter = std::find_if(
          tracing_session.config.trigger_config().triggers().begin(),
          tracing_session.config.trigger_config().triggers().end(),
          [&trigger_name](const TraceConfig::TriggerConfig::Trigger& trigger) {
            return trigger.name() == trigger_name;
          });
      if (iter == tracing_session.config.trigger_config().triggers().end()) {
        continue;
      }

      // If this trigger requires a certain producer to have sent it
      // (non-empty producer_name()) ensure the producer who sent this trigger
      // matches.
      if (!iter->producer_name_regex().empty() &&
          !std::regex_match(
              producer->name_,
              std::regex(iter->producer_name_regex(), std::regex::extended))) {
        continue;
      }

      const bool triggers_already_received =
          !tracing_session.received_triggers.empty();
      tracing_session.received_triggers.push_back(
          {static_cast<uint64_t>(base::GetBootTimeNs().count()), iter->name(),
           producer->name_, producer->uid_});
      auto weak_this = weak_ptr_factory_.GetWeakPtr();
      switch (tracing_session.config.trigger_config().trigger_mode()) {
        case TraceConfig::TriggerConfig::START_TRACING:
          // If the session has already been triggered and moved past
          // CONFIGURED then we don't need to repeat StartTracing. This would
          // work fine (StartTracing would return false) but would add error
          // logs.
          if (tracing_session.state != TracingSession::CONFIGURED)
            break;

          PERFETTO_DLOG("Triggering '%s' on tracing session %" PRIu64
                        " with duration of %" PRIu32 "ms.",
                        iter->name().c_str(), tsid, iter->stop_delay_ms());
          // We override the trace duration to be the trigger's requested
          // value, this ensures that the trace will end after this amount
          // of time has passed.
          tracing_session.config.set_duration_ms(iter->stop_delay_ms());
          StartTracing(tsid);
          break;
        case TraceConfig::TriggerConfig::STOP_TRACING:
          // Only stop the trace once to avoid confusing log messages. I.E.
          // when we've already hit the first trigger we've already Posted the
          // task to FlushAndDisable. So all future triggers will just break
          // out.
          if (triggers_already_received)
            break;

          PERFETTO_DLOG("Triggering '%s' on tracing session %" PRIu64
                        " with duration of %" PRIu32 "ms.",
                        iter->name().c_str(), tsid, iter->stop_delay_ms());
          // Now that we've seen a trigger we need to stop, flush, and disable
          // this session after the configured |stop_delay_ms|.
          task_runner_->PostDelayedTask(
              [weak_this, tsid] {
                // Skip entirely the flush if the trace session doesn't exist
                // anymore. This is to prevent misleading error messages to be
                // logged.
                if (weak_this && weak_this->GetTracingSession(tsid))
                  weak_this->FlushAndDisableTracing(tsid);
              },
              // If this trigger is zero this will immediately executable and
              // will happen shortly.
              iter->stop_delay_ms());
          break;
        case TraceConfig::TriggerConfig::UNSPECIFIED:
          PERFETTO_ELOG("Trigger activated but trigger mode unspecified.");
          break;
      }
    }
  }
}

// Always invoked kDataSourceStopTimeoutMs after DisableTracing(). In nominal
// conditions all data sources should have acked the stop and this will early
// out.
void TracingServiceImpl::OnDisableTracingTimeout(TracingSessionID tsid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session ||
      tracing_session->state != TracingSession::DISABLING_WAITING_STOP_ACKS) {
    return;  // Tracing session was successfully disabled.
  }

  PERFETTO_ILOG("Timeout while waiting for ACKs for tracing session %" PRIu64,
                tsid);
  PERFETTO_DCHECK(!tracing_session->AllDataSourceInstancesStopped());
  DisableTracingNotifyConsumerAndFlushFile(tracing_session);
}

void TracingServiceImpl::DisableTracingNotifyConsumerAndFlushFile(
    TracingSession* tracing_session) {
  PERFETTO_DCHECK(tracing_session->state != TracingSession::DISABLED);
  for (auto& inst_kv : tracing_session->data_source_instances) {
    if (inst_kv.second.state == DataSourceInstance::STOPPED)
      continue;
    inst_kv.second.state = DataSourceInstance::STOPPED;
    ProducerEndpointImpl* producer = GetProducer(inst_kv.first);
    PERFETTO_DCHECK(producer);
    if (tracing_session->consumer_maybe_null) {
      tracing_session->consumer_maybe_null->OnDataSourceInstanceStateChange(
          *producer, inst_kv.second);
    }
  }
  tracing_session->state = TracingSession::DISABLED;

  // Scrape any remaining chunks that weren't flushed by the producers.
  for (auto& producer_id_and_producer : producers_)
    ScrapeSharedMemoryBuffers(tracing_session, producer_id_and_producer.second);

  if (tracing_session->write_into_file) {
    tracing_session->write_period_ms = 0;
    ReadBuffers(tracing_session->id, nullptr);
  }

  if (tracing_session->consumer_maybe_null)
    tracing_session->consumer_maybe_null->NotifyOnTracingDisabled();
}

void TracingServiceImpl::Flush(TracingSessionID tsid,
                               uint32_t timeout_ms,
                               ConsumerEndpoint::FlushCallback callback) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session) {
    PERFETTO_DLOG("Flush() failed, invalid session ID %" PRIu64, tsid);
    return;
  }

  if (!timeout_ms)
    timeout_ms = tracing_session->flush_timeout_ms();

  if (tracing_session->pending_flushes.size() > 1000) {
    PERFETTO_ELOG("Too many flushes (%zu) pending for the tracing session",
                  tracing_session->pending_flushes.size());
    callback(false);
    return;
  }

  FlushRequestID flush_request_id = ++last_flush_request_id_;
  PendingFlush& pending_flush =
      tracing_session->pending_flushes
          .emplace_hint(tracing_session->pending_flushes.end(),
                        flush_request_id, PendingFlush(std::move(callback)))
          ->second;

  // Send a flush request to each producer involved in the tracing session. In
  // order to issue a flush request we have to build a map of all data source
  // instance ids enabled for each producer.
  std::map<ProducerID, std::vector<DataSourceInstanceID>> flush_map;
  for (const auto& data_source_inst : tracing_session->data_source_instances) {
    const ProducerID producer_id = data_source_inst.first;
    const DataSourceInstanceID ds_inst_id = data_source_inst.second.instance_id;
    flush_map[producer_id].push_back(ds_inst_id);
  }

  for (const auto& kv : flush_map) {
    ProducerID producer_id = kv.first;
    ProducerEndpointImpl* producer = GetProducer(producer_id);
    const std::vector<DataSourceInstanceID>& data_sources = kv.second;
    producer->Flush(flush_request_id, data_sources);
    pending_flush.producers.insert(producer_id);
  }

  // If there are no producers to flush (realistically this happens only in
  // some tests) fire OnFlushTimeout() straight away, without waiting.
  if (flush_map.empty())
    timeout_ms = 0;

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this, tsid, flush_request_id] {
        if (weak_this)
          weak_this->OnFlushTimeout(tsid, flush_request_id);
      },
      timeout_ms);
}

void TracingServiceImpl::NotifyFlushDoneForProducer(
    ProducerID producer_id,
    FlushRequestID flush_request_id) {
  for (auto& kv : tracing_sessions_) {
    // Remove all pending flushes <= |flush_request_id| for |producer_id|.
    auto& pending_flushes = kv.second.pending_flushes;
    auto end_it = pending_flushes.upper_bound(flush_request_id);
    for (auto it = pending_flushes.begin(); it != end_it;) {
      PendingFlush& pending_flush = it->second;
      pending_flush.producers.erase(producer_id);
      if (pending_flush.producers.empty()) {
        auto weak_this = weak_ptr_factory_.GetWeakPtr();
        TracingSessionID tsid = kv.first;
        auto callback = std::move(pending_flush.callback);
        task_runner_->PostTask([weak_this, tsid, callback]() {
          if (weak_this) {
            weak_this->CompleteFlush(tsid, std::move(callback),
                                     /*success=*/true);
          }
        });
        it = pending_flushes.erase(it);
      } else {
        it++;
      }
    }  // for (pending_flushes)
  }    // for (tracing_session)
}

void TracingServiceImpl::OnFlushTimeout(TracingSessionID tsid,
                                        FlushRequestID flush_request_id) {
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session)
    return;
  auto it = tracing_session->pending_flushes.find(flush_request_id);
  if (it == tracing_session->pending_flushes.end())
    return;  // Nominal case: flush was completed and acked on time.

  // If there were no producers to flush, consider it a success.
  bool success = it->second.producers.empty();

  auto callback = std::move(it->second.callback);
  tracing_session->pending_flushes.erase(it);
  CompleteFlush(tsid, std::move(callback), success);
}

void TracingServiceImpl::CompleteFlush(TracingSessionID tsid,
                                       ConsumerEndpoint::FlushCallback callback,
                                       bool success) {
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (tracing_session) {
    // Producers may not have been able to flush all their data, even if they
    // indicated flush completion. If possible, also collect uncommitted chunks
    // to make sure we have everything they wrote so far.
    for (auto& producer_id_and_producer : producers_) {
      ScrapeSharedMemoryBuffers(tracing_session,
                                producer_id_and_producer.second);
    }
  }
  callback(success);
}

void TracingServiceImpl::ScrapeSharedMemoryBuffers(
    TracingSession* tracing_session,
    ProducerEndpointImpl* producer) {
  if (!producer->smb_scraping_enabled_)
    return;

  // Can't copy chunks if we don't know about any trace writers.
  if (producer->writers_.empty())
    return;

  // Performance optimization: On flush or session disconnect, this method is
  // called for each producer. If the producer doesn't participate in the
  // session, there's no need to scape its chunks right now. We can tell if a
  // producer participates in the session by checking if the producer is allowed
  // to write into the session's log buffers.
  const auto& session_buffers = tracing_session->buffers_index;
  bool producer_in_session =
      std::any_of(session_buffers.begin(), session_buffers.end(),
                  [producer](BufferID buffer_id) {
                    return producer->allowed_target_buffers_.count(buffer_id);
                  });
  if (!producer_in_session)
    return;

  PERFETTO_DLOG("Scraping SMB for producer %" PRIu16, producer->id_);

  // Find and copy any uncommitted chunks from the SMB.
  //
  // In nominal conditions, the page layout of the used SMB pages should never
  // change because the service is the only one who is supposed to modify used
  // pages (to make them free again).
  //
  // However, the code here needs to deal with the case of a malicious producer
  // altering the SMB in unpredictable ways. Thankfully the SMB size is
  // immutable, so a chunk will always point to some valid memory, even if the
  // producer alters the intended layout and chunk header concurrently.
  // Ultimately a malicious producer altering the SMB's chunk layout while we
  // are iterating in this function is not any different from the case of a
  // malicious producer asking to commit a chunk made of random data, which is
  // something this class has to deal with regardless.
  //
  // The only legitimate mutations that can happen from sane producers,
  // concurrently to this function, are:
  //   A. free pages being partitioned,
  //   B. free chunks being migrated to kChunkBeingWritten,
  //   C. kChunkBeingWritten chunks being migrated to kChunkCompleted.

  SharedMemoryABI* abi = &producer->shmem_abi_;
  // num_pages() is immutable after the SMB is initialized and cannot be changed
  // even by a producer even if malicious.
  for (size_t page_idx = 0; page_idx < abi->num_pages(); page_idx++) {
    uint32_t layout = abi->GetPageLayout(page_idx);

    uint32_t used_chunks = abi->GetUsedChunks(layout);  // Returns a bitmap.
    // Skip empty pages.
    if (used_chunks == 0)
      continue;

    // Scrape the chunks that are currently used. These should be either in
    // state kChunkBeingWritten or kChunkComplete.
    for (uint32_t chunk_idx = 0; used_chunks; chunk_idx++, used_chunks >>= 1) {
      if (!(used_chunks & 1))
        continue;

      SharedMemoryABI::ChunkState state =
          SharedMemoryABI::GetChunkStateFromLayout(layout, chunk_idx);
      PERFETTO_DCHECK(state == SharedMemoryABI::kChunkBeingWritten ||
                      state == SharedMemoryABI::kChunkComplete);
      bool chunk_complete = state == SharedMemoryABI::kChunkComplete;

      SharedMemoryABI::Chunk chunk =
          abi->GetChunkUnchecked(page_idx, layout, chunk_idx);

      uint16_t packet_count;
      uint8_t flags;
      // GetPacketCountAndFlags has acquire_load semantics.
      std::tie(packet_count, flags) = chunk.GetPacketCountAndFlags();

      // It only makes sense to copy an incomplete chunk if there's at least
      // one full packet available. (The producer may not have completed the
      // last packet in it yet, so we need at least 2.)
      if (!chunk_complete && packet_count < 2)
        continue;

      // At this point, it is safe to access the remaining header fields of
      // the chunk. Even if the chunk was only just transferred from
      // kChunkFree into kChunkBeingWritten state, the header should be
      // written completely once the packet count increased above 1 (it was
      // reset to 0 by the service when the chunk was freed).

      WriterID writer_id = chunk.writer_id();
      base::Optional<BufferID> target_buffer_id =
          producer->buffer_id_for_writer(writer_id);

      // We can only scrape this chunk if we know which log buffer to copy it
      // into.
      if (!target_buffer_id)
        continue;

      // Skip chunks that don't belong to the requested tracing session.
      bool target_buffer_belongs_to_session =
          std::find(session_buffers.begin(), session_buffers.end(),
                    *target_buffer_id) != session_buffers.end();
      if (!target_buffer_belongs_to_session)
        continue;

      uint32_t chunk_id =
          chunk.header()->chunk_id.load(std::memory_order_relaxed);

      CopyProducerPageIntoLogBuffer(
          producer->id_, producer->uid_, writer_id, chunk_id, *target_buffer_id,
          packet_count, flags, chunk_complete, chunk.payload_begin(),
          chunk.payload_size());
    }
  }
}

void TracingServiceImpl::FlushAndDisableTracing(TracingSessionID tsid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Triggering final flush for %" PRIu64, tsid);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  Flush(tsid, 0, [weak_this, tsid](bool success) {
    PERFETTO_DLOG("Flush done (success: %d), disabling trace session %" PRIu64,
                  success, tsid);
    if (!weak_this)
      return;
    TracingSession* session = weak_this->GetTracingSession(tsid);
    if (session->consumer_maybe_null) {
      // If the consumer is still attached, just disable the session but give it
      // a chance to read the contents.
      weak_this->DisableTracing(tsid);
    } else {
      // If the consumer detached, destroy the session. If the consumer did
      // start the session in long-tracing mode, the service will have saved
      // the contents to the passed file. If not, the contents will be
      // destroyed.
      weak_this->FreeBuffers(tsid);
    }
  });
}

void TracingServiceImpl::PeriodicFlushTask(TracingSessionID tsid,
                                           bool post_next_only) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session || tracing_session->state != TracingSession::STARTED)
    return;

  uint32_t flush_period_ms = tracing_session->config.flush_period_ms();
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this, tsid] {
        if (weak_this)
          weak_this->PeriodicFlushTask(tsid, /*post_next_only=*/false);
      },
      flush_period_ms - (base::GetWallTimeMs().count() % flush_period_ms));

  if (post_next_only)
    return;

  PERFETTO_DLOG("Triggering periodic flush for trace session %" PRIu64, tsid);
  Flush(tsid, 0, [](bool success) {
    if (!success)
      PERFETTO_ELOG("Periodic flush timed out");
  });
}

void TracingServiceImpl::PeriodicClearIncrementalStateTask(
    TracingSessionID tsid,
    bool post_next_only) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session || tracing_session->state != TracingSession::STARTED)
    return;

  uint32_t clear_period_ms =
      tracing_session->config.incremental_state_config().clear_period_ms();
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this, tsid] {
        if (weak_this)
          weak_this->PeriodicClearIncrementalStateTask(
              tsid, /*post_next_only=*/false);
      },
      clear_period_ms - (base::GetWallTimeMs().count() % clear_period_ms));

  if (post_next_only)
    return;

  PERFETTO_DLOG(
      "Performing periodic incremental state clear for trace session %" PRIu64,
      tsid);

  // Queue the IPCs to producers with active data sources that opted in.
  std::map<ProducerID, std::vector<DataSourceInstanceID>> clear_map;
  for (const auto& kv : tracing_session->data_source_instances) {
    ProducerID producer_id = kv.first;
    const DataSourceInstance& data_source = kv.second;
    if (data_source.handles_incremental_state_clear)
      clear_map[producer_id].push_back(data_source.instance_id);
  }

  for (const auto& kv : clear_map) {
    ProducerID producer_id = kv.first;
    const std::vector<DataSourceInstanceID>& data_sources = kv.second;
    ProducerEndpointImpl* producer = GetProducer(producer_id);
    if (!producer) {
      PERFETTO_DFATAL("Producer does not exist.");
      continue;
    }
    producer->ClearIncrementalState(data_sources);
  }
}

// Note: when this is called to write into a file passed when starting tracing
// |consumer| will be == nullptr (as opposite to the case of a consumer asking
// to send the trace data back over IPC).
bool TracingServiceImpl::ReadBuffers(TracingSessionID tsid,
                                     ConsumerEndpointImpl* consumer) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session) {
    // This will be hit systematically from the PostDelayedTask when directly
    // writing into the file (in which case consumer == nullptr). Suppress the
    // log in this case as it's just spam.
    if (consumer) {
      PERFETTO_DLOG("Cannot ReadBuffers(): no tracing session is active");
    }
    return false;
  }

  // When a tracing session is waiting for a trigger it is considered empty. If
  // a tracing session finishes and moves into DISABLED without ever receiving a
  // trigger the trace should never return any data. This includes the synthetic
  // packets like TraceConfig and Clock snapshots. So we bail out early and let
  // the consumer know there is no data.
  if (!tracing_session->config.trigger_config().triggers().empty() &&
      tracing_session->received_triggers.empty()) {
    PERFETTO_DLOG(
        "ReadBuffers(): tracing session has not received a trigger yet.");
    return false;
  }

  // This can happen if the file is closed by a previous task because it reaches
  // |max_file_size_bytes|.
  if (!tracing_session->write_into_file && !consumer)
    return false;

  if (tracing_session->write_into_file && consumer) {
    // If the consumer enabled tracing and asked to save the contents into the
    // passed file makes little sense to also try to read the buffers over IPC,
    // as that would just steal data from the periodic draining task.
    PERFETTO_DFATAL("Consumer trying to read from write_into_file session.");
    return false;
  }

  std::vector<TracePacket> packets;
  packets.reserve(1024);  // Just an educated guess to avoid trivial expansions.

  std::move(tracing_session->initial_clock_snapshot_.begin(),
            tracing_session->initial_clock_snapshot_.end(),
            std::back_inserter(packets));
  tracing_session->initial_clock_snapshot_.clear();

  base::TimeMillis now = base::GetWallTimeMs();
  if (now >= tracing_session->last_snapshot_time + kSnapshotsInterval) {
    tracing_session->last_snapshot_time = now;
    // Don't emit the stats immediately, but instead wait until no more trace
    // data is available to read. That way, any problems that occur while
    // reading from the buffers are reflected in the emitted stats. This is
    // particularly important for use cases where ReadBuffers is only ever
    // called after the tracing session is stopped.
    tracing_session->should_emit_stats = true;
    SnapshotSyncMarker(&packets);

    if (!tracing_session->config.builtin_data_sources()
             .disable_clock_snapshotting()) {
      // We don't want to put a root timestamp in this snapshot as the packet
      // may be very out of order with respect to the actual trace packets
      // since consuming the trace may happen at any point after it starts.
      SnapshotClocks(&packets, /*set_root_timestamp=*/false);
    }
  }
  if (!tracing_session->config.builtin_data_sources().disable_trace_config()) {
    MaybeEmitTraceConfig(tracing_session, &packets);
    MaybeEmitReceivedTriggers(tracing_session, &packets);
  }
  if (!tracing_session->config.builtin_data_sources().disable_system_info())
    MaybeEmitSystemInfo(tracing_session, &packets);
  if (!tracing_session->config.builtin_data_sources().disable_service_events())
    MaybeEmitServiceEvents(tracing_session, &packets);

  size_t packets_bytes = 0;  // SUM(slice.size() for each slice in |packets|).
  size_t total_slices = 0;   // SUM(#slices in |packets|).

  // Add up size for packets added by the Maybe* calls above.
  for (const TracePacket& packet : packets) {
    packets_bytes += packet.size();
    total_slices += packet.slices().size();
  }

  // This is a rough threshold to determine how much to read from the buffer in
  // each task. This is to avoid executing a single huge sending task for too
  // long and risk to hit the watchdog. This is *not* an upper bound: we just
  // stop accumulating new packets and PostTask *after* we cross this threshold.
  // This constant essentially balances the PostTask and IPC overhead vs the
  // responsiveness of the service. An extremely small value will cause one IPC
  // and one PostTask for each slice but will keep the service extremely
  // responsive. An extremely large value will batch the send for the full
  // buffer in one large task, will hit the blocking send() once the socket
  // buffers are full and hang the service for a bit (until the consumer
  // catches up).
  static constexpr size_t kApproxBytesPerTask = 32768;
  bool did_hit_threshold = false;

  // TODO(primiano): Extend the ReadBuffers API to allow reading only some
  // buffers, not all of them in one go.
  for (size_t buf_idx = 0;
       buf_idx < tracing_session->num_buffers() && !did_hit_threshold;
       buf_idx++) {
    auto tbuf_iter = buffers_.find(tracing_session->buffers_index[buf_idx]);
    if (tbuf_iter == buffers_.end()) {
      PERFETTO_DFATAL("Buffer not found.");
      continue;
    }
    TraceBuffer& tbuf = *tbuf_iter->second;
    tbuf.BeginRead();
    while (!did_hit_threshold) {
      TracePacket packet;
      TraceBuffer::PacketSequenceProperties sequence_properties{};
      bool previous_packet_dropped;
      if (!tbuf.ReadNextTracePacket(&packet, &sequence_properties,
                                    &previous_packet_dropped)) {
        break;
      }
      PERFETTO_DCHECK(sequence_properties.producer_id_trusted != 0);
      PERFETTO_DCHECK(sequence_properties.writer_id != 0);
      PERFETTO_DCHECK(sequence_properties.producer_uid_trusted != kInvalidUid);
      PERFETTO_DCHECK(packet.size() > 0);
      if (!PacketStreamValidator::Validate(packet.slices())) {
        tracing_session->invalid_packets++;
        PERFETTO_DLOG("Dropping invalid packet");
        continue;
      }

      // Append a slice with the trusted field data. This can't be spoofed
      // because above we validated that the existing slices don't contain any
      // trusted fields. For added safety we append instead of prepending
      // because according to protobuf semantics, if the same field is
      // encountered multiple times the last instance takes priority. Note that
      // truncated packets are also rejected, so the producer can't give us a
      // partial packet (e.g., a truncated string) which only becomes valid when
      // the trusted data is appended here.
      Slice slice = Slice::Allocate(32);
      protozero::StaticBuffered<protos::pbzero::TracePacket> trusted_packet(
          slice.own_data(), slice.size);
      trusted_packet->set_trusted_uid(
          static_cast<int32_t>(sequence_properties.producer_uid_trusted));
      trusted_packet->set_trusted_packet_sequence_id(
          tracing_session->GetPacketSequenceID(
              sequence_properties.producer_id_trusted,
              sequence_properties.writer_id));
      if (previous_packet_dropped)
        trusted_packet->set_previous_packet_dropped(previous_packet_dropped);
      slice.size = trusted_packet.Finalize();
      packet.AddSlice(std::move(slice));

      // Append the packet (inclusive of the trusted uid) to |packets|.
      packets_bytes += packet.size();
      total_slices += packet.slices().size();
      did_hit_threshold = packets_bytes >= kApproxBytesPerTask &&
                          !tracing_session->write_into_file;
      packets.emplace_back(std::move(packet));
    }  // for(packets...)
  }    // for(buffers...)

  const bool has_more = did_hit_threshold;
  if (!has_more && tracing_session->should_emit_stats) {
    size_t prev_packets_size = packets.size();
    SnapshotStats(tracing_session, &packets);
    tracing_session->should_emit_stats = false;

    // Add sizes of packets emitted by SnapshotStats.
    for (size_t i = prev_packets_size; i < packets.size(); ++i) {
      packets_bytes += packets[i].size();
      total_slices += packets[i].slices().size();
    }
  }

  // If the caller asked us to write into a file by setting
  // |write_into_file| == true in the trace config, drain the packets read
  // (if any) into the given file descriptor.
  if (tracing_session->write_into_file) {
    const uint64_t max_size = tracing_session->max_file_size_bytes
                                  ? tracing_session->max_file_size_bytes
                                  : std::numeric_limits<size_t>::max();

    // When writing into a file, the file should look like a root trace.proto
    // message. Each packet should be prepended with a proto preamble stating
    // its field id (within trace.proto) and size. Hence the addition below.
    const size_t max_iovecs = total_slices + packets.size();

    size_t num_iovecs = 0;
    bool stop_writing_into_file = tracing_session->write_period_ms == 0;
    std::unique_ptr<struct iovec[]> iovecs(new struct iovec[max_iovecs]);
    size_t num_iovecs_at_last_packet = 0;
    uint64_t bytes_about_to_be_written = 0;
    for (TracePacket& packet : packets) {
      std::tie(iovecs[num_iovecs].iov_base, iovecs[num_iovecs].iov_len) =
          packet.GetProtoPreamble();
      bytes_about_to_be_written += iovecs[num_iovecs].iov_len;
      num_iovecs++;
      for (const Slice& slice : packet.slices()) {
        // writev() doesn't change the passed pointer. However, struct iovec
        // take a non-const ptr because it's the same struct used by readv().
        // Hence the const_cast here.
        char* start = static_cast<char*>(const_cast<void*>(slice.start));
        bytes_about_to_be_written += slice.size;
        iovecs[num_iovecs++] = {start, slice.size};
      }

      if (tracing_session->bytes_written_into_file +
              bytes_about_to_be_written >=
          max_size) {
        stop_writing_into_file = true;
        num_iovecs = num_iovecs_at_last_packet;
        break;
      }

      num_iovecs_at_last_packet = num_iovecs;
    }
    PERFETTO_DCHECK(num_iovecs <= max_iovecs);
    int fd = *tracing_session->write_into_file;

    uint64_t total_wr_size = 0;

    // writev() can take at most IOV_MAX entries per call. Batch them.
    constexpr size_t kIOVMax = IOV_MAX;
    for (size_t i = 0; i < num_iovecs; i += kIOVMax) {
      int iov_batch_size = static_cast<int>(std::min(num_iovecs - i, kIOVMax));
      ssize_t wr_size = PERFETTO_EINTR(writev(fd, &iovecs[i], iov_batch_size));
      if (wr_size <= 0) {
        PERFETTO_PLOG("writev() failed");
        stop_writing_into_file = true;
        break;
      }
      total_wr_size += static_cast<size_t>(wr_size);
    }

    tracing_session->bytes_written_into_file += total_wr_size;

    PERFETTO_DLOG("Draining into file, written: %" PRIu64 " KB, stop: %d",
                  (total_wr_size + 1023) / 1024, stop_writing_into_file);
    if (stop_writing_into_file) {
      // Ensure all data was written to the file before we close it.
      base::FlushFile(fd);
      tracing_session->write_into_file.reset();
      tracing_session->write_period_ms = 0;
      if (tracing_session->state == TracingSession::STARTED)
        DisableTracing(tsid);
      return true;
    }

    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    task_runner_->PostDelayedTask(
        [weak_this, tsid] {
          if (weak_this)
            weak_this->ReadBuffers(tsid, nullptr);
        },
        tracing_session->delay_to_next_write_period_ms());
    return true;
  }  // if (tracing_session->write_into_file)

  if (has_more) {
    auto weak_consumer = consumer->GetWeakPtr();
    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    task_runner_->PostTask([weak_this, weak_consumer, tsid] {
      if (!weak_this || !weak_consumer)
        return;
      weak_this->ReadBuffers(tsid, weak_consumer.get());
    });
  }

  // Keep this as tail call, just in case the consumer re-enters.
  consumer->consumer_->OnTraceData(std::move(packets), has_more);
  return true;
}

void TracingServiceImpl::FreeBuffers(TracingSessionID tsid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Freeing buffers for session %" PRIu64, tsid);
  TracingSession* tracing_session = GetTracingSession(tsid);
  if (!tracing_session) {
    PERFETTO_DLOG("FreeBuffers() failed, invalid session ID %" PRIu64, tsid);
    return;  // TODO(primiano): signal failure?
  }
  DisableTracing(tsid, /*disable_immediately=*/true);

  PERFETTO_DCHECK(tracing_session->AllDataSourceInstancesStopped());
  tracing_session->data_source_instances.clear();

  for (auto& producer_entry : producers_) {
    ProducerEndpointImpl* producer = producer_entry.second;
    producer->OnFreeBuffers(tracing_session->buffers_index);
  }

  for (BufferID buffer_id : tracing_session->buffers_index) {
    buffer_ids_.Free(buffer_id);
    PERFETTO_DCHECK(buffers_.count(buffer_id) == 1);
    buffers_.erase(buffer_id);
  }
  bool notify_traceur = tracing_session->config.notify_traceur();
  tracing_sessions_.erase(tsid);
  UpdateMemoryGuardrail();

  PERFETTO_LOG("Tracing session %" PRIu64 " ended, total sessions:%zu", tsid,
               tracing_sessions_.size());

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  static const char kTraceurProp[] = "sys.trace.trace_end_signal";
  if (notify_traceur && __system_property_set(kTraceurProp, "1"))
    PERFETTO_ELOG("Failed to setprop %s=1", kTraceurProp);
#else
  base::ignore_result(notify_traceur);
#endif
}

void TracingServiceImpl::RegisterDataSource(ProducerID producer_id,
                                            const DataSourceDescriptor& desc) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Producer %" PRIu16 " registered data source \"%s\"",
                producer_id, desc.name().c_str());

  PERFETTO_DCHECK(!desc.name().empty());
  auto reg_ds = data_sources_.emplace(desc.name(),
                                      RegisteredDataSource{producer_id, desc});

  // If there are existing tracing sessions, we need to check if the new
  // data source is enabled by any of them.
  if (tracing_sessions_.empty())
    return;

  ProducerEndpointImpl* producer = GetProducer(producer_id);
  if (!producer) {
    PERFETTO_DFATAL("Producer not found.");
    return;
  }

  for (auto& iter : tracing_sessions_) {
    TracingSession& tracing_session = iter.second;
    if (tracing_session.state != TracingSession::STARTED &&
        tracing_session.state != TracingSession::CONFIGURED) {
      continue;
    }

    TraceConfig::ProducerConfig producer_config;
    for (auto& config : tracing_session.config.producers()) {
      if (producer->name_ == config.producer_name()) {
        producer_config = config;
        break;
      }
    }
    for (const TraceConfig::DataSource& cfg_data_source :
         tracing_session.config.data_sources()) {
      if (cfg_data_source.config().name() != desc.name())
        continue;
      DataSourceInstance* ds_inst = SetupDataSource(
          cfg_data_source, producer_config, reg_ds->second, &tracing_session);
      if (ds_inst && tracing_session.state == TracingSession::STARTED)
        StartDataSourceInstance(producer, &tracing_session, ds_inst);
    }
  }
}

void TracingServiceImpl::StopDataSourceInstance(ProducerEndpointImpl* producer,
                                                TracingSession* tracing_session,
                                                DataSourceInstance* instance,
                                                bool disable_immediately) {
  const DataSourceInstanceID ds_inst_id = instance->instance_id;
  if (instance->will_notify_on_stop && !disable_immediately) {
    instance->state = DataSourceInstance::STOPPING;
  } else {
    instance->state = DataSourceInstance::STOPPED;
  }
  if (tracing_session->consumer_maybe_null) {
    tracing_session->consumer_maybe_null->OnDataSourceInstanceStateChange(
        *producer, *instance);
  }
  producer->StopDataSource(ds_inst_id);
}

void TracingServiceImpl::UnregisterDataSource(ProducerID producer_id,
                                              const std::string& name) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Producer %" PRIu16 " unregistered data source \"%s\"",
                producer_id, name.c_str());
  PERFETTO_CHECK(producer_id);
  ProducerEndpointImpl* producer = GetProducer(producer_id);
  PERFETTO_DCHECK(producer);
  for (auto& kv : tracing_sessions_) {
    auto& ds_instances = kv.second.data_source_instances;
    bool removed = false;
    for (auto it = ds_instances.begin(); it != ds_instances.end();) {
      if (it->first == producer_id && it->second.data_source_name == name) {
        DataSourceInstanceID ds_inst_id = it->second.instance_id;
        if (it->second.state != DataSourceInstance::STOPPED) {
          if (it->second.state != DataSourceInstance::STOPPING)
            StopDataSourceInstance(producer, &kv.second, &it->second,
                                   /* disable_immediately = */ false);
          // Mark the instance as stopped immediately, since we are
          // unregistering it below.
          if (it->second.state == DataSourceInstance::STOPPING)
            NotifyDataSourceStopped(producer_id, ds_inst_id);
        }
        it = ds_instances.erase(it);
        removed = true;
      } else {
        ++it;
      }
    }  // for (data_source_instances)
    if (removed)
      MaybeNotifyAllDataSourcesStarted(&kv.second);
  }  // for (tracing_session)

  for (auto it = data_sources_.begin(); it != data_sources_.end(); ++it) {
    if (it->second.producer_id == producer_id &&
        it->second.descriptor.name() == name) {
      data_sources_.erase(it);
      return;
    }
  }

  PERFETTO_DFATAL(
      "Tried to unregister a non-existent data source \"%s\" for "
      "producer %" PRIu16,
      name.c_str(), producer_id);
}

TracingServiceImpl::DataSourceInstance* TracingServiceImpl::SetupDataSource(
    const TraceConfig::DataSource& cfg_data_source,
    const TraceConfig::ProducerConfig& producer_config,
    const RegisteredDataSource& data_source,
    TracingSession* tracing_session) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  ProducerEndpointImpl* producer = GetProducer(data_source.producer_id);
  PERFETTO_DCHECK(producer);
  // An existing producer that is not ftrace could have registered itself as
  // ftrace, we must not enable it in that case.
  if (lockdown_mode_ && producer->uid_ != uid_) {
    PERFETTO_DLOG("Lockdown mode: not enabling producer %hu", producer->id_);
    return nullptr;
  }
  // TODO(primiano): Add tests for registration ordering (data sources vs
  // consumers).
  if (!NameMatchesFilter(producer->name_,
                         cfg_data_source.producer_name_filter(),
                         cfg_data_source.producer_name_regex_filter())) {
    PERFETTO_DLOG("Data source: %s is filtered out for producer: %s",
                  cfg_data_source.config().name().c_str(),
                  producer->name_.c_str());
    return nullptr;
  }

  auto relative_buffer_id = cfg_data_source.config().target_buffer();
  if (relative_buffer_id >= tracing_session->num_buffers()) {
    PERFETTO_LOG(
        "The TraceConfig for DataSource %s specified a target_buffer out of "
        "bound (%d). Skipping it.",
        cfg_data_source.config().name().c_str(), relative_buffer_id);
    return nullptr;
  }

  // Create a copy of the DataSourceConfig specified in the trace config. This
  // will be passed to the producer after translating the |target_buffer| id.
  // The |target_buffer| parameter passed by the consumer in the trace config is
  // relative to the buffers declared in the same trace config. This has to be
  // translated to the global BufferID before passing it to the producers, which
  // don't know anything about tracing sessions and consumers.

  DataSourceInstanceID inst_id = ++last_data_source_instance_id_;
  auto insert_iter = tracing_session->data_source_instances.emplace(
      std::piecewise_construct,  //
      std::forward_as_tuple(producer->id_),
      std::forward_as_tuple(
          inst_id,
          cfg_data_source.config(),  //  Deliberate copy.
          data_source.descriptor.name(),
          data_source.descriptor.will_notify_on_start(),
          data_source.descriptor.will_notify_on_stop(),
          data_source.descriptor.handles_incremental_state_clear()));
  DataSourceInstance* ds_instance = &insert_iter->second;

  // New data source instance starts out in CONFIGURED state.
  if (tracing_session->consumer_maybe_null) {
    tracing_session->consumer_maybe_null->OnDataSourceInstanceStateChange(
        *producer, *ds_instance);
  }

  DataSourceConfig& ds_config = ds_instance->config;
  ds_config.set_trace_duration_ms(tracing_session->config.duration_ms());
  ds_config.set_stop_timeout_ms(tracing_session->data_source_stop_timeout_ms());
  ds_config.set_enable_extra_guardrails(
      tracing_session->config.enable_extra_guardrails());
  ds_config.set_tracing_session_id(tracing_session->id);
  BufferID global_id = tracing_session->buffers_index[relative_buffer_id];
  PERFETTO_DCHECK(global_id);
  ds_config.set_target_buffer(global_id);

  PERFETTO_DLOG("Setting up data source %s with target buffer %" PRIu16,
                ds_config.name().c_str(), global_id);
  if (!producer->shared_memory()) {
    // Determine the SMB page size. Must be an integer multiple of 4k.
    // As for the SMB size below, the decision tree is as follows:
    // 1. Give priority to what is defined in the trace config.
    // 2. If unset give priority to the hint passed by the producer.
    // 3. Keep within bounds and ensure it's a multiple of 4k.
    size_t page_size = producer_config.page_size_kb() * 1024;
    if (page_size == 0)
      page_size = producer->shmem_page_size_hint_bytes_;

    // Determine the SMB size. Must be an integer multiple of the SMB page size.
    // The decision tree is as follows:
    // 1. Give priority to what defined in the trace config.
    // 2. If unset give priority to the hint passed by the producer.
    // 3. Keep within bounds and ensure it's a multiple of the page size.
    size_t shm_size = producer_config.shm_size_kb() * 1024;
    if (shm_size == 0)
      shm_size = producer->shmem_size_hint_bytes_;

    auto valid_sizes = EnsureValidShmSizes(shm_size, page_size);
    if (valid_sizes != std::tie(shm_size, page_size)) {
      PERFETTO_DLOG(
          "Invalid configured SMB sizes: shm_size %zu page_size %zu. Falling "
          "back to shm_size %zu page_size %zu.",
          shm_size, page_size, std::get<0>(valid_sizes),
          std::get<1>(valid_sizes));
    }
    std::tie(shm_size, page_size) = valid_sizes;

    // TODO(primiano): right now Create() will suicide in case of OOM if the
    // mmap fails. We should instead gracefully fail the request and tell the
    // client to go away.
    PERFETTO_DLOG("Creating SMB of %zu KB for producer \"%s\"", shm_size / 1024,
                  producer->name_.c_str());
    auto shared_memory = shm_factory_->CreateSharedMemory(shm_size);
    producer->SetupSharedMemory(std::move(shared_memory), page_size,
                                /*provided_by_producer=*/false);
  }
  producer->SetupDataSource(inst_id, ds_config);
  return ds_instance;
}

// Note: all the fields % *_trusted ones are untrusted, as in, the Producer
// might be lying / returning garbage contents. |src| and |size| can be trusted
// in terms of being a valid pointer, but not the contents.
void TracingServiceImpl::CopyProducerPageIntoLogBuffer(
    ProducerID producer_id_trusted,
    uid_t producer_uid_trusted,
    WriterID writer_id,
    ChunkID chunk_id,
    BufferID buffer_id,
    uint16_t num_fragments,
    uint8_t chunk_flags,
    bool chunk_complete,
    const uint8_t* src,
    size_t size) {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  ProducerEndpointImpl* producer = GetProducer(producer_id_trusted);
  if (!producer) {
    PERFETTO_DFATAL("Producer not found.");
    chunks_discarded_++;
    return;
  }

  TraceBuffer* buf = GetBufferByID(buffer_id);
  if (!buf) {
    PERFETTO_DLOG("Could not find target buffer %" PRIu16
                  " for producer %" PRIu16,
                  buffer_id, producer_id_trusted);
    chunks_discarded_++;
    return;
  }

  // Verify that the producer is actually allowed to write into the target
  // buffer specified in the request. This prevents a malicious producer from
  // injecting data into a log buffer that belongs to a tracing session the
  // producer is not part of.
  if (!producer->is_allowed_target_buffer(buffer_id)) {
    PERFETTO_ELOG("Producer %" PRIu16
                  " tried to write into forbidden target buffer %" PRIu16,
                  producer_id_trusted, buffer_id);
    PERFETTO_DFATAL("Forbidden target buffer");
    chunks_discarded_++;
    return;
  }

  // If the writer was registered by the producer, it should only write into the
  // buffer it was registered with.
  base::Optional<BufferID> associated_buffer =
      producer->buffer_id_for_writer(writer_id);
  if (associated_buffer && *associated_buffer != buffer_id) {
    PERFETTO_ELOG("Writer %" PRIu16 " of producer %" PRIu16
                  " was registered to write into target buffer %" PRIu16
                  ", but tried to write into buffer %" PRIu16,
                  writer_id, producer_id_trusted, *associated_buffer,
                  buffer_id);
    PERFETTO_DFATAL("Wrong target buffer");
    chunks_discarded_++;
    return;
  }

  buf->CopyChunkUntrusted(producer_id_trusted, producer_uid_trusted, writer_id,
                          chunk_id, num_fragments, chunk_flags, chunk_complete,
                          src, size);
}

void TracingServiceImpl::ApplyChunkPatches(
    ProducerID producer_id_trusted,
    const std::vector<CommitDataRequest::ChunkToPatch>& chunks_to_patch) {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  for (const auto& chunk : chunks_to_patch) {
    const ChunkID chunk_id = static_cast<ChunkID>(chunk.chunk_id());
    const WriterID writer_id = static_cast<WriterID>(chunk.writer_id());
    TraceBuffer* buf =
        GetBufferByID(static_cast<BufferID>(chunk.target_buffer()));
    static_assert(std::numeric_limits<ChunkID>::max() == kMaxChunkID,
                  "Add a '|| chunk_id > kMaxChunkID' below if this fails");
    if (!writer_id || writer_id > kMaxWriterID || !buf) {
      // This can genuinely happen when the trace is stopped. The producers
      // might see the stop signal with some delay and try to keep sending
      // patches left soon after.
      PERFETTO_DLOG(
          "Received invalid chunks_to_patch request from Producer: %" PRIu16
          ", BufferID: %" PRIu32 " ChunkdID: %" PRIu32 " WriterID: %" PRIu16,
          producer_id_trusted, chunk.target_buffer(), chunk_id, writer_id);
      patches_discarded_ += static_cast<uint64_t>(chunk.patches_size());
      continue;
    }

    // Note, there's no need to validate that the producer is allowed to write
    // to the specified buffer ID (or that it's the correct buffer ID for a
    // registered TraceWriter). That's because TraceBuffer uses the producer ID
    // and writer ID to look up the chunk to patch. If the producer specifies an
    // incorrect buffer, this lookup will fail and TraceBuffer will ignore the
    // patches. Because the producer ID is trusted, there's also no way for a
    // malicious producer to patch another producer's data.

    // Speculate on the fact that there are going to be a limited amount of
    // patches per request, so we can allocate the |patches| array on the stack.
    std::array<TraceBuffer::Patch, 1024> patches;  // Uninitialized.
    if (chunk.patches().size() > patches.size()) {
      PERFETTO_ELOG("Too many patches (%zu) batched in the same request",
                    patches.size());
      PERFETTO_DFATAL("Too many patches");
      patches_discarded_ += static_cast<uint64_t>(chunk.patches_size());
      continue;
    }

    size_t i = 0;
    for (const auto& patch : chunk.patches()) {
      const std::string& patch_data = patch.data();
      if (patch_data.size() != patches[i].data.size()) {
        PERFETTO_ELOG("Received patch from producer: %" PRIu16
                      " of unexpected size %zu",
                      producer_id_trusted, patch_data.size());
        patches_discarded_++;
        continue;
      }
      patches[i].offset_untrusted = patch.offset();
      memcpy(&patches[i].data[0], patch_data.data(), patches[i].data.size());
      i++;
    }
    buf->TryPatchChunkContents(producer_id_trusted, writer_id, chunk_id,
                               &patches[0], i, chunk.has_more_patches());
  }
}

TracingServiceImpl::TracingSession* TracingServiceImpl::GetDetachedSession(
    uid_t uid,
    const std::string& key) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  for (auto& kv : tracing_sessions_) {
    TracingSession* session = &kv.second;
    if (session->consumer_uid == uid && session->detach_key == key) {
      PERFETTO_DCHECK(session->consumer_maybe_null == nullptr);
      return session;
    }
  }
  return nullptr;
}

TracingServiceImpl::TracingSession* TracingServiceImpl::GetTracingSession(
    TracingSessionID tsid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto it = tsid ? tracing_sessions_.find(tsid) : tracing_sessions_.end();
  if (it == tracing_sessions_.end())
    return nullptr;
  return &it->second;
}

ProducerID TracingServiceImpl::GetNextProducerID() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_CHECK(producers_.size() < kMaxProducerID);
  do {
    ++last_producer_id_;
  } while (producers_.count(last_producer_id_) || last_producer_id_ == 0);
  PERFETTO_DCHECK(last_producer_id_ > 0 && last_producer_id_ <= kMaxProducerID);
  return last_producer_id_;
}

TraceBuffer* TracingServiceImpl::GetBufferByID(BufferID buffer_id) {
  auto buf_iter = buffers_.find(buffer_id);
  if (buf_iter == buffers_.end())
    return nullptr;
  return &*buf_iter->second;
}

void TracingServiceImpl::OnStartTriggersTimeout(TracingSessionID tsid) {
  // Skip entirely the flush if the trace session doesn't exist anymore.
  // This is to prevent misleading error messages to be logged.
  //
  // if the trace has started from the trigger we rely on
  // the |stop_delay_ms| from the trigger so don't flush and
  // disable if we've moved beyond a CONFIGURED state
  auto* tracing_session_ptr = GetTracingSession(tsid);
  if (tracing_session_ptr &&
      tracing_session_ptr->state == TracingSession::CONFIGURED) {
    PERFETTO_DLOG("Disabling TracingSession %" PRIu64
                  " since no triggers activated.",
                  tsid);
    // No data should be returned from ReadBuffers() regardless of if we
    // call FreeBuffers() or DisableTracing(). This is because in
    // STOP_TRACING we need this promise in either case, and using
    // DisableTracing() allows a graceful shutdown. Consumers can follow
    // their normal path and check the buffers through ReadBuffers() and
    // the code won't hang because the tracing session will still be
    // alive just disabled.
    DisableTracing(tsid);
  }
}

void TracingServiceImpl::UpdateMemoryGuardrail() {
#if PERFETTO_BUILDFLAG(PERFETTO_WATCHDOG)
  uint64_t total_buffer_bytes = 0;

  // Sum up all the shared memory buffers.
  for (const auto& id_to_producer : producers_) {
    if (id_to_producer.second->shared_memory())
      total_buffer_bytes += id_to_producer.second->shared_memory()->size();
  }

  // Sum up all the trace buffers.
  for (const auto& id_to_buffer : buffers_) {
    total_buffer_bytes += id_to_buffer.second->size();
  }

  // Set the guard rail to 32MB + the sum of all the buffers over a 30 second
  // interval.
  uint64_t guardrail = base::kWatchdogDefaultMemorySlack + total_buffer_bytes;
  base::Watchdog::GetInstance()->SetMemoryLimit(guardrail, 30 * 1000);
#endif
}

void TracingServiceImpl::SnapshotSyncMarker(std::vector<TracePacket>* packets) {
  // The sync marks are used to tokenize large traces efficiently.
  // See description in trace_packet.proto.
  if (sync_marker_packet_size_ == 0) {
    // The marker ABI expects that the marker is written after the uid.
    // Protozero guarantees that fields are written in the same order of the
    // calls. The ResynchronizeTraceStreamUsingSyncMarker test verifies the ABI.
    protozero::StaticBuffered<protos::pbzero::TracePacket> packet(
        &sync_marker_packet_[0], sizeof(sync_marker_packet_));
    packet->set_trusted_uid(static_cast<int32_t>(uid_));
    packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);

    // Keep this last.
    packet->set_synchronization_marker(kSyncMarker, sizeof(kSyncMarker));
    sync_marker_packet_size_ = packet.Finalize();
  }
  packets->emplace_back();
  packets->back().AddSlice(&sync_marker_packet_[0], sync_marker_packet_size_);
}

void TracingServiceImpl::SnapshotClocks(std::vector<TracePacket>* packets,
                                        bool set_root_timestamp) {
  protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
  uint64_t root_timestamp_ns = 0;
  auto* clock_snapshot = packet->set_clock_snapshot();

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) &&    \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
  struct {
    clockid_t id;
    protos::pbzero::ClockSnapshot::Clock::BuiltinClocks type;
    struct timespec ts;
  } clocks[] = {
      {CLOCK_BOOTTIME, protos::pbzero::ClockSnapshot::Clock::BOOTTIME, {0, 0}},
      {CLOCK_REALTIME_COARSE,
       protos::pbzero::ClockSnapshot::Clock::REALTIME_COARSE,
       {0, 0}},
      {CLOCK_MONOTONIC_COARSE,
       protos::pbzero::ClockSnapshot::Clock::MONOTONIC_COARSE,
       {0, 0}},
      {CLOCK_REALTIME, protos::pbzero::ClockSnapshot::Clock::REALTIME, {0, 0}},
      {CLOCK_MONOTONIC,
       protos::pbzero::ClockSnapshot::Clock::MONOTONIC,
       {0, 0}},
      {CLOCK_MONOTONIC_RAW,
       protos::pbzero::ClockSnapshot::Clock::MONOTONIC_RAW,
       {0, 0}},
      {CLOCK_PROCESS_CPUTIME_ID,
       protos::pbzero::ClockSnapshot::Clock::PROCESS_CPUTIME,
       {0, 0}},
      {CLOCK_THREAD_CPUTIME_ID,
       protos::pbzero::ClockSnapshot::Clock::THREAD_CPUTIME,
       {0, 0}},
  };
  // First snapshot all the clocks as atomically as we can.
  for (auto& clock : clocks) {
    if (clock_gettime(clock.id, &clock.ts) == -1)
      PERFETTO_DLOG("clock_gettime failed for clock %d", clock.id);
  }
  for (auto& clock : clocks) {
    if (set_root_timestamp &&
        clock.type == protos::pbzero::ClockSnapshot::Clock::BOOTTIME) {
      root_timestamp_ns =
          static_cast<uint64_t>(base::FromPosixTimespec(clock.ts).count());
    }
    auto* c = clock_snapshot->add_clocks();
    c->set_clock_id(static_cast<uint32_t>(clock.type));
    c->set_timestamp(
        static_cast<uint64_t>(base::FromPosixTimespec(clock.ts).count()));
  }
#else  // !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX) &&
       // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) &&
       // !PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
  auto wall_time_ns = static_cast<uint64_t>(base::GetWallTimeNs().count());
  if (set_root_timestamp)
    root_timestamp_ns = wall_time_ns;
  auto* c = clock_snapshot->add_clocks();
  c->set_clock_id(protos::pbzero::ClockSnapshot::Clock::MONOTONIC);
  c->set_timestamp(wall_time_ns);
  // The default trace clock is boot time, so we always need to emit a path to
  // it. However since we don't actually have a boot time source on these
  // platforms, pretend that wall time equals boot time.
  c = clock_snapshot->add_clocks();
  c->set_clock_id(protos::pbzero::ClockSnapshot::Clock::BOOTTIME);
  c->set_timestamp(wall_time_ns);
#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX) &&
        // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

  if (root_timestamp_ns)
    packet->set_timestamp(root_timestamp_ns);
  packet->set_trusted_uid(static_cast<int32_t>(uid_));
  packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);

  SerializeAndAppendPacket(packets, packet.SerializeAsArray());
}

void TracingServiceImpl::SnapshotStats(TracingSession* tracing_session,
                                       std::vector<TracePacket>* packets) {
  protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
  packet->set_trusted_uid(static_cast<int32_t>(uid_));
  packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);
  GetTraceStats(tracing_session).Serialize(packet->set_trace_stats());
  SerializeAndAppendPacket(packets, packet.SerializeAsArray());
}

TraceStats TracingServiceImpl::GetTraceStats(TracingSession* tracing_session) {
  TraceStats trace_stats;
  trace_stats.set_producers_connected(static_cast<uint32_t>(producers_.size()));
  trace_stats.set_producers_seen(last_producer_id_);
  trace_stats.set_data_sources_registered(
      static_cast<uint32_t>(data_sources_.size()));
  trace_stats.set_data_sources_seen(last_data_source_instance_id_);
  trace_stats.set_tracing_sessions(
      static_cast<uint32_t>(tracing_sessions_.size()));
  trace_stats.set_total_buffers(static_cast<uint32_t>(buffers_.size()));
  trace_stats.set_chunks_discarded(chunks_discarded_);
  trace_stats.set_patches_discarded(patches_discarded_);
  trace_stats.set_invalid_packets(tracing_session->invalid_packets);

  for (BufferID buf_id : tracing_session->buffers_index) {
    TraceBuffer* buf = GetBufferByID(buf_id);
    if (!buf) {
      PERFETTO_DFATAL("Buffer not found.");
      continue;
    }
    *trace_stats.add_buffer_stats() = buf->stats();
  }  // for (buf in session).
  return trace_stats;
}

void TracingServiceImpl::MaybeEmitTraceConfig(
    TracingSession* tracing_session,
    std::vector<TracePacket>* packets) {
  if (tracing_session->did_emit_config)
    return;
  tracing_session->did_emit_config = true;
  protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
  packet->set_trusted_uid(static_cast<int32_t>(uid_));
  packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);
  tracing_session->config.Serialize(packet->set_trace_config());
  SerializeAndAppendPacket(packets, packet.SerializeAsArray());
}

void TracingServiceImpl::MaybeEmitSystemInfo(
    TracingSession* tracing_session,
    std::vector<TracePacket>* packets) {
  if (tracing_session->did_emit_system_info)
    return;
  tracing_session->did_emit_system_info = true;
  protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
  auto* info = packet->set_system_info();
  base::ignore_result(info);  // For PERFETTO_OS_WIN.
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)
  struct utsname uname_info;
  if (uname(&uname_info) == 0) {
    auto* utsname_info = info->set_utsname();
    utsname_info->set_sysname(uname_info.sysname);
    utsname_info->set_version(uname_info.version);
    utsname_info->set_machine(uname_info.machine);
    utsname_info->set_release(uname_info.release);
  }
#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  char value[PROP_VALUE_MAX];
  if (__system_property_get("ro.build.fingerprint", value)) {
    info->set_android_build_fingerprint(value);
  } else {
    PERFETTO_ELOG("Unable to read ro.build.fingerprint");
  }
  info->set_hz(sysconf(_SC_CLK_TCK));
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  packet->set_trusted_uid(static_cast<int32_t>(uid_));
  packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);
  SerializeAndAppendPacket(packets, packet.SerializeAsArray());
}

void TracingServiceImpl::MaybeEmitServiceEvents(
    TracingSession* tracing_session,
    std::vector<TracePacket>* packets) {
  int64_t all_start_ns = tracing_session->time_all_data_source_started.count();
  if (!tracing_session->did_emit_all_data_source_started && all_start_ns > 0) {
    tracing_session->did_emit_all_data_source_started = true;
    protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
    packet->set_timestamp(static_cast<uint64_t>(all_start_ns));
    packet->set_trusted_uid(static_cast<int32_t>(uid_));
    packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);
    packet->set_service_event()->set_all_data_sources_started(true);
    SerializeAndAppendPacket(packets, packet.SerializeAsArray());
  }
}

void TracingServiceImpl::MaybeEmitReceivedTriggers(
    TracingSession* tracing_session,
    std::vector<TracePacket>* packets) {
  PERFETTO_DCHECK(tracing_session->num_triggers_emitted_into_trace <=
                  tracing_session->received_triggers.size());
  for (size_t i = tracing_session->num_triggers_emitted_into_trace;
       i < tracing_session->received_triggers.size(); ++i) {
    const auto& info = tracing_session->received_triggers[i];
    protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
    auto* trigger = packet->set_trigger();
    trigger->set_trigger_name(info.trigger_name);
    trigger->set_producer_name(info.producer_name);
    trigger->set_trusted_producer_uid(static_cast<int32_t>(info.producer_uid));

    packet->set_timestamp(info.boot_time_ns);
    packet->set_trusted_uid(static_cast<int32_t>(uid_));
    packet->set_trusted_packet_sequence_id(kServicePacketSequenceID);
    SerializeAndAppendPacket(packets, packet.SerializeAsArray());
    ++tracing_session->num_triggers_emitted_into_trace;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TracingServiceImpl::ConsumerEndpointImpl implementation
////////////////////////////////////////////////////////////////////////////////

TracingServiceImpl::ConsumerEndpointImpl::ConsumerEndpointImpl(
    TracingServiceImpl* service,
    base::TaskRunner* task_runner,
    Consumer* consumer,
    uid_t uid)
    : task_runner_(task_runner),
      service_(service),
      consumer_(consumer),
      uid_(uid),
      weak_ptr_factory_(this) {}

TracingServiceImpl::ConsumerEndpointImpl::~ConsumerEndpointImpl() {
  service_->DisconnectConsumer(this);
  consumer_->OnDisconnect();
}

void TracingServiceImpl::ConsumerEndpointImpl::NotifyOnTracingDisabled() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto weak_this = GetWeakPtr();
  task_runner_->PostTask([weak_this] {
    if (weak_this)
      weak_this->consumer_->OnTracingDisabled();
  });
}

void TracingServiceImpl::ConsumerEndpointImpl::EnableTracing(
    const TraceConfig& cfg,
    base::ScopedFile fd) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!service_->EnableTracing(this, cfg, std::move(fd)))
    NotifyOnTracingDisabled();
}

void TracingServiceImpl::ConsumerEndpointImpl::ChangeTraceConfig(
    const TraceConfig& cfg) {
  if (!tracing_session_id_) {
    PERFETTO_LOG(
        "Consumer called ChangeTraceConfig() but tracing was "
        "not active");
    return;
  }
  service_->ChangeTraceConfig(this, cfg);
}

void TracingServiceImpl::ConsumerEndpointImpl::StartTracing() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!tracing_session_id_) {
    PERFETTO_LOG("Consumer called StartTracing() but tracing was not active");
    return;
  }
  service_->StartTracing(tracing_session_id_);
}

void TracingServiceImpl::ConsumerEndpointImpl::DisableTracing() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!tracing_session_id_) {
    PERFETTO_LOG("Consumer called DisableTracing() but tracing was not active");
    return;
  }
  service_->DisableTracing(tracing_session_id_);
}

void TracingServiceImpl::ConsumerEndpointImpl::ReadBuffers() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!tracing_session_id_) {
    PERFETTO_LOG("Consumer called ReadBuffers() but tracing was not active");
    consumer_->OnTraceData({}, /* has_more = */ false);
    return;
  }
  if (!service_->ReadBuffers(tracing_session_id_, this)) {
    consumer_->OnTraceData({}, /* has_more = */ false);
  }
}

void TracingServiceImpl::ConsumerEndpointImpl::FreeBuffers() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!tracing_session_id_) {
    PERFETTO_LOG("Consumer called FreeBuffers() but tracing was not active");
    return;
  }
  service_->FreeBuffers(tracing_session_id_);
  tracing_session_id_ = 0;
}

void TracingServiceImpl::ConsumerEndpointImpl::Flush(uint32_t timeout_ms,
                                                     FlushCallback callback) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!tracing_session_id_) {
    PERFETTO_LOG("Consumer called Flush() but tracing was not active");
    return;
  }
  service_->Flush(tracing_session_id_, timeout_ms, callback);
}

void TracingServiceImpl::ConsumerEndpointImpl::Detach(const std::string& key) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  bool success = service_->DetachConsumer(this, key);
  auto weak_this = GetWeakPtr();
  task_runner_->PostTask([weak_this, success] {
    if (weak_this)
      weak_this->consumer_->OnDetach(success);
  });
}

void TracingServiceImpl::ConsumerEndpointImpl::Attach(const std::string& key) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  bool success = service_->AttachConsumer(this, key);
  auto weak_this = GetWeakPtr();
  task_runner_->PostTask([weak_this, success] {
    if (!weak_this)
      return;
    Consumer* consumer = weak_this->consumer_;
    TracingSession* session =
        weak_this->service_->GetTracingSession(weak_this->tracing_session_id_);
    if (!session) {
      consumer->OnAttach(false, TraceConfig());
      return;
    }
    consumer->OnAttach(success, session->config);
  });
}

void TracingServiceImpl::ConsumerEndpointImpl::GetTraceStats() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  bool success = false;
  TraceStats stats;
  TracingSession* session = service_->GetTracingSession(tracing_session_id_);
  if (session) {
    success = true;
    stats = service_->GetTraceStats(session);
  }
  auto weak_this = GetWeakPtr();
  task_runner_->PostTask([weak_this, success, stats] {
    if (weak_this)
      weak_this->consumer_->OnTraceStats(success, stats);
  });
}

void TracingServiceImpl::ConsumerEndpointImpl::ObserveEvents(
    uint32_t events_mask) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  observable_events_mask_ = events_mask;
  TracingSession* session = service_->GetTracingSession(tracing_session_id_);
  if (!session)
    return;

  if (observable_events_mask_ & ObservableEvents::TYPE_DATA_SOURCES_INSTANCES) {
    // Issue initial states.
    for (const auto& kv : session->data_source_instances) {
      ProducerEndpointImpl* producer = service_->GetProducer(kv.first);
      PERFETTO_DCHECK(producer);
      OnDataSourceInstanceStateChange(*producer, kv.second);
    }
  }

  // If the ObserveEvents() call happens after data sources have acked already
  // notify immediately.
  if (observable_events_mask_ &
      ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED) {
    service_->MaybeNotifyAllDataSourcesStarted(session);
  }
}

void TracingServiceImpl::ConsumerEndpointImpl::OnDataSourceInstanceStateChange(
    const ProducerEndpointImpl& producer,
    const DataSourceInstance& instance) {
  if (!(observable_events_mask_ &
        ObservableEvents::TYPE_DATA_SOURCES_INSTANCES)) {
    return;
  }

  if (instance.state != DataSourceInstance::CONFIGURED &&
      instance.state != DataSourceInstance::STARTED &&
      instance.state != DataSourceInstance::STOPPED) {
    return;
  }

  auto* observable_events = AddObservableEvents();
  auto* change = observable_events->add_instance_state_changes();
  change->set_producer_name(producer.name_);
  change->set_data_source_name(instance.data_source_name);
  if (instance.state == DataSourceInstance::STARTED) {
    change->set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
  } else {
    change->set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED);
  }
}

void TracingServiceImpl::ConsumerEndpointImpl::OnAllDataSourcesStarted() {
  if (!(observable_events_mask_ &
        ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED)) {
    return;
  }
  auto* observable_events = AddObservableEvents();
  observable_events->set_all_data_sources_started(true);
}

base::WeakPtr<TracingServiceImpl::ConsumerEndpointImpl>
TracingServiceImpl::ConsumerEndpointImpl::GetWeakPtr() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

ObservableEvents*
TracingServiceImpl::ConsumerEndpointImpl::AddObservableEvents() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!observable_events_) {
    observable_events_.reset(new ObservableEvents());
    auto weak_this = GetWeakPtr();
    task_runner_->PostTask([weak_this] {
      if (!weak_this)
        return;

      // Move into a temporary to allow reentrancy in OnObservableEvents.
      auto observable_events = std::move(weak_this->observable_events_);
      weak_this->consumer_->OnObservableEvents(*observable_events);
    });
  }
  return observable_events_.get();
}

void TracingServiceImpl::ConsumerEndpointImpl::QueryServiceState(
    QueryServiceStateCallback callback) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingServiceState svc_state;

  const auto& sessions = service_->tracing_sessions_;
  svc_state.set_num_sessions(static_cast<int>(sessions.size()));

  int num_started = 0;
  for (const auto& kv : sessions)
    num_started += kv.second.state == TracingSession::State::STARTED ? 1 : 0;
  svc_state.set_num_sessions_started(static_cast<int>(num_started));

  for (const auto& kv : service_->producers_) {
    auto* producer = svc_state.add_producers();
    producer->set_id(static_cast<int>(kv.first));
    producer->set_name(kv.second->name_);
    producer->set_uid(static_cast<int32_t>(producer->uid()));
  }

  for (const auto& kv : service_->data_sources_) {
    const auto& registered_data_source = kv.second;
    auto* data_source = svc_state.add_data_sources();
    *data_source->mutable_ds_descriptor() = registered_data_source.descriptor;
    data_source->set_producer_id(
        static_cast<int>(registered_data_source.producer_id));
  }
  callback(/*success=*/true, svc_state);
}

void TracingServiceImpl::ConsumerEndpointImpl::QueryCapabilities(
    QueryCapabilitiesCallback callback) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  TracingServiceCapabilities caps;
  caps.set_has_query_capabilities(true);
  caps.set_has_trace_config_output_path(true);
  caps.add_observable_events(ObservableEvents::TYPE_DATA_SOURCES_INSTANCES);
  caps.add_observable_events(ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED);
  static_assert(ObservableEvents::Type_MAX ==
                    ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED,
                "");
  callback(caps);
}

////////////////////////////////////////////////////////////////////////////////
// TracingServiceImpl::ProducerEndpointImpl implementation
////////////////////////////////////////////////////////////////////////////////

TracingServiceImpl::ProducerEndpointImpl::ProducerEndpointImpl(
    ProducerID id,
    uid_t uid,
    TracingServiceImpl* service,
    base::TaskRunner* task_runner,
    Producer* producer,
    const std::string& producer_name,
    bool in_process,
    bool smb_scraping_enabled)
    : id_(id),
      uid_(uid),
      service_(service),
      task_runner_(task_runner),
      producer_(producer),
      name_(producer_name),
      in_process_(in_process),
      smb_scraping_enabled_(smb_scraping_enabled),
      weak_ptr_factory_(this) {}

TracingServiceImpl::ProducerEndpointImpl::~ProducerEndpointImpl() {
  service_->DisconnectProducer(id_);
  producer_->OnDisconnect();
}

void TracingServiceImpl::ProducerEndpointImpl::RegisterDataSource(
    const DataSourceDescriptor& desc) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (desc.name().empty()) {
    PERFETTO_DLOG("Received RegisterDataSource() with empty name");
    return;
  }

  service_->RegisterDataSource(id_, desc);
}

void TracingServiceImpl::ProducerEndpointImpl::UnregisterDataSource(
    const std::string& name) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  service_->UnregisterDataSource(id_, name);
}

void TracingServiceImpl::ProducerEndpointImpl::RegisterTraceWriter(
    uint32_t writer_id,
    uint32_t target_buffer) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  writers_[static_cast<WriterID>(writer_id)] =
      static_cast<BufferID>(target_buffer);
}

void TracingServiceImpl::ProducerEndpointImpl::UnregisterTraceWriter(
    uint32_t writer_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  writers_.erase(static_cast<WriterID>(writer_id));
}

void TracingServiceImpl::ProducerEndpointImpl::CommitData(
    const CommitDataRequest& req_untrusted,
    CommitDataCallback callback) {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  if (metatrace::IsEnabled(metatrace::TAG_TRACE_SERVICE)) {
    PERFETTO_METATRACE_COUNTER(TAG_TRACE_SERVICE, TRACE_SERVICE_COMMIT_DATA,
                               EncodeCommitDataRequest(id_, req_untrusted));
  }

  if (!shared_memory_) {
    PERFETTO_DLOG(
        "Attempted to commit data before the shared memory was allocated.");
    return;
  }
  PERFETTO_DCHECK(shmem_abi_.is_valid());
  for (const auto& entry : req_untrusted.chunks_to_move()) {
    const uint32_t page_idx = entry.page();
    if (page_idx >= shmem_abi_.num_pages())
      continue;  // A buggy or malicious producer.

    SharedMemoryABI::Chunk chunk =
        shmem_abi_.TryAcquireChunkForReading(page_idx, entry.chunk());
    if (!chunk.is_valid()) {
      PERFETTO_DLOG("Asked to move chunk %d:%d, but it's not complete",
                    entry.page(), entry.chunk());
      continue;
    }

    // TryAcquireChunkForReading() has load-acquire semantics. Once acquired,
    // the ABI contract expects the producer to not touch the chunk anymore
    // (until the service marks that as free). This is why all the reads below
    // are just memory_order_relaxed. Also, the code here assumes that all this
    // data can be malicious and just gives up if anything is malformed.
    BufferID buffer_id = static_cast<BufferID>(entry.target_buffer());
    const SharedMemoryABI::ChunkHeader& chunk_header = *chunk.header();
    WriterID writer_id = chunk_header.writer_id.load(std::memory_order_relaxed);
    ChunkID chunk_id = chunk_header.chunk_id.load(std::memory_order_relaxed);
    auto packets = chunk_header.packets.load(std::memory_order_relaxed);
    uint16_t num_fragments = packets.count;
    uint8_t chunk_flags = packets.flags;

    service_->CopyProducerPageIntoLogBuffer(
        id_, uid_, writer_id, chunk_id, buffer_id, num_fragments, chunk_flags,
        /*chunk_complete=*/true, chunk.payload_begin(), chunk.payload_size());

    // This one has release-store semantics.
    shmem_abi_.ReleaseChunkAsFree(std::move(chunk));
  }  // for(chunks_to_move)

  service_->ApplyChunkPatches(id_, req_untrusted.chunks_to_patch());

  if (req_untrusted.flush_request_id()) {
    service_->NotifyFlushDoneForProducer(id_, req_untrusted.flush_request_id());
  }

  // Keep this invocation last. ProducerIPCService::CommitData() relies on this
  // callback being invoked within the same callstack and not posted. If this
  // changes, the code there needs to be changed accordingly.
  if (callback)
    callback();
}

void TracingServiceImpl::ProducerEndpointImpl::SetupSharedMemory(
    std::unique_ptr<SharedMemory> shared_memory,
    size_t page_size_bytes,
    bool provided_by_producer) {
  PERFETTO_DCHECK(!shared_memory_ && !shmem_abi_.is_valid());
  PERFETTO_DCHECK(page_size_bytes % 1024 == 0);

  shared_memory_ = std::move(shared_memory);
  shared_buffer_page_size_kb_ = page_size_bytes / 1024;
  is_shmem_provided_by_producer_ = provided_by_producer;

  shmem_abi_.Initialize(reinterpret_cast<uint8_t*>(shared_memory_->start()),
                        shared_memory_->size(),
                        shared_buffer_page_size_kb() * 1024);
  if (in_process_) {
    inproc_shmem_arbiter_.reset(new SharedMemoryArbiterImpl(
        shared_memory_->start(), shared_memory_->size(),
        shared_buffer_page_size_kb_ * 1024, this, task_runner_));
  }

  OnTracingSetup();
  service_->UpdateMemoryGuardrail();
}

SharedMemory* TracingServiceImpl::ProducerEndpointImpl::shared_memory() const {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  return shared_memory_.get();
}

size_t TracingServiceImpl::ProducerEndpointImpl::shared_buffer_page_size_kb()
    const {
  return shared_buffer_page_size_kb_;
}

void TracingServiceImpl::ProducerEndpointImpl::ActivateTriggers(
    const std::vector<std::string>& triggers) {
  service_->ActivateTriggers(id_, triggers);
}

void TracingServiceImpl::ProducerEndpointImpl::StopDataSource(
    DataSourceInstanceID ds_inst_id) {
  // TODO(primiano): When we'll support tearing down the SMB, at this point we
  // should send the Producer a TearDownTracing if all its data sources have
  // been disabled (see b/77532839 and aosp/655179 PS1).
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, ds_inst_id] {
    if (weak_this)
      weak_this->producer_->StopDataSource(ds_inst_id);
  });
}

SharedMemoryArbiter*
TracingServiceImpl::ProducerEndpointImpl::MaybeSharedMemoryArbiter() {
  if (!inproc_shmem_arbiter_) {
    PERFETTO_FATAL(
        "The in-process SharedMemoryArbiter can only be used when "
        "CreateProducer has been called with in_process=true and after tracing "
        "has started.");
  }

  PERFETTO_DCHECK(in_process_);
  return inproc_shmem_arbiter_.get();
}

bool TracingServiceImpl::ProducerEndpointImpl::IsShmemProvidedByProducer()
    const {
  return is_shmem_provided_by_producer_;
}

// Can be called on any thread.
std::unique_ptr<TraceWriter>
TracingServiceImpl::ProducerEndpointImpl::CreateTraceWriter(
    BufferID buf_id,
    BufferExhaustedPolicy buffer_exhausted_policy) {
  PERFETTO_DCHECK(MaybeSharedMemoryArbiter());
  return MaybeSharedMemoryArbiter()->CreateTraceWriter(buf_id,
                                                       buffer_exhausted_policy);
}

void TracingServiceImpl::ProducerEndpointImpl::NotifyFlushComplete(
    FlushRequestID id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DCHECK(MaybeSharedMemoryArbiter());
  return MaybeSharedMemoryArbiter()->NotifyFlushComplete(id);
}

void TracingServiceImpl::ProducerEndpointImpl::OnTracingSetup() {
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this] {
    if (weak_this)
      weak_this->producer_->OnTracingSetup();
  });
}

void TracingServiceImpl::ProducerEndpointImpl::Flush(
    FlushRequestID flush_request_id,
    const std::vector<DataSourceInstanceID>& data_sources) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, flush_request_id, data_sources] {
    if (weak_this) {
      weak_this->producer_->Flush(flush_request_id, data_sources.data(),
                                  data_sources.size());
    }
  });
}

void TracingServiceImpl::ProducerEndpointImpl::SetupDataSource(
    DataSourceInstanceID ds_id,
    const DataSourceConfig& config) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  allowed_target_buffers_.insert(static_cast<BufferID>(config.target_buffer()));
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, ds_id, config] {
    if (weak_this)
      weak_this->producer_->SetupDataSource(ds_id, std::move(config));
  });
}

void TracingServiceImpl::ProducerEndpointImpl::StartDataSource(
    DataSourceInstanceID ds_id,
    const DataSourceConfig& config) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, ds_id, config] {
    if (weak_this)
      weak_this->producer_->StartDataSource(ds_id, std::move(config));
  });
}

void TracingServiceImpl::ProducerEndpointImpl::NotifyDataSourceStarted(
    DataSourceInstanceID data_source_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  service_->NotifyDataSourceStarted(id_, data_source_id);
}

void TracingServiceImpl::ProducerEndpointImpl::NotifyDataSourceStopped(
    DataSourceInstanceID data_source_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  service_->NotifyDataSourceStopped(id_, data_source_id);
}

void TracingServiceImpl::ProducerEndpointImpl::OnFreeBuffers(
    const std::vector<BufferID>& target_buffers) {
  if (allowed_target_buffers_.empty())
    return;
  for (BufferID buffer : target_buffers)
    allowed_target_buffers_.erase(buffer);
}

void TracingServiceImpl::ProducerEndpointImpl::ClearIncrementalState(
    const std::vector<DataSourceInstanceID>& data_sources) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_this, data_sources] {
    if (weak_this) {
      weak_this->producer_->ClearIncrementalState(data_sources.data(),
                                                  data_sources.size());
    }
  });
}

void TracingServiceImpl::ProducerEndpointImpl::Sync(
    std::function<void()> callback) {
  task_runner_->PostTask(callback);
}

////////////////////////////////////////////////////////////////////////////////
// TracingServiceImpl::TracingSession implementation
////////////////////////////////////////////////////////////////////////////////

TracingServiceImpl::TracingSession::TracingSession(
    TracingSessionID session_id,
    ConsumerEndpointImpl* consumer,
    const TraceConfig& new_config)
    : id(session_id),
      consumer_maybe_null(consumer),
      consumer_uid(consumer->uid_),
      config(new_config) {}

}  // namespace perfetto
