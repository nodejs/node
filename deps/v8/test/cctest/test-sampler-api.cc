// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests the sampling API in include/v8.h

#include <map>
#include <string>
#include "include/v8.h"
#include "src/flags/flags.h"
#include "test/cctest/cctest.h"

namespace {

class Sample {
 public:
  enum { kFramesLimit = 255 };

  Sample() = default;

  using const_iterator = const void* const*;
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return &data_[data_.length()]; }

  int size() const { return data_.length(); }
  v8::internal::Vector<void*>& data() { return data_; }

 private:
  v8::internal::EmbeddedVector<void*, kFramesLimit> data_;
};


class SamplingTestHelper {
 public:
  struct CodeEventEntry {
    std::string name;
    const void* code_start;
    size_t code_len;
  };
  using CodeEntries = std::map<const void*, CodeEventEntry>;

  explicit SamplingTestHelper(const std::string& test_function)
      : sample_is_taken_(false), isolate_(CcTest::isolate()) {
    CHECK(!instance_);
    instance_ = this;
    v8::HandleScope scope(isolate_);
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    global->Set(v8_str("CollectSample"),
                v8::FunctionTemplate::New(isolate_, CollectSample));
    LocalContext env(isolate_, nullptr, global);
    isolate_->SetJitCodeEventHandler(v8::kJitCodeEventDefault,
                                     JitCodeEventHandler);
    CompileRun(v8_str(test_function.c_str()));
  }

  ~SamplingTestHelper() {
    isolate_->SetJitCodeEventHandler(v8::kJitCodeEventDefault, nullptr);
    instance_ = nullptr;
  }

  Sample& sample() { return sample_; }

  const CodeEventEntry* FindEventEntry(const void* address) {
    CodeEntries::const_iterator it = code_entries_.upper_bound(address);
    if (it == code_entries_.begin()) return nullptr;
    const CodeEventEntry& entry = (--it)->second;
    const void* code_end =
        static_cast<const uint8_t*>(entry.code_start) + entry.code_len;
    return address < code_end ? &entry : nullptr;
  }

 private:
  static void CollectSample(const v8::FunctionCallbackInfo<v8::Value>& args) {
    instance_->DoCollectSample();
  }

  static void JitCodeEventHandler(const v8::JitCodeEvent* event) {
    instance_->DoJitCodeEventHandler(event);
  }

  // The JavaScript calls this function when on full stack depth.
  void DoCollectSample() {
    v8::RegisterState state;
#if defined(USE_SIMULATOR)
    SimulatorHelper simulator_helper;
    if (!simulator_helper.Init(isolate_)) return;
    simulator_helper.FillRegisters(&state);
#else
    state.pc = nullptr;
    state.fp = &state;
    state.sp = &state;
#endif
    v8::SampleInfo info;
    isolate_->GetStackSample(state, sample_.data().begin(),
                             static_cast<size_t>(sample_.size()), &info);
    size_t frames_count = info.frames_count;
    CHECK_LE(frames_count, static_cast<size_t>(sample_.size()));
    sample_.data().Truncate(static_cast<int>(frames_count));
    sample_is_taken_ = true;
  }

  void DoJitCodeEventHandler(const v8::JitCodeEvent* event) {
    if (sample_is_taken_) return;
    switch (event->type) {
      case v8::JitCodeEvent::CODE_ADDED: {
        CodeEventEntry entry;
        entry.name = std::string(event->name.str, event->name.len);
        entry.code_start = event->code_start;
        entry.code_len = event->code_len;
        code_entries_.insert(std::make_pair(entry.code_start, entry));
        break;
      }
      case v8::JitCodeEvent::CODE_MOVED: {
        CodeEntries::iterator it = code_entries_.find(event->code_start);
        CHECK(it != code_entries_.end());
        code_entries_.erase(it);
        CodeEventEntry entry;
        entry.name = std::string(event->name.str, event->name.len);
        entry.code_start = event->new_code_start;
        entry.code_len = event->code_len;
        code_entries_.insert(std::make_pair(entry.code_start, entry));
        break;
      }
      case v8::JitCodeEvent::CODE_REMOVED:
        code_entries_.erase(event->code_start);
        break;
      default:
        break;
    }
  }

  Sample sample_;
  bool sample_is_taken_;
  v8::Isolate* isolate_;
  CodeEntries code_entries_;

  static SamplingTestHelper* instance_;
};

SamplingTestHelper* SamplingTestHelper::instance_;

}  // namespace


// A JavaScript function which takes stack depth
// (minimum value 2) as an argument.
// When at the bottom of the recursion,
// the JavaScript code calls into C++ test code,
// waiting for the sampler to take a sample.
static const char* test_function =
    "function func(depth) {"
    "  if (depth == 2) CollectSample();"
    "  else return func(depth - 1);"
    "}";


TEST(StackDepthIsConsistent) {
  SamplingTestHelper helper(std::string(test_function) + "func(8);");
  CHECK_EQ(8, helper.sample().size());
}


TEST(StackDepthDoesNotExceedMaxValue) {
  SamplingTestHelper helper(std::string(test_function) + "func(300);");
  CHECK_EQ(Sample::kFramesLimit, helper.sample().size());
}


// The captured sample should have three pc values.
// They should fall in the range where the compiled code resides.
// The expected stack is:
// bottom of stack [{anon script}, outer, inner] top of stack
//                              ^      ^       ^
// sample.stack indices         2      1       0
TEST(StackFramesConsistent) {
  i::FLAG_allow_natives_syntax = true;
  const char* test_script =
      "function test_sampler_api_inner() {"
      "  CollectSample();"
      "  return 0;"
      "}"
      "function test_sampler_api_outer() {"
      "  return test_sampler_api_inner();"
      "}"
      "%NeverOptimizeFunction(test_sampler_api_inner);"
      "%NeverOptimizeFunction(test_sampler_api_outer);"
      "test_sampler_api_outer();";

  SamplingTestHelper helper(test_script);
  Sample& sample = helper.sample();
  CHECK_EQ(3, sample.size());

  const SamplingTestHelper::CodeEventEntry* entry;
  entry = helper.FindEventEntry(sample.begin()[0]);
  CHECK(entry);
  CHECK(std::string::npos != entry->name.find("test_sampler_api_inner"));

  entry = helper.FindEventEntry(sample.begin()[1]);
  CHECK(entry);
  CHECK(std::string::npos != entry->name.find("test_sampler_api_outer"));
}
