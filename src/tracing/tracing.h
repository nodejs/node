#ifndef SRC_TRACING_TRACING_H_
#define SRC_TRACING_TRACING_H_

#ifndef V8_USE_PERFETTO
#error The tracing.h and tracing.cc use the Perfetto dependency to implement
#error trace event support. This header should only be used when configure is
#error run using the --use-perfetto option.
#endif

#include "node_mutex.h"

#define PERFETTO_TRACK_EVENT_NAMESPACE node::tracing

#include "perfetto/tracing.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "protos/perfetto/config/data_source_config.gen.h"
#include "protos/perfetto/config/trace_config.gen.h"
#include "protos/perfetto/config/track_event/track_event_config.gen.h"
#include "protos/perfetto/trace/track_event/counter_descriptor.pbzero.h"

#include "v8-platform.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace perfetto {
class TracingSession;
namespace trace_processor {
class TraceProcessorStorage;
}  // namespace trace_processor
}  // namespace perfetto

#define CATEGORY(name, desc) perfetto::Category(name).SetDescription(desc)
#define GROUP(names) perfetto::Category::Group(names)
PERFETTO_DEFINE_CATEGORIES(
  CATEGORY("node", "Node.js Events"),
  CATEGORY("node.async_hooks", "Async Handle Lifecycle Events"),
  CATEGORY("node.bootstrap", "Bootstrap Milestones"),
  CATEGORY("node.console", "Console Related Events"),
  CATEGORY("node.crypto", "Crypto Events"),
  CATEGORY("node.dns", "DNS Subsystem Events"),
  CATEGORY("node.dns.native", "DNS Subsystem Events (native)"),
  CATEGORY("node.fs", "Filesystem Subsystem Events"),
  CATEGORY("node.fs.sync", "Filesystem Subsystem Events (sync)"),
  CATEGORY("node.fs_dir", "Filesystem Subsystem Events (dir)"),
  CATEGORY("node.fs_dir.sync", "Filesystem Subsystem Events (dir, sync)"),
  CATEGORY("node.native_immediates", "Native Immediate Callbacks"),
  CATEGORY("node.perf", "Perf_hooks Subsystem Events"),
  CATEGORY("node.perf.event_loop", "Perf_hooks Event Loop Details"),
  CATEGORY("node.perf.timerify", "Perf_hooks Subsystem Events (timerify)"),
  CATEGORY("node.perf.usertiming", "Perf_hooks Subsystem Events (usertiming)"),
  CATEGORY("node.process", "Node.js Process Metadata"),
  CATEGORY("node.promises", "Promise Related Details"),
  CATEGORY("node.promises.rejections", "Promise Rejection Counter"),
  CATEGORY("node.vm", "VM Subsystem Events"),
  CATEGORY("node.vm.script", "VM Subsystem Events (script)"),

  GROUP("node,node.async_hooks"),
  GROUP("node,node.bootstrap"),
  GROUP("node,node.console"),
  GROUP("node,node.crypto"),
  GROUP("node,node.dns"),
  GROUP("node,node.dns,node.dns.native"),
  GROUP("node,node.fs"),
  GROUP("node,node.fs,node.fs.sync"),
  GROUP("node,node.fs_dir"),
  GROUP("node,node.fs_dir,node.fs_dir.sync"),
  GROUP("node,node.native_immediates"),
  GROUP("node,node.perf"),
  GROUP("node,node.perf,node.perf.event_loop"),
  GROUP("node,node.perf,node.perf.timerify"),
  GROUP("node,node.perf,node.perf.usertiming"),
  GROUP("node,node.process"),
  GROUP("node,node.promises"),
  GROUP("node,node.promises,node.promises.rejections"),
  GROUP("node,node.vm"),
  GROUP("node,node.vm,node.vm.script")
);
#undef CATEGORY
#undef GROUP

#define SHARED_CATEGORIES(V)                                                  \
  V(async_hooks)                                                              \
  V(console)

#define TRACE_EVENT_COUNTER(category, name, ...)  \
  PERFETTO_INTERNAL_TRACK_EVENT(                  \
      category, PERFETTO_GET_STATIC_STRING(name), \
      ::perfetto::protos::pbzero::TrackEvent::TYPE_COUNTER, ##__VA_ARGS__)

#define TRACE_EVENT_NODE_COUNTER(category, name, track, value)                \
  TRACE_EVENT_COUNTER(                                                        \
      category,                                                               \
      name,                                                                   \
      per_process::v8_platform.tracing_service_.track,                        \
      [&](perfetto::EventContext ctx) {                                       \
        ctx.event()->set_counter_value(value);                                \
      })

namespace node {
namespace tracing {

static constexpr uint32_t kMinFlushPeriod = 100;

#define NODEJS_COUNTER_TRACKS(V)                                              \
  V(UNHANDLEDREJECTIONS, "Unhandled Rejections")                              \
  V(REJECTIONSHANDLEDAFTER, "Rejections Handled After")

class TracingService;

struct TracingServiceListener {
  virtual void OnStartTracing() = 0;
  virtual void OnStopTracing() = 0;
};

// Node's v8::TracingController implementation. The TracingService
// will use this by default, but an alternative may be specified
// using TracingService::Options.
class TracingController : public v8::TracingController,
                          public TracingServiceListener {
 public:
  TracingController() = default;
  ~TracingController() = default;

  void AddTraceStateObserver(
      v8::TracingController::TraceStateObserver* observer) override;

  void RemoveTraceStateObserver(
      v8::TracingController::TraceStateObserver* observer) override;

  void OnStartTracing() override;
  void OnStopTracing() override;

 private:
  Mutex mutex_;
  std::atomic_bool tracing_{false};
  std::unordered_set<v8::TracingController::TraceStateObserver*> observers_;
};

class TracingSessionBase : public TracingServiceListener {
 public:
  explicit TracingSessionBase(TracingService* service);

  virtual ~TracingSessionBase() = default;

  TracingService* service() { return service_; }

 private:
  TracingService* service_;
};

// The TracingService controls the state of tracing within the Node.js
// process. For every Node.js process there is at most one TracingService
// instance. For standalone Node.js, the TracingService is created at
// process startup and passed in to the Node instance. For embedded Node.js,
// the embedder environment is responsible for providing the TracingService
// instance.
//
// Example:
//   TracingService::Options options;
//   options.backends = static_cast<TracingService::Backend>(
//     static_cast<int>(TracingService::Backend::IN_PROCESS) |
//     static_cast<int>(TracingService::Backend::SYSTEM) |
//     static_cast<int>(TracingService::Backend::CUSTOM)
//   );
//
//   TracingService tracing_service_(options);
//
class TracingService {
 public:
  enum class Backend {
    // The in-process Perfetto backend. This is the default and is
    // supported on all platforms.
    IN_PROCESS = 1,

    // The perfetto System backend. This backend uses Perfetto's own
    // IPC mechanism to connect to a Perfetto traced-daemon. This is
    // only supported on POSIX systems.
    // (Not-yet implemented)
    SYSTEM = 2,

    // The Node.js custom backend. This backend allows Node.js users
    // to define their own mechanism for consuming traces via the use
    // of a Native Addon.
    // (Not-yet implemented)
    CUSTOM = 4
  };

  struct Options {
    Backend backends = Backend::IN_PROCESS;
    uint32_t page_size_hint_kb = 0;
    uint32_t size_hint_kb = 0;
  };

  // Tracks the enabled/disabled state of categories that are used from
  // JavaScript.
  struct SharedCategoryState {
#define V(name) uint8_t name;
    SHARED_CATEGORIES(V)
#undef V
  };

#define V(name, _)                                                             \
  name ## _COUNTER_TRACK(                                                      \
      perfetto::Track(track_counter_++, perfetto::ThreadTrack::Current())),
  explicit TracingService(
      std::unique_ptr<TracingController> controller
          = std::make_unique<TracingController>())
      : NODEJS_COUNTER_TRACKS(V)
        tracing_controller_(std::move(controller)) {
  }
#undef V

  ~TracingService();

  void Init(const Options& options);

  // Blocks until the configured Tracing backends are ready and initialized.
  // Returns immediately if the backends are already initialized.
  void WaitForBackends();

  // True if this TracingService is configured to use the specified backend.
  bool using_backend(Backend backend) const {
    return static_cast<int>(backends_) & static_cast<int>(backend);
  }

  // Returns the v8::TracingController instance to use for the V8 platform.
  v8::TracingController* tracing_controller() {
    return tracing_controller_.get();
  }

  // Creates and returns a new tracing session. The TracingService keeps track
  // of all tracing sessions and will ensure that they are properly terminated
  // when the TracingService is torn down.
  void AddTracingSession(TracingSessionBase* listener);

  void RemoveTracingSession(TracingSessionBase* listener);

  // Starts the TracingService if it has not already been started.
  // All active TracingSessions are notified.
  bool StartTracing();

  // Stops the TracingService if it is active. All active TracingSessions
  // are notified.
  bool StopTracing();

#define V(name, _) perfetto::Track name ## _COUNTER_TRACK;
  NODEJS_COUNTER_TRACKS(V)
#undef V

  size_t GetNextSessionNum() { return ++session_counter_; }

  SharedCategoryState* shared_category_state() {
    return &shared_category_state_;
  }

 private:
  void InitTracks();
  void UpdateSharedCategoryState();

  Mutex mutex_;
  std::unique_ptr<TracingController> tracing_controller_;
  Backend backends_ = Backend::IN_PROCESS;
  std::unordered_set<TracingSessionBase*> sessions_;
  std::atomic_bool tracing_;
  SharedCategoryState shared_category_state_;

  int track_counter_ = 1000;
  size_t session_counter_ = 0;
};

template <typename TracingSessionTraits>
class TracingSession : public TracingSessionBase {
 public:
  using State = typename TracingSessionTraits::State;
  using Settings = typename TracingSessionTraits::Settings;
  struct Options {
    static constexpr uint32_t kDefaultSizeKb = 4096;
    uint32_t size_kb = kDefaultSizeKb;
    uint32_t duration = 0;
    uint32_t flush_period = 0;
    std::vector<std::string> enabled_categories;

    Settings settings;
  };

  TracingSession(TracingService* service, const Options& options)
      : TracingSessionBase(service) {
    ::perfetto::TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(options.size_kb);
    trace_config.set_unique_session_name(
        std::string("session-") + std::to_string(service->GetNextSessionNum()));
    if (options.flush_period > 0) {
      trace_config.set_flush_period_ms(
          std::max(kMinFlushPeriod, options.flush_period));
    }
    if (options.duration > 0)
      trace_config.set_duration_ms(options.duration);
    auto ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("track_event");
    perfetto::protos::gen::TrackEventConfig te_config;
    te_config.add_disabled_categories("*");
    for (const auto& category : options.enabled_categories)
      te_config.add_enabled_categories(category);
    ds_config->set_track_event_config_raw(te_config.SerializeAsString());
    session_ = perfetto::Tracing::NewTrace(
        perfetto::BackendType::kUnspecifiedBackend);
    session_->SetOnStopCallback([&]() {
      started_.store(false);
      OnStopTracing();
    });
    TracingSessionTraits::Init(this, options, trace_config);
  }

  // If running_ is false, sets running_ to true and starts
  // tracing, otherwise returns immediately.
  void OnStartTracing() override {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
      return;

    TracingSessionTraits::OnStartTracing(this);

    started_.store(true);
    session_->StartBlocking();
  }

  // If running_ is false, returns immedidately.
  // Otherwise, sets running_ to false and...
  // If started_ is true, stops tracing.
  // Tears down the session
  void OnStopTracing() override {
    bool expected_running = true;
    bool expected_started = true;

    if (!running_.compare_exchange_strong(expected_running, false))
       return;

    if (started_.compare_exchange_strong(expected_started, false))
      session_->StopBlocking();

    TracingSessionTraits::OnStopTracing(this);
    service()->RemoveTracingSession(this);
  }

  bool is_tracing() const { return started_.load(); }

  State& state() { return state_; }

  perfetto::TracingSession* session() {
    return session_.get();
  }

 private:
  std::atomic_bool started_{false};
  std::atomic_bool running_{false};
  std::unique_ptr<perfetto::trace_processor::TraceProcessorStorage>
      trace_processor_;
  std::unique_ptr<perfetto::TracingSession> session_;
  State state_ {};
};

struct FileTracingSessionTraits {
  struct Settings {
    std::string file_pattern;
  };

  struct State {
    int id = -1;
    int fd = -1;
    int seq = 0;
  };

  static void OpenNewFileForStreaming(
      TracingSession<FileTracingSessionTraits>* session,
      const TracingSession<FileTracingSessionTraits>::Options& options);

  static void CloseFile(TracingSession<FileTracingSessionTraits>* session);

  static void Init(
      TracingSession<FileTracingSessionTraits>* session,
      const TracingSession<FileTracingSessionTraits>::Options& options,
      const perfetto::TraceConfig& config);
  static void OnStartTracing(
      TracingSession<FileTracingSessionTraits>* session);
  static void OnStopTracing(
      TracingSession<FileTracingSessionTraits>* session);
};

using FileTracingSession = TracingSession<FileTracingSessionTraits>;

static inline void SetProcessTrackTitle(const std::string& name) {
  node::tracing::TrackEvent::SetTrackDescriptor(
      perfetto::ProcessTrack::Current(),
      [&](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name(name);
      });
}

static inline void SetThreadTrackTitle(const std::string& name) {
  node::tracing::TrackEvent::SetTrackDescriptor(
      perfetto::ThreadTrack::Current(),
      [&](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name(name);
      });
}

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_TRACING_H_
