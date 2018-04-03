// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Tests of logging functions from log.h

#ifdef __linux__
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <cmath>
#endif  // __linux__

#include <unordered_set>
#include "src/api.h"
#include "src/log-utils.h"
#include "src/log.h"
#include "src/objects-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/snapshot/natives.h"
#include "src/utils.h"
#include "src/v8.h"
#include "src/v8threads.h"
#include "src/version.h"
#include "src/vm-state-inl.h"
#include "test/cctest/cctest.h"

using v8::internal::Address;
using v8::internal::EmbeddedVector;
using v8::internal::Logger;
using v8::internal::StrLength;

namespace {


#define SETUP_FLAGS()                            \
  bool saved_log = i::FLAG_log;                  \
  bool saved_prof = i::FLAG_prof;                \
  i::FLAG_log = true;                            \
  i::FLAG_prof = true;                           \
  i::FLAG_logfile = i::Log::kLogToTemporaryFile; \
  i::FLAG_logfile_per_isolate = false

static const char* StrNStr(const char* s1, const char* s2, size_t n) {
  CHECK_EQ(s1[n], '\0');
  return strstr(s1, s2);
}

// Look for a log line which starts with {prefix} and ends with {suffix}.
static const char* FindLogLine(const char* start, const char* end,
                               const char* prefix,
                               const char* suffix = nullptr) {
  CHECK_LT(start, end);
  CHECK_EQ(end[0], '\0');
  size_t prefixLength = strlen(prefix);
  // Loop through the input until we find /{prefix}[^\n]+{suffix}/.
  while (start < end) {
    const char* prefixResult = strstr(start, prefix);
    if (!prefixResult) return NULL;
    if (suffix == nullptr) return prefixResult;
    const char* suffixResult =
        StrNStr(prefixResult, suffix, (end - prefixResult));
    if (!suffixResult) return NULL;
    // Check that there are no newlines in between the {prefix} and the {suffix}
    // results.
    const char* newlineResult =
        StrNStr(prefixResult, "\n", (end - prefixResult));
    if (!newlineResult) return prefixResult;
    if (newlineResult > suffixResult) return prefixResult;
    start = prefixResult + prefixLength;
  }
  return NULL;
}

class ScopedLoggerInitializer {
 public:
  ScopedLoggerInitializer(bool saved_log, bool saved_prof, v8::Isolate* isolate)
      : saved_log_(saved_log),
        saved_prof_(saved_prof),
        temp_file_(nullptr),
        isolate_(isolate),
        isolate_scope_(isolate),
        scope_(isolate),
        env_(v8::Context::New(isolate)),
        logger_(reinterpret_cast<i::Isolate*>(isolate)->logger()) {
    env_->Enter();
  }

  ~ScopedLoggerInitializer() {
    env_->Exit();
    logger_->TearDown();
    if (temp_file_ != nullptr) fclose(temp_file_);
    i::FLAG_prof = saved_prof_;
    i::FLAG_log = saved_log_;
    log_.Dispose();
  }

  v8::Local<v8::Context>& env() { return env_; }

  v8::Isolate* isolate() { return isolate_; }

  Logger* logger() { return logger_; }

  void PrintLog(int nofLines = 0) {
    if (nofLines <= 0) {
      printf("%s", log_.start());
      return;
    }
    // Try to print the last {nofLines} of the log.
    const char* start = log_.start();
    const char* current = log_.end();
    while (current > start && nofLines > 0) {
      current--;
      if (*current == '\n') nofLines--;
    }
    printf(
        "======================================================\n"
        "Last log lines:\n...%s\n"
        "======================================================\n",
        current);
  }

  v8::Local<v8::String> GetLogString() {
    return v8::String::NewFromUtf8(isolate_, log_.start(),
                                   v8::NewStringType::kNormal, log_.length())
        .ToLocalChecked();
  }

  void StopLogging() {
    bool exists = false;
    log_ = i::ReadFile(StopLoggingGetTempFile(), &exists, true);
    CHECK(exists);
  }

  const char* FindLine(const char* prefix, const char* suffix = nullptr,
                       const char* start = nullptr) {
    // Make sure that StopLogging() has been called before.
    CHECK(log_.size());
    if (start == nullptr) start = log_.start();
    const char* end = log_.start() + log_.length();
    return FindLogLine(start, end, prefix, suffix);
  }

  // Find all log lines specified by the {prefix, suffix} pairs and ensure they
  // occurr in the specified order.
  void FindLogLines(const char* pairs[][2], size_t limit,
                    const char* start = nullptr) {
    const char* prefix = pairs[0][0];
    const char* suffix = pairs[0][1];
    const char* last_position = FindLine(prefix, suffix, start);
    if (last_position == nullptr) {
      PrintLog(50);
      V8_Fatal(__FILE__, __LINE__, "Could not find log line: %s ... %s", prefix,
               suffix);
    }
    CHECK(last_position);
    for (size_t i = 1; i < limit; i++) {
      prefix = pairs[i][0];
      suffix = pairs[i][1];
      const char* position = FindLine(prefix, suffix, start);
      if (position == nullptr) {
        PrintLog(50);
        V8_Fatal(__FILE__, __LINE__, "Could not find log line: %s ... %s",
                 prefix, suffix);
      }
      // Check that all string positions are in order.
      if (position <= last_position) {
        PrintLog(50);
        V8_Fatal(__FILE__, __LINE__,
                 "Log statements not in expected order (prev=%p, current=%p): "
                 "%s ... %s",
                 reinterpret_cast<const void*>(last_position),
                 reinterpret_cast<const void*>(position), prefix, suffix);
      }
      last_position = position;
    }
  }

  void LogCompiledFunctions() { logger_->LogCompiledFunctions(); }

  void StringEvent(const char* name, const char* value) {
    logger_->StringEvent(name, value);
  }

  void ExtractAllAddresses(std::unordered_set<uintptr_t>* map,
                           const char* prefix, int field_index) {
    // Make sure that StopLogging() has been called before.
    CHECK(log_.size());
    const char* current = log_.start();
    while (current != nullptr) {
      current = FindLine(prefix, nullptr, current);
      if (current == nullptr) return;
      // Find token number {index}.
      const char* previous;
      for (int i = 0; i <= field_index; i++) {
        previous = current;
        current = strchr(current + 1, ',');
        if (current == nullptr) break;
        // Skip the comma.
        current++;
      }
      if (current == nullptr) break;
      uintptr_t address = strtoll(previous, nullptr, 16);
      CHECK_LT(0, address);
      map->insert(address);
    }
  }

 private:
  FILE* StopLoggingGetTempFile() {
    temp_file_ = logger_->TearDown();
    CHECK(temp_file_);
    fflush(temp_file_);
    rewind(temp_file_);
    return temp_file_;
  }

  const bool saved_log_;
  const bool saved_prof_;
  FILE* temp_file_;
  v8::Isolate* isolate_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope scope_;
  v8::Local<v8::Context> env_;
  Logger* logger_;
  i::Vector<const char> log_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLoggerInitializer);
};

}  // namespace

TEST(FindLogLine) {
  const char* string =
      "prefix1, stuff,   suffix1\n"
      "prefix2, stuff\n, suffix2\n"
      "prefix3suffix3\n"
      "prefix4 suffix4";
  const char* end = string + strlen(string);
  // Make sure the vector contains the terminating \0 character.
  CHECK(FindLogLine(string, end, "prefix1, stuff,   suffix1"));
  CHECK(FindLogLine(string, end, "prefix1, stuff"));
  CHECK(FindLogLine(string, end, "prefix1"));
  CHECK(FindLogLine(string, end, "prefix1", "suffix1"));
  CHECK(FindLogLine(string, end, "prefix1", "suffix1"));
  CHECK(!FindLogLine(string, end, "prefix2", "suffix2"));
  CHECK(!FindLogLine(string, end, "prefix1", "suffix2"));
  CHECK(!FindLogLine(string, end, "prefix1", "suffix3"));
  CHECK(FindLogLine(string, end, "prefix3", "suffix3"));
  CHECK(FindLogLine(string, end, "prefix4", "suffix4"));
  CHECK(!FindLogLine(string, end, "prefix4", "suffix4XXXXXXXXXXXX"));
  CHECK(
      !FindLogLine(string, end, "prefix4XXXXXXXXXXXXXXXXXXXXXXxxx", "suffix4"));
  CHECK(!FindLogLine(string, end, "suffix", "suffix5XXXXXXXXXXXXXXXXXXXX"));
}

// BUG(913). Need to implement support for profiling multiple VM threads.
#if 0

namespace {

class LoopingThread : public v8::internal::Thread {
 public:
  explicit LoopingThread(v8::internal::Isolate* isolate)
      : v8::internal::Thread(isolate),
        semaphore_(new v8::internal::Semaphore(0)),
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
  explicit LoopingJsThread(v8::internal::Isolate* isolate)
      : LoopingThread(isolate) { }
  void RunLoop() {
    v8::Locker locker;
    CHECK_NOT_NULL(CcTest::i_isolate());
    CHECK_GT(CcTest::i_isolate()->thread_manager()->CurrentId(), 0);
    SetV8ThreadId();
    while (IsRunning()) {
      v8::HandleScope scope;
      v8::Persistent<v8::Context> context = v8::Context::New();
      CHECK(!context.IsEmpty());
      {
        v8::Context::Scope context_scope(context);
        SignalRunning();
        CompileRun(
            "var j; for (var i=0; i<10000; ++i) { j = Math.sin(i); }");
      }
      context.Dispose();
      i::OS::Sleep(v8::base::TimeDelta::FromMilliseconds(1));
    }
  }
};


class LoopingNonJsThread : public LoopingThread {
 public:
  explicit LoopingNonJsThread(v8::internal::Isolate* isolate)
      : LoopingThread(isolate) { }
  void RunLoop() {
    v8::Locker locker;
    v8::Unlocker unlocker;
    // Now thread has V8's id, but will not run VM code.
    CHECK_NOT_NULL(CcTest::i_isolate());
    CHECK_GT(CcTest::i_isolate()->thread_manager()->CurrentId(), 0);
    double i = 10;
    SignalRunning();
    while (IsRunning()) {
      i = std::sin(i);
      i::OS::Sleep(v8::base::TimeDelta::FromMilliseconds(1));
    }
  }
};


class TestSampler : public v8::internal::Sampler {
 public:
  explicit TestSampler(v8::internal::Isolate* isolate)
      : Sampler(isolate, 0, true, true),
        semaphore_(new v8::internal::Semaphore(0)),
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
  TestSampler* sampler = nullptr;
  {
    v8::Locker locker;
    sampler = new TestSampler(CcTest::i_isolate());
    sampler->Start();
    CHECK(sampler->IsActive());
  }

  LoopingJsThread jsThread(CcTest::i_isolate());
  jsThread.Start();
  LoopingNonJsThread nonJsThread(CcTest::i_isolate());
  nonJsThread.Start();

  CHECK(!sampler->WasSampleStackCalled());
  jsThread.WaitForRunning();
  jsThread.SendSigProf();
  CHECK(sampler->WaitForTick());
  CHECK(sampler->WasSampleStackCalled());
  sampler->Reset();
  CHECK(!sampler->WasSampleStackCalled());
  nonJsThread.WaitForRunning();
  nonJsThread.SendSigProf();
  CHECK(!sampler->WaitForTick());
  CHECK(!sampler->WasSampleStackCalled());
  sampler->Stop();

  jsThread.Stop();
  nonJsThread.Stop();
  jsThread.Join();
  nonJsThread.Join();

  delete sampler;
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
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = v8::Context::New(CcTest::isolate());
  env->Enter();

  SimpleExternalString source_ext_str("(function ext() {})();");
  v8::Local<v8::String> source =
      v8::String::NewExternalTwoByte(CcTest::isolate(), &source_ext_str)
          .ToLocalChecked();
  // Script needs to have a name in order to trigger InitLineEnds execution.
  v8::Local<v8::String> origin =
      v8::String::NewFromUtf8(CcTest::isolate(), "issue-23768-test",
                              v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::Local<v8::Script> evil_script = CompileWithOrigin(source, origin);
  CHECK(!evil_script.IsEmpty());
  CHECK(!evil_script->Run(env).IsEmpty());
  i::Handle<i::ExternalTwoByteString> i_source(
      i::ExternalTwoByteString::cast(*v8::Utils::OpenHandle(*source)));
  // This situation can happen if source was an external string disposed
  // by its owner.
  i_source->set_resource(nullptr);

  // Must not crash.
  CcTest::i_isolate()->logger()->LogCompiledFunctions();
}


static void ObjMethod1(const v8::FunctionCallbackInfo<v8::Value>& args) {
}


TEST(LogCallbacks) {
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    v8::Local<v8::FunctionTemplate> obj = v8::Local<v8::FunctionTemplate>::New(
        isolate, v8::FunctionTemplate::New(isolate));
    obj->SetClassName(v8_str("Obj"));
    v8::Local<v8::ObjectTemplate> proto = obj->PrototypeTemplate();
    v8::Local<v8::Signature> signature = v8::Signature::New(isolate, obj);
    proto->Set(v8_str("method1"),
               v8::FunctionTemplate::New(isolate, ObjMethod1,
                                         v8::Local<v8::Value>(), signature),
               static_cast<v8::PropertyAttribute>(v8::DontDelete));

    logger.env()
        ->Global()
        ->Set(logger.env(), v8_str("Obj"),
              obj->GetFunction(logger.env()).ToLocalChecked())
        .FromJust();
    CompileRun("Obj.prototype.method1.toString();");

    logger.LogCompiledFunctions();

    logger.StopLogging();

    Address ObjMethod1_entry = reinterpret_cast<Address>(ObjMethod1);
#if USES_FUNCTION_DESCRIPTORS
    ObjMethod1_entry = *FUNCTION_ENTRYPOINT_ADDRESS(ObjMethod1_entry);
#endif
    i::EmbeddedVector<char, 100> ref_data;
    i::SNPrintF(ref_data, ",0x%" V8PRIxPTR ",1,method1",
                reinterpret_cast<intptr_t>(ObjMethod1_entry));
    CHECK(logger.FindLine("code-creation,Callback,-2,", ref_data.start()));
  }
  isolate->Dispose();
}


static void Prop1Getter(v8::Local<v8::String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
}

static void Prop1Setter(v8::Local<v8::String> property,
                        v8::Local<v8::Value> value,
                        const v8::PropertyCallbackInfo<void>& info) {
}

static void Prop2Getter(v8::Local<v8::String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
}


TEST(LogAccessorCallbacks) {
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    v8::Local<v8::FunctionTemplate> obj = v8::Local<v8::FunctionTemplate>::New(
        isolate, v8::FunctionTemplate::New(isolate));
    obj->SetClassName(v8_str("Obj"));
    v8::Local<v8::ObjectTemplate> inst = obj->InstanceTemplate();
    inst->SetAccessor(v8_str("prop1"), Prop1Getter, Prop1Setter);
    inst->SetAccessor(v8_str("prop2"), Prop2Getter);

    logger.logger()->LogAccessorCallbacks();

    logger.StopLogging();

    Address Prop1Getter_entry = reinterpret_cast<Address>(Prop1Getter);
#if USES_FUNCTION_DESCRIPTORS
    Prop1Getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop1Getter_entry);
#endif
    EmbeddedVector<char, 100> prop1_getter_record;
    i::SNPrintF(prop1_getter_record, ",0x%" V8PRIxPTR ",1,get prop1",
                reinterpret_cast<intptr_t>(Prop1Getter_entry));
    CHECK(logger.FindLine("code-creation,Callback,-2,",
                          prop1_getter_record.start()));

    Address Prop1Setter_entry = reinterpret_cast<Address>(Prop1Setter);
#if USES_FUNCTION_DESCRIPTORS
    Prop1Setter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop1Setter_entry);
#endif
    EmbeddedVector<char, 100> prop1_setter_record;
    i::SNPrintF(prop1_setter_record, ",0x%" V8PRIxPTR ",1,set prop1",
                reinterpret_cast<intptr_t>(Prop1Setter_entry));
    CHECK(logger.FindLine("code-creation,Callback,-2,",
                          prop1_setter_record.start()));

    Address Prop2Getter_entry = reinterpret_cast<Address>(Prop2Getter);
#if USES_FUNCTION_DESCRIPTORS
    Prop2Getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop2Getter_entry);
#endif
    EmbeddedVector<char, 100> prop2_getter_record;
    i::SNPrintF(prop2_getter_record, ",0x%" V8PRIxPTR ",1,get prop2",
                reinterpret_cast<intptr_t>(Prop2Getter_entry));
    CHECK(logger.FindLine("code-creation,Callback,-2,",
                          prop2_getter_record.start()));
  }
  isolate->Dispose();
}

// Test that logging of code create / move events is equivalent to traversal of
// a resulting heap.
TEST(EquivalenceOfLoggingAndTraversal) {
  // This test needs to be run on a "clean" V8 to ensure that snapshot log
  // is loaded. This is always true when running using tools/test.py because
  // it launches a new cctest instance for every test. To be sure that launching
  // cctest manually also works, please be sure that no tests below
  // are using V8.

  // Start with profiling to capture all code events from the beginning.
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    // Compile and run a function that creates other functions.
    CompileRun(
        "(function f(obj) {\n"
        "  obj.test =\n"
        "    (function a(j) { return function b() { return j; } })(100);\n"
        "})(this);");
    logger.logger()->StopProfiler();
    reinterpret_cast<i::Isolate*>(isolate)->heap()->CollectAllGarbage(
        i::Heap::kMakeHeapIterableMask, i::GarbageCollectionReason::kTesting);
    logger.StringEvent("test-logging-done", "");

    // Iterate heap to find compiled functions, will write to log.
    logger.LogCompiledFunctions();
    logger.StringEvent("test-traversal-done", "");

    logger.StopLogging();

    v8::Local<v8::String> log_str = logger.GetLogString();
    logger.env()
        ->Global()
        ->Set(logger.env(), v8_str("_log"), log_str)
        .FromJust();

    // Load the Test snapshot's sources, see log-eq-of-logging-and-traversal.js
    i::Vector<const char> source =
        i::NativesCollection<i::TEST>::GetScriptsSource();
    v8::Local<v8::String> source_str =
        v8::String::NewFromUtf8(isolate, source.start(),
                                v8::NewStringType::kNormal, source.length())
            .ToLocalChecked();
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Script> script = CompileWithOrigin(source_str, "");
    if (script.IsEmpty()) {
      v8::String::Utf8Value exception(isolate, try_catch.Exception());
      FATAL("compile: %s\n", *exception);
    }
    v8::Local<v8::Value> result;
    if (!script->Run(logger.env()).ToLocal(&result)) {
      v8::String::Utf8Value exception(isolate, try_catch.Exception());
      FATAL("run: %s\n", *exception);
    }
    // The result either be the "true" literal or problem description.
    if (!result->IsTrue()) {
      v8::Local<v8::String> s = result->ToString(logger.env()).ToLocalChecked();
      i::ScopedVector<char> data(s->Utf8Length() + 1);
      CHECK(data.start());
      s->WriteUtf8(data.start());
      FATAL("%s\n", data.start());
    }
  }
  isolate->Dispose();
}


TEST(LogVersion) {
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    logger.StopLogging();

    i::EmbeddedVector<char, 100> ref_data;
    i::SNPrintF(ref_data, "%d,%d,%d,%d,%d", i::Version::GetMajor(),
                i::Version::GetMinor(), i::Version::GetBuild(),
                i::Version::GetPatch(), i::Version::IsCandidate());
    CHECK(logger.FindLine("v8-version,", ref_data.start()));
  }
  isolate->Dispose();
}


// https://crbug.com/539892
// CodeCreateEvents with really large names should not crash.
TEST(Issue539892) {
  class : public i::CodeEventLogger {
   public:
    void CodeMoveEvent(i::AbstractCode* from, Address to) override {}
    void CodeDisableOptEvent(i::AbstractCode* code,
                             i::SharedFunctionInfo* shared) override {}

   private:
    void LogRecordedBuffer(i::AbstractCode* code, i::SharedFunctionInfo* shared,
                           const char* name, int length) override {}
  } code_event_logger;
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    logger.logger()->addCodeEventListener(&code_event_logger);

    // Function with a really large name.
    const char* source_text =
        "(function "
        "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac"
        "(){})();";

    CompileRun(source_text);

    // Must not crash.
    logger.LogCompiledFunctions();
  }
  isolate->Dispose();
}

TEST(LogAll) {
  SETUP_FLAGS();
  i::FLAG_log_all = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    // Function that will
    const char* source_text =
        "function testAddFn(a,b) { return a + b };"
        "let result;"
        "for (let i = 0; i < 100000; i++) { result = testAddFn(i, i); };"
        "testAddFn('1', 1);"
        "for (let i = 0; i < 100000; i++) { result = testAddFn('1', i); }";
    CompileRun(source_text);

    logger.StopLogging();

    // We should find at least one code-creation even for testAddFn();
    CHECK(logger.FindLine("api,v8::Context::New"));
    CHECK(logger.FindLine("timer-event-start", "V8.CompileCode"));
    CHECK(logger.FindLine("timer-event-end", "V8.CompileCode"));
    CHECK(logger.FindLine("code-creation,Script", ":1:1"));
    CHECK(logger.FindLine("api,v8::Script::Run"));
    CHECK(logger.FindLine("code-creation,LazyCompile,", "testAddFn"));
    if (i::FLAG_opt && !i::FLAG_always_opt) {
      CHECK(logger.FindLine("code-deopt,", "soft"));
      CHECK(logger.FindLine("timer-event-start", "V8.DeoptimizeCode"));
      CHECK(logger.FindLine("timer-event-end", "V8.DeoptimizeCode"));
    }
  }
  isolate->Dispose();
}

TEST(TraceMaps) {
  SETUP_FLAGS();
  i::FLAG_trace_maps = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    // Try to create many different kind of maps to make sure the logging won't
    // crash. More detailed tests are implemented separately.
    const char* source_text =
        "let a = {};"
        "for (let i = 0; i < 500; i++) { a['p'+i] = i };"
        "class Test { constructor(i) { this.a = 1; this['p'+i] = 1; }};"
        "let t = new Test();"
        "t.b = 1; t.c = 1; t.d = 3;"
        "for (let i = 0; i < 100; i++) { t = new Test(i) };"
        "t.b = {};";
    CompileRun(source_text);

    logger.StopLogging();

    // Mostly superficial checks.
    CHECK(logger.FindLine("map,InitialMap", ",0x"));
    CHECK(logger.FindLine("map,Transition", ",0x"));
    CHECK(logger.FindLine("map-details", ",0x"));
  }
  i::FLAG_trace_maps = false;
  isolate->Dispose();
}

TEST(LogMaps) {
  // Test that all Map details from Maps in the snapshot are logged properly.
  SETUP_FLAGS();
  i::FLAG_trace_maps = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    logger.StopLogging();
    // Extract all the map-detail entry addresses from the log.
    std::unordered_set<uintptr_t> map_addresses;
    logger.ExtractAllAddresses(&map_addresses, "map-details", 2);
    i::Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
    i::HeapIterator iterator(heap);
    i::DisallowHeapAllocation no_gc;

    // Iterate over all maps on the heap.
    size_t i = 0;
    for (i::HeapObject* obj = iterator.next(); obj != nullptr;
         obj = iterator.next()) {
      i++;
      if (!obj->IsMap()) continue;
      uintptr_t address = reinterpret_cast<uintptr_t>(obj);
      if (map_addresses.find(address) != map_addresses.end()) continue;
      logger.PrintLog(200);
      i::Map::cast(obj)->Print();
      V8_Fatal(__FILE__, __LINE__,
               "Map (%p, #%zu) was not logged during startup with --trace-maps!"
               "\n# Expected Log Line: map_details, ... %p"
               "\n# Use logger::PrintLog() for more details.",
               reinterpret_cast<void*>(obj), i, reinterpret_cast<void*>(obj));
    }
    logger.PrintLog(200);
  }
  i::FLAG_log_function_events = false;
  isolate->Dispose();
}

TEST(ConsoleTimeEvents) {
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    // Test that console time events are properly logged
    const char* source_text =
        "console.time();"
        "console.timeEnd();"
        "console.timeStamp();"
        "console.time('timerEvent1');"
        "console.timeEnd('timerEvent1');"
        "console.timeStamp('timerEvent2');"
        "console.timeStamp('timerEvent3');";
    CompileRun(source_text);

    logger.StopLogging();

    const char* pairs[][2] = {{"timer-event-start,default,", nullptr},
                              {"timer-event-end,default,", nullptr},
                              {"timer-event,default,", nullptr},
                              {"timer-event-start,timerEvent1,", nullptr},
                              {"timer-event-end,timerEvent1,", nullptr},
                              {"timer-event,timerEvent2,", nullptr},
                              {"timer-event,timerEvent3,", nullptr}};
    logger.FindLogLines(pairs, arraysize(pairs));
  }

  isolate->Dispose();
}

TEST(LogFunctionEvents) {
  // Always opt and stress opt will break the fine-grained log order.
  if (i::FLAG_always_opt) return;

  SETUP_FLAGS();
  i::FLAG_log_function_events = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    const char* source_text =
        "function lazyNotExecutedFunction() { return 'lazy' };"
        "function lazyFunction() { "
        "  function lazyInnerFunction() { return 'lazy' };"
        "  return lazyInnerFunction;"
        "};"
        "let innerFn = lazyFunction();"
        "innerFn();"
        "(function eagerFunction(){ return 'eager' })();"
        "function Foo() { this.foo = function(){}; };"
        "let i = new Foo(); i.foo();";
    CompileRun(source_text);

    logger.StopLogging();

    // TODO(cbruni): Extend with first-execution log statements.
    CHECK_NULL(
        logger.FindLine("function,compile-lazy,", ",lazyNotExecutedFunction"));
    // Only consider the log starting from the first preparse statement on.
    const char* start =
        logger.FindLine("function,preparse-", ",lazyNotExecutedFunction");
    const char* pairs[][2] = {
        // Step 1: parsing top-level script, preparsing functions
        {"function,preparse-", ",lazyNotExecutedFunction"},
        // Missing name for preparsing lazyInnerFunction
        // {"function,preparse-", nullptr},
        {"function,preparse-", ",lazyFunction"},
        {"function,full-parse,", ",eagerFunction"},
        {"function,preparse-", ",Foo"},
        // Missing name for inner preparsing of Foo.foo
        // {"function,preparse-", nullptr},
        // Missing name for top-level script.
        {"function,parse-script,", nullptr},

        // Step 2: compiling top-level script and eager functions
        // - Compiling script without name.
        {"function,compile,,", nullptr},
        {"function,compile,", ",eagerFunction"},

        // Step 3: start executing script
        // Step 4. - lazy parse, lazy compiling and execute skipped functions
        //         - execute eager functions.
        {"function,parse-function,", ",lazyFunction"},
        {"function,compile-lazy,", ",lazyFunction"},
        {"function,first-execution,", ",lazyFunction"},

        {"function,parse-function,", ",lazyInnerFunction"},
        {"function,compile-lazy,", ",lazyInnerFunction"},
        {"function,first-execution,", ",lazyInnerFunction"},

        {"function,first-execution,", ",eagerFunction"},

        {"function,parse-function,", ",Foo"},
        {"function,compile-lazy,", ",Foo"},
        {"function,first-execution,", ",Foo"},

        {"function,parse-function,", ",Foo.foo"},
        {"function,compile-lazy,", ",Foo.foo"},
        {"function,first-execution,", ",Foo.foo"},
    };
    logger.FindLogLines(pairs, arraysize(pairs), start);
  }
  i::FLAG_log_function_events = false;
  isolate->Dispose();
}
