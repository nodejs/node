#ifndef SRC_TRACING_AGENT_PERFETTO_H_
#define SRC_TRACING_AGENT_PERFETTO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_mutex.h"
#include "tracing/agent.h"
#include "tracing/trace_event_perfetto.h"
#include "util.h"
#include "uv.h"
#include "v8-platform.h"

#include <list>
#include <set>
#include <string>
#include <unordered_map>

namespace node {
namespace tracing {

// Consumes trace data produced by a PerfettoSessionReader. All methods run on
// the agent's dedicated background tracing thread (the loop passed to
// InitializeOnThread), never on the application's main thread or Perfetto's
// internal thread, so implementations may block (e.g. do synchronous I/O).
class TraceWriter {
 public:
  virtual ~TraceWriter() = default;
  // 'chunk' is guaranteed to contain 1 or more full trace packets, which
  // can be decoded using trace.proto. No partial or truncated packets are
  // exposed.
  virtual void AppendTraceChunk(std::vector<char> chunk) = 0;
  virtual void Flush(bool blocking) = 0;
  virtual void InitializeOnThread(uv_loop_t* loop) {}
};

// Owns one Perfetto tracing session and pumps its trace data out to a
// TraceWriter using the consumer ReadTrace() API, reading on a fixed period.
//
// Threading: two threads are involved.
//   - Perfetto's internal thread invokes ReadTraceCallback. It only copies the
//     borrowed chunk into pending_chunks_ (under chunks_mutex_) and signals
//     read_async_; it never touches the writer or the file.
//   - The agent's dedicated tracing loop thread runs everything else. A
//     repeating timer (read_timer_) starts a read every kReadPeriodMs, and
//     OnReadAsync drains pending_chunks_ into the writer there. This is why the
//     writer can safely block on synchronous I/O (see TraceWriter).
// read_in_progress_ prevents a timer tick from starting a new read while a
// previous ReadTrace() cycle is still delivering chunks.
class PerfettoSessionReader final {
 public:
  struct Deleter {
    void operator()(PerfettoSessionReader* ptr) const noexcept;
  };
  using Ptr = std::unique_ptr<PerfettoSessionReader, Deleter>;
  static Ptr Create(perfetto::TraceConfig config,
                    std::unique_ptr<TraceWriter> writer) {
    return Ptr(new PerfettoSessionReader(config, std::move(writer)));
  }

  void ChangeTraceConfig(perfetto::TraceConfig config);

  void InitializeOnThread(uv_loop_t* loop);

 private:
  explicit PerfettoSessionReader(perfetto::TraceConfig config,
                                 std::unique_ptr<TraceWriter> writer);
  ~PerfettoSessionReader();
  void ReadTraceCallback(perfetto::TracingSession::ReadTraceCallbackArgs args);
  void SessionStopCallback();
  void Read();

  static void OnReadAsync(uv_async_t* async);
  static void OnReadTimer(uv_timer_t* timer);
  static void OnHandleClose(uv_handle_t* handle);

  uv_loop_t* tracing_loop_;
  uv_async_t read_async_;
  uv_timer_t read_timer_;
  int handles_pending_close_ = 0;
  std::atomic<bool> stop_requested_ = false;
  std::atomic<bool> read_in_progress_ = false;

  Mutex chunks_mutex_;
  std::list<std::vector<char>> pending_chunks_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_ = nullptr;

  std::unique_ptr<TraceWriter> writer_;
};

class PerfettoTracingAgent final : public Agent {
 public:
  enum UseDefaultCategoryMode {
    kUseDefaultCategories,
    kIgnoreDefaultCategories
  };

  PerfettoTracingAgent();
  ~PerfettoTracingAgent() override;

  v8::TracingController* GetTracingController() override { return nullptr; }

  AgentWriterHandle AddClient(const std::set<std::string>& categories,
                              std::unique_ptr<TraceWriter> writer,
                              enum UseDefaultCategoryMode mode);
  std::string GetEnabledCategories() const override;

  void StartTracing(const std::string& categories) override;
  void StopTracing() override;
  AgentWriterHandle* GetDefaultWriterHandle() override;

 private:
  static constexpr int kDefaultHandleId = -1;

  void Disconnect(int client) override;

  void Enable(int id, const std::set<std::string>& categories) override;
  void Disable(int id, const std::set<std::string>& categories) override;

  void InitializeWritersOnThread();
  void Start();

  perfetto::TraceConfig CreateTraceConfig(
      std::multiset<std::string> categories) const;

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;

  bool started_ = false;

  int next_writer_id_ = 1;
  std::unordered_map<int, std::multiset<std::string>> categories_;
  std::unordered_map<int, PerfettoSessionReader::Ptr> writers_;

  Mutex initialize_writer_mutex_;
  ConditionVariable initialize_writer_condvar_;
  uv_async_t initialize_writer_async_;
  std::set<PerfettoSessionReader*> to_be_initialized_;

  std::optional<AgentWriterHandle> tracing_file_writer_;
};

}  // namespace tracing
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TRACING_AGENT_PERFETTO_H_
