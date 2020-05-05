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
#include "src/api/api-inl.h"
#include "src/builtins/builtins.h"
#include "src/codegen/compilation-cache.h"
#include "src/execution/vm-state-inl.h"
#include "src/init/v8.h"
#include "src/logging/log-utils.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/utils/ostreams.h"
#include "src/utils/version.h"
#include "test/cctest/cctest.h"

using v8::internal::Address;
using v8::internal::EmbeddedVector;
using v8::internal::Logger;

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

  void PrintLog() {
    i::StdoutStream os;
    os << raw_log_ << std::flush;
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

  std::unordered_set<uintptr_t> ExtractLogAddresses(std::string search_term,
                                                    size_t address_column,
                                                    bool allow_duplicates) {
    CHECK_GT(log_.size(), 0);
    // Map addresses of Maps to log_lines.
    std::unordered_map<uintptr_t, std::string> map;
    size_t current = 0;
    while (true) {
      current = IndexOfLine({search_term}, current);
      if (current == std::string::npos) break;
      std::string current_line = log_.at(current);
      std::vector<std::string> columns = Split(current_line, ',');
      ++current;  // Skip the found line.
      // TODO(crbug.com/v8/8084): These two continue lines should really be
      // errors. But on Windows the log is sometimes mysteriously cut off at the
      // end. If the cut-off point happens to fall in the address field, the
      // conditions will be triggered.
      if (address_column >= columns.size()) continue;
      uintptr_t address =
          strtoull(columns.at(address_column).c_str(), nullptr, 16);
      if (address == 0) continue;
      if (!allow_duplicates) {
        auto match = map.find(address);
        // Ignore same address but different log line.
        if (match != map.end() && match->second.compare(current_line) == 0) {
          for (size_t i = 0; i < current; i++) {
            printf("%s\n", log_.at(i).c_str());
          }
          printf("%zu\n", current);
          FATAL("%s, ... %p apperead twice:\n    %s", search_term.c_str(),
                reinterpret_cast<void*>(address), current_line.c_str());
        }
      }
      map.insert({address, current_line});
    }
    // Extract all keys.
    std::unordered_set<uintptr_t> result;
    for (auto key_value : map) {
      result.insert(key_value.first);
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
      : utf_source_(i::OwnedVector<uint16_t>::Of(i::CStrVector(source))) {}
  ~SimpleExternalString() override = default;
  size_t length() const override { return utf_source_.size(); }
  const uint16_t* data() const override { return utf_source_.begin(); }

 private:
  i::OwnedVector<uint16_t> utf_source_;
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
      v8::String::NewFromUtf8Literal(CcTest::isolate(), "issue-23768-test");
  v8::Local<v8::Script> evil_script =
      CompileWithOrigin(source, origin, v8_bool(false));
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

UNINITIALIZED_TEST(LogCallbacks) {
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
        {"code-creation,Callback,-2,", std::string(suffix_buffer.begin())}));
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

UNINITIALIZED_TEST(LogAccessorCallbacks) {
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
                               std::string(prop1_getter_record.begin())}));

    Address Prop1Setter_entry = reinterpret_cast<Address>(Prop1Setter);
#if USES_FUNCTION_DESCRIPTORS
    Prop1Setter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop1Setter_entry);
#endif
    EmbeddedVector<char, 100> prop1_setter_record;
    i::SNPrintF(prop1_setter_record, ",0x%" V8PRIxPTR ",1,set prop1",
                Prop1Setter_entry);
    CHECK(logger.ContainsLine({"code-creation,Callback,-2,",
                               std::string(prop1_setter_record.begin())}));

    Address Prop2Getter_entry = reinterpret_cast<Address>(Prop2Getter);
#if USES_FUNCTION_DESCRIPTORS
    Prop2Getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(Prop2Getter_entry);
#endif
    EmbeddedVector<char, 100> prop2_getter_record;
    i::SNPrintF(prop2_getter_record, ",0x%" V8PRIxPTR ",1,get prop2",
                Prop2Getter_entry);
    CHECK(logger.ContainsLine({"code-creation,Callback,-2,",
                               std::string(prop2_getter_record.begin())}));
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(LogVersion) {
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
        logger.ContainsLine({"v8-version,", std::string(line_buffer.begin())}));
  }
  isolate->Dispose();
}

// https://crbug.com/539892
// CodeCreateEvents with really large names should not crash.
UNINITIALIZED_TEST(Issue539892) {
  class FakeCodeEventLogger : public i::CodeEventLogger {
   public:
    explicit FakeCodeEventLogger(i::Isolate* isolate)
        : CodeEventLogger(isolate) {}

    void CodeMoveEvent(i::AbstractCode from, i::AbstractCode to) override {}
    void CodeDisableOptEvent(i::Handle<i::AbstractCode> code,
                             i::Handle<i::SharedFunctionInfo> shared) override {
    }

   private:
    void LogRecordedBuffer(i::Handle<i::AbstractCode> code,
                           i::MaybeHandle<i::SharedFunctionInfo> maybe_shared,
                           const char* name, int length) override {}
    void LogRecordedBuffer(const i::wasm::WasmCode* code, const char* name,
                           int length) override {}
  };

  SETUP_FLAGS();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  FakeCodeEventLogger code_event_logger(reinterpret_cast<i::Isolate*>(isolate));

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

UNINITIALIZED_TEST(LogAll) {
  SETUP_FLAGS();
  i::FLAG_log_all = true;
  i::FLAG_turbo_inlining = false;
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

    const char* source_text = R"(
        function testAddFn(a,b) {
          return a + b
        };
        let result;

        // Warm up the ICs.
        %PrepareFunctionForOptimization(testAddFn);
        for (let i = 0; i < 100000; i++) {
          result = testAddFn(i, i);
        };

        // Enforce optimization.
        %OptimizeFunctionOnNextCall(testAddFn);
        result = testAddFn(1, 1);

        // Cause deopt.
        testAddFn('1', 1)
        for (let i = 0; i < 100000; i++) {
          result = testAddFn('1', i);
        }
      )";
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
      CHECK(logger.ContainsLine({"timer-event-start", "V8.DeoptimizeCode"}));
      CHECK(logger.ContainsLine({"timer-event-end", "V8.DeoptimizeCode"}));
    }
  }
  isolate->Dispose();
}

#if !defined(V8_TARGET_ARCH_ARM) && !defined(V8_TARGET_ARCH_S390X)
UNINITIALIZED_TEST(LogInterpretedFramesNativeStack) {
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

UNINITIALIZED_TEST(LogInterpretedFramesNativeStackWithSerialization) {
  SETUP_FLAGS();
  i::FLAG_interpreted_frames_native_stack = true;
  i::FLAG_always_opt = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::ScriptCompiler::CachedData* cache = nullptr;

  bool has_cache = cache != nullptr;
  // NOTE(mmarchini): Runs the test two times. The first time it will compile
  // our script and will create a code cache for it. The second time we'll
  // deserialize the cache and check if our function was logged correctly.
  // We disallow compilation on the second run to ensure we're loading from
  // cache.
  do {
    v8::Isolate* isolate = v8::Isolate::New(create_params);

    {
      ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);

      has_cache = cache != nullptr;
      v8::ScriptCompiler::CompileOptions options =
          has_cache ? v8::ScriptCompiler::kConsumeCodeCache
                    : v8::ScriptCompiler::kEagerCompile;

      v8::HandleScope scope(isolate);
      v8::Isolate::Scope isolate_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Local<v8::String> source = v8_str(
          "function eyecatcher() { return a * a; } return eyecatcher();");
      v8::Local<v8::String> arg_str = v8_str("a");
      v8::ScriptOrigin origin(v8_str("filename"));

      i::DisallowCompilation* no_compile_expected =
          has_cache ? new i::DisallowCompilation(
                          reinterpret_cast<i::Isolate*>(isolate))
                    : nullptr;

      v8::ScriptCompiler::Source script_source(source, origin, cache);
      v8::Local<v8::Function> fun =
          v8::ScriptCompiler::CompileFunctionInContext(
              context, &script_source, 1, &arg_str, 0, nullptr, options)
              .ToLocalChecked();
      if (has_cache) {
        logger.StopLogging();
        CHECK(logger.ContainsLine({"InterpretedFunction", "eyecatcher"}));
      }
      v8::Local<v8::Value> arg = v8_num(3);
      v8::Local<v8::Value> result =
          fun->Call(context, v8::Undefined(isolate), 1, &arg).ToLocalChecked();
      CHECK_EQ(9, result->Int32Value(context).FromJust());
      cache = v8::ScriptCompiler::CreateCodeCacheForFunction(fun);

      if (no_compile_expected != nullptr) delete no_compile_expected;
    }

    isolate->Dispose();
  } while (!has_cache);
  delete cache;
}
#endif  // !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_S390X

UNINITIALIZED_TEST(ExternalCodeEventListener) {
  i::FLAG_log = false;
  i::FLAG_prof = false;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    v8::HandleScope scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

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
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(ExternalCodeEventListenerInnerFunctions) {
  i::FLAG_log = false;
  i::FLAG_prof = false;

  v8::ScriptCompiler::CachedData* cache;
  static const char* source_cstring =
      "(function f1() { return (function f2() {}); })()";

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  {  // Test that we emit the correct code events from eagerly compiling.
    v8::HandleScope scope(isolate1);
    v8::Isolate::Scope isolate_scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    v8::Context::Scope context_scope(context);

    TestCodeEventHandler code_event_handler(isolate1);
    code_event_handler.Enable();

    v8::Local<v8::String> source_string = v8_str(source_cstring);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_string, origin);
    v8::Local<v8::UnboundScript> script =
        v8::ScriptCompiler::CompileUnboundScript(isolate1, &source)
            .ToLocalChecked();
    CHECK_EQ(code_event_handler.CountLines("Script", "f1"), 1);
    CHECK_EQ(code_event_handler.CountLines("Script", "f2"), 1);
    cache = v8::ScriptCompiler::CreateCodeCache(script);
  }
  isolate1->Dispose();

  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  {  // Test that we emit the correct code events from deserialization.
    v8::HandleScope scope(isolate2);
    v8::Isolate::Scope isolate_scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope context_scope(context);

    TestCodeEventHandler code_event_handler(isolate2);
    code_event_handler.Enable();

    v8::Local<v8::String> source_string = v8_str(source_cstring);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_string, origin, cache);
    {
      i::DisallowCompilation no_compile_expected(
          reinterpret_cast<i::Isolate*>(isolate2));
      v8::ScriptCompiler::CompileUnboundScript(
          isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
          .ToLocalChecked();
    }
    CHECK_EQ(code_event_handler.CountLines("Script", "f1"), 1);
    CHECK_EQ(code_event_handler.CountLines("Script", "f2"), 1);
  }
  isolate2->Dispose();
}

#if !defined(V8_TARGET_ARCH_ARM) && !defined(V8_TARGET_ARCH_S390X)
UNINITIALIZED_TEST(ExternalCodeEventListenerWithInterpretedFramesNativeStack) {
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
#endif  // !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_S390X

UNINITIALIZED_TEST(TraceMaps) {
  SETUP_FLAGS();
  i::FLAG_trace_maps = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    // Try to create many different kind of maps to make sure the logging won't
    // crash. More detailed tests are implemented separately.
    const char* source_text = R"(
      let a = {};
      for (let i = 0; i < 500; i++) {
        a['p'+i] = i
      };
      class Test {
        constructor(i) {
          this.a = 1;
          this['p'+i] = 1;
        }
      };
      let t = new Test();
      t.b = 1; t.c = 1; t.d = 3;
      for (let i = 0; i < 100; i++) {
        t = new Test(i)
      };
      t.b = {};
    )";
    CompileRunChecked(isolate, source_text);

    logger.StopLogging();

    // Mostly superficial checks.
    CHECK(logger.ContainsLine({"map,InitialMap", ",0x"}));
    CHECK(logger.ContainsLine({"map,Transition", ",0x"}));
    CHECK(logger.ContainsLine({"map-details", ",0x"}));
  }
  i::FLAG_trace_maps = false;
  isolate->Dispose();
}

namespace {
// Ensure that all Maps found on the heap have a single corresponding map-create
// and map-details entry in the v8.log.
void ValidateMapDetailsLogging(v8::Isolate* isolate,
                               ScopedLoggerInitializer* logger) {
  // map-create might have duplicates if a Map address is reused after a gc.
  std::unordered_set<uintptr_t> map_create_addresses =
      logger->ExtractLogAddresses("map-create", 2, true);
  std::unordered_set<uintptr_t> map_details_addresses =
      logger->ExtractLogAddresses("map-details", 2, false);

  // Iterate over all maps on the heap.
  i::Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
  i::HeapObjectIterator iterator(heap);
  i::DisallowHeapAllocation no_gc;
  size_t i = 0;
  for (i::HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (!obj.IsMap()) continue;
    i++;
    uintptr_t address = obj.ptr();
    if (map_create_addresses.find(address) == map_create_addresses.end()) {
      // logger->PrintLog();
      i::Map::cast(obj).Print();
      FATAL(
          "Map (%p, #%zu) creation not logged during startup with "
          "--trace-maps!"
          "\n# Expected Log Line: map-create, ... %p",
          reinterpret_cast<void*>(obj.ptr()), i,
          reinterpret_cast<void*>(obj.ptr()));
    } else if (map_details_addresses.find(address) ==
               map_details_addresses.end()) {
      // logger->PrintLog();
      i::Map::cast(obj).Print();
      FATAL(
          "Map (%p, #%zu) details not logged during startup with "
          "--trace-maps!"
          "\n# Expected Log Line: map-details, ... %p",
          reinterpret_cast<void*>(obj.ptr()), i,
          reinterpret_cast<void*>(obj.ptr()));
    }
  }
}

}  // namespace

UNINITIALIZED_TEST(LogMapsDetailsStartup) {
  // Reusing map addresses might cause these tests to fail.
  if (i::FLAG_gc_global || i::FLAG_stress_compaction ||
      i::FLAG_stress_incremental_marking) {
    return;
  }
  // Test that all Map details from Maps in the snapshot are logged properly.
  SETUP_FLAGS();
  i::FLAG_trace_maps = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    logger.StopLogging();
    ValidateMapDetailsLogging(isolate, &logger);
  }

  i::FLAG_log_function_events = false;
  isolate->Dispose();
}

UNINITIALIZED_TEST(LogMapsDetailsCode) {
  // Reusing map addresses might cause these tests to fail.
  if (i::FLAG_gc_global || i::FLAG_stress_compaction ||
      i::FLAG_stress_incremental_marking) {
    return;
  }
  SETUP_FLAGS();
  i::FLAG_retain_maps_for_n_gc = 0xFFFFFFF;
  i::FLAG_trace_maps = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  const char* source = R"(
    // Normal properties overflowing into dict-mode.
    let a = {};
    for (let i = 0; i < 500; i++) {
      a['p'+i] = i
    };
    // Constructor / initial maps
    function Constructor(dictElements=false) {
      this.a = 1;
      this.b = 2;
      this.c = 3;
      if (dictElements) {
        this[0xFFFFF] = 1;
      }
      this.d = 4;
      this.e = 5;
      this.f = 5;
    }
    // Keep objects and their maps alive to avoid reusing map addresses.
    let instances = [];
    let instance;
    for (let i =0; i < 500; i++) {
      instances.push(new Constructor());
    }
    // Map deprecation.
    for (let i =0; i < 500; i++) {
      instance = new Constructor();
      instance.d = 1.1;
      instances.push(instance);
    }
    for (let i =0; i < 500; i++) {
      instance = new Constructor();
      instance.b = 1.1;
      instances.push(instance);
    }
    for (let i =0; i < 500; i++) {
      instance = new Constructor();
      instance.c = Object;
      instances.push(instance);
    }
    // Create instance with dict-elements.
    instances.push(new Constructor(true));

    // Class
    class Test {
      constructor(i) {
        this.a = 1;
        this['p'+i] = 1;
      }
    };
    let t = new Test();
    t.b = 1; t.c = 1; t.d = 3;
    for (let i = 0; i < 100; i++) {
      t = new Test(i);
      instances.push(t);
    }
    t.b = {};

    // Anonymous classes
    function create(value) {
      return new class {
        constructor() {
          this.value = value;
        }
      }
    }
    for (let i = 0; i < 100; i++) {
      instances.push(create(i));
    };

    // Modifying some protoypes.
    Array.prototype.helper = () => 1;
    [1,2,3].helper();
  )";
  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    CompileRunChecked(isolate, source);
    logger.StopLogging();
    ValidateMapDetailsLogging(isolate, &logger);
  }

  i::FLAG_log_function_events = false;
  isolate->Dispose();
}

UNINITIALIZED_TEST(LogMapsDetailsContexts) {
  // Reusing map addresses might cause these tests to fail.
  if (i::FLAG_gc_global || i::FLAG_stress_compaction ||
      i::FLAG_stress_incremental_marking) {
    return;
  }
  // Test that all Map details from Maps in the snapshot are logged properly.
  SETUP_FLAGS();
  i::FLAG_trace_maps = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    ScopedLoggerInitializer logger(saved_log, saved_prof, isolate);
    // Use the default context.
    CompileRunChecked(isolate, "{a:1}");
    // Create additional contexts.
    v8::Local<v8::Context> env1 = v8::Context::New(isolate);
    env1->Enter();
    CompileRun(env1, "{b:1}").ToLocalChecked();

    v8::Local<v8::Context> env2 = v8::Context::New(isolate);
    env2->Enter();
    CompileRun(env2, "{c:1}").ToLocalChecked();
    env2->Exit();
    env1->Exit();

    logger.StopLogging();
    ValidateMapDetailsLogging(isolate, &logger);
  }

  i::FLAG_log_function_events = false;
  isolate->Dispose();
}

UNINITIALIZED_TEST(ConsoleTimeEvents) {
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

UNINITIALIZED_TEST(LogFunctionEvents) {
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

UNINITIALIZED_TEST(BuiltinsNotLoggedAsLazyCompile) {
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
        {"code-creation,Builtin,3,", std::string(buffer.begin())}));

    i::SNPrintF(buffer, ",0x%" V8PRIxPTR ",%d,", builtin->InstructionStart(),
                builtin->InstructionSize());
    CHECK(!logger.ContainsLine(
        {"code-creation,LazyCompile,3,", std::string(buffer.begin())}));
  }
  isolate->Dispose();
}
