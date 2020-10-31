#ifndef V8_USE_PERFETTO
#error The tracing.h and tracing.cc use the Perfetto dependency to implement
#error trace event support. This file should only be compiled when configure is
#error run using the --use-perfetto option.
#endif

#include "tracing/tracing.h"
#include "util-inl.h"
#include "uv.h"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace node {

namespace tracing {

perfetto::Track PROMISE_COUNTERS;

void TracingController::AddTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {
  {
    Mutex::ScopedLock lock(mutex_);
    observers_.insert(observer);
    if (!tracing_.load(std::memory_order_acquire))
      return;
  }
  observer->OnTraceEnabled();
}

void TracingController::RemoveTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {
  Mutex::ScopedLock lock(mutex_);
  observers_.erase(observer);
}

void TracingController::OnStartTracing() {
  std::unordered_set<v8::TracingController::TraceStateObserver*> copy;
  {
    Mutex::ScopedLock lock(mutex_);
    tracing_.store(true, std::memory_order_release);
    copy = observers_;
  }
  for (auto observer : copy)
    observer->OnTraceEnabled();
}

void TracingController::OnStopTracing() {
  bool expected = true;
  if (!tracing_.compare_exchange_strong(expected, false))
    return;

  std::unordered_set<v8::TracingController::TraceStateObserver*> copy;
  {
    Mutex::ScopedLock lock(mutex_);
    copy = observers_;
  }
  for (auto observer : copy)
    observer->OnTraceDisabled();
}

TracingSessionBase::TracingSessionBase(TracingService* service)
    : service_(service) {
  service->AddTracingSession(this);
}

void TracingService::InitTracks() {
#define V(key, name)                                                          \
  do {                                                                        \
    node::tracing::TrackEvent::SetTrackDescriptor(                            \
      key ## _COUNTER_TRACK,                                                  \
      [&](perfetto::protos::pbzero::TrackDescriptor* desc) {                  \
        desc->set_name(name);                                                 \
        auto counter_desc = desc->set_counter();                              \
        counter_desc->set_unit(                                               \
          perfetto::protos::pbzero::CounterDescriptor_Unit_UNIT_COUNT);       \
        counter_desc->set_is_incremental(false);                              \
        counter_desc->set_unit_multiplier(1);                                 \
      }                                                                       \
    );                                                                        \
  } while (0);
  NODEJS_COUNTER_TRACKS(V)
#undef V
}

void TracingService::UpdateSharedCategoryState() {
#define V(name)                                                               \
  do {                                                                        \
    shared_category_state_.name =                                             \
        TRACE_EVENT_CATEGORY_ENABLED("node." #name);                          \
  } while (0);
  SHARED_CATEGORIES(V)
#undef V
}

void TracingService::Init(const Options& options) {
  backends_ = options.backends;

  perfetto::TracingInitArgs init_args;

  if (options.page_size_hint_kb > 0)
    init_args.shmem_page_size_hint_kb = options.page_size_hint_kb;

  if (options.size_hint_kb > 0)
    init_args.shmem_size_hint_kb = options.size_hint_kb;

  if (LIKELY(using_backend(TracingService::Backend::IN_PROCESS)))
    init_args.backends |= perfetto::BackendType::kInProcessBackend;

  // System and custom backends are not yet supported
  CHECK(!using_backend(TracingService::Backend::SYSTEM));
  CHECK(!using_backend(TracingService::Backend::CUSTOM));

  perfetto::Tracing::Initialize(init_args);
  InitTracks();
  node::tracing::TrackEvent::Register();

  // WaitForBackends will synchronously lock until the backends are fully
  // initialized. For now this is a non-op because we're only using the
  // in-proc backend.
  WaitForBackends();
}

TracingService::~TracingService() {}

void TracingService::AddTracingSession(TracingSessionBase* listener) {
  Mutex::ScopedLock lock(mutex_);
  sessions_.insert(listener);
  UpdateSharedCategoryState();
  tracing_controller_->OnStartTracing();
}

void TracingService::RemoveTracingSession(TracingSessionBase* listener) {
  sessions_.erase(listener);
  UpdateSharedCategoryState();
  tracing_controller_->OnStopTracing();
}

void TracingService::WaitForBackends() {}

bool TracingService::StartTracing() {
  bool expected = false;
  if (!tracing_.compare_exchange_strong(expected, true))
    return false;

  // Notify the sessions before notifying the controller.
  // In StopTracing we'll do the opposite.
  std::unordered_set<TracingSessionBase*> copy;
  {
    Mutex::ScopedLock lock(mutex_);
    copy = sessions_;
    for (auto session : copy)
      session->OnStartTracing();
  }

  UpdateSharedCategoryState();

  tracing_controller_->OnStartTracing();
  return true;
}

bool TracingService::StopTracing() {
  bool expected = true;
  if (!tracing_.compare_exchange_strong(expected, false))
    return false;

  // Notify the controller before notifying the sessions.
  tracing_controller_->OnStopTracing();
  std::unordered_set<TracingSessionBase*> copy;
  {
    Mutex::ScopedLock lock(mutex_);
    copy = sessions_;
    for (auto session : copy) {
      session->OnStopTracing();
    }
  }

  UpdateSharedCategoryState();

  return true;
}

namespace {
void replace_substring(std::string* target,
                       const std::string& search,
                       const std::string& insert) {
  size_t pos = target->find(search);
  for (; pos != std::string::npos; pos = target->find(search, pos)) {
    target->replace(pos, search.size(), insert);
    pos += insert.size();
  }
}
}  // namespace

void FileTracingSessionTraits::OpenNewFileForStreaming(
    FileTracingSession* session,
    const FileTracingSession::Options& options) {

  FileTracingSessionTraits::CloseFile(session);

  ++session->state().seq;

  std::string filepath(options.settings.file_pattern);
  replace_substring(&filepath, "${pid}",
                    std::to_string(uv_os_getpid()));
  replace_substring(&filepath, "${rotation}",
                    std::to_string(session->state().seq));

  uv_fs_t req;
  session->state().fd = uv_fs_open(
      nullptr,
      &req,
      filepath.c_str(),
      O_CREAT | O_WRONLY | O_TRUNC, 0644, nullptr);
  uv_fs_req_cleanup(&req);
  if (session->state().fd < 0) {
    fprintf(stderr, "Could not open trace file %s: %s\n",
            filepath.c_str(),
            uv_strerror(session->state().fd));
    session->state().fd = -1;
  }
}

void FileTracingSessionTraits::CloseFile(FileTracingSession* session) {
  if (session->state().fd != -1) {
    uv_fs_t req;
    CHECK_EQ(uv_fs_close(nullptr, &req, session->state().fd, nullptr), 0);
    uv_fs_req_cleanup(&req);
    session->state().fd = -1;
  }
}

void FileTracingSessionTraits::Init(
    FileTracingSession* session,
    const FileTracingSession::Options& options,
    const perfetto::TraceConfig& config) {
  OpenNewFileForStreaming(session, options);
  session->session()->Setup(config, session->state().fd);
}

void FileTracingSessionTraits::OnStartTracing(
    FileTracingSession* session) {
}

void FileTracingSessionTraits::OnStopTracing(
    FileTracingSession* session) {
  FileTracingSessionTraits::CloseFile(session);
}

}  // namespace tracing
}  // namespace node
