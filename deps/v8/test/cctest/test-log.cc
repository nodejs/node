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


class ScopedLoggerInitializer {
 public:
  ScopedLoggerInitializer(bool saved_log, bool saved_prof, v8::Isolate* isolate)
      : saved_log_(saved_log),
        saved_prof_(saved_prof),
        temp_file_(NULL),
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
    if (temp_file_ != NULL) fclose(temp_file_);
    i::FLAG_prof = saved_prof_;
    i::FLAG_log = saved_log_;
  }

  v8::Local<v8::Context>& env() { return env_; }

  v8::Isolate* isolate() { return isolate_; }

  Logger* logger() { return logger_; }

  FILE* StopLoggingGetTempFile() {
    temp_file_ = logger_->TearDown();
    CHECK(temp_file_);
    fflush(temp_file_);
    rewind(temp_file_);
    return temp_file_;
  }

 private:
  const bool saved_log_;
  const bool saved_prof_;
  FILE* temp_file_;
  v8::Isolate* isolate_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope scope_;
  v8::Local<v8::Context> env_;
  Logger* logger_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLoggerInitializer);
};

}  // namespace


static const char* StrNStr(const char* s1, const char* s2, int n) {
  if (s1[n] == '\0') return strstr(s1, s2);
  i::ScopedVector<char> str(n + 1);
  i::StrNCpy(str, s1, static_cast<size_t>(n));
  str[n] = '\0';
  char* found = strstr(str.start(), s2);
  return found != NULL ? s1 + (found - str.start()) : NULL;
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
    CHECK(CcTest::i_isolate() != NULL);
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
    CHECK(CcTest::i_isolate() != NULL);
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
  TestSampler* sampler = NULL;
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
  i_source->set_resource(NULL);

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
    ScopedLoggerInitializer initialize_logger(saved_log, saved_prof, isolate);
    Logger* logger = initialize_logger.logger();

    v8::Local<v8::FunctionTemplate> obj = v8::Local<v8::FunctionTemplate>::New(
        isolate, v8::FunctionTemplate::New(isolate));
    obj->SetClassName(v8_str("Obj"));
    v8::Local<v8::ObjectTemplate> proto = obj->PrototypeTemplate();
    v8::Local<v8::Signature> signature = v8::Signature::New(isolate, obj);
    proto->Set(v8_str("method1"),
               v8::FunctionTemplate::New(isolate, ObjMethod1,
                                         v8::Local<v8::Value>(), signature),
               static_cast<v8::PropertyAttribute>(v8::DontDelete));

    initialize_logger.env()
        ->Global()
        ->Set(initialize_logger.env(), v8_str("Obj"),
              obj->GetFunction(initialize_logger.env()).ToLocalChecked())
        .FromJust();
    CompileRun("Obj.prototype.method1.toString();");

    logger->LogCompiledFunctions();

    bool exists = false;
    i::Vector<const char> log(
        i::ReadFile(initialize_logger.StopLoggingGetTempFile(), &exists, true));
    CHECK(exists);

    Address ObjMethod1_entry = reinterpret_cast<Address>(ObjMethod1);
#if USES_FUNCTION_DESCRIPTORS
    ObjMethod1_entry = *FUNCTION_ENTRYPOINT_ADDRESS(ObjMethod1_entry);
#endif
    i::EmbeddedVector<char, 100> ref_data;
    i::SNPrintF(ref_data, "code-creation,Callback,-2,%p,1,\"method1\"",
                static_cast<void*>(ObjMethod1_entry));

    CHECK(StrNStr(log.start(), ref_data.start(), log.length()));
    log.Dispose();
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
    ScopedLoggerInitializer initialize_logger(saved_log, saved_prof, isolate);
    Logger* logger = initialize_logger.logger();

    v8::Local<v8::FunctionTemplate> obj = v8::Local<v8::FunctionTemplate>::New(
        isolate, v8::FunctionTemplate::New(isolate));
    obj->SetClassName(v8_str("Obj"));
    v8::Local<v8::ObjectTemplate> inst = obj->InstanceTemplate();
    inst->SetAccessor(v8_str("prop1"), Prop1Getter, Prop1Setter);
    inst->SetAccessor(v8_str("prop2"), Prop2Getter);

    logger->LogAccessorCallbacks();

    bool exists = false;
    i::Vector<const char> log(
        i::ReadFile(initialize_logger.StopLoggingGetTempFile(), &exists, true));
    CHECK(exists);

    Address Prop1Getter_entry = reinterpret_cast<Address>(Prop1Getter);
#if USES_FUNCTION_DESCRIPTORS
    Prop1Getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop1Getter_entry);
#endif
    EmbeddedVector<char, 100> prop1_getter_record;
    i::SNPrintF(prop1_getter_record,
                "code-creation,Callback,-2,%p,1,\"get prop1\"",
                static_cast<void*>(Prop1Getter_entry));
    CHECK(StrNStr(log.start(), prop1_getter_record.start(), log.length()));

    Address Prop1Setter_entry = reinterpret_cast<Address>(Prop1Setter);
#if USES_FUNCTION_DESCRIPTORS
    Prop1Setter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop1Setter_entry);
#endif
    EmbeddedVector<char, 100> prop1_setter_record;
    i::SNPrintF(prop1_setter_record,
                "code-creation,Callback,-2,%p,1,\"set prop1\"",
                static_cast<void*>(Prop1Setter_entry));
    CHECK(StrNStr(log.start(), prop1_setter_record.start(), log.length()));

    Address Prop2Getter_entry = reinterpret_cast<Address>(Prop2Getter);
#if USES_FUNCTION_DESCRIPTORS
    Prop2Getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop2Getter_entry);
#endif
    EmbeddedVector<char, 100> prop2_getter_record;
    i::SNPrintF(prop2_getter_record,
                "code-creation,Callback,-2,%p,1,\"get prop2\"",
                static_cast<void*>(Prop2Getter_entry));
    CHECK(StrNStr(log.start(), prop2_getter_record.start(), log.length()));
    log.Dispose();
  }
  isolate->Dispose();
}


typedef i::NativesCollection<i::TEST> TestSources;


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
    ScopedLoggerInitializer initialize_logger(saved_log, saved_prof, isolate);
    Logger* logger = initialize_logger.logger();

    // Compile and run a function that creates other functions.
    CompileRun(
        "(function f(obj) {\n"
        "  obj.test =\n"
        "    (function a(j) { return function b() { return j; } })(100);\n"
        "})(this);");
    logger->StopProfiler();
    reinterpret_cast<i::Isolate*>(isolate)->heap()->CollectAllGarbage(
        i::Heap::kMakeHeapIterableMask, i::GarbageCollectionReason::kTesting);
    logger->StringEvent("test-logging-done", "");

    // Iterate heap to find compiled functions, will write to log.
    logger->LogCompiledFunctions();
    logger->StringEvent("test-traversal-done", "");

    bool exists = false;
    i::Vector<const char> log(
        i::ReadFile(initialize_logger.StopLoggingGetTempFile(), &exists, true));
    CHECK(exists);
    v8::Local<v8::String> log_str =
        v8::String::NewFromUtf8(isolate, log.start(),
                                v8::NewStringType::kNormal, log.length())
            .ToLocalChecked();
    initialize_logger.env()
        ->Global()
        ->Set(initialize_logger.env(), v8_str("_log"), log_str)
        .FromJust();

    i::Vector<const char> source = TestSources::GetScriptsSource();
    v8::Local<v8::String> source_str =
        v8::String::NewFromUtf8(isolate, source.start(),
                                v8::NewStringType::kNormal, source.length())
            .ToLocalChecked();
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Script> script = CompileWithOrigin(source_str, "");
    if (script.IsEmpty()) {
      v8::String::Utf8Value exception(try_catch.Exception());
      printf("compile: %s\n", *exception);
      CHECK(false);
    }
    v8::Local<v8::Value> result;
    if (!script->Run(initialize_logger.env()).ToLocal(&result)) {
      v8::String::Utf8Value exception(try_catch.Exception());
      printf("run: %s\n", *exception);
      CHECK(false);
    }
    // The result either be a "true" literal or problem description.
    if (!result->IsTrue()) {
      v8::Local<v8::String> s =
          result->ToString(initialize_logger.env()).ToLocalChecked();
      i::ScopedVector<char> data(s->Utf8Length() + 1);
      CHECK(data.start());
      s->WriteUtf8(data.start());
      printf("%s\n", data.start());
      // Make sure that our output is written prior crash due to CHECK failure.
      fflush(stdout);
      CHECK(false);
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
    ScopedLoggerInitializer initialize_logger(saved_log, saved_prof, isolate);
    bool exists = false;
    i::Vector<const char> log(
        i::ReadFile(initialize_logger.StopLoggingGetTempFile(), &exists, true));
    CHECK(exists);
    i::EmbeddedVector<char, 100> ref_data;
    i::SNPrintF(ref_data, "v8-version,%d,%d,%d,%d,%d", i::Version::GetMajor(),
                i::Version::GetMinor(), i::Version::GetBuild(),
                i::Version::GetPatch(), i::Version::IsCandidate());
    CHECK(StrNStr(log.start(), ref_data.start(), log.length()));
    log.Dispose();
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
    ScopedLoggerInitializer initialize_logger(saved_log, saved_prof, isolate);
    Logger* logger = initialize_logger.logger();
    logger->addCodeEventListener(&code_event_logger);

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
    logger->LogCompiledFunctions();
  }
  isolate->Dispose();
}
