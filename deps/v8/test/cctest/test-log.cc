// Copyright 2006-2009 the V8 project authors. All rights reserved.
//
// Tests of logging functions from log.h

#ifdef ENABLE_LOGGING_AND_PROFILING

#ifdef __linux__
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif  // __linux__

#include "v8.h"
#include "log.h"
#include "cpu-profiler.h"
#include "v8threads.h"
#include "cctest.h"

using v8::internal::Address;
using v8::internal::EmbeddedVector;
using v8::internal::Logger;
using v8::internal::StrLength;

namespace i = v8::internal;

static void SetUp() {
  // Log to memory buffer.
  i::FLAG_logfile = "*";
  i::FLAG_log = true;
  Logger::Setup();
}

static void TearDown() {
  Logger::TearDown();
}


TEST(EmptyLog) {
  SetUp();
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 100));
  TearDown();
}


TEST(GetMessages) {
  SetUp();
  Logger::StringEvent("aaa", "bbb");
  Logger::StringEvent("cccc", "dddd");
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 0));
  char log_lines[100];
  memset(log_lines, 0, sizeof(log_lines));
  // See Logger::StringEvent.
  const char* line_1 = "aaa,\"bbb\"\n";
  const int line_1_len = StrLength(line_1);
  // The exact size.
  CHECK_EQ(line_1_len, Logger::GetLogLines(0, log_lines, line_1_len));
  CHECK_EQ(line_1, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  // A bit more than the first line length.
  CHECK_EQ(line_1_len, Logger::GetLogLines(0, log_lines, line_1_len + 3));
  log_lines[line_1_len] = '\0';
  CHECK_EQ(line_1, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  const char* line_2 = "cccc,\"dddd\"\n";
  const int line_2_len = StrLength(line_2);
  // Now start with line_2 beginning.
  CHECK_EQ(0, Logger::GetLogLines(line_1_len, log_lines, 0));
  CHECK_EQ(line_2_len, Logger::GetLogLines(line_1_len, log_lines, line_2_len));
  CHECK_EQ(line_2, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  CHECK_EQ(line_2_len,
           Logger::GetLogLines(line_1_len, log_lines, line_2_len + 3));
  CHECK_EQ(line_2, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  // Now get entire buffer contents.
  const char* all_lines = "aaa,\"bbb\"\ncccc,\"dddd\"\n";
  const int all_lines_len = StrLength(all_lines);
  CHECK_EQ(all_lines_len, Logger::GetLogLines(0, log_lines, all_lines_len));
  CHECK_EQ(all_lines, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  CHECK_EQ(all_lines_len, Logger::GetLogLines(0, log_lines, all_lines_len + 3));
  CHECK_EQ(all_lines, log_lines);
  memset(log_lines, 0, sizeof(log_lines));
  TearDown();
}


static int GetLogLines(int start_pos, i::Vector<char>* buffer) {
  return Logger::GetLogLines(start_pos, buffer->start(), buffer->length());
}


TEST(BeyondWritePosition) {
  SetUp();
  Logger::StringEvent("aaa", "bbb");
  Logger::StringEvent("cccc", "dddd");
  // See Logger::StringEvent.
  const char* all_lines = "aaa,\"bbb\"\ncccc,\"dddd\"\n";
  const int all_lines_len = StrLength(all_lines);
  EmbeddedVector<char, 100> buffer;
  const int beyond_write_pos = all_lines_len;
  CHECK_EQ(0, Logger::GetLogLines(beyond_write_pos, buffer.start(), 1));
  CHECK_EQ(0, GetLogLines(beyond_write_pos, &buffer));
  CHECK_EQ(0, Logger::GetLogLines(beyond_write_pos + 1, buffer.start(), 1));
  CHECK_EQ(0, GetLogLines(beyond_write_pos + 1, &buffer));
  CHECK_EQ(0, Logger::GetLogLines(beyond_write_pos + 100, buffer.start(), 1));
  CHECK_EQ(0, GetLogLines(beyond_write_pos + 100, &buffer));
  CHECK_EQ(0, Logger::GetLogLines(10 * 1024 * 1024, buffer.start(), 1));
  CHECK_EQ(0, GetLogLines(10 * 1024 * 1024, &buffer));
  TearDown();
}


TEST(MemoryLoggingTurnedOff) {
  // Log to stdout
  i::FLAG_logfile = "-";
  i::FLAG_log = true;
  Logger::Setup();
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 0));
  CHECK_EQ(0, Logger::GetLogLines(0, NULL, 100));
  CHECK_EQ(0, Logger::GetLogLines(100, NULL, 100));
  Logger::TearDown();
}


static void CompileAndRunScript(const char *src) {
  v8::Script::Compile(v8::String::New(src))->Run();
}


namespace v8 {
namespace internal {

class LoggerTestHelper : public AllStatic {
 public:
  static bool IsSamplerActive() { return Logger::IsProfilerSamplerActive(); }
};

}  // namespace v8::internal
}  // namespace v8

using v8::internal::LoggerTestHelper;


// Under Linux, we need to check if signals were delivered to avoid false
// positives.  Under other platforms profiling is done via a high-priority
// thread, so this case never happen.
static bool was_sigprof_received = true;
#ifdef __linux__

struct sigaction old_sigprof_handler;
pthread_t our_thread;

static void SigProfSignalHandler(int signal, siginfo_t* info, void* context) {
  if (signal != SIGPROF || !pthread_equal(pthread_self(), our_thread)) return;
  was_sigprof_received = true;
  old_sigprof_handler.sa_sigaction(signal, info, context);
}

#endif  // __linux__


namespace {

class ScopedLoggerInitializer {
 public:
  explicit ScopedLoggerInitializer(bool prof_lazy)
      : saved_prof_lazy_(i::FLAG_prof_lazy),
        saved_prof_(i::FLAG_prof),
        saved_prof_auto_(i::FLAG_prof_auto),
        trick_to_run_init_flags_(init_flags_(prof_lazy)),
        need_to_set_up_logger_(i::V8::IsRunning()),
        scope_(),
        env_(v8::Context::New()) {
    if (need_to_set_up_logger_) Logger::Setup();
    env_->Enter();
  }

  ~ScopedLoggerInitializer() {
    env_->Exit();
    Logger::TearDown();
    i::FLAG_prof_lazy = saved_prof_lazy_;
    i::FLAG_prof = saved_prof_;
    i::FLAG_prof_auto = saved_prof_auto_;
  }

  v8::Handle<v8::Context>& env() { return env_; }

 private:
  static bool init_flags_(bool prof_lazy) {
    i::FLAG_prof = true;
    i::FLAG_prof_lazy = prof_lazy;
    i::FLAG_prof_auto = false;
    i::FLAG_logfile = "*";
    return prof_lazy;
  }

  const bool saved_prof_lazy_;
  const bool saved_prof_;
  const bool saved_prof_auto_;
  const bool trick_to_run_init_flags_;
  const bool need_to_set_up_logger_;
  v8::HandleScope scope_;
  v8::Handle<v8::Context> env_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLoggerInitializer);
};


class LogBufferMatcher {
 public:
  LogBufferMatcher() {
    // Skip all initially logged stuff.
    log_pos_ = GetLogLines(0, &buffer_);
  }

  int log_pos() { return log_pos_; }

  int GetNextChunk() {
    int chunk_size = GetLogLines(log_pos_, &buffer_);
    CHECK_GT(buffer_.length(), chunk_size);
    buffer_[chunk_size] = '\0';
    log_pos_ += chunk_size;
    return chunk_size;
  }

  const char* Find(const char* substr) {
    return strstr(buffer_.start(), substr);
  }

  const char* Find(const i::Vector<char>& substr) {
    return Find(substr.start());
  }

  bool IsInSequence(const char* s1, const char* s2) {
    const char* s1_pos = Find(s1);
    const char* s2_pos = Find(s2);
    CHECK_NE(NULL, s1_pos);
    CHECK_NE(NULL, s2_pos);
    return s1_pos < s2_pos;
  }

  void PrintBuffer() {
    puts(buffer_.start());
  }

 private:
  EmbeddedVector<char, 102400> buffer_;
  int log_pos_;
};

}  // namespace


static void CheckThatProfilerWorks(LogBufferMatcher* matcher) {
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 0);
  CHECK(LoggerTestHelper::IsSamplerActive());

  // Verify that the current map of compiled functions has been logged.
  CHECK_GT(matcher->GetNextChunk(), 0);
  const char* code_creation = "\ncode-creation,";  // eq. to /^code-creation,/
  CHECK_NE(NULL, matcher->Find(code_creation));

#ifdef __linux__
  // Intercept SIGPROF handler to make sure that the test process
  // had received it. Under load, system can defer it causing test failure.
  // It is important to execute this after 'ResumeProfiler'.
  our_thread = pthread_self();
  was_sigprof_received = false;
  struct sigaction sa;
  sa.sa_sigaction = SigProfSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  CHECK_EQ(0, sigaction(SIGPROF, &sa, &old_sigprof_handler));
#endif  // __linux__

  // Force compiler to generate new code by parametrizing source.
  EmbeddedVector<char, 100> script_src;
  i::OS::SNPrintF(script_src,
                  "function f%d(x) { return %d * x; }"
                  "for (var i = 0; i < 10000; ++i) { f%d(i); }",
                  matcher->log_pos(), matcher->log_pos(), matcher->log_pos());
  // Run code for 200 msecs to get some ticks.
  const double end_time = i::OS::TimeCurrentMillis() + 200;
  while (i::OS::TimeCurrentMillis() < end_time) {
    CompileAndRunScript(script_src.start());
    // Yield CPU to give Profiler thread a chance to process ticks.
    i::OS::Sleep(1);
  }

  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 0);
  CHECK(!LoggerTestHelper::IsSamplerActive());

  // Wait 50 msecs to allow Profiler thread to process the last
  // tick sample it has got.
  i::OS::Sleep(50);

  // Now we must have compiler and tick records.
  CHECK_GT(matcher->GetNextChunk(), 0);
  matcher->PrintBuffer();
  CHECK_NE(NULL, matcher->Find(code_creation));
  const char* tick = "\ntick,";
  const bool ticks_found = matcher->Find(tick) != NULL;
  CHECK_EQ(was_sigprof_received, ticks_found);
}


TEST(ProfLazyMode) {
  ScopedLoggerInitializer initialize_logger(true);

  // No sampling should happen prior to resuming profiler.
  CHECK(!LoggerTestHelper::IsSamplerActive());

  LogBufferMatcher matcher;
  // Nothing must be logged until profiling is resumed.
  CHECK_EQ(0, matcher.log_pos());

  CompileAndRunScript("var a = (function(x) { return x + 1; })(10);");

  // Nothing must be logged while profiling is suspended.
  CHECK_EQ(0, matcher.GetNextChunk());

  CheckThatProfilerWorks(&matcher);

  CompileAndRunScript("var a = (function(x) { return x + 1; })(10);");

  // No new data beyond last retrieved position.
  CHECK_EQ(0, matcher.GetNextChunk());

  // Check that profiling can be resumed again.
  CheckThatProfilerWorks(&matcher);
}


// Profiling multiple threads that use V8 is currently only available on Linux.
#ifdef __linux__

namespace {

class LoopingThread : public v8::internal::Thread {
 public:
  LoopingThread()
      : v8::internal::Thread(),
        semaphore_(v8::internal::OS::CreateSemaphore(0)),
        run_(true) {
  }

  virtual ~LoopingThread() { delete semaphore_; }

  void Run() {
    self_ = pthread_self();
    RunLoop();
  }

  void SendSigProf() { pthread_kill(self_, SIGPROF); }

  void Stop() { run_ = false; }

  bool WaitForRunning() { return semaphore_->Wait(1000000); }

 protected:
  bool IsRunning() { return run_; }

  virtual void RunLoop() = 0;

  void SetV8ThreadId() {
    v8_thread_id_ = v8::V8::GetCurrentThreadId();
  }

  void SignalRunning() { semaphore_->Signal(); }

 private:
  v8::internal::Semaphore* semaphore_;
  bool run_;
  pthread_t self_;
  int v8_thread_id_;
};


class LoopingJsThread : public LoopingThread {
 public:
  void RunLoop() {
    v8::Locker locker;
    CHECK(v8::internal::ThreadManager::HasId());
    SetV8ThreadId();
    while (IsRunning()) {
      v8::HandleScope scope;
      v8::Persistent<v8::Context> context = v8::Context::New();
      CHECK(!context.IsEmpty());
      {
        v8::Context::Scope context_scope(context);
        SignalRunning();
        CompileAndRunScript(
            "var j; for (var i=0; i<10000; ++i) { j = Math.sin(i); }");
      }
      context.Dispose();
      i::OS::Sleep(1);
    }
  }
};


class LoopingNonJsThread : public LoopingThread {
 public:
  void RunLoop() {
    v8::Locker locker;
    v8::Unlocker unlocker;
    // Now thread has V8's id, but will not run VM code.
    CHECK(v8::internal::ThreadManager::HasId());
    double i = 10;
    SignalRunning();
    while (IsRunning()) {
      i = sin(i);
      i::OS::Sleep(1);
    }
  }
};


class TestSampler : public v8::internal::Sampler {
 public:
  TestSampler()
      : Sampler(0, true),
        semaphore_(v8::internal::OS::CreateSemaphore(0)),
        was_sample_stack_called_(false) {
  }

  ~TestSampler() { delete semaphore_; }

  void SampleStack(v8::internal::TickSample*) {
    was_sample_stack_called_ = true;
  }

  void Tick(v8::internal::TickSample*) { semaphore_->Signal(); }

  bool WaitForTick() { return semaphore_->Wait(1000000); }

  void Reset() { was_sample_stack_called_ = false; }

  bool WasSampleStackCalled() { return was_sample_stack_called_; }

 private:
  v8::internal::Semaphore* semaphore_;
  bool was_sample_stack_called_;
};


}  // namespace

TEST(ProfMultipleThreads) {
  LoopingJsThread jsThread;
  jsThread.Start();
  LoopingNonJsThread nonJsThread;
  nonJsThread.Start();

  TestSampler sampler;
  sampler.Start();
  CHECK(!sampler.WasSampleStackCalled());
  jsThread.WaitForRunning();
  jsThread.SendSigProf();
  CHECK(sampler.WaitForTick());
  CHECK(sampler.WasSampleStackCalled());
  sampler.Reset();
  CHECK(!sampler.WasSampleStackCalled());
  nonJsThread.WaitForRunning();
  nonJsThread.SendSigProf();
  CHECK(sampler.WaitForTick());
  CHECK(!sampler.WasSampleStackCalled());
  sampler.Stop();

  jsThread.Stop();
  nonJsThread.Stop();
  jsThread.Join();
  nonJsThread.Join();
}

#endif  // __linux__


// Test for issue http://crbug.com/23768 in Chromium.
// Heap can contain scripts with already disposed external sources.
// We need to verify that LogCompiledFunctions doesn't crash on them.
namespace {

class SimpleExternalString : public v8::String::ExternalStringResource {
 public:
  explicit SimpleExternalString(const char* source)
      : utf_source_(StrLength(source)) {
    for (int i = 0; i < utf_source_.length(); ++i)
      utf_source_[i] = source[i];
  }
  virtual ~SimpleExternalString() {}
  virtual size_t length() const { return utf_source_.length(); }
  virtual const uint16_t* data() const { return utf_source_.start(); }
 private:
  i::ScopedVector<uint16_t> utf_source_;
};

}  // namespace

TEST(Issue23768) {
  v8::HandleScope scope;
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

  SimpleExternalString source_ext_str("(function ext() {})();");
  v8::Local<v8::String> source = v8::String::NewExternal(&source_ext_str);
  // Script needs to have a name in order to trigger InitLineEnds execution.
  v8::Handle<v8::String> origin = v8::String::New("issue-23768-test");
  v8::Handle<v8::Script> evil_script = v8::Script::Compile(source, origin);
  CHECK(!evil_script.IsEmpty());
  CHECK(!evil_script->Run().IsEmpty());
  i::Handle<i::ExternalTwoByteString> i_source(
      i::ExternalTwoByteString::cast(*v8::Utils::OpenHandle(*source)));
  // This situation can happen if source was an external string disposed
  // by its owner.
  i_source->set_resource(NULL);

  // Must not crash.
  i::Logger::LogCompiledFunctions();
}


static v8::Handle<v8::Value> ObjMethod1(const v8::Arguments& args) {
  return v8::Handle<v8::Value>();
}

TEST(LogCallbacks) {
  ScopedLoggerInitializer initialize_logger(false);
  LogBufferMatcher matcher;

  v8::Persistent<v8::FunctionTemplate> obj =
      v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New());
  obj->SetClassName(v8::String::New("Obj"));
  v8::Handle<v8::ObjectTemplate> proto = obj->PrototypeTemplate();
  v8::Local<v8::Signature> signature = v8::Signature::New(obj);
  proto->Set(v8::String::New("method1"),
             v8::FunctionTemplate::New(ObjMethod1,
                                       v8::Handle<v8::Value>(),
                                       signature),
             static_cast<v8::PropertyAttribute>(v8::DontDelete));

  initialize_logger.env()->Global()->Set(v8_str("Obj"), obj->GetFunction());
  CompileAndRunScript("Obj.prototype.method1.toString();");

  i::Logger::LogCompiledFunctions();
  CHECK_GT(matcher.GetNextChunk(), 0);

  const char* callback_rec = "code-creation,Callback,";
  char* pos = const_cast<char*>(matcher.Find(callback_rec));
  CHECK_NE(NULL, pos);
  pos += strlen(callback_rec);
  EmbeddedVector<char, 100> ref_data;
  i::OS::SNPrintF(ref_data,
                  "0x%" V8PRIxPTR ",1,\"method1\"", ObjMethod1);
  *(pos + strlen(ref_data.start())) = '\0';
  CHECK_EQ(ref_data.start(), pos);

  obj.Dispose();
}


static v8::Handle<v8::Value> Prop1Getter(v8::Local<v8::String> property,
                                         const v8::AccessorInfo& info) {
  return v8::Handle<v8::Value>();
}

static void Prop1Setter(v8::Local<v8::String> property,
                                         v8::Local<v8::Value> value,
                                         const v8::AccessorInfo& info) {
}

static v8::Handle<v8::Value> Prop2Getter(v8::Local<v8::String> property,
                                         const v8::AccessorInfo& info) {
  return v8::Handle<v8::Value>();
}

TEST(LogAccessorCallbacks) {
  ScopedLoggerInitializer initialize_logger(false);
  LogBufferMatcher matcher;

  v8::Persistent<v8::FunctionTemplate> obj =
      v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New());
  obj->SetClassName(v8::String::New("Obj"));
  v8::Handle<v8::ObjectTemplate> inst = obj->InstanceTemplate();
  inst->SetAccessor(v8::String::New("prop1"), Prop1Getter, Prop1Setter);
  inst->SetAccessor(v8::String::New("prop2"), Prop2Getter);

  i::Logger::LogAccessorCallbacks();
  CHECK_GT(matcher.GetNextChunk(), 0);
  matcher.PrintBuffer();

  EmbeddedVector<char, 100> prop1_getter_record;
  i::OS::SNPrintF(prop1_getter_record,
                  "code-creation,Callback,0x%" V8PRIxPTR ",1,\"get prop1\"",
                  Prop1Getter);
  CHECK_NE(NULL, matcher.Find(prop1_getter_record));
  EmbeddedVector<char, 100> prop1_setter_record;
  i::OS::SNPrintF(prop1_setter_record,
                  "code-creation,Callback,0x%" V8PRIxPTR ",1,\"set prop1\"",
                  Prop1Setter);
  CHECK_NE(NULL, matcher.Find(prop1_setter_record));
  EmbeddedVector<char, 100> prop2_getter_record;
  i::OS::SNPrintF(prop2_getter_record,
                  "code-creation,Callback,0x%" V8PRIxPTR ",1,\"get prop2\"",
                  Prop2Getter);
  CHECK_NE(NULL, matcher.Find(prop2_getter_record));

  obj.Dispose();
}


TEST(LogTags) {
  ScopedLoggerInitializer initialize_logger(false);
  LogBufferMatcher matcher;

  const char* open_tag = "open-tag,";
  const char* close_tag = "close-tag,";

  // Check compatibility with the old style behavior.
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 0);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 0);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  CHECK_EQ(NULL, matcher.Find(open_tag));
  CHECK_EQ(NULL, matcher.Find(close_tag));

  const char* open_tag1 = "open-tag,1\n";
  const char* close_tag1 = "close-tag,1\n";

  // Check non-nested tag case.
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  CHECK_GT(matcher.GetNextChunk(), 0);
  CHECK(matcher.IsInSequence(open_tag1, close_tag1));

  const char* open_tag2 = "open-tag,2\n";
  const char* close_tag2 = "close-tag,2\n";

  // Check nested tags case.
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 2);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 2);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  CHECK_GT(matcher.GetNextChunk(), 0);
  // open_tag1 < open_tag2 < close_tag2 < close_tag1
  CHECK(matcher.IsInSequence(open_tag1, open_tag2));
  CHECK(matcher.IsInSequence(open_tag2, close_tag2));
  CHECK(matcher.IsInSequence(close_tag2, close_tag1));

  // Check overlapped tags case.
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 2);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 2);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  CHECK_GT(matcher.GetNextChunk(), 0);
  // open_tag1 < open_tag2 < close_tag1 < close_tag2
  CHECK(matcher.IsInSequence(open_tag1, open_tag2));
  CHECK(matcher.IsInSequence(open_tag2, close_tag1));
  CHECK(matcher.IsInSequence(close_tag1, close_tag2));

  const char* open_tag3 = "open-tag,3\n";
  const char* close_tag3 = "close-tag,3\n";

  // Check pausing overflow case.
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 2);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 2);
  CHECK_EQ(v8::PROFILER_MODULE_CPU, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 3);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 3);
  CHECK_EQ(v8::PROFILER_MODULE_NONE, Logger::GetActiveProfilerModules());
  // Must be no tags, because logging must be disabled.
  CHECK_EQ(NULL, matcher.Find(open_tag3));
  CHECK_EQ(NULL, matcher.Find(close_tag3));
}


TEST(IsLoggingPreserved) {
  ScopedLoggerInitializer initialize_logger(false);

  CHECK(Logger::is_logging());
  Logger::ResumeProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK(Logger::is_logging());
  Logger::PauseProfiler(v8::PROFILER_MODULE_CPU, 1);
  CHECK(Logger::is_logging());

  CHECK(Logger::is_logging());
  Logger::ResumeProfiler(
      v8::PROFILER_MODULE_HEAP_STATS | v8::PROFILER_MODULE_JS_CONSTRUCTORS, 1);
  CHECK(Logger::is_logging());
  Logger::PauseProfiler(
      v8::PROFILER_MODULE_HEAP_STATS | v8::PROFILER_MODULE_JS_CONSTRUCTORS, 1);
  CHECK(Logger::is_logging());

  CHECK(Logger::is_logging());
  Logger::ResumeProfiler(
      v8::PROFILER_MODULE_CPU |
      v8::PROFILER_MODULE_HEAP_STATS | v8::PROFILER_MODULE_JS_CONSTRUCTORS, 1);
  CHECK(Logger::is_logging());
  Logger::PauseProfiler(
      v8::PROFILER_MODULE_CPU |
      v8::PROFILER_MODULE_HEAP_STATS | v8::PROFILER_MODULE_JS_CONSTRUCTORS, 1);
  CHECK(Logger::is_logging());
}


static inline bool IsStringEqualTo(const char* r, const char* s) {
  return strncmp(r, s, strlen(r)) == 0;
}


static bool Consume(const char* str, char** buf) {
  if (IsStringEqualTo(str, *buf)) {
    *buf += strlen(str);
    return true;
  }
  return false;
}


namespace {

// A code entity is a pointer to a position of code-creation event in buffer log
// offset to a point where entity size begins, i.e.: '255,"func"\n'. This makes
// comparing code entities pretty easy.
typedef char* CodeEntityInfo;

class Interval {
 public:
  Interval()
      : min_addr_(reinterpret_cast<Address>(-1)),
        max_addr_(reinterpret_cast<Address>(0)), next_(NULL) {}

  ~Interval() { delete next_; }

  size_t Length() {
    size_t result = max_addr_ - min_addr_ + 1;
    if (next_ != NULL) result += next_->Length();
    return result;
  }

  void CloneFrom(Interval* src) {
    while (src != NULL) {
      RegisterAddress(src->min_addr_);
      RegisterAddress(src->max_addr_);
      src = src->next_;
    }
  }

  bool Contains(Address addr) {
    if (min_addr_ <= addr && addr <= max_addr_) {
      return true;
    }
    if (next_ != NULL) {
      return next_->Contains(addr);
    } else {
      return false;
    }
  }

  size_t GetIndex(Address addr) {
    if (min_addr_ <= addr && addr <= max_addr_) {
      return addr - min_addr_;
    }
    CHECK_NE(NULL, next_);
    return (max_addr_ - min_addr_ + 1) + next_->GetIndex(addr);
  }

  Address GetMinAddr() {
    return next_ == NULL ? min_addr_ : i::Min(min_addr_, next_->GetMinAddr());
  }

  Address GetMaxAddr() {
    return next_ == NULL ? max_addr_ : i::Max(max_addr_, next_->GetMaxAddr());
  }

  void RegisterAddress(Address addr) {
    if (min_addr_ == reinterpret_cast<Address>(-1)
        || (size_t)(addr > min_addr_ ?
           addr - min_addr_ : min_addr_ - addr) < MAX_DELTA) {
      if (addr < min_addr_) min_addr_ = addr;
      if (addr > max_addr_) max_addr_ = addr;
    } else {
      if (next_ == NULL) next_ = new Interval();
      next_->RegisterAddress(addr);
    }
  }

  Address raw_min_addr() { return min_addr_; }

  Address raw_max_addr() { return max_addr_; }

  Interval* get_next() { return next_; }

 private:
  static const size_t MAX_DELTA = 0x100000;
  Address min_addr_;
  Address max_addr_;
  Interval* next_;
};


// A structure used to return log parsing results.
class ParseLogResult {
 public:
  ParseLogResult()
      : entities_map(NULL), entities(NULL),
        max_entities(0) {}

  ~ParseLogResult() {
    i::DeleteArray(entities_map);
    i::DeleteArray(entities);
  }

  void AllocateEntities() {
    // Make sure that the test doesn't operate on a bogus log.
    CHECK_GT(max_entities, 0);
    CHECK_GT(bounds.GetMinAddr(), 0);
    CHECK_GT(bounds.GetMaxAddr(), bounds.GetMinAddr());

    entities = i::NewArray<CodeEntityInfo>(max_entities);
    for (int i = 0; i < max_entities; ++i) {
      entities[i] = NULL;
    }
    const size_t map_length = bounds.Length();
    entities_map = i::NewArray<int>(static_cast<int>(map_length));
    for (size_t i = 0; i < map_length; ++i) {
      entities_map[i] = -1;
    }
  }

  bool HasIndexForAddress(Address addr) {
    return bounds.Contains(addr);
  }

  size_t GetIndexForAddress(Address addr) {
    CHECK(HasIndexForAddress(addr));
    return bounds.GetIndex(addr);
  }

  CodeEntityInfo GetEntity(Address addr) {
    if (HasIndexForAddress(addr)) {
      size_t idx = GetIndexForAddress(addr);
      int item = entities_map[idx];
      return item != -1 ? entities[item] : NULL;
    }
    return NULL;
  }

  void ParseAddress(char* start) {
    Address addr =
        reinterpret_cast<Address>(strtoul(start, NULL, 16));  // NOLINT
    bounds.RegisterAddress(addr);
  }

  Address ConsumeAddress(char** start) {
    char* end_ptr;
    Address addr =
        reinterpret_cast<Address>(strtoul(*start, &end_ptr, 16));  // NOLINT
    CHECK(HasIndexForAddress(addr));
    *start = end_ptr;
    return addr;
  }

  Interval bounds;
  // Memory map of entities start addresses.
  int* entities_map;
  // An array of code entities.
  CodeEntityInfo* entities;
  // Maximal entities count. Actual entities count can be lower,
  // empty entity slots are pointing to NULL.
  int max_entities;
};

}  // namespace


typedef void (*ParserBlock)(char* start, char* end, ParseLogResult* result);

static void ParserCycle(
    char* start, char* end, ParseLogResult* result,
    ParserBlock block_creation, ParserBlock block_delete,
    ParserBlock block_move) {

  const char* code_creation = "code-creation,";
  const char* code_delete = "code-delete,";
  const char* code_move = "code-move,";

  const char* lazy_compile = "LazyCompile,";
  const char* script = "Script,";
  const char* function = "Function,";

  while (start < end) {
    if (Consume(code_creation, &start)) {
      if (Consume(lazy_compile, &start)
          || Consume(script, &start)
          || Consume(function, &start)) {
        block_creation(start, end, result);
      }
    } else if (Consume(code_delete, &start)) {
      block_delete(start, end, result);
    } else if (Consume(code_move, &start)) {
      block_move(start, end, result);
    }
    while (start < end && *start != '\n') ++start;
    ++start;
  }
}


static void Pass1CodeCreation(char* start, char* end, ParseLogResult* result) {
  result->ParseAddress(start);
  ++result->max_entities;
}


static void Pass1CodeDelete(char* start, char* end, ParseLogResult* result) {
  result->ParseAddress(start);
}


static void Pass1CodeMove(char* start, char* end, ParseLogResult* result) {
  result->ParseAddress(start);
  // Skip old address.
  while (start < end && *start != ',') ++start;
  CHECK_GT(end, start);
  ++start;  // Skip ','.
  result->ParseAddress(start);
}


static void Pass2CodeCreation(char* start, char* end, ParseLogResult* result) {
  Address addr = result->ConsumeAddress(&start);
  CHECK_GT(end, start);
  ++start;  // Skip ','.

  size_t idx = result->GetIndexForAddress(addr);
  result->entities_map[idx] = -1;
  for (int i = 0; i < result->max_entities; ++i) {
    // Find an empty slot and fill it.
    if (result->entities[i] == NULL) {
      result->entities[i] = start;
      result->entities_map[idx] = i;
      break;
    }
  }
  // Make sure that a slot was found.
  CHECK_GE(result->entities_map[idx], 0);
}


static void Pass2CodeDelete(char* start, char* end, ParseLogResult* result) {
  Address addr = result->ConsumeAddress(&start);
  size_t idx = result->GetIndexForAddress(addr);
  // There can be code deletes that are not related to JS code.
  if (result->entities_map[idx] >= 0) {
    result->entities[result->entities_map[idx]] = NULL;
    result->entities_map[idx] = -1;
  }
}


static void Pass2CodeMove(char* start, char* end, ParseLogResult* result) {
  Address from_addr = result->ConsumeAddress(&start);
  CHECK_GT(end, start);
  ++start;  // Skip ','.
  Address to_addr = result->ConsumeAddress(&start);
  CHECK_GT(end, start);

  size_t from_idx = result->GetIndexForAddress(from_addr);
  size_t to_idx = result->GetIndexForAddress(to_addr);
  // There can be code moves that are not related to JS code.
  if (from_idx != to_idx && result->entities_map[from_idx] >= 0) {
    CHECK_EQ(-1, result->entities_map[to_idx]);
    result->entities_map[to_idx] = result->entities_map[from_idx];
    result->entities_map[from_idx] = -1;
  };
}


static void ParseLog(char* start, char* end, ParseLogResult* result) {
  // Pass 1: Calculate boundaries of addresses and entities count.
  ParserCycle(start, end, result,
              Pass1CodeCreation, Pass1CodeDelete, Pass1CodeMove);

  printf("min_addr: %p, max_addr: %p, entities: %d\n",
         result->bounds.GetMinAddr(), result->bounds.GetMaxAddr(),
         result->max_entities);

  result->AllocateEntities();

  // Pass 2: Fill in code entries data.
  ParserCycle(start, end, result,
              Pass2CodeCreation, Pass2CodeDelete, Pass2CodeMove);
}


static inline void PrintCodeEntityInfo(CodeEntityInfo entity) {
  const int max_len = 50;
  if (entity != NULL) {
    char* eol = strchr(entity, '\n');
    int len = static_cast<int>(eol - entity);
    len = len <= max_len ? len : max_len;
    printf("%-*.*s ", max_len, len, entity);
  } else {
    printf("%*s", max_len + 1, "");
  }
}


static void PrintCodeEntitiesInfo(
    bool is_equal, Address addr,
    CodeEntityInfo l_entity, CodeEntityInfo r_entity) {
  printf("%c %p ", is_equal ? ' ' : '*', addr);
  PrintCodeEntityInfo(l_entity);
  PrintCodeEntityInfo(r_entity);
  printf("\n");
}


static inline int StrChrLen(const char* s, char c) {
  return static_cast<int>(strchr(s, c) - s);
}


static bool AreFuncSizesEqual(CodeEntityInfo ref_s, CodeEntityInfo new_s) {
  int ref_len = StrChrLen(ref_s, ',');
  int new_len = StrChrLen(new_s, ',');
  return ref_len == new_len && strncmp(ref_s, new_s, ref_len) == 0;
}


static bool AreFuncNamesEqual(CodeEntityInfo ref_s, CodeEntityInfo new_s) {
  // Skip size.
  ref_s = strchr(ref_s, ',') + 1;
  new_s = strchr(new_s, ',') + 1;
  int ref_len = StrChrLen(ref_s, '\n');
  int new_len = StrChrLen(new_s, '\n');
  // If reference is anonymous (""), it's OK to have anything in new.
  if (ref_len == 2) return true;
  // A special case for ErrorPrototype. Haven't yet figured out why they
  // are different.
  const char* error_prototype = "\"ErrorPrototype";
  if (IsStringEqualTo(error_prototype, ref_s)
      && IsStringEqualTo(error_prototype, new_s)) {
    return true;
  }
  // Built-in objects have problems too.
  const char* built_ins[] = {
      "\"Boolean\"", "\"Function\"", "\"Number\"",
      "\"Object\"", "\"Script\"", "\"String\""
  };
  for (size_t i = 0; i < sizeof(built_ins) / sizeof(*built_ins); ++i) {
    if (IsStringEqualTo(built_ins[i], new_s)) {
      return true;
    }
  }
  return ref_len == new_len && strncmp(ref_s, new_s, ref_len) == 0;
}


static bool AreEntitiesEqual(CodeEntityInfo ref_e, CodeEntityInfo new_e) {
  if (ref_e == NULL && new_e != NULL) return true;
  if (ref_e != NULL && new_e != NULL) {
    return AreFuncSizesEqual(ref_e, new_e) && AreFuncNamesEqual(ref_e, new_e);
  }
  if (ref_e != NULL && new_e == NULL) {
    // args_count entities (argument adapters) are not found by heap traversal,
    // but they are not needed because they doesn't contain any code.
    ref_e = strchr(ref_e, ',') + 1;
    const char* args_count = "\"args_count:";
    return IsStringEqualTo(args_count, ref_e);
  }
  return false;
}


// Test that logging of code create / move / delete events
// is equivalent to traversal of a resulting heap.
TEST(EquivalenceOfLoggingAndTraversal) {
  // This test needs to be run on a "clean" V8 to ensure that snapshot log
  // is loaded. This is always true when running using tools/test.py because
  // it launches a new cctest instance for every test. To be sure that launching
  // cctest manually also works, please be sure that no tests below
  // are using V8.
  //
  // P.S. No, V8 can't be re-initialized after disposal, see include/v8.h.
  CHECK(!i::V8::IsRunning());

  i::FLAG_logfile = "*";
  i::FLAG_log = true;
  i::FLAG_log_code = true;

  // Make sure objects move.
  bool saved_always_compact = i::FLAG_always_compact;
  if (!i::FLAG_never_compact) {
    i::FLAG_always_compact = true;
  }

  v8::HandleScope scope;
  v8::Handle<v8::Value> global_object = v8::Handle<v8::Value>();
  v8::Handle<v8::Context> env = v8::Context::New(
      0, v8::Handle<v8::ObjectTemplate>(), global_object);
  env->Enter();

  // Compile and run a function that creates other functions.
  CompileAndRunScript(
      "(function f(obj) {\n"
      "  obj.test =\n"
      "    (function a(j) { return function b() { return j; } })(100);\n"
      "})(this);");
  i::Heap::CollectAllGarbage(false);

  EmbeddedVector<char, 204800> buffer;
  int log_size;
  ParseLogResult ref_result;

  // Retrieve the log.
  {
    // Make sure that no GCs occur prior to LogCompiledFunctions call.
    i::AssertNoAllocation no_alloc;

    log_size = GetLogLines(0, &buffer);
    CHECK_GT(log_size, 0);
    CHECK_GT(buffer.length(), log_size);

    // Fill a map of compiled code objects.
    ParseLog(buffer.start(), buffer.start() + log_size, &ref_result);
  }

  // Iterate heap to find compiled functions, will write to log.
  i::Logger::LogCompiledFunctions();
  char* new_log_start = buffer.start() + log_size;
  const int new_log_size = Logger::GetLogLines(
      log_size, new_log_start, buffer.length() - log_size);
  CHECK_GT(new_log_size, 0);
  CHECK_GT(buffer.length(), log_size + new_log_size);

  // Fill an equivalent map of compiled code objects.
  ParseLogResult new_result;
  ParseLog(new_log_start, new_log_start + new_log_size, &new_result);

  // Test their actual equivalence.
  Interval combined;
  combined.CloneFrom(&ref_result.bounds);
  combined.CloneFrom(&new_result.bounds);
  Interval* iter = &combined;
  bool results_equal = true;

  while (iter != NULL) {
    for (Address addr = iter->raw_min_addr();
         addr <= iter->raw_max_addr(); ++addr) {
      CodeEntityInfo ref_entity = ref_result.GetEntity(addr);
      CodeEntityInfo new_entity = new_result.GetEntity(addr);
      if (ref_entity != NULL || new_entity != NULL) {
        const bool equal = AreEntitiesEqual(ref_entity, new_entity);
        if (!equal) results_equal = false;
        PrintCodeEntitiesInfo(equal, addr, ref_entity, new_entity);
      }
    }
    iter = iter->get_next();
  }
  // Make sure that all log data is written prior crash due to CHECK failure.
  fflush(stdout);
  CHECK(results_equal);

  env->Exit();
  Logger::TearDown();
  i::FLAG_always_compact = saved_always_compact;
}

#endif  // ENABLE_LOGGING_AND_PROFILING
