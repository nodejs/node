#include "tracing/agent_perfetto.h"

#include <cstdio>
#include <string>
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_options.h"
#include "trace_event.h"

#include "trace_event_perfetto.h"

namespace node {
namespace tracing {
namespace {

// Perfetto's file writer drains buffers on a fixed period (default 5s) instead
// of continuously; mirror that cadence here.
constexpr uint64_t kReadPeriodMs = 5000;

// Rotate to a new file once the current one reaches this size, so no single
// trace file grows without bound.
constexpr uint64_t kMaxFileSizeBytes = 64 * 1024 * 1024;  // 64 MiB

void replace_substring(std::string* target,
                       const std::string& search,
                       const std::string& insert) {
  size_t pos = target->find(search);
  for (; pos != std::string::npos; pos = target->find(search, pos)) {
    target->replace(pos, search.size(), insert);
    pos += insert.size();
  }
}

std::set<std::string> flatten(
    const std::unordered_map<int, std::multiset<std::string>>& map) {
  std::set<std::string> result;
  for (const auto& id_value : map)
    result.insert(id_value.second.begin(), id_value.second.end());
  return result;
}

}  // namespace

// Writes trace chunks to a file, rotating by size. It deliberately uses the
// synchronous uv_fs_* APIs, mirroring Perfetto's internal file writer which
// does blocking writev() on its own service thread.
//
// Synchronous writes are the right fit here, not just simpler:
//   - These writes run on the dedicated tracing loop thread, so blocking on
//     disk stalls neither the application's main thread nor Perfetto's internal
//     thread.
//   - The buffer is drained periodically (see kReadPeriodMs) and each chunk is
//     already contiguous, so there is little I/O to overlap; the thread-pool
//     offload of async uv_fs_* would buy little.
//   - It keeps buffer lifetime, write ordering, flushing, rotation, and
//     shutdown trivial. Async writes would require keeping each buffer alive
//     until its callback, serializing to one in-flight write per fd, and a
//     condition-variable wait for blocking Flush() (see NodeTraceWriter).
class SimpleWriter : public TraceWriter {
 public:
  explicit SimpleWriter(std::string log_file_pattern)
      : log_file_pattern_(std::move(log_file_pattern)) {}

  ~SimpleWriter() override {
    if (fd_ < 0) return;
    uv_fs_t req;
    uv_fs_close(loop_, &req, fd_, nullptr);
    uv_fs_req_cleanup(&req);
  }

  void InitializeOnThread(uv_loop_t* loop) override {
    loop_ = loop;
    OpenNewFileForStreaming();
  }

  // Synchronously writes a chunk, rotating to a new file once it grows too
  // large. See the class comment for why the write is synchronous.
  void AppendTraceChunk(std::vector<char> chunk) override {
    if (fd_ < 0) return;
    uv_buf_t buf =
        uv_buf_init(chunk.data(), static_cast<unsigned int>(chunk.size()));
    uv_fs_t req;
    int written = uv_fs_write(loop_, &req, fd_, &buf, 1, -1, nullptr);
    uv_fs_req_cleanup(&req);
    if (written < 0) return;

    bytes_written_ += written;
    // Each chunk holds only whole trace packets, so rotating on a chunk
    // boundary leaves every file a self-contained, valid trace.
    if (bytes_written_ >= kMaxFileSizeBytes) OpenNewFileForStreaming();
  }

  void Flush(bool blocking) override {
    if (fd_ < 0) return;
    uv_fs_t req;
    uv_fs_fsync(loop_, &req, fd_, nullptr);
    uv_fs_req_cleanup(&req);
  }

 private:
  // Opens the next rotation file, evaluating a JS-style template that accepts
  // ${pid} and ${rotation}, mirroring NodeTraceWriter::OpenNewFileForStreaming.
  void OpenNewFileForStreaming() {
    ++file_num_;
    uv_fs_t req;

    std::string filepath(log_file_pattern_);
    replace_substring(&filepath, "${pid}", std::to_string(uv_os_getpid()));
    replace_substring(&filepath, "${rotation}", std::to_string(file_num_));

    if (fd_ >= 0) {
      uv_fs_close(loop_, &req, fd_, nullptr);
      uv_fs_req_cleanup(&req);
    }

    fd_ = uv_fs_open(loop_,
                     &req,
                     filepath.c_str(),
                     UV_FS_O_CREAT | UV_FS_O_WRONLY | UV_FS_O_TRUNC,
                     0644,
                     nullptr);
    uv_fs_req_cleanup(&req);
    if (fd_ < 0) {
      fprintf(stderr,
              "Could not open trace file %s: %s\n",
              filepath.c_str(),
              uv_strerror(fd_));
      fd_ = -1;
    }
    bytes_written_ = 0;
  }

  std::string log_file_pattern_;
  uv_loop_t* loop_ = nullptr;
  uv_file fd_ = -1;
  int file_num_ = 0;
  uint64_t bytes_written_ = 0;
};

void PerfettoSessionReader::Deleter::operator()(
    PerfettoSessionReader* ptr) const noexcept {
  ptr->tracing_session_->FlushBlocking();
  ptr->Read();
  ptr->tracing_session_->Stop();
}

PerfettoSessionReader::PerfettoSessionReader(
    perfetto::TraceConfig config, std::unique_ptr<TraceWriter> writer)
    : writer_(std::move(writer)) {
  tracing_session_ =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kUnspecifiedBackend);
  tracing_session_->Setup(config);
  tracing_session_->SetOnStopCallback(
      std::bind(&PerfettoSessionReader::SessionStopCallback, this));
  tracing_session_->StartBlocking();
}

PerfettoSessionReader::~PerfettoSessionReader() {}

void PerfettoSessionReader::ChangeTraceConfig(perfetto::TraceConfig config) {
  tracing_session_->ChangeTraceConfig(config);
}

void PerfettoSessionReader::InitializeOnThread(uv_loop_t* loop) {
  tracing_loop_ = loop;
  CHECK_EQ(uv_async_init(tracing_loop_, &read_async_, OnReadAsync), 0);
  read_async_.data = this;
  CHECK_EQ(uv_timer_init(tracing_loop_, &read_timer_), 0);
  read_timer_.data = this;
  writer_->InitializeOnThread(loop);

  // Drain the trace buffer periodically instead of continuously, mirroring
  // Perfetto's file writer which reschedules ReadBuffersIntoFile() every
  // write_period_ms.
  CHECK_EQ(
      uv_timer_start(&read_timer_, OnReadTimer, kReadPeriodMs, kReadPeriodMs),
      0);
}

void PerfettoSessionReader::Read() {
  if (stop_requested_) return;
  // Skip if a previous periodic read is still draining the buffer.
  bool expected = false;
  if (!read_in_progress_.compare_exchange_strong(expected, true)) return;
  tracing_session_->ReadTrace(std::bind(
      &PerfettoSessionReader::ReadTraceCallback, this, std::placeholders::_1));
}

void PerfettoSessionReader::ReadTraceCallback(
    perfetto::TracingSession::ReadTraceCallbackArgs args) {
  // On Perfetto internal thread.
  {
    Mutex::ScopedLock lock(chunks_mutex_);
    if (args.size > 0)
      pending_chunks_.emplace_back(args.data, args.data + args.size);
  }
  // A single ReadTrace() cycle can yield multiple callbacks; the last one has
  // has_more == false, which clears read_in_progress_ so the next timer tick
  // can start a new read.
  read_in_progress_ = args.has_more;
  uv_async_send(&read_async_);
}

void PerfettoSessionReader::SessionStopCallback() {
  stop_requested_ = true;
  uv_async_send(&read_async_);
}

// static
void PerfettoSessionReader::OnReadAsync(uv_async_t* async) {
  PerfettoSessionReader* reader =
      static_cast<PerfettoSessionReader*>(async->data);
  std::list<std::vector<char>> chunks_to_write;
  {
    Mutex::ScopedLock lock(reader->chunks_mutex_);
    std::swap(chunks_to_write, reader->pending_chunks_);
  }

  while (!chunks_to_write.empty()) {
    std::vector<char>& chunk = chunks_to_write.front();
    reader->writer_->AppendTraceChunk(std::move(chunk));
    chunks_to_write.pop_front();
  }

  if (reader->stop_requested_ && reader->handles_pending_close_ == 0) {
    reader->writer_->Flush(true);

    reader->handles_pending_close_ = 2;
    uv_timer_stop(&reader->read_timer_);
    uv_close(reinterpret_cast<uv_handle_t*>(&reader->read_async_),
             OnHandleClose);
    uv_close(reinterpret_cast<uv_handle_t*>(&reader->read_timer_),
             OnHandleClose);
  }
}

// static
void PerfettoSessionReader::OnReadTimer(uv_timer_t* timer) {
  PerfettoSessionReader* reader =
      static_cast<PerfettoSessionReader*>(timer->data);
  reader->Read();
}

// static
void PerfettoSessionReader::OnHandleClose(uv_handle_t* handle) {
  PerfettoSessionReader* reader =
      static_cast<PerfettoSessionReader*>(handle->data);
  if (--reader->handles_pending_close_ == 0) delete reader;
}

PerfettoTracingAgent::PerfettoTracingAgent() {
  CHECK_EQ(uv_loop_init(&tracing_loop_), 0);
  CHECK_EQ(uv_async_init(&tracing_loop_,
                         &initialize_writer_async_,
                         [](uv_async_t* async) {
                           PerfettoTracingAgent* agent = ContainerOf(
                               &PerfettoTracingAgent::initialize_writer_async_,
                               async);
                           agent->InitializeWritersOnThread();
                         }),
           0);

  // Set up the in-process backend that the tracing controller will connect
  // to.
  perfetto::TracingInitArgs init_args;
  init_args.backends = perfetto::BackendType::kInProcessBackend;
  perfetto::Tracing::Initialize(init_args);

  node::TrackEvent::Register();
}

void PerfettoTracingAgent::InitializeWritersOnThread() {
  Mutex::ScopedLock lock(initialize_writer_mutex_);
  while (!to_be_initialized_.empty()) {
    auto head = *to_be_initialized_.begin();
    head->InitializeOnThread(&tracing_loop_);
    to_be_initialized_.erase(head);
  }
  initialize_writer_condvar_.Broadcast(lock);

  uv_unref(reinterpret_cast<uv_handle_t*>(&initialize_writer_async_));
}

PerfettoTracingAgent::~PerfettoTracingAgent() {
  StopTracing();

  categories_.clear();
  writers_.clear();

  uv_close(reinterpret_cast<uv_handle_t*>(&initialize_writer_async_), nullptr);
  uv_run(&tracing_loop_, UV_RUN_ONCE);
  CheckedUvLoopClose(&tracing_loop_);
}

void PerfettoTracingAgent::Start() {
  if (started_) return;

  // This thread should be created *after* async handles are created
  // (within NodeTraceWriter and NodeTraceBuffer constructors).
  // Otherwise the thread could shut down prematurely.
  CHECK_EQ(0,
           uv_thread_create(
               &thread_,
               [](void* arg) {
                 uv_thread_setname("TraceEventWorker");
                 PerfettoTracingAgent* agent =
                     static_cast<PerfettoTracingAgent*>(arg);
                 uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
               },
               this));

  started_ = true;
}

AgentWriterHandle* PerfettoTracingAgent::GetDefaultWriterHandle() {
  return tracing_file_writer_.has_value() ? &tracing_file_writer_.value()
                                          : nullptr;
}

AgentWriterHandle PerfettoTracingAgent::AddClient(
    const std::set<std::string>& categories,
    std::unique_ptr<TraceWriter> writer,
    enum UseDefaultCategoryMode mode) {
  Start();

  const std::set<std::string>* use_categories = &categories;

  std::set<std::string> categories_with_default;
  if (mode == kUseDefaultCategories) {
    categories_with_default.insert(categories.begin(), categories.end());
    categories_with_default.insert(categories_[kDefaultHandleId].begin(),
                                   categories_[kDefaultHandleId].end());
    use_categories = &categories_with_default;
  }

  int id = next_writer_id_++;
  PerfettoSessionReader::Ptr reader = PerfettoSessionReader::Create(
      CreateTraceConfig({use_categories->begin(), use_categories->end()}),
      std::move(writer));

  auto* raw = reader.get();
  writers_[id] = std::move(reader);
  categories_[id] = {use_categories->begin(), use_categories->end()};

  {
    Mutex::ScopedLock lock(initialize_writer_mutex_);
    to_be_initialized_.insert(raw);
    uv_async_send(&initialize_writer_async_);
    while (to_be_initialized_.count(raw) > 0)
      initialize_writer_condvar_.Wait(lock);
  }

  return AgentWriterHandle(this, id);
}

void PerfettoTracingAgent::StartTracing(const std::string& categories) {
  if (tracing_file_writer_.has_value()) return;

  using std::operator""sv;
  auto parts = std::views::split(categories, ","sv);

  std::set<std::string> categories_set;
  for (const auto& s : parts) {
    categories_set.emplace(std::string(s.data(), s.size()));
  }

  tracing_file_writer_ =
      AddClient(categories_set,
                std::make_unique<SimpleWriter>(
                    per_process::cli_options->trace_event_file_pattern),
                kUseDefaultCategories);
}

void PerfettoTracingAgent::StopTracing() {
  if (!started_) return;
  // Perform final Flush on TraceBuffer. We don't want the tracing controller
  // to flush the buffer again on destruction of the V8::Platform.
  writers_.clear();
  started_ = false;

  // Thread should finish when the tracing loop is stopped.
  uv_thread_join(&thread_);
}

void PerfettoTracingAgent::Disconnect(int client) {
  if (client == kDefaultHandleId) return;
  {
    Mutex::ScopedLock lock(initialize_writer_mutex_);
    to_be_initialized_.erase(writers_[client].get());
  }

  writers_.erase(client);
  categories_.erase(client);
}

void PerfettoTracingAgent::Enable(int id,
                                  const std::set<std::string>& categories) {
  if (categories.empty()) return;

  categories_[id].insert(categories.begin(), categories.end());

  writers_[id]->ChangeTraceConfig(CreateTraceConfig(categories_[id]));
}

void PerfettoTracingAgent::Disable(int id,
                                   const std::set<std::string>& categories) {
  std::multiset<std::string>& writer_categories = categories_[id];
  for (const std::string& category : categories) {
    auto it = writer_categories.find(category);
    if (it != writer_categories.end()) writer_categories.erase(it);
  }

  writers_[id]->ChangeTraceConfig(CreateTraceConfig(writer_categories));
}

std::string PerfettoTracingAgent::GetEnabledCategories() const {
  std::string categories;
  for (const std::string& category : flatten(categories_)) {
    if (!categories.empty()) categories += ',';
    categories += category;
  }
  return categories;
}

perfetto::TraceConfig PerfettoTracingAgent::CreateTraceConfig(
    std::multiset<std::string> categories) const {
  perfetto::TraceConfig perfetto_trace_config;
  perfetto_trace_config.add_buffers()->set_size_kb(4096);
  auto ds_config = perfetto_trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("track_event");
  perfetto::protos::gen::TrackEventConfig te_config;
  te_config.add_disabled_categories("*");
  for (const auto& category : categories)
    te_config.add_enabled_categories(category);
  ds_config->set_track_event_config_raw(te_config.SerializeAsString());
  return perfetto_trace_config;
}

}  // namespace tracing
}  // namespace node
