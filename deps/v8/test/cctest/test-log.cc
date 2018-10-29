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

#include <unordered_set>
#include <vector>
#include "src/api-inl.h"
#include "src/builtins/builtins.h"
#include "src/log-utils.h"
#include "src/log.h"
#include "src/objects-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/snapshot/natives.h"
#include "src/v8.h"
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

static std::vector<std::string> Split(const std::string& s, char delimiter) {
  std::vector<std::string> result;
  std::string line;
  std::istringstream stream(s);
  while (std::getline(stream, line, delimiter)) {
    result.push_back(line);
  }
  return result;
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
  }

  v8::Local<v8::Context>& env() { return env_; }

  v8::Isolate* isolate() { return isolate_; }

  i::Isolate* i_isolate() { return reinterpret_cast<i::Isolate*>(isolate()); }

  Logger* logger() { return logger_; }

  v8::Local<v8::String> GetLogString() {
    int length = static_cast<int>(raw_log_.size());
    return v8::String::NewFromUtf8(isolate_, raw_log_.c_str(),
                                   v8::NewStringType::kNormal, length)
        .ToLocalChecked();
  }

  void StopLogging() {
    bool exists = false;
    raw_log_ = i::ReadFile(StopLoggingGetTempFile(), &exists, true);
    log_ = Split(raw_log_, '\n');
    CHECK(exists);
  }

  // Searches |log_| for a line which contains all the strings in |search_terms|
  // as substrings, starting from the index |start|, and returns the index of
  // the found line. Returns std::string::npos if no line is found.
  size_t IndexOfLine(const std::vector<std::string>& search_terms,
                     size_t start = 0) {
    for (size_t i = start; i < log_.size(); ++i) {
      const std::string& line = log_.at(i);
      bool all_terms_found = true;
      for (const std::string& term : search_terms) {
        all_terms_found &= line.find(term) != std::string::npos;
      }
      if (all_terms_found) return i;
    }
    return std::string::npos;
  }

  bool ContainsLine(const std::vector<std::string>& search_terms,
                    size_t start = 0) {
    return IndexOfLine(search_terms, start) != std::string::npos;
  }

  // Calls IndexOfLine for each set of substring terms in
  // |all_line_search_terms|, in order. Returns true if they're all found.
  bool ContainsLinesInOrder(
      const std::vector<std::vector<std::string>>& all_line_search_terms,
      size_t start = 0) {
    CHECK_GT(log_.size(), 0);
    for (auto& search_terms : all_line_search_terms) {
      start = IndexOfLine(search_terms, start);
      if (start == std::string::npos) return false;
      ++start;  // Skip the found line.
    }
    return true;
  }

  std::unordered_set<uintptr_t> ExtractAllAddresses(std::string search_term,
                                                    size_t address_column) {
    CHECK_GT(log_.size(), 0);
    std::unordered_set<uintptr_t> result;
    size_t start = 0;
    while (true) {
      start = IndexOfLine({search_term}, start);
      if (start == std::string::npos) break;
      std::vector<std::string> columns = Split(log_.at(start), ',');
      ++start;  // Skip the found line.
      // TODO(crbug.com/v8/8084): These two continue lines should really be
      // errors. But on Windows the log is sometimes mysteriously cut off at the
      // end. If the cut-off point happens to fall in the address field, the
      // conditions will be triggered.
      if (address_column >= columns.size()) continue;
      uintptr_t address =
          strtoll(columns.at(address_column).c_str(), nullptr, 16);
      if (address == 0) continue;
      result.insert(address);
    }
    return result;
  }

  void LogCodeObjects() { logger_->LogCodeObjects(); }
  void LogCompiledFunctions() { logger_->LogCompiledFunctions(); }

  void StringEvent(const char* name, const char* value) {
    logger_->StringEvent(name, value);
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

  std::string raw_log_;
  std::vector<std::string> log_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLoggerInitializer);
};

class TestCodeEventHandler : public v8::CodeEventHandler {
 public:
  explicit TestCodeEventHandler(v8::Isolate* isolate)
      : v8::CodeEventHandler(isolate), isolate_(isolate) {}

  size_t CountLines(std::string prefix, std::string suffix = std::string()) {
    if (event_log_.empty()) return 0;

    size_t match = 0;
    for (const std::string& line : event_log_) {
      size_t prefix_pos = line.find(prefix);
      if (prefix_pos == std::string::npos) continue;
      size_t suffix_pos = line.rfind(suffix);
      if (suffix_pos == std::string::npos) continue;
      if (suffix_pos != line.length() - suffix.length()) continue;
      if (prefix_pos >= suffix_pos) continue;
      match++;
    }

    return match;
  }

  void Handle(v8::CodeEvent* code_event) override {
    std::string log_line = "";
    log_line += v8::CodeEvent::GetCodeEventTypeName(code_event->GetCodeType());
    log_line += " ";
    log_line += FormatName(code_event);
    event_log_.push_back(log_line);
  }

 private:
  std::string FormatName(v8::CodeEvent* code_event) {
    std::string name = std::string(code_event->GetComment());
    if (name.empty()) {
      v8::Local<v8::String> functionName = code_event->GetFunctionName();
      std::string buffer(functionName->Utf8Length(isolate_) + 1, 0);
      functionName->WriteUtf8(isolate_, &buffer[0],
                              functionName->Utf8Length(isolate_) + 1);
      // Sanitize name, removing unwanted \0 resulted from WriteUtf8
      name = std::string(buffer.c_str());
    }

    return name;
  }

  std::vector<std::string> event_log_;
  v8::Isolate* isolate_;
};

}  // namespace

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
  ~SimpleExternalString() override = default;
  size_t length() const override { return utf_source_.length(); }
  const uint16_t* data() const override { return utf_source_.start(); }
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
      i::ExternalTwoByteString::cast(*v8::Utils::OpenHandle(*source)),
      CcTest::i_isolate());
  // This situation can happen if source was an external string disposed
  // by its owner.
  i_source->SetResource(CcTest::i_isolate(), nullptr);

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
    i::EmbeddedVector<char, 100> suffix_buffer;
    i::SNPrintF(suffix_buffer, ",0x%" V8PRIxPTR ",1,method1", ObjMethod1_entry);
    CHECK(logger.ContainsLine(
        {"code-creation,Callback,-2,", std::string(suffix_buffer.start())}));
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
                Prop1Getter_entry);
    CHECK(logger.ContainsLine({"code-creation,Callback,-2,",
                               std::string(prop1_getter_record.start())}));

    Address Prop1Setter_entry = reinterpret_cast<Address>(Prop1Setter);
#if USES_FUNCTION_DESCRIPTORS
    Prop1Setter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop1Setter_entry);
#endif
    EmbeddedVector<char, 100> prop1_setter_record;
    i::SNPrintF(prop1_setter_record, ",0x%" V8PRIxPTR ",1,set prop1",
                Prop1Setter_entry);
    CHECK(logger.ContainsLine({"code-creation,Callback,-2,",
                               std::string(prop1_setter_record.start())}));

    Address Prop2Getter_entry = reinterpret_cast<Address>(Prop2Getter);
#if USES_FUNCTION_DESCRIPTORS
    Prop2Getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop2Getter_entry);
#endif
    EmbeddedVector<char, 100> prop2_getter_record;
    i::SNPrintF(prop2_getter_record, ",0x%" V8PRIxPTR ",1,get prop2",
                Prop2Getter_entry);
    CHECK(logger.ContainsLine({"code-creation,Callback,-2,",
                               std::string(prop2_getter_record.start())}));
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
    CcTest::PreciseCollectAllGarbage();
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
      i::ScopedVector<char> data(s->Utf8Length(isolate) + 1);
      CHECK(data.start());
      s->WriteUtf8(isolate, data.start());
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

    i::EmbeddedVector<char, 100> line_buffer;
    i::SNPrintF(line_buffer, "%d,%d,%d,%d,%d", i::Version::GetMajor(),
                i::Version::GetMinor(), i::Version::GetBuild(),
                i::Version::GetPatch(), i::Version::IsCandidate());
    CHECK(
        logger.ContainsLine({"v8-version,", std::string(line_buffer.start())}));
  }
  isolate->Dispose();
}


// https://crbug.com/539892
// CodeCreateEvents with really large names should not crash.
TEST(Issue539892) {
  class FakeCodeEventLogger : public i::CodeEventLogger {
   public:
    explicit FakeCodeEventLogger(i::Isolate* isolate)
        : CodeEventLogger(isolate) {}

    void CodeMoveEvent(i::AbstractCode* from, i::AbstractCode* to) override {}
    void CodeDisableOptEvent(i::AbstractCode* code,
                             i::SharedFunctionInfo* shared) override {}

   private:
    void LogRecordedBuffer(i::AbstractCode* code, i::SharedFunctionInfo* shared,
                           const char* name, int length) override {}
    void LogRecordedBuffer(const i::wasm::WasmCode* code, const char* name,
                           int length) override {}
  } code_event_logger(CcTest::i_isolate());
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    logger.logger()->AddCodeEventListener(&code_event_logger);

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
  i::FLAG_turbo_inlining = false;
  i::FLAG_enable_one_shot_optimization = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    const char* source_text =
        "function testAddFn(a,b) { return a + b };"
        "let result;"
        "for (let i = 0; i < 100000; i++) { result = testAddFn(i, i); };"
        "testAddFn('1', 1);"
        "for (let i = 0; i < 100000; i++) { result = testAddFn('1', i); }";
    CompileRun(source_text);

    logger.StopLogging();

    // We should find at least one code-creation even for testAddFn();
    CHECK(logger.ContainsLine({"api,v8::Context::New"}));
    CHECK(logger.ContainsLine({"timer-event-start", "V8.CompileCode"}));
    CHECK(logger.ContainsLine({"timer-event-end", "V8.CompileCode"}));
    CHECK(logger.ContainsLine({"code-creation,Script", ":1:1"}));
    CHECK(logger.ContainsLine({"api,v8::Script::Run"}));
    CHECK(logger.ContainsLine({"code-creation,LazyCompile,", "testAddFn"}));
    if (i::FLAG_opt && !i::FLAG_always_opt) {
      CHECK(logger.ContainsLine({"code-deopt,", "not a Smi"}));
      if (i::FLAG_enable_one_shot_optimization)
        CHECK(logger.ContainsLine({"code-deopt,", "DeoptimizeNow"}));
      CHECK(logger.ContainsLine({"timer-event-start", "V8.DeoptimizeCode"}));
      CHECK(logger.ContainsLine({"timer-event-end", "V8.DeoptimizeCode"}));
    }
  }
  isolate->Dispose();
}

TEST(LogInterpretedFramesNativeStack) {
  SETUP_FLAGS();
  i::FLAG_interpreted_frames_native_stack = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    const char* source_text =
        "function testLogInterpretedFramesNativeStack(a,b) { return a + b };"
        "testLogInterpretedFramesNativeStack('1', 1);";
    CompileRun(source_text);

    logger.StopLogging();

    CHECK(logger.ContainsLine(
        {"InterpretedFunction", "testLogInterpretedFramesNativeStack"}));
  }
  isolate->Dispose();
}

TEST(ExternalCodeEventListener) {
  i::FLAG_log = false;
  i::FLAG_prof = false;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    v8::HandleScope scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    context->Enter();

    TestCodeEventHandler code_event_handler(isolate);

    const char* source_text_before_start =
        "function testCodeEventListenerBeforeStart(a,b) { return a + b };"
        "testCodeEventListenerBeforeStart('1', 1);";
    CompileRun(source_text_before_start);

    CHECK_EQ(code_event_handler.CountLines("LazyCompile",
                                           "testCodeEventListenerBeforeStart"),
             0);

    code_event_handler.Enable();

    CHECK_GE(code_event_handler.CountLines("LazyCompile",
                                           "testCodeEventListenerBeforeStart"),
             1);

    const char* source_text_after_start =
        "function testCodeEventListenerAfterStart(a,b) { return a + b };"
        "testCodeEventListenerAfterStart('1', 1);";
    CompileRun(source_text_after_start);

    CHECK_GE(code_event_handler.CountLines("LazyCompile",
                                           "testCodeEventListenerAfterStart"),
             1);

    context->Exit();
  }
  isolate->Dispose();
}

TEST(ExternalCodeEventListenerWithInterpretedFramesNativeStack) {
  i::FLAG_log = false;
  i::FLAG_prof = false;
  i::FLAG_interpreted_frames_native_stack = true;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    v8::HandleScope scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    context->Enter();

    TestCodeEventHandler code_event_handler(isolate);

    const char* source_text_before_start =
        "function testCodeEventListenerBeforeStart(a,b) { return a + b };"
        "testCodeEventListenerBeforeStart('1', 1);";
    CompileRun(source_text_before_start);

    CHECK_EQ(code_event_handler.CountLines("InterpretedFunction",
                                           "testCodeEventListenerBeforeStart"),
             0);

    code_event_handler.Enable();

    CHECK_GE(code_event_handler.CountLines("InterpretedFunction",
                                           "testCodeEventListenerBeforeStart"),
             1);

    const char* source_text_after_start =
        "function testCodeEventListenerAfterStart(a,b) { return a + b };"
        "testCodeEventListenerAfterStart('1', 1);";
    CompileRun(source_text_after_start);

    CHECK_GE(code_event_handler.CountLines("InterpretedFunction",
                                           "testCodeEventListenerAfterStart"),
             1);

    CHECK_EQ(
        code_event_handler.CountLines("Builtin", "InterpreterEntryTrampoline"),
        1);

    context->Exit();
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
    CHECK(logger.ContainsLine({"map,InitialMap", ",0x"}));
    CHECK(logger.ContainsLine({"map,Transition", ",0x"}));
    CHECK(logger.ContainsLine({"map-details", ",0x"}));
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
    std::unordered_set<uintptr_t> map_addresses =
        logger.ExtractAllAddresses("map-details", 2);
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
      i::Map::cast(obj)->Print();
      V8_Fatal(__FILE__, __LINE__,
               "Map (%p, #%zu) was not logged during startup with --trace-maps!"
               "\n# Expected Log Line: map_details, ... %p",
               reinterpret_cast<void*>(obj), i, reinterpret_cast<void*>(obj));
    }
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

    std::vector<std::vector<std::string>> lines = {
        {"timer-event-start,default,"},   {"timer-event-end,default,"},
        {"timer-event,default,"},         {"timer-event-start,timerEvent1,"},
        {"timer-event-end,timerEvent1,"}, {"timer-event,timerEvent2,"},
        {"timer-event,timerEvent3,"}};
    CHECK(logger.ContainsLinesInOrder(lines));
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

    // Run some warmup code to help ignoring existing log entries.
    CompileRun(
        "function warmUp(a) {"
        " let b = () => 1;"
        " return function(c) { return a+b+c; };"
        "};"
        "warmUp(1)(2);"
        "(function warmUpEndMarkerFunction(){})();");

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

    // Ignore all the log entries that happened before warmup
    size_t start = logger.IndexOfLine(
        {"function,first-execution", "warmUpEndMarkerFunction"});
    CHECK(start != std::string::npos);
    std::vector<std::vector<std::string>> lines = {
        // Create a new script
        {"script,create"},
        {"script-details"},
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
        {"function,parse-script,"},

        // Step 2: compiling top-level script and eager functions
        // - Compiling script without name.
        {"function,compile,"},
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
    CHECK(logger.ContainsLinesInOrder(lines, start));
  }
  i::FLAG_log_function_events = false;
  isolate->Dispose();
}

TEST(BuiltinsNotLoggedAsLazyCompile) {
  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    logger.LogCodeObjects();
    logger.LogCompiledFunctions();
    logger.StopLogging();

    i::Handle<i::Code> builtin = logger.i_isolate()->builtins()->builtin_handle(
        i::Builtins::kBooleanConstructor);
    i::EmbeddedVector<char, 100> buffer;

    // Should only be logged as "Builtin" with a name, never as "LazyCompile".
    i::SNPrintF(buffer, ",0x%" V8PRIxPTR ",%d,BooleanConstructor",
                builtin->InstructionStart(), builtin->InstructionSize());
    CHECK(logger.ContainsLine(
        {"code-creation,Builtin,3,", std::string(buffer.start())}));

    i::SNPrintF(buffer, ",0x%" V8PRIxPTR ",%d,", builtin->InstructionStart(),
                builtin->InstructionSize());
    CHECK(!logger.ContainsLine(
        {"code-creation,LazyCompile,3,", std::string(buffer.start())}));
  }
  isolate->Dispose();
}
