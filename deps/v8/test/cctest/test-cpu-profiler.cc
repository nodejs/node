// Copyright 2010 the V8 project authors. All rights reserved.
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
// Tests of profiles generator and utilities.

#include <limits>
#include <memory>

#include "src/init/v8.h"

#include "include/v8-profiler.h"
#include "src/api/api-inl.h"
#include "src/base/platform/platform.h"
#include "src/codegen/source-position-table.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/libplatform/default-platform.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/cpu-profiler-inl.h"
#include "src/profiler/profiler-listener.h"
#include "src/profiler/tracing-cpu-profiler.h"
#include "src/utils/utils.h"
#include "test/cctest/cctest.h"
#include "test/cctest/profiler-extension.h"

#include "include/libplatform/v8-tracing.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {
namespace test_cpu_profiler {

// Helper methods
static v8::Local<v8::Function> GetFunction(v8::Local<v8::Context> env,
                                           const char* name) {
  return v8::Local<v8::Function>::Cast(
      env->Global()->Get(env, v8_str(name)).ToLocalChecked());
}

static size_t offset(const char* src, const char* substring) {
  const char* it = strstr(src, substring);
  CHECK(it);
  return static_cast<size_t>(it - src);
}

template <typename A, typename B>
static int dist(A a, B b) {
  return abs(static_cast<int>(a) - static_cast<int>(b));
}

static const char* reason(const i::DeoptimizeReason reason) {
  return i::DeoptimizeReasonToString(reason);
}

TEST(StartStop) {
  i::Isolate* isolate = CcTest::i_isolate();
  CpuProfilesCollection profiles(isolate);
  ProfileGenerator generator(&profiles);
  std::unique_ptr<ProfilerEventsProcessor> processor(
      new SamplingEventsProcessor(isolate, &generator,
                                  v8::base::TimeDelta::FromMicroseconds(100),
                                  true));
  processor->Start();
  processor->StopSynchronously();
}

static void EnqueueTickSampleEvent(ProfilerEventsProcessor* proc,
                                   i::Address frame1,
                                   i::Address frame2 = kNullAddress,
                                   i::Address frame3 = kNullAddress) {
  v8::internal::TickSample sample;
  sample.pc = reinterpret_cast<void*>(frame1);
  sample.tos = reinterpret_cast<void*>(frame1);
  sample.frames_count = 0;
  if (frame2 != kNullAddress) {
    sample.stack[0] = reinterpret_cast<void*>(frame2);
    sample.frames_count = 1;
  }
  if (frame3 != kNullAddress) {
    sample.stack[1] = reinterpret_cast<void*>(frame3);
    sample.frames_count = 2;
  }
  sample.timestamp = base::TimeTicks::HighResolutionNow();
  proc->AddSample(sample);
}

namespace {

class TestSetup {
 public:
  TestSetup()
      : old_flag_prof_browser_mode_(i::FLAG_prof_browser_mode) {
    i::FLAG_prof_browser_mode = false;
  }

  ~TestSetup() {
    i::FLAG_prof_browser_mode = old_flag_prof_browser_mode_;
  }

 private:
  bool old_flag_prof_browser_mode_;
};

}  // namespace

i::AbstractCode CreateCode(LocalContext* env) {
  static int counter = 0;
  i::EmbeddedVector<char, 256> script;
  i::EmbeddedVector<char, 32> name;

  i::SNPrintF(name, "function_%d", ++counter);
  const char* name_start = name.begin();
  i::SNPrintF(script,
      "function %s() {\n"
           "var counter = 0;\n"
           "for (var i = 0; i < %d; ++i) counter += i;\n"
           "return '%s_' + counter;\n"
       "}\n"
       "%s();\n", name_start, counter, name_start, name_start);
  CompileRun(script.begin());

  i::Handle<i::JSFunction> fun = i::Handle<i::JSFunction>::cast(
      v8::Utils::OpenHandle(*GetFunction(env->local(), name_start)));
  return fun->abstract_code();
}

TEST(CodeEvents) {
  CcTest::InitializeVM();
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  TestSetup test_setup;

  i::HandleScope scope(isolate);

  i::AbstractCode aaa_code = CreateCode(&env);
  i::AbstractCode comment_code = CreateCode(&env);
  i::AbstractCode comment2_code = CreateCode(&env);
  i::AbstractCode moved_code = CreateCode(&env);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      isolate, generator, v8::base::TimeDelta::FromMicroseconds(100), true);
  processor->Start();
  ProfilerListener profiler_listener(isolate, processor);
  isolate->logger()->AddCodeEventListener(&profiler_listener);

  // Enqueue code creation events.
  const char* aaa_str = "aaa";
  i::Handle<i::String> aaa_name = factory->NewStringFromAsciiChecked(aaa_str);
  profiler_listener.CodeCreateEvent(i::Logger::FUNCTION_TAG, aaa_code,
                                    *aaa_name);
  profiler_listener.CodeCreateEvent(i::Logger::BUILTIN_TAG, comment_code,
                                    "comment");
  profiler_listener.CodeCreateEvent(i::Logger::BUILTIN_TAG, comment2_code,
                                    "comment2");
  profiler_listener.CodeMoveEvent(comment2_code, moved_code);

  // Enqueue a tick event to enable code events processing.
  EnqueueTickSampleEvent(processor, aaa_code.InstructionStart());

  isolate->logger()->RemoveCodeEventListener(&profiler_listener);
  processor->StopSynchronously();

  // Check the state of profile generator.
  CodeEntry* aaa =
      generator->code_map()->FindEntry(aaa_code.InstructionStart());
  CHECK(aaa);
  CHECK_EQ(0, strcmp(aaa_str, aaa->name()));

  CodeEntry* comment =
      generator->code_map()->FindEntry(comment_code.InstructionStart());
  CHECK(comment);
  CHECK_EQ(0, strcmp("comment", comment->name()));

  CHECK(!generator->code_map()->FindEntry(comment2_code.InstructionStart()));

  CodeEntry* comment2 =
      generator->code_map()->FindEntry(moved_code.InstructionStart());
  CHECK(comment2);
  CHECK_EQ(0, strcmp("comment2", comment2->name()));
}

template<typename T>
static int CompareProfileNodes(const T* p1, const T* p2) {
  return strcmp((*p1)->entry()->name(), (*p2)->entry()->name());
}

TEST(TickEvents) {
  TestSetup test_setup;
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  i::AbstractCode frame1_code = CreateCode(&env);
  i::AbstractCode frame2_code = CreateCode(&env);
  i::AbstractCode frame3_code = CreateCode(&env);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      CcTest::i_isolate(), generator,
      v8::base::TimeDelta::FromMicroseconds(100), true);
  CpuProfiler profiler(isolate, kDebugNaming, profiles, generator, processor);
  profiles->StartProfiling("");
  processor->Start();
  ProfilerListener profiler_listener(isolate, processor);
  isolate->logger()->AddCodeEventListener(&profiler_listener);

  profiler_listener.CodeCreateEvent(i::Logger::BUILTIN_TAG, frame1_code, "bbb");
  profiler_listener.CodeCreateEvent(i::Logger::STUB_TAG, frame2_code, "ccc");
  profiler_listener.CodeCreateEvent(i::Logger::BUILTIN_TAG, frame3_code, "ddd");

  EnqueueTickSampleEvent(processor, frame1_code.raw_instruction_start());
  EnqueueTickSampleEvent(
      processor,
      frame2_code.raw_instruction_start() + frame2_code.ExecutableSize() / 2,
      frame1_code.raw_instruction_start() + frame1_code.ExecutableSize() / 2);
  EnqueueTickSampleEvent(processor, frame3_code.raw_instruction_end() - 1,
                         frame2_code.raw_instruction_end() - 1,
                         frame1_code.raw_instruction_end() - 1);

  isolate->logger()->RemoveCodeEventListener(&profiler_listener);
  processor->StopSynchronously();
  CpuProfile* profile = profiles->StopProfiling("");
  CHECK(profile);

  // Check call trees.
  const std::vector<ProfileNode*>* top_down_root_children =
      profile->top_down()->root()->children();
  CHECK_EQ(1, top_down_root_children->size());
  CHECK_EQ(0, strcmp("bbb", top_down_root_children->back()->entry()->name()));
  const std::vector<ProfileNode*>* top_down_bbb_children =
      top_down_root_children->back()->children();
  CHECK_EQ(1, top_down_bbb_children->size());
  CHECK_EQ(0, strcmp("ccc", top_down_bbb_children->back()->entry()->name()));
  const std::vector<ProfileNode*>* top_down_stub_children =
      top_down_bbb_children->back()->children();
  CHECK_EQ(1, top_down_stub_children->size());
  CHECK_EQ(0, strcmp("ddd", top_down_stub_children->back()->entry()->name()));
  const std::vector<ProfileNode*>* top_down_ddd_children =
      top_down_stub_children->back()->children();
  CHECK(top_down_ddd_children->empty());
}

// http://crbug/51594
// This test must not crash.
TEST(CrashIfStoppingLastNonExistentProfile) {
  CcTest::InitializeVM();
  TestSetup test_setup;
  std::unique_ptr<CpuProfiler> profiler(new CpuProfiler(CcTest::i_isolate()));
  profiler->StartProfiling("1");
  profiler->StopProfiling("2");
  profiler->StartProfiling("1");
  profiler->StopProfiling("");
}

// http://code.google.com/p/v8/issues/detail?id=1398
// Long stacks (exceeding max frames limit) must not be erased.
TEST(Issue1398) {
  TestSetup test_setup;
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  i::AbstractCode code = CreateCode(&env);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      CcTest::i_isolate(), generator,
      v8::base::TimeDelta::FromMicroseconds(100), true);
  CpuProfiler profiler(isolate, kDebugNaming, profiles, generator, processor);
  profiles->StartProfiling("");
  processor->Start();
  ProfilerListener profiler_listener(isolate, processor);

  profiler_listener.CodeCreateEvent(i::Logger::BUILTIN_TAG, code, "bbb");

  v8::internal::TickSample sample;
  sample.pc = reinterpret_cast<void*>(code.InstructionStart());
  sample.tos = nullptr;
  sample.frames_count = v8::TickSample::kMaxFramesCount;
  for (unsigned i = 0; i < sample.frames_count; ++i) {
    sample.stack[i] = reinterpret_cast<void*>(code.InstructionStart());
  }
  sample.timestamp = base::TimeTicks::HighResolutionNow();
  processor->AddSample(sample);

  processor->StopSynchronously();
  CpuProfile* profile = profiles->StopProfiling("");
  CHECK(profile);

  unsigned actual_depth = 0;
  const ProfileNode* node = profile->top_down()->root();
  while (!node->children()->empty()) {
    node = node->children()->back();
    ++actual_depth;
  }

  CHECK_EQ(1 + v8::TickSample::kMaxFramesCount, actual_depth);  // +1 for PC.
}

TEST(DeleteAllCpuProfiles) {
  CcTest::InitializeVM();
  TestSetup test_setup;
  std::unique_ptr<CpuProfiler> profiler(new CpuProfiler(CcTest::i_isolate()));
  CHECK_EQ(0, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());

  profiler->StartProfiling("1");
  profiler->StopProfiling("1");
  CHECK_EQ(1, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());
  profiler->StartProfiling("1");
  profiler->StartProfiling("2");
  profiler->StopProfiling("2");
  profiler->StopProfiling("1");
  CHECK_EQ(2, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());

  // Test profiling cancellation by the 'delete' command.
  profiler->StartProfiling("1");
  profiler->StartProfiling("2");
  CHECK_EQ(0, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());
}


static bool FindCpuProfile(v8::CpuProfiler* v8profiler,
                           const v8::CpuProfile* v8profile) {
  i::CpuProfiler* profiler = reinterpret_cast<i::CpuProfiler*>(v8profiler);
  const i::CpuProfile* profile =
      reinterpret_cast<const i::CpuProfile*>(v8profile);
  int length = profiler->GetProfilesCount();
  for (int i = 0; i < length; i++) {
    if (profile == profiler->GetProfile(i))
      return true;
  }
  return false;
}


TEST(DeleteCpuProfile) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = v8::CpuProfiler::New(env->GetIsolate());
  i::CpuProfiler* iprofiler = reinterpret_cast<i::CpuProfiler*>(cpu_profiler);

  CHECK_EQ(0, iprofiler->GetProfilesCount());
  v8::Local<v8::String> name1 = v8_str("1");
  cpu_profiler->StartProfiling(name1);
  v8::CpuProfile* p1 = cpu_profiler->StopProfiling(name1);
  CHECK(p1);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  CHECK(FindCpuProfile(cpu_profiler, p1));
  p1->Delete();
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::Local<v8::String> name2 = v8_str("2");
  cpu_profiler->StartProfiling(name2);
  v8::CpuProfile* p2 = cpu_profiler->StopProfiling(name2);
  CHECK(p2);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  CHECK(FindCpuProfile(cpu_profiler, p2));
  v8::Local<v8::String> name3 = v8_str("3");
  cpu_profiler->StartProfiling(name3);
  v8::CpuProfile* p3 = cpu_profiler->StopProfiling(name3);
  CHECK(p3);
  CHECK_EQ(2, iprofiler->GetProfilesCount());
  CHECK_NE(p2, p3);
  CHECK(FindCpuProfile(cpu_profiler, p3));
  CHECK(FindCpuProfile(cpu_profiler, p2));
  p2->Delete();
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  CHECK(!FindCpuProfile(cpu_profiler, p2));
  CHECK(FindCpuProfile(cpu_profiler, p3));
  p3->Delete();
  CHECK_EQ(0, iprofiler->GetProfilesCount());
  cpu_profiler->Dispose();
}


TEST(ProfileStartEndTime) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = v8::CpuProfiler::New(env->GetIsolate());

  v8::Local<v8::String> profile_name = v8_str("test");
  cpu_profiler->StartProfiling(profile_name);
  const v8::CpuProfile* profile = cpu_profiler->StopProfiling(profile_name);
  CHECK(profile->GetStartTime() <= profile->GetEndTime());
  cpu_profiler->Dispose();
}

class ProfilerHelper {
 public:
  explicit ProfilerHelper(const v8::Local<v8::Context>& context)
      : context_(context),
        profiler_(v8::CpuProfiler::New(context->GetIsolate())) {
    i::ProfilerExtension::set_profiler(profiler_);
  }
  ~ProfilerHelper() {
    i::ProfilerExtension::set_profiler(static_cast<CpuProfiler*>(nullptr));
    profiler_->Dispose();
  }

  using ProfilingMode = v8::CpuProfilingMode;

  v8::CpuProfile* Run(
      v8::Local<v8::Function> function, v8::Local<v8::Value> argv[], int argc,
      unsigned min_js_samples = 0, unsigned min_external_samples = 0,
      ProfilingMode mode = ProfilingMode::kLeafNodeLineNumbers,
      unsigned max_samples = CpuProfilingOptions::kNoSampleLimit);

  v8::CpuProfiler* profiler() { return profiler_; }

 private:
  v8::Local<v8::Context> context_;
  v8::CpuProfiler* profiler_;
};

v8::CpuProfile* ProfilerHelper::Run(v8::Local<v8::Function> function,
                                    v8::Local<v8::Value> argv[], int argc,
                                    unsigned min_js_samples,
                                    unsigned min_external_samples,
                                    ProfilingMode mode, unsigned max_samples) {
  v8::Local<v8::String> profile_name = v8_str("my_profile");

  profiler_->SetSamplingInterval(100);
  profiler_->StartProfiling(profile_name, {mode, max_samples});

  v8::internal::CpuProfiler* iprofiler =
      reinterpret_cast<v8::internal::CpuProfiler*>(profiler_);
  v8::sampler::Sampler* sampler =
      reinterpret_cast<i::SamplingEventsProcessor*>(iprofiler->processor())
          ->sampler();
  sampler->StartCountingSamples();
  do {
    function->Call(context_, context_->Global(), argc, argv).ToLocalChecked();
  } while (sampler->js_sample_count() < min_js_samples ||
           sampler->external_sample_count() < min_external_samples);

  v8::CpuProfile* profile = profiler_->StopProfiling(profile_name);

  CHECK(profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  return profile;
}

static unsigned TotalHitCount(const v8::CpuProfileNode* node) {
  unsigned hit_count = node->GetHitCount();
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i)
    hit_count += TotalHitCount(node->GetChild(i));
  return hit_count;
}

static const v8::CpuProfileNode* FindChild(v8::Local<v8::Context> context,
                                           const v8::CpuProfileNode* node,
                                           const char* name) {
  int count = node->GetChildrenCount();
  v8::Local<v8::String> name_handle = v8_str(name);
  for (int i = 0; i < count; i++) {
    const v8::CpuProfileNode* child = node->GetChild(i);
    if (name_handle->Equals(context, child->GetFunctionName()).FromJust()) {
      return child;
    }
  }
  return nullptr;
}

static const v8::CpuProfileNode* FindChild(const v8::CpuProfileNode* node,
                                           const char* name) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::CpuProfileNode* child = node->GetChild(i);
    if (strcmp(child->GetFunctionNameStr(), name) == 0) {
      return child;
    }
  }
  return nullptr;
}

static const v8::CpuProfileNode* GetChild(v8::Local<v8::Context> context,
                                          const v8::CpuProfileNode* node,
                                          const char* name) {
  const v8::CpuProfileNode* result = FindChild(context, node, name);
  if (!result) FATAL("Failed to GetChild: %s", name);
  return result;
}

static void CheckSimpleBranch(v8::Local<v8::Context> context,
                              const v8::CpuProfileNode* node,
                              const char* names[], int length) {
  for (int i = 0; i < length; i++) {
    const char* name = names[i];
    node = GetChild(context, node, name);
  }
}

static const ProfileNode* GetSimpleBranch(v8::Local<v8::Context> context,
                                          v8::CpuProfile* profile,
                                          const char* names[], int length) {
  const v8::CpuProfileNode* node = profile->GetTopDownRoot();
  for (int i = 0; i < length; i++) {
    node = GetChild(context, node, names[i]);
  }
  return reinterpret_cast<const ProfileNode*>(node);
}

struct NameLinePair {
  const char* name;
  int line_number;
};

static const v8::CpuProfileNode* FindChild(const v8::CpuProfileNode* node,
                                           NameLinePair pair) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::CpuProfileNode* child = node->GetChild(i);
    // The name and line number must match, or if the requested line number was
    // -1, then match any function of the same name.
    if (strcmp(child->GetFunctionNameStr(), pair.name) == 0 &&
        (child->GetLineNumber() == pair.line_number ||
         pair.line_number == -1)) {
      return child;
    }
  }
  return nullptr;
}

static const v8::CpuProfileNode* GetChild(const v8::CpuProfileNode* node,
                                          NameLinePair pair) {
  const v8::CpuProfileNode* result = FindChild(node, pair);
  if (!result) FATAL("Failed to GetChild: %s:%d", pair.name, pair.line_number);
  return result;
}

static void CheckBranch(const v8::CpuProfileNode* node, NameLinePair path[],
                        int length) {
  for (int i = 0; i < length; i++) {
    NameLinePair pair = path[i];
    node = GetChild(node, pair);
  }
}

static const char* cpu_profiler_test_source =
    "%NeverOptimizeFunction(loop);\n"
    "%NeverOptimizeFunction(delay);\n"
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(baz);\n"
    "%NeverOptimizeFunction(foo);\n"
    "%NeverOptimizeFunction(start);\n"
    "function loop(timeout) {\n"
    "  this.mmm = 0;\n"
    "  var start = Date.now();\n"
    "  do {\n"
    "    var n = 1000;\n"
    "    while(n > 1) {\n"
    "      n--;\n"
    "      this.mmm += n * n * n;\n"
    "    }\n"
    "  } while (Date.now() - start < timeout);\n"
    "}\n"
    "function delay() { loop(10); }\n"
    "function bar() { delay(); }\n"
    "function baz() { delay(); }\n"
    "function foo() {\n"
    "  delay();\n"
    "  bar();\n"
    "  delay();\n"
    "  baz();\n"
    "}\n"
    "function start(duration) {\n"
    "  var start = Date.now();\n"
    "  do {\n"
    "    foo();\n"
    "  } while (Date.now() - start < duration);\n"
    "}\n";

// Check that the profile tree for the script above will look like the
// following:
//
// [Top down]:
//  1062     0   (root) [-1]
//  1054     0    start [-1]
//  1054     1      foo [-1]
//   265     0        baz [-1]
//   265     1          delay [-1]
//   264   264            loop [-1]
//   525     3        delay [-1]
//   522   522          loop [-1]
//   263     0        bar [-1]
//   263     1          delay [-1]
//   262   262            loop [-1]
//     2     2    (program) [-1]
//     6     6    (garbage collector) [-1]
TEST(CollectCpuProfile) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(cpu_profiler_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  int32_t profiling_interval_ms = 200;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), profiling_interval_ms)};
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  const v8::CpuProfileNode* foo_node = GetChild(env.local(), start_node, "foo");

  const char* bar_branch[] = {"bar", "delay", "loop"};
  CheckSimpleBranch(env.local(), foo_node, bar_branch, arraysize(bar_branch));
  const char* baz_branch[] = {"baz", "delay", "loop"};
  CheckSimpleBranch(env.local(), foo_node, baz_branch, arraysize(baz_branch));
  const char* delay_branch[] = {"delay", "loop"};
  CheckSimpleBranch(env.local(), foo_node, delay_branch,
                    arraysize(delay_branch));

  profile->Delete();
}

TEST(CollectCpuProfileCallerLineNumbers) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(cpu_profiler_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  int32_t profiling_interval_ms = 200;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), profiling_interval_ms)};
  ProfilerHelper helper(env.local());
  helper.Run(function, args, arraysize(args), 1000, 0,
             v8::CpuProfilingMode::kCallerLineNumbers, 0);
  v8::CpuProfile* profile =
      helper.Run(function, args, arraysize(args), 1000, 0,
                 v8::CpuProfilingMode::kCallerLineNumbers, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(root, {"start", 27});
  const v8::CpuProfileNode* foo_node = GetChild(start_node, {"foo", 30});

  NameLinePair bar_branch[] = {{"bar", 23}, {"delay", 19}, {"loop", 18}};
  CheckBranch(foo_node, bar_branch, arraysize(bar_branch));
  NameLinePair baz_branch[] = {{"baz", 25}, {"delay", 20}, {"loop", 18}};
  CheckBranch(foo_node, baz_branch, arraysize(baz_branch));
  NameLinePair delay_at22_branch[] = {{"delay", 22}, {"loop", 18}};
  CheckBranch(foo_node, delay_at22_branch, arraysize(delay_at22_branch));
  NameLinePair delay_at24_branch[] = {{"delay", 24}, {"loop", 18}};
  CheckBranch(foo_node, delay_at24_branch, arraysize(delay_at24_branch));

  profile->Delete();
}

static const char* hot_deopt_no_frame_entry_test_source =
    "%NeverOptimizeFunction(foo);\n"
    "%NeverOptimizeFunction(start);\n"
    "function foo(a, b) {\n"
    "  return a + b;\n"
    "}\n"
    "function start(timeout) {\n"
    "  var start = Date.now();\n"
    "  do {\n"
    "    for (var i = 1; i < 1000; ++i) foo(1, i);\n"
    "    var duration = Date.now() - start;\n"
    "  } while (duration < timeout);\n"
    "  return duration;\n"
    "}\n";

// Check that the profile tree for the script above will look like the
// following:
//
// [Top down]:
//  1062     0  (root) [-1]
//  1054     0    start [-1]
//  1054     1      foo [-1]
//     2     2    (program) [-1]
//     6     6    (garbage collector) [-1]
//
// The test checks no FP ranges are present in a deoptimized function.
// If 'foo' has no ranges the samples falling into the prologue will miss the
// 'start' function on the stack, so 'foo' will be attached to the (root).
TEST(HotDeoptNoFrameEntry) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(hot_deopt_no_frame_entry_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  int32_t profiling_interval_ms = 200;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), profiling_interval_ms)};
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 1000);
  function->Call(env.local(), env->Global(), arraysize(args), args)
      .ToLocalChecked();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "foo");

  profile->Delete();
}

TEST(CollectCpuProfileSamples) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(cpu_profiler_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  int32_t profiling_interval_ms = 200;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), profiling_interval_ms)};
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile =
      helper.Run(function, args, arraysize(args), 1000, 0);

  CHECK_LE(200, profile->GetSamplesCount());
  uint64_t end_time = profile->GetEndTime();
  uint64_t current_time = profile->GetStartTime();
  CHECK_LE(current_time, end_time);
  for (int i = 0; i < profile->GetSamplesCount(); i++) {
    CHECK(profile->GetSample(i));
    uint64_t timestamp = profile->GetSampleTimestamp(i);
    CHECK_LE(current_time, timestamp);
    CHECK_LE(timestamp, end_time);
    current_time = timestamp;
  }

  profile->Delete();
}

static const char* cpu_profiler_test_source2 =
    "%NeverOptimizeFunction(loop);\n"
    "%NeverOptimizeFunction(delay);\n"
    "%NeverOptimizeFunction(start);\n"
    "function loop() {}\n"
    "function delay() { loop(); }\n"
    "function start(duration) {\n"
    "  var start = Date.now();\n"
    "  do {\n"
    "    for (var i = 0; i < 10000; ++i) delay();\n"
    "  } while (Date.now() - start < duration);\n"
    "}";

// Check that the profile tree doesn't contain unexpected traces:
//  - 'loop' can be called only by 'delay'
//  - 'delay' may be called only by 'start'
// The profile will look like the following:
//
// [Top down]:
//   135     0   (root) [-1] #1
//   121    72    start [-1] #3
//    49    33      delay [-1] #4
//    16    16        loop [-1] #5
//    14    14    (program) [-1] #2
TEST(SampleWhenFrameIsNotSetup) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(cpu_profiler_test_source2);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  int32_t duration_ms = 100;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), duration_ms)};
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  const v8::CpuProfileNode* delay_node =
      GetChild(env.local(), start_node, "delay");
  GetChild(env.local(), delay_node, "loop");

  profile->Delete();
}

static const char* native_accessor_test_source = "function start(count) {\n"
"  for (var i = 0; i < count; i++) {\n"
"    var o = instance.foo;\n"
"    instance.foo = o + 1;\n"
"  }\n"
"}\n";

class TestApiCallbacks {
 public:
  explicit TestApiCallbacks(int min_duration_ms)
      : min_duration_ms_(min_duration_ms),
        is_warming_up_(false) {}

  static void Getter(v8::Local<v8::String> name,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
    TestApiCallbacks* data = FromInfo(info);
    data->Wait();
  }

  static void Setter(v8::Local<v8::String> name,
                     v8::Local<v8::Value> value,
                     const v8::PropertyCallbackInfo<void>& info) {
    TestApiCallbacks* data = FromInfo(info);
    data->Wait();
  }

  static void Callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
    TestApiCallbacks* data = FromInfo(info);
    data->Wait();
  }

  void set_warming_up(bool value) { is_warming_up_ = value; }

 private:
  void Wait() {
    if (is_warming_up_) return;
    v8::Platform* platform = v8::internal::V8::GetCurrentPlatform();
    double start = platform->CurrentClockTimeMillis();
    double duration = 0;
    while (duration < min_duration_ms_) {
      v8::base::OS::Sleep(v8::base::TimeDelta::FromMilliseconds(1));
      duration = platform->CurrentClockTimeMillis() - start;
    }
  }

  template <typename T>
  static TestApiCallbacks* FromInfo(const T& info) {
    void* data = v8::External::Cast(*info.Data())->Value();
    return reinterpret_cast<TestApiCallbacks*>(data);
  }

  int min_duration_ms_;
  bool is_warming_up_;
};


// Test that native accessors are properly reported in the CPU profile.
// This test checks the case when the long-running accessors are called
// only once and the optimizer doesn't have chance to change the invocation
// code.
TEST(NativeAccessorUninitializedIC) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(isolate);
  v8::Local<v8::ObjectTemplate> instance_template =
      func_template->InstanceTemplate();

  TestApiCallbacks accessors(100);
  v8::Local<v8::External> data = v8::External::New(isolate, &accessors);
  instance_template->SetAccessor(v8_str("foo"), &TestApiCallbacks::Getter,
                                 &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Object> instance =
      func->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("instance"), instance).FromJust();

  CompileRun(native_accessor_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  ProfilerHelper helper(env.local());
  int32_t repeat_count = 1;
  v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, repeat_count)};
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 0, 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "get foo");
  GetChild(env.local(), start_node, "set foo");

  profile->Delete();
}


// Test that native accessors are properly reported in the CPU profile.
// This test makes sure that the accessors are called enough times to become
// hot and to trigger optimizations.
TEST(NativeAccessorMonomorphicIC) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(isolate);
  v8::Local<v8::ObjectTemplate> instance_template =
      func_template->InstanceTemplate();

  TestApiCallbacks accessors(1);
  v8::Local<v8::External> data =
      v8::External::New(isolate, &accessors);
  instance_template->SetAccessor(v8_str("foo"), &TestApiCallbacks::Getter,
                                 &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Object> instance =
      func->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("instance"), instance).FromJust();

  CompileRun(native_accessor_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  {
    // Make sure accessors ICs are in monomorphic state before starting
    // profiling.
    accessors.set_warming_up(true);
    int32_t warm_up_iterations = 3;
    v8::Local<v8::Value> args[] = {
        v8::Integer::New(isolate, warm_up_iterations)};
    function->Call(env.local(), env->Global(), arraysize(args), args)
        .ToLocalChecked();
    accessors.set_warming_up(false);
  }

  int32_t repeat_count = 100;
  v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, repeat_count)};
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 0, 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "get foo");
  GetChild(env.local(), start_node, "set foo");

  profile->Delete();
}


static const char* native_method_test_source = "function start(count) {\n"
"  for (var i = 0; i < count; i++) {\n"
"    instance.fooMethod();\n"
"  }\n"
"}\n";


TEST(NativeMethodUninitializedIC) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  TestApiCallbacks callbacks(100);
  v8::Local<v8::External> data = v8::External::New(isolate, &callbacks);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(isolate);
  func_template->SetClassName(v8_str("Test_InstanceConstructor"));
  v8::Local<v8::ObjectTemplate> proto_template =
      func_template->PrototypeTemplate();
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, func_template);
  proto_template->Set(
      v8_str("fooMethod"),
      v8::FunctionTemplate::New(isolate, &TestApiCallbacks::Callback, data,
                                signature, 0));

  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Object> instance =
      func->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("instance"), instance).FromJust();

  CompileRun(native_method_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  ProfilerHelper helper(env.local());
  int32_t repeat_count = 1;
  v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, repeat_count)};
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 0, 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "fooMethod");

  profile->Delete();
}


TEST(NativeMethodMonomorphicIC) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  TestApiCallbacks callbacks(1);
  v8::Local<v8::External> data =
      v8::External::New(isolate, &callbacks);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(isolate);
  func_template->SetClassName(v8_str("Test_InstanceCostructor"));
  v8::Local<v8::ObjectTemplate> proto_template =
      func_template->PrototypeTemplate();
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, func_template);
  proto_template->Set(
      v8_str("fooMethod"),
      v8::FunctionTemplate::New(isolate, &TestApiCallbacks::Callback, data,
                                signature, 0));

  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Object> instance =
      func->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("instance"), instance).FromJust();

  CompileRun(native_method_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");
  {
    // Make sure method ICs are in monomorphic state before starting
    // profiling.
    callbacks.set_warming_up(true);
    int32_t warm_up_iterations = 3;
    v8::Local<v8::Value> args[] = {
        v8::Integer::New(isolate, warm_up_iterations)};
    function->Call(env.local(), env->Global(), arraysize(args), args)
        .ToLocalChecked();
    callbacks.set_warming_up(false);
  }

  ProfilerHelper helper(env.local());
  int32_t repeat_count = 100;
  v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, repeat_count)};
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 0, 200);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  GetChild(env.local(), root, "start");
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "fooMethod");

  profile->Delete();
}


static const char* bound_function_test_source =
    "function foo() {\n"
    "  startProfiling('my_profile');\n"
    "}\n"
    "function start() {\n"
    "  var callback = foo.bind(this);\n"
    "  callback();\n"
    "}";


TEST(BoundFunctionCall) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  CompileRun(bound_function_test_source);
  v8::Local<v8::Function> function = GetFunction(env, "start");

  ProfilerHelper helper(env);
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();

  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  GetChild(env, start_node, "foo");

  profile->Delete();
}

// This tests checks distribution of the samples through the source lines.
static void TickLines(bool optimize) {
#ifndef V8_LITE_MODE
  FLAG_opt = optimize;
#endif  // V8_LITE_MODE
  CcTest::InitializeVM();
  LocalContext env;
  i::FLAG_allow_natives_syntax = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);

  i::EmbeddedVector<char, 512> script;
  i::EmbeddedVector<char, 64> prepare_opt;
  i::EmbeddedVector<char, 64> optimize_call;

  const char* func_name = "func";
  if (optimize) {
    i::SNPrintF(prepare_opt, "%%PrepareFunctionForOptimization(%s);\n",
                func_name);
    i::SNPrintF(optimize_call, "%%OptimizeFunctionOnNextCall(%s);\n",
                func_name);
  } else {
    prepare_opt[0] = '\0';
    optimize_call[0] = '\0';
  }
  i::SNPrintF(script,
              "function %s() {\n"
              "  var n = 0;\n"
              "  var m = 100*100;\n"
              "  while (m > 1) {\n"
              "    m--;\n"
              "    n += m * m * m;\n"
              "  }\n"
              "}\n"
              "%s"
              "%s();\n"
              "%s"
              "%s();\n",
              func_name, prepare_opt.begin(), func_name, optimize_call.begin(),
              func_name);

  CompileRun(script.begin());

  i::Handle<i::JSFunction> func = i::Handle<i::JSFunction>::cast(
      v8::Utils::OpenHandle(*GetFunction(env.local(), func_name)));
  CHECK(!func->shared().is_null());
  CHECK(!func->shared().abstract_code().is_null());
  CHECK(!optimize || func->IsOptimized() ||
        !CcTest::i_isolate()->use_optimizer());
  i::AbstractCode code = func->abstract_code();
  CHECK(!code.is_null());
  i::Address code_address = code.raw_instruction_start();
  CHECK_NE(code_address, kNullAddress);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      CcTest::i_isolate(), generator,
      v8::base::TimeDelta::FromMicroseconds(100), true);
  CpuProfiler profiler(isolate, kDebugNaming, profiles, generator, processor);
  profiles->StartProfiling("");
  // TODO(delphick): Stop using the CpuProfiler internals here: This forces
  // LogCompiledFunctions so that source positions are collected everywhere.
  // This would normally happen automatically with CpuProfiler::StartProfiling
  // but doesn't because it's constructed with a generator and a processor.
  isolate->logger()->LogCompiledFunctions();
  processor->Start();
  ProfilerListener profiler_listener(isolate, processor);

  // Enqueue code creation events.
  i::Handle<i::String> str = factory->NewStringFromAsciiChecked(func_name);
  int line = 1;
  int column = 1;
  profiler_listener.CodeCreateEvent(i::Logger::FUNCTION_TAG, code,
                                    func->shared(), *str, line, column);

  // Enqueue a tick event to enable code events processing.
  EnqueueTickSampleEvent(processor, code_address);

  processor->StopSynchronously();

  CpuProfile* profile = profiles->StopProfiling("");
  CHECK(profile);

  // Check the state of profile generator.
  CodeEntry* func_entry = generator->code_map()->FindEntry(code_address);
  CHECK(func_entry);
  CHECK_EQ(0, strcmp(func_name, func_entry->name()));
  const i::SourcePositionTable* line_info = func_entry->line_info();
  CHECK(line_info);
  CHECK_NE(v8::CpuProfileNode::kNoLineNumberInfo,
           line_info->GetSourceLineNumber(100));

  // Check the hit source lines using V8 Public APIs.
  const i::ProfileTree* tree = profile->top_down();
  ProfileNode* root = tree->root();
  CHECK(root);
  ProfileNode* func_node = root->FindChild(func_entry);
  CHECK(func_node);

  // Add 10 faked ticks to source line #5.
  int hit_line = 5;
  int hit_count = 10;
  for (int i = 0; i < hit_count; i++) func_node->IncrementLineTicks(hit_line);

  unsigned int line_count = func_node->GetHitLineCount();
  CHECK_EQ(2u, line_count);  // Expect two hit source lines - #1 and #5.
  ScopedVector<v8::CpuProfileNode::LineTick> entries(line_count);
  CHECK(func_node->GetLineTicks(&entries[0], line_count));
  int value = 0;
  for (int i = 0; i < entries.length(); i++)
    if (entries[i].line == hit_line) {
      value = entries[i].hit_count;
      break;
    }
  CHECK_EQ(hit_count, value);
}

TEST(TickLinesBaseline) { TickLines(false); }

TEST(TickLinesOptimized) { TickLines(true); }

static const char* call_function_test_source =
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(start);\n"
    "function bar(n) {\n"
    "  var s = 0;\n"
    "  for (var i = 0; i < n; i++) s += i * i * i;\n"
    "  return s;\n"
    "}\n"
    "function start(duration) {\n"
    "  var start = Date.now();\n"
    "  do {\n"
    "    for (var i = 0; i < 100; ++i)\n"
    "      bar.call(this, 1000);\n"
    "  } while (Date.now() - start < duration);\n"
    "}";

// Test that if we sampled thread when it was inside FunctionCall buitin then
// its caller frame will be '(unresolved function)' as we have no reliable way
// to resolve it.
//
// [Top down]:
//    96     0   (root) [-1] #1
//     1     1    (garbage collector) [-1] #4
//     5     0    (unresolved function) [-1] #5
//     5     5      call [-1] #6
//    71    70    start [-1] #3
//     1     1      bar [-1] #7
//    19    19    (program) [-1] #2
TEST(FunctionCallSample) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Collect garbage that might have be generated while installing
  // extensions.
  CcTest::CollectAllGarbage();

  CompileRun(call_function_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  ProfilerHelper helper(env.local());
  int32_t duration_ms = 100;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), duration_ms)};
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "bar");

  const v8::CpuProfileNode* unresolved_node =
      FindChild(env.local(), root, i::CodeEntry::kUnresolvedFunctionName);
  CHECK(!unresolved_node || GetChild(env.local(), unresolved_node, "call"));

  profile->Delete();
}

static const char* function_apply_test_source =
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(test);\n"
    "%NeverOptimizeFunction(start);\n"
    "function bar(n) {\n"
    "  var s = 0;\n"
    "  for (var i = 0; i < n; i++) s += i * i * i;\n"
    "  return s;\n"
    "}\n"
    "function test() {\n"
    "  bar.apply(this, [1000]);\n"
    "}\n"
    "function start(duration) {\n"
    "  var start = Date.now();\n"
    "  do {\n"
    "    for (var i = 0; i < 100; ++i) test();\n"
    "  } while (Date.now() - start < duration);\n"
    "}";

// [Top down]:
//    94     0   (root) [-1] #0 1
//     2     2    (garbage collector) [-1] #0 7
//    82    49    start [-1] #16 3
//     1     0      (unresolved function) [-1] #0 8
//     1     1        apply [-1] #0 9
//    32    21      test [-1] #16 4
//     2     2        bar [-1] #16 6
//    10    10    (program) [-1] #0 2
TEST(FunctionApplySample) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(function_apply_test_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  ProfilerHelper helper(env.local());
  int32_t duration_ms = 100;
  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), duration_ms)};
  v8::CpuProfile* profile = helper.Run(function, args, arraysize(args), 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  const v8::CpuProfileNode* test_node =
      GetChild(env.local(), start_node, "test");
  GetChild(env.local(), test_node, "bar");

  const v8::CpuProfileNode* unresolved_node =
      FindChild(env.local(), start_node, CodeEntry::kUnresolvedFunctionName);
  CHECK(!unresolved_node || GetChild(env.local(), unresolved_node, "apply"));

  profile->Delete();
}

static const char* cpu_profiler_deep_stack_test_source =
    "function foo(n) {\n"
    "  if (n)\n"
    "    foo(n - 1);\n"
    "  else\n"
    "    collectSample();\n"
    "}\n"
    "function start() {\n"
    "  startProfiling('my_profile');\n"
    "  foo(250);\n"
    "}\n";

// Check a deep stack
//
// [Top down]:
//    0  (root) 0 #1
//    2    (program) 0 #2
//    0    start 21 #3 no reason
//    0      foo 21 #4 no reason
//    0        foo 21 #5 no reason
//                ....
//    0          foo 21 #254 no reason
TEST(CpuProfileDeepStack) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);

  CompileRun(cpu_profiler_deep_stack_test_source);
  v8::Local<v8::Function> function = GetFunction(env, "start");

  v8::Local<v8::String> profile_name = v8_str("my_profile");
  function->Call(env, env->Global(), 0, nullptr).ToLocalChecked();
  v8::CpuProfile* profile = helper.profiler()->StopProfiling(profile_name);
  CHECK(profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* node = GetChild(env, root, "start");
  for (int i = 0; i <= 250; ++i) {
    node = GetChild(env, node, "foo");
  }
  CHECK(!FindChild(env, node, "foo"));

  profile->Delete();
}

static const char* js_native_js_test_source =
    "%NeverOptimizeFunction(foo);\n"
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(start);\n"
    "function foo(n) {\n"
    "  var s = 0;\n"
    "  for (var i = 0; i < n; i++) s += i * i * i;\n"
    "  return s;\n"
    "}\n"
    "function bar() {\n"
    "  foo(1000);\n"
    "}\n"
    "function start() {\n"
    "  CallJsFunction(bar);\n"
    "}";

static void CallJsFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Function> function = info[0].As<v8::Function>();
  v8::Local<v8::Value> argv[] = {info[1]};
  function->Call(info.GetIsolate()->GetCurrentContext(), info.This(),
                 arraysize(argv), argv)
      .ToLocalChecked();
}

// [Top down]:
//    58     0   (root) #0 1
//     2     2    (program) #0 2
//    56     1    start #16 3
//    55     0      CallJsFunction #0 4
//    55     1        bar #16 5
//    54    54          foo #16 6
TEST(JsNativeJsSample) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      env->GetIsolate(), CallJsFunction);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env).ToLocalChecked();
  func->SetName(v8_str("CallJsFunction"));
  env->Global()->Set(env, v8_str("CallJsFunction"), func).FromJust();

  CompileRun(js_native_js_test_source);
  v8::Local<v8::Function> function = GetFunction(env, "start");

  ProfilerHelper helper(env);
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  const v8::CpuProfileNode* native_node =
      GetChild(env, start_node, "CallJsFunction");
  const v8::CpuProfileNode* bar_node = GetChild(env, native_node, "bar");
  GetChild(env, bar_node, "foo");

  profile->Delete();
}

static const char* js_native_js_runtime_js_test_source =
    "%NeverOptimizeFunction(foo);\n"
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(start);\n"
    "function foo(n) {\n"
    "  var s = 0;\n"
    "  for (var i = 0; i < n; i++) s += i * i * i;\n"
    "  return s;\n"
    "}\n"
    "var bound = foo.bind(this);\n"
    "function bar() {\n"
    "  bound(1000);\n"
    "}\n"
    "function start() {\n"
    "  CallJsFunction(bar);\n"
    "}";

// [Top down]:
//    57     0   (root) #0 1
//    55     1    start #16 3
//    54     0      CallJsFunction #0 4
//    54     3        bar #16 5
//    51    51          foo #16 6
//     2     2    (program) #0 2
TEST(JsNativeJsRuntimeJsSample) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      env->GetIsolate(), CallJsFunction);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env).ToLocalChecked();
  func->SetName(v8_str("CallJsFunction"));
  env->Global()->Set(env, v8_str("CallJsFunction"), func).FromJust();

  CompileRun(js_native_js_runtime_js_test_source);
  ProfilerHelper helper(env);
  v8::Local<v8::Function> function = GetFunction(env, "start");
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  const v8::CpuProfileNode* native_node =
      GetChild(env, start_node, "CallJsFunction");
  const v8::CpuProfileNode* bar_node = GetChild(env, native_node, "bar");
  GetChild(env, bar_node, "foo");

  profile->Delete();
}

static void CallJsFunction2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::base::OS::Print("In CallJsFunction2\n");
  CallJsFunction(info);
}

static const char* js_native1_js_native2_js_test_source =
    "%NeverOptimizeFunction(foo);\n"
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(start);\n"
    "function foo() {\n"
    "  var s = 0;\n"
    "  for (var i = 0; i < 1000; i++) s += i * i * i;\n"
    "  return s;\n"
    "}\n"
    "function bar() {\n"
    "  CallJsFunction2(foo);\n"
    "}\n"
    "function start() {\n"
    "  CallJsFunction1(bar);\n"
    "}";

// [Top down]:
//    57     0   (root) #0 1
//    55     1    start #16 3
//    54     0      CallJsFunction1 #0 4
//    54     0        bar #16 5
//    54     0          CallJsFunction2 #0 6
//    54    54            foo #16 7
//     2     2    (program) #0 2
TEST(JsNative1JsNative2JsSample) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::Function> func1 =
      v8::FunctionTemplate::New(env->GetIsolate(), CallJsFunction)
          ->GetFunction(env)
          .ToLocalChecked();
  func1->SetName(v8_str("CallJsFunction1"));
  env->Global()->Set(env, v8_str("CallJsFunction1"), func1).FromJust();

  v8::Local<v8::Function> func2 =
      v8::FunctionTemplate::New(env->GetIsolate(), CallJsFunction2)
          ->GetFunction(env)
          .ToLocalChecked();
  func2->SetName(v8_str("CallJsFunction2"));
  env->Global()->Set(env, v8_str("CallJsFunction2"), func2).FromJust();

  CompileRun(js_native1_js_native2_js_test_source);

  ProfilerHelper helper(env);
  v8::Local<v8::Function> function = GetFunction(env, "start");
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 1000);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  const v8::CpuProfileNode* native_node1 =
      GetChild(env, start_node, "CallJsFunction1");
  const v8::CpuProfileNode* bar_node = GetChild(env, native_node1, "bar");
  const v8::CpuProfileNode* native_node2 =
      GetChild(env, bar_node, "CallJsFunction2");
  GetChild(env, native_node2, "foo");

  profile->Delete();
}

static const char* js_force_collect_sample_source =
    "function start() {\n"
    "  CallCollectSample();\n"
    "}";

static void CallCollectSample(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::CpuProfiler::CollectSample(info.GetIsolate());
}

TEST(CollectSampleAPI) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(env->GetIsolate(), CallCollectSample);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env).ToLocalChecked();
  func->SetName(v8_str("CallCollectSample"));
  env->Global()->Set(env, v8_str("CallCollectSample"), func).FromJust();

  CompileRun(js_force_collect_sample_source);
  ProfilerHelper helper(env);
  v8::Local<v8::Function> function = GetFunction(env, "start");
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  CHECK_LE(1, start_node->GetChildrenCount());
  GetChild(env, start_node, "CallCollectSample");

  profile->Delete();
}

static const char* js_native_js_runtime_multiple_test_source =
    "%NeverOptimizeFunction(foo);\n"
    "%NeverOptimizeFunction(bar);\n"
    "%NeverOptimizeFunction(start);\n"
    "function foo() {\n"
    "  return Math.sin(Math.random());\n"
    "}\n"
    "var bound = foo.bind(this);\n"
    "function bar() {\n"
    "  return bound();\n"
    "}\n"
    "function start() {\n"
    "  startProfiling('my_profile');\n"
    "  var startTime = Date.now();\n"
    "  do {\n"
    "    CallJsFunction(bar);\n"
    "  } while (Date.now() - startTime < 200);\n"
    "}";

// The test check multiple entrances/exits between JS and native code.
//
// [Top down]:
//    (root) #0 1
//      start #16 3
//        CallJsFunction #0 4
//          bar #16 5
//            foo #16 6
//      (program) #0 2
TEST(JsNativeJsRuntimeJsSampleMultiple) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(env->GetIsolate(), CallJsFunction);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env).ToLocalChecked();
  func->SetName(v8_str("CallJsFunction"));
  env->Global()->Set(env, v8_str("CallJsFunction"), func).FromJust();

  CompileRun(js_native_js_runtime_multiple_test_source);

  ProfilerHelper helper(env);
  v8::Local<v8::Function> function = GetFunction(env, "start");
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 500, 500);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  const v8::CpuProfileNode* native_node =
      GetChild(env, start_node, "CallJsFunction");
  const v8::CpuProfileNode* bar_node = GetChild(env, native_node, "bar");
  GetChild(env, bar_node, "foo");

  profile->Delete();
}

static const char* inlining_test_source =
    "var finish = false;\n"
    "function action(n) {\n"
    "  var s = 0;\n"
    "  for (var i = 0; i < n; ++i) s += i*i*i;\n"
    "  if (finish)\n"
    "    startProfiling('my_profile');\n"
    "  return s;\n"
    "}\n"
    "function level3() { return action(100); }\n"
    "function level2() { return level3() * 2; }\n"
    "function level1() { return level2(); }\n"
    "function start() {\n"
    "  var n = 100;\n"
    "  while (--n)\n"
    "    level1();\n"
    "  finish = true;\n"
    "  level1();\n"
    "}"
    "%PrepareFunctionForOptimization(level1);\n"
    "%PrepareFunctionForOptimization(level2);\n"
    "%PrepareFunctionForOptimization(level3);\n"
    "%NeverOptimizeFunction(action);\n"
    "%NeverOptimizeFunction(start);\n"
    "level1();\n"
    "%OptimizeFunctionOnNextCall(level1);\n"
    "%OptimizeFunctionOnNextCall(level2);\n"
    "%OptimizeFunctionOnNextCall(level3);\n";

// The test check multiple entrances/exits between JS and native code.
//
// [Top down]:
//    (root) #0 1
//      start #16 3
//        level1 #0 4
//          level2 #16 5
//            level3 #16 6
//              action #16 7
//      (program) #0 2
TEST(Inlining) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);

  CompileRun(inlining_test_source);
  v8::Local<v8::Function> function = GetFunction(env, "start");

  v8::Local<v8::String> profile_name = v8_str("my_profile");
  function->Call(env, env->Global(), 0, nullptr).ToLocalChecked();
  v8::CpuProfile* profile = helper.profiler()->StopProfiling(profile_name);
  CHECK(profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  const v8::CpuProfileNode* level1_node = GetChild(env, start_node, "level1");
  const v8::CpuProfileNode* level2_node = GetChild(env, level1_node, "level2");
  const v8::CpuProfileNode* level3_node = GetChild(env, level2_node, "level3");
  GetChild(env, level3_node, "action");

  profile->Delete();
}

static const char* inlining_test_source2 = R"(
    function action(n) {
      var s = 0;
      for (var i = 0; i < n; ++i) s += i*i*i;
      return s;
    }
    function level4() {
      action(100);
      return action(100);
    }
    function level3() {
      const a = level4();
      const b = level4();
      return a + b * 1.1;
    }
    function level2() {
      return level3() * 2;
    }
    function level1() {
      action(1);
      action(200);
      action(1);
      return level2();
    }
    function start(n) {
      while (--n)
        level1();
    };
    %NeverOptimizeFunction(action);
    %NeverOptimizeFunction(start);
    %PrepareFunctionForOptimization(level1);
    %PrepareFunctionForOptimization(level2);
    %PrepareFunctionForOptimization(level3);
    %PrepareFunctionForOptimization(level4);
    level1();
    level1();
    %OptimizeFunctionOnNextCall(level1);
    %OptimizeFunctionOnNextCall(level2);
    %OptimizeFunctionOnNextCall(level3);
    %OptimizeFunctionOnNextCall(level4);
    level1();
  )";

// The simulator builds are extremely slow. We run them with fewer iterations.
#ifdef USE_SIMULATOR
const double load_factor = 0.01;
#else
const double load_factor = 1.0;
#endif

// [Top down]:
//     0  (root):0 0 #1
//    13    start:34 6 #3
//              bailed out due to 'Optimization is always disabled'
//    19      level1:36 6 #4
//    16        action:29 6 #14
//                  bailed out due to 'Optimization is always disabled'
//  2748        action:30 6 #10
//                  bailed out due to 'Optimization is always disabled'
//    18        action:31 6 #15
//                  bailed out due to 'Optimization is always disabled'
//     0        level2:32 6 #5
//     0          level3:26 6 #6
//    12            level4:22 6 #11
//  1315              action:17 6 #13
//                        bailed out due to 'Optimization is always disabled'
//  1324              action:18 6 #12
//                        bailed out due to 'Optimization is always disabled'
//    16            level4:21 6 #7
//  1268              action:17 6 #9
//                        bailed out due to 'Optimization is always disabled'
//  1322              action:18 6 #8
//                        bailed out due to 'Optimization is always disabled'
//     2    (program):0 0 #2
TEST(Inlining2) {
  FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  CompileRun(inlining_test_source2);
  v8::Local<v8::Function> function = GetFunction(env, "start");

  v8::CpuProfiler* profiler = v8::CpuProfiler::New(CcTest::isolate());
  v8::Local<v8::String> profile_name = v8_str("inlining");
  profiler->StartProfiling(
      profile_name,
      CpuProfilingOptions{v8::CpuProfilingMode::kCallerLineNumbers});

  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), 50000 * load_factor)};
  function->Call(env, env->Global(), arraysize(args), args).ToLocalChecked();
  v8::CpuProfile* profile = profiler->StopProfiling(profile_name);
  CHECK(profile);

  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");

  NameLinePair l421_a17[] = {{"level1", 27},
                             {"level2", 23},
                             {"level3", 17},
                             {"level4", 12},
                             {"action", 8}};
  CheckBranch(start_node, l421_a17, arraysize(l421_a17));
  NameLinePair l422_a17[] = {{"level1", 27},
                             {"level2", 23},
                             {"level3", 17},
                             {"level4", 13},
                             {"action", 8}};
  CheckBranch(start_node, l422_a17, arraysize(l422_a17));

  NameLinePair l421_a18[] = {{"level1", 27},
                             {"level2", 23},
                             {"level3", 17},
                             {"level4", 12},
                             {"action", 9}};
  CheckBranch(start_node, l421_a18, arraysize(l421_a18));
  NameLinePair l422_a18[] = {{"level1", 27},
                             {"level2", 23},
                             {"level3", 17},
                             {"level4", 13},
                             {"action", 9}};
  CheckBranch(start_node, l422_a18, arraysize(l422_a18));

  NameLinePair action_direct[] = {{"level1", 27}, {"action", 21}};
  CheckBranch(start_node, action_direct, arraysize(action_direct));

  profile->Delete();
  profiler->Dispose();
}

static const char* cross_script_source_a = R"(





    %NeverOptimizeFunction(action);
    function action(n) {
      var s = 0;
      for (var i = 0; i < n; ++i) s += i*i*i;
      return s;
    }
    function level1() {
      const a = action(1);
      const b = action(200);
      const c = action(1);
      return a + b + c;
    }
  )";

static const char* cross_script_source_b = R"(
    %PrepareFunctionForOptimization(start);
    %PrepareFunctionForOptimization(level1);
    start(1);
    start(1);
    %OptimizeFunctionOnNextCall(start);
    %OptimizeFunctionOnNextCall(level1);
    start(1);
    function start(n) {
      while (--n)
        level1();
    };
  )";

TEST(CrossScriptInliningCallerLineNumbers) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::Script> script_a =
      CompileWithOrigin(cross_script_source_a, "script_a", false);
  v8::Local<v8::Script> script_b =
      CompileWithOrigin(cross_script_source_b, "script_b", false);

  script_a->Run(env).ToLocalChecked();
  script_b->Run(env).ToLocalChecked();

  v8::Local<v8::Function> function = GetFunction(env, "start");

  v8::CpuProfiler* profiler = v8::CpuProfiler::New(CcTest::isolate());
  v8::Local<v8::String> profile_name = v8_str("inlining");
  profiler->StartProfiling(profile_name,
                           v8::CpuProfilingMode::kCallerLineNumbers);

  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), 20000 * load_factor)};
  function->Call(env, env->Global(), arraysize(args), args).ToLocalChecked();
  v8::CpuProfile* profile = profiler->StopProfiling(profile_name);
  CHECK(profile);

  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  CHECK_EQ(0, strcmp("script_b", start_node->GetScriptResourceNameStr()));

  NameLinePair l19_a10[] = {{"level1", 11}, {"action", 15}};
  CheckBranch(start_node, l19_a10, arraysize(l19_a10));

  const v8::CpuProfileNode* level1_node = GetChild(env, start_node, "level1");
  CHECK_EQ(0, strcmp("script_a", level1_node->GetScriptResourceNameStr()));

  const v8::CpuProfileNode* action_node = GetChild(env, level1_node, "action");
  CHECK_EQ(0, strcmp("script_a", action_node->GetScriptResourceNameStr()));

  profile->Delete();
  profiler->Dispose();
}

static const char* cross_script_source_c = R"(
    function level3() {
      const a = action(1);
      const b = action(100);
      const c = action(1);
      return a + b + c;
    }
    %NeverOptimizeFunction(action);
    function action(n) {
      var s = 0;
      for (var i = 0; i < n; ++i) s += i*i*i;
      return s;
    }
  )";

static const char* cross_script_source_d = R"(
    function level2() {
      const p = level3();
      const q = level3();
      return p + q;
    }
  )";

static const char* cross_script_source_e = R"(
    function level1() {
      return level2() + 1000;
    }
  )";

static const char* cross_script_source_f = R"(
    %PrepareFunctionForOptimization(start);
    %PrepareFunctionForOptimization(level1);
    %PrepareFunctionForOptimization(level2);
    %PrepareFunctionForOptimization(level3);
    start(1);
    start(1);
    %OptimizeFunctionOnNextCall(start);
    %OptimizeFunctionOnNextCall(level1);
    %OptimizeFunctionOnNextCall(level2);
    %OptimizeFunctionOnNextCall(level3);
    start(1);
    function start(n) {
      while (--n)
        level1();
    };
  )";

TEST(CrossScriptInliningCallerLineNumbers2) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::Local<v8::Script> script_c =
      CompileWithOrigin(cross_script_source_c, "script_c", false);
  v8::Local<v8::Script> script_d =
      CompileWithOrigin(cross_script_source_d, "script_d", false);
  v8::Local<v8::Script> script_e =
      CompileWithOrigin(cross_script_source_e, "script_e", false);
  v8::Local<v8::Script> script_f =
      CompileWithOrigin(cross_script_source_f, "script_f", false);

  script_c->Run(env).ToLocalChecked();
  script_d->Run(env).ToLocalChecked();
  script_e->Run(env).ToLocalChecked();
  script_f->Run(env).ToLocalChecked();

  v8::Local<v8::Function> function = GetFunction(env, "start");

  v8::CpuProfiler* profiler = v8::CpuProfiler::New(CcTest::isolate());
  v8::Local<v8::String> profile_name = v8_str("inlining");
  profiler->StartProfiling(profile_name,
                           v8::CpuProfilingMode::kCallerLineNumbers);

  v8::Local<v8::Value> args[] = {
      v8::Integer::New(env->GetIsolate(), 20000 * load_factor)};
  function->Call(env, env->Global(), arraysize(args), args).ToLocalChecked();
  v8::CpuProfile* profile = profiler->StopProfiling(profile_name);
  CHECK(profile);

  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env, root, "start");
  CHECK_EQ(0, strcmp("script_f", start_node->GetScriptResourceNameStr()));

  const v8::CpuProfileNode* level1_node = GetChild(env, start_node, "level1");
  CHECK_EQ(0, strcmp("script_e", level1_node->GetScriptResourceNameStr()));

  const v8::CpuProfileNode* level2_node = GetChild(env, level1_node, "level2");
  CHECK_EQ(0, strcmp("script_d", level2_node->GetScriptResourceNameStr()));

  const v8::CpuProfileNode* level3_node = GetChild(env, level2_node, "level3");
  CHECK_EQ(0, strcmp("script_c", level3_node->GetScriptResourceNameStr()));

  const v8::CpuProfileNode* action_node = GetChild(env, level3_node, "action");
  CHECK_EQ(0, strcmp("script_c", action_node->GetScriptResourceNameStr()));

  profile->Delete();
  profiler->Dispose();
}

// [Top down]:
//     0   (root) #0 1
//     2    (program) #0 2
//     3    (idle) #0 3
TEST(IdleTime) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = v8::CpuProfiler::New(env->GetIsolate());

  v8::Local<v8::String> profile_name = v8_str("my_profile");
  cpu_profiler->StartProfiling(profile_name);

  i::Isolate* isolate = CcTest::i_isolate();
  i::ProfilerEventsProcessor* processor =
      reinterpret_cast<i::CpuProfiler*>(cpu_profiler)->processor();

  processor->AddCurrentStack(true);
  isolate->SetIdle(true);
  for (int i = 0; i < 3; i++) {
    processor->AddCurrentStack(true);
  }
  isolate->SetIdle(false);
  processor->AddCurrentStack(true);

  v8::CpuProfile* profile = cpu_profiler->StopProfiling(profile_name);
  CHECK(profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* program_node =
      GetChild(env.local(), root, CodeEntry::kProgramEntryName);
  CHECK_EQ(0, program_node->GetChildrenCount());
  CHECK_GE(program_node->GetHitCount(), 2u);

  const v8::CpuProfileNode* idle_node =
      GetChild(env.local(), root, CodeEntry::kIdleEntryName);
  CHECK_EQ(0, idle_node->GetChildrenCount());
  CHECK_GE(idle_node->GetHitCount(), 3u);

  profile->Delete();
  cpu_profiler->Dispose();
}

static void CheckFunctionDetails(v8::Isolate* isolate,
                                 const v8::CpuProfileNode* node,
                                 const char* name, const char* script_name,
                                 bool is_shared_cross_origin, int script_id,
                                 int line, int column,
                                 const v8::CpuProfileNode* parent) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  CHECK(v8_str(name)->Equals(context, node->GetFunctionName()).FromJust());
  CHECK_EQ(0, strcmp(name, node->GetFunctionNameStr()));
  CHECK(v8_str(script_name)
            ->Equals(context, node->GetScriptResourceName())
            .FromJust());
  CHECK_EQ(0, strcmp(script_name, node->GetScriptResourceNameStr()));
  CHECK_EQ(script_id, node->GetScriptId());
  CHECK_EQ(line, node->GetLineNumber());
  CHECK_EQ(column, node->GetColumnNumber());
  CHECK_EQ(parent, node->GetParent());
  CHECK_EQ(v8::CpuProfileNode::kScript, node->GetSourceType());
}

TEST(FunctionDetails) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);

  v8::Local<v8::Script> script_a = CompileWithOrigin(
      "%NeverOptimizeFunction(foo);\n"
      "%NeverOptimizeFunction(bar);\n"
      "    function foo\n() { bar(); }\n"
      " function bar() { startProfiling(); }\n",
      "script_a", false);
  script_a->Run(env).ToLocalChecked();
  v8::Local<v8::Script> script_b = CompileWithOrigin(
      "%NeverOptimizeFunction(baz);"
      "\n\n   function baz() { foo(); }\n"
      "\n\nbaz();\n"
      "stopProfiling();\n",
      "script_b", true);
  script_b->Run(env).ToLocalChecked();
  const v8::CpuProfile* profile = i::ProfilerExtension::last_profile;
  const v8::CpuProfileNode* current = profile->GetTopDownRoot();
  reinterpret_cast<ProfileNode*>(
      const_cast<v8::CpuProfileNode*>(current))->Print(0);
  // The tree should look like this:
  //  0   (root) 0 #1
  //  0    "" 19 #2 no reason script_b:1
  //  0      baz 19 #3 TryCatchStatement script_b:3
  //  0        foo 18 #4 TryCatchStatement script_a:2
  //  1          bar 18 #5 no reason script_a:3
  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  CHECK_EQ(root->GetParent(), nullptr);
  const v8::CpuProfileNode* script = GetChild(env, root, "");
  CheckFunctionDetails(env->GetIsolate(), script, "", "script_b", true,
                       script_b->GetUnboundScript()->GetId(), 1, 1, root);
  const v8::CpuProfileNode* baz = GetChild(env, script, "baz");
  CheckFunctionDetails(env->GetIsolate(), baz, "baz", "script_b", true,
                       script_b->GetUnboundScript()->GetId(), 3, 16, script);
  const v8::CpuProfileNode* foo = GetChild(env, baz, "foo");
  CheckFunctionDetails(env->GetIsolate(), foo, "foo", "script_a", false,
                       script_a->GetUnboundScript()->GetId(), 4, 1, baz);
  const v8::CpuProfileNode* bar = GetChild(env, foo, "bar");
  CheckFunctionDetails(env->GetIsolate(), bar, "bar", "script_a", false,
                       script_a->GetUnboundScript()->GetId(), 5, 14, foo);
}

TEST(FunctionDetailsInlining) {
  if (!CcTest::i_isolate()->use_optimizer() || i::FLAG_always_opt) return;
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);

  // alpha is in a_script, beta in b_script. beta is
  // inlined in alpha, but it should be attributed to b_script.

  v8::Local<v8::Script> script_b = CompileWithOrigin(
      "function beta(k) {\n"
      "  let sum = 2;\n"
      "  for(let i = 0; i < k; i ++) {\n"
      "    sum += i;\n"
      "    sum = sum + 'a';\n"
      "  }\n"
      "  return sum;\n"
      "}\n"
      "\n",
      "script_b", true);

  v8::Local<v8::Script> script_a = CompileWithOrigin(
      "function alpha(p) {\n"
      "  let res = beta(p);\n"
      "  res = res + res;\n"
      "  return res;\n"
      "}\n"
      "let p = 2;\n"
      "\n"
      "\n"
      "// Warm up before profiling or the inlining doesn't happen.\n"
      "%PrepareFunctionForOptimization(alpha);\n"
      "p = alpha(p);\n"
      "p = alpha(p);\n"
      "%OptimizeFunctionOnNextCall(alpha);\n"
      "p = alpha(p);\n"
      "\n"
      "\n"
      "startProfiling();\n"
      "for(let i = 0; i < 10000; i++) {\n"
      "  p = alpha(p);\n"
      "}\n"
      "stopProfiling();\n"
      "\n"
      "\n",
      "script_a", false);

  script_b->Run(env).ToLocalChecked();
  script_a->Run(env).ToLocalChecked();

  const v8::CpuProfile* profile = i::ProfilerExtension::last_profile;
  const v8::CpuProfileNode* current = profile->GetTopDownRoot();
  reinterpret_cast<ProfileNode*>(const_cast<v8::CpuProfileNode*>(current))
      ->Print(0);
  //   The tree should look like this:
  //  0  (root) 0 #1
  //  5    (program) 0 #6
  //  2     14 #2 script_a:1
  //    ;;; deopted at script_id: 14 position: 299 with reason 'Insufficient
  //    type feedback for call'.
  //  1      alpha 14 #4 script_a:1
  //  9        beta 13 #5 script_b:0
  //  0      startProfiling 0 #3

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  CHECK_EQ(root->GetParent(), nullptr);
  const v8::CpuProfileNode* script = GetChild(env, root, "");
  CheckFunctionDetails(env->GetIsolate(), script, "", "script_a", false,
                       script_a->GetUnboundScript()->GetId(), 1, 1, root);
  const v8::CpuProfileNode* alpha = FindChild(env, script, "alpha");
  // Return early if profiling didn't sample alpha.
  if (!alpha) return;
  CheckFunctionDetails(env->GetIsolate(), alpha, "alpha", "script_a", false,
                       script_a->GetUnboundScript()->GetId(), 1, 15, script);
  const v8::CpuProfileNode* beta = FindChild(env, alpha, "beta");
  if (!beta) return;
  CheckFunctionDetails(env->GetIsolate(), beta, "beta", "script_b", true,
                       script_b->GetUnboundScript()->GetId(), 1, 14, alpha);
}

TEST(DontStopOnFinishedProfileDelete) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  v8::CpuProfiler* profiler = v8::CpuProfiler::New(env->GetIsolate());
  i::CpuProfiler* iprofiler = reinterpret_cast<i::CpuProfiler*>(profiler);

  CHECK_EQ(0, iprofiler->GetProfilesCount());
  v8::Local<v8::String> outer = v8_str("outer");
  profiler->StartProfiling(outer);
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::Local<v8::String> inner = v8_str("inner");
  profiler->StartProfiling(inner);
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::CpuProfile* inner_profile = profiler->StopProfiling(inner);
  CHECK(inner_profile);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  inner_profile->Delete();
  inner_profile = nullptr;
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::CpuProfile* outer_profile = profiler->StopProfiling(outer);
  CHECK(outer_profile);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  outer_profile->Delete();
  outer_profile = nullptr;
  CHECK_EQ(0, iprofiler->GetProfilesCount());
  profiler->Dispose();
}


const char* GetBranchDeoptReason(v8::Local<v8::Context> context,
                                 i::CpuProfile* iprofile, const char* branch[],
                                 int length) {
  v8::CpuProfile* profile = reinterpret_cast<v8::CpuProfile*>(iprofile);
  const ProfileNode* iopt_function = nullptr;
  iopt_function = GetSimpleBranch(context, profile, branch, length);
  CHECK_EQ(1U, iopt_function->deopt_infos().size());
  return iopt_function->deopt_infos()[0].deopt_reason;
}


// deopt at top function
TEST(CollectDeoptEvents) {
  if (!CcTest::i_isolate()->use_optimizer() || i::FLAG_always_opt) return;
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);
  i::CpuProfiler* iprofiler =
      reinterpret_cast<i::CpuProfiler*>(helper.profiler());

  const char opt_source[] =
      "function opt_function%d(value, depth) {\n"
      "  if (depth) return opt_function%d(value, depth - 1);\n"
      "\n"
      "  return  10 / value;\n"
      "}\n"
      "\n";

  for (int i = 0; i < 3; ++i) {
    i::EmbeddedVector<char, sizeof(opt_source) + 100> buffer;
    i::SNPrintF(buffer, opt_source, i, i);
    v8::Script::Compile(env, v8_str(buffer.begin()))
        .ToLocalChecked()
        ->Run(env)
        .ToLocalChecked();
  }

  const char* source =
      "startProfiling();\n"
      "\n"
      "%PrepareFunctionForOptimization(opt_function0);\n"
      "\n"
      "opt_function0(1, 1);\n"
      "\n"
      "%OptimizeFunctionOnNextCall(opt_function0)\n"
      "\n"
      "opt_function0(1, 1);\n"
      "\n"
      "opt_function0(undefined, 1);\n"
      "\n"
      "%PrepareFunctionForOptimization(opt_function1);\n"
      "\n"
      "opt_function1(1, 1);\n"
      "\n"
      "%OptimizeFunctionOnNextCall(opt_function1)\n"
      "\n"
      "opt_function1(1, 1);\n"
      "\n"
      "opt_function1(NaN, 1);\n"
      "\n"
      "%PrepareFunctionForOptimization(opt_function2);\n"
      "\n"
      "opt_function2(1, 1);\n"
      "\n"
      "%OptimizeFunctionOnNextCall(opt_function2)\n"
      "\n"
      "opt_function2(1, 1);\n"
      "\n"
      "opt_function2(0, 1);\n"
      "\n"
      "stopProfiling();\n"
      "\n";

  v8::Script::Compile(env, v8_str(source))
      .ToLocalChecked()
      ->Run(env)
      .ToLocalChecked();
  i::CpuProfile* iprofile = iprofiler->GetProfile(0);
  iprofile->Print();
  /* The expected profile
  [Top down]:
      0  (root) 0 #1
     23     32 #2
      1      opt_function2 31 #7
      1        opt_function2 31 #8
                  ;;; deopted at script_id: 31 position: 106 with reason
  'division by zero'.
      2      opt_function0 29 #3
      4        opt_function0 29 #4
                  ;;; deopted at script_id: 29 position: 108 with reason 'not a
  heap number'.
      0      opt_function1 30 #5
      1        opt_function1 30 #6
                  ;;; deopted at script_id: 30 position: 108 with reason 'lost
  precision or NaN'.
  */

  {
    const char* branch[] = {"", "opt_function0", "opt_function0"};
    const char* deopt_reason =
        GetBranchDeoptReason(env, iprofile, branch, arraysize(branch));
    if (deopt_reason != reason(i::DeoptimizeReason::kNotAHeapNumber) &&
        deopt_reason != reason(i::DeoptimizeReason::kNotASmi)) {
      FATAL("%s", deopt_reason);
    }
  }
  {
    const char* branch[] = {"", "opt_function1", "opt_function1"};
    const char* deopt_reason =
        GetBranchDeoptReason(env, iprofile, branch, arraysize(branch));
    if (deopt_reason != reason(i::DeoptimizeReason::kNaN) &&
        deopt_reason != reason(i::DeoptimizeReason::kLostPrecisionOrNaN) &&
        deopt_reason != reason(i::DeoptimizeReason::kNotASmi)) {
      FATAL("%s", deopt_reason);
    }
  }
  {
    const char* branch[] = {"", "opt_function2", "opt_function2"};
    CHECK_EQ(reason(i::DeoptimizeReason::kDivisionByZero),
             GetBranchDeoptReason(env, iprofile, branch, arraysize(branch)));
  }
  iprofiler->DeleteProfile(iprofile);
}


TEST(SourceLocation) {
  i::FLAG_always_opt = true;
  LocalContext env;
  v8::HandleScope scope(CcTest::isolate());

  const char* source =
      "function CompareStatementWithThis() {\n"
      "  if (this === 1) {}\n"
      "}\n"
      "CompareStatementWithThis();\n";

  v8::Script::Compile(env.local(), v8_str(source))
      .ToLocalChecked()
      ->Run(env.local())
      .ToLocalChecked();
}

static const char* inlined_source =
    "function opt_function(left, right) { var k = left*right; return k + 1; "
    "}\n";
//   0.........1.........2.........3.........4....*....5.........6......*..7


// deopt at the first level inlined function
TEST(DeoptAtFirstLevelInlinedSource) {
  if (!CcTest::i_isolate()->use_optimizer() || i::FLAG_always_opt) return;
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);
  i::CpuProfiler* iprofiler =
      reinterpret_cast<i::CpuProfiler*>(helper.profiler());

  //   0.........1.........2.........3.........4.........5.........6.........7
  const char* source =
      "function test(left, right) { return opt_function(left, right); }\n"
      "\n"
      "startProfiling();\n"
      "\n"
      "%PrepareFunctionForOptimization(test);\n"
      "\n"
      "test(10, 10);\n"
      "\n"
      "%OptimizeFunctionOnNextCall(test)\n"
      "\n"
      "test(10, 10);\n"
      "\n"
      "test(undefined, 1e9);\n"
      "\n"
      "stopProfiling();\n"
      "\n";

  v8::Local<v8::Script> inlined_script = v8_compile(inlined_source);
  inlined_script->Run(env).ToLocalChecked();
  int inlined_script_id = inlined_script->GetUnboundScript()->GetId();

  v8::Local<v8::Script> script = v8_compile(source);
  script->Run(env).ToLocalChecked();
  int script_id = script->GetUnboundScript()->GetId();

  i::CpuProfile* iprofile = iprofiler->GetProfile(0);
  iprofile->Print();
  /* The expected profile output
  [Top down]:
      0  (root) 0 #1
     10     30 #2
      1      test 30 #3
                ;;; deopted at script_id: 29 position: 45 with reason 'not a
  heap number'.
                ;;;     Inline point: script_id 30 position: 36.
      4        opt_function 29 #4
  */
  v8::CpuProfile* profile = reinterpret_cast<v8::CpuProfile*>(iprofile);

  const char* branch[] = {"", "test"};
  const ProfileNode* itest_node =
      GetSimpleBranch(env, profile, branch, arraysize(branch));
  const std::vector<v8::CpuProfileDeoptInfo>& deopt_infos =
      itest_node->deopt_infos();
  CHECK_EQ(1U, deopt_infos.size());

  const v8::CpuProfileDeoptInfo& info = deopt_infos[0];
  CHECK(reason(i::DeoptimizeReason::kNotASmi) == info.deopt_reason ||
        reason(i::DeoptimizeReason::kNotAHeapNumber) == info.deopt_reason);
  CHECK_EQ(2U, info.stack.size());
  CHECK_EQ(inlined_script_id, info.stack[0].script_id);
  CHECK_LE(dist(offset(inlined_source, "*right"), info.stack[0].position), 1);
  CHECK_EQ(script_id, info.stack[1].script_id);
  CHECK_EQ(offset(source, "opt_function(left,"), info.stack[1].position);

  iprofiler->DeleteProfile(iprofile);
}


// deopt at the second level inlined function
TEST(DeoptAtSecondLevelInlinedSource) {
  if (!CcTest::i_isolate()->use_optimizer() || i::FLAG_always_opt) return;
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);
  i::CpuProfiler* iprofiler =
      reinterpret_cast<i::CpuProfiler*>(helper.profiler());

  //   0.........1.........2.........3.........4.........5.........6.........7
  const char* source =
      "function test2(left, right) { return opt_function(left, right); }\n"
      "function test1(left, right) { return test2(left, right); } \n"
      "\n"
      "startProfiling();\n"
      "\n"
      "%EnsureFeedbackVectorForFunction(opt_function);\n"
      "%EnsureFeedbackVectorForFunction(test2);\n"
      "%PrepareFunctionForOptimization(test1);\n"
      "\n"
      "test1(10, 10);\n"
      "\n"
      "%OptimizeFunctionOnNextCall(test1)\n"
      "\n"
      "test1(10, 10);\n"
      "\n"
      "test1(undefined, 1e9);\n"
      "\n"
      "stopProfiling();\n"
      "\n";

  v8::Local<v8::Script> inlined_script = v8_compile(inlined_source);
  inlined_script->Run(env).ToLocalChecked();
  int inlined_script_id = inlined_script->GetUnboundScript()->GetId();

  v8::Local<v8::Script> script = v8_compile(source);
  script->Run(env).ToLocalChecked();
  int script_id = script->GetUnboundScript()->GetId();

  i::CpuProfile* iprofile = iprofiler->GetProfile(0);
  iprofile->Print();
  /* The expected profile output
  [Top down]:
      0  (root) 0 #1
     11     30 #2
      1      test1 30 #3
                ;;; deopted at script_id: 29 position: 45 with reason 'not a
  heap number'.
                ;;;     Inline point: script_id 30 position: 37.
                ;;;     Inline point: script_id 30 position: 103.
      1        test2 30 #4
      3          opt_function 29 #5
  */

  v8::CpuProfile* profile = reinterpret_cast<v8::CpuProfile*>(iprofile);

  const char* branch[] = {"", "test1"};
  const ProfileNode* itest_node =
      GetSimpleBranch(env, profile, branch, arraysize(branch));
  const std::vector<v8::CpuProfileDeoptInfo>& deopt_infos =
      itest_node->deopt_infos();
  CHECK_EQ(1U, deopt_infos.size());

  const v8::CpuProfileDeoptInfo info = deopt_infos[0];
  CHECK(reason(i::DeoptimizeReason::kNotASmi) == info.deopt_reason ||
        reason(i::DeoptimizeReason::kNotAHeapNumber) == info.deopt_reason);
  CHECK_EQ(3U, info.stack.size());
  CHECK_EQ(inlined_script_id, info.stack[0].script_id);
  CHECK_LE(dist(offset(inlined_source, "*right"), info.stack[0].position), 1);
  CHECK_EQ(script_id, info.stack[1].script_id);
  CHECK_EQ(offset(source, "opt_function(left,"), info.stack[1].position);
  CHECK_EQ(offset(source, "test2(left, right);"), info.stack[2].position);

  iprofiler->DeleteProfile(iprofile);
}


// deopt in untracked function
TEST(DeoptUntrackedFunction) {
  if (!CcTest::i_isolate()->use_optimizer() || i::FLAG_always_opt) return;
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);
  ProfilerHelper helper(env);
  i::CpuProfiler* iprofiler =
      reinterpret_cast<i::CpuProfiler*>(helper.profiler());

  //   0.........1.........2.........3.........4.........5.........6.........7
  const char* source =
      "function test(left, right) { return opt_function(left, right); }\n"
      "\n"
      "%EnsureFeedbackVectorForFunction(opt_function);"
      "%PrepareFunctionForOptimization(test);\n"
      "\n"
      "test(10, 10);\n"
      "\n"
      "%OptimizeFunctionOnNextCall(test)\n"
      "\n"
      "test(10, 10);\n"
      "\n"
      "startProfiling();\n"  // profiler started after compilation.
      "\n"
      "test(undefined, 10);\n"
      "\n"
      "stopProfiling();\n"
      "\n";

  v8::Local<v8::Script> inlined_script = v8_compile(inlined_source);
  inlined_script->Run(env).ToLocalChecked();

  v8::Local<v8::Script> script = v8_compile(source);
  script->Run(env).ToLocalChecked();

  i::CpuProfile* iprofile = iprofiler->GetProfile(0);
  iprofile->Print();
  v8::CpuProfile* profile = reinterpret_cast<v8::CpuProfile*>(iprofile);

  const char* branch[] = {"", "test"};
  const ProfileNode* itest_node =
      GetSimpleBranch(env, profile, branch, arraysize(branch));
  CHECK_EQ(0U, itest_node->deopt_infos().size());

  iprofiler->DeleteProfile(iprofile);
}

using v8::platform::tracing::TraceBuffer;
using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceObject;

namespace {

class CpuProfileEventChecker : public v8::platform::tracing::TraceWriter {
 public:
  void AppendTraceEvent(TraceObject* trace_event) override {
    if (trace_event->name() != std::string("Profile") &&
        trace_event->name() != std::string("ProfileChunk"))
      return;
    CHECK(!profile_id_ || trace_event->id() == profile_id_);
    CHECK_EQ(1, trace_event->num_args());
    CHECK_EQ(TRACE_VALUE_TYPE_CONVERTABLE, trace_event->arg_types()[0]);
    profile_id_ = trace_event->id();
    v8::ConvertableToTraceFormat* arg =
        trace_event->arg_convertables()[0].get();
    result_json_ += result_json_.empty() ? "[" : ",\n";
    arg->AppendAsTraceFormat(&result_json_);
  }
  void Flush() override { result_json_ += "]"; }

  const std::string& result_json() const { return result_json_; }
  void Reset() {
    result_json_.clear();
    profile_id_ = 0;
  }

 private:
  std::string result_json_;
  uint64_t profile_id_ = 0;
};

}  // namespace

TEST(TracingCpuProfiler) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  CpuProfileEventChecker* event_checker = new CpuProfileEventChecker();
  TraceBuffer* ring_buffer =
      TraceBuffer::CreateTraceBufferRingBuffer(1, event_checker);
  auto* tracing_controller =
      static_cast<v8::platform::tracing::TracingController*>(
          i::V8::GetCurrentPlatform()->GetTracingController());
  tracing_controller->Initialize(ring_buffer);

#ifdef V8_USE_PERFETTO
  std::ostringstream perfetto_output;
  tracing_controller->InitializeForPerfetto(&perfetto_output);
#endif

  bool result = false;
  for (int run_duration = 50; !result; run_duration += 50) {
    TraceConfig* trace_config = new TraceConfig();
    trace_config->AddIncludedCategory(
        TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"));
    trace_config->AddIncludedCategory(
        TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler.hires"));

    std::string test_code = R"(
        function foo() {
          let s = 0;
          const endTime = Date.now() + )" +
                            std::to_string(run_duration) + R"(
          while (Date.now() < endTime) s += Math.cos(s);
          return s;
        }
        foo();)";

    tracing_controller->StartTracing(trace_config);
    CompileRun(test_code.c_str());
    tracing_controller->StopTracing();

    std::string profile_json = event_checker->result_json();
    event_checker->Reset();
    CHECK_LT(0u, profile_json.length());
    printf("Profile JSON: %s\n", profile_json.c_str());

    std::string profile_checker_code = R"(
        function checkProfile(json) {
          const profile_header = json[0];
          if (typeof profile_header['startTime'] !== 'number')
            return false;
          return json.some(event => (event.lines || []).some(line => line));
        }
        checkProfile()" + profile_json +
                                       ")";
    result = CompileRunChecked(CcTest::isolate(), profile_checker_code.c_str())
                 ->IsTrue();
  }

  static_cast<v8::platform::tracing::TracingController*>(
      i::V8::GetCurrentPlatform()->GetTracingController())
      ->Initialize(nullptr);
}

TEST(Issue763073) {
  class AllowNativesSyntax {
   public:
    AllowNativesSyntax()
        : allow_natives_syntax_(i::FLAG_allow_natives_syntax),
          trace_deopt_(i::FLAG_trace_deopt) {
      i::FLAG_allow_natives_syntax = true;
      i::FLAG_trace_deopt = true;
    }

    ~AllowNativesSyntax() {
      i::FLAG_allow_natives_syntax = allow_natives_syntax_;
      i::FLAG_trace_deopt = trace_deopt_;
    }

   private:
    bool allow_natives_syntax_;
    bool trace_deopt_;
  };

  AllowNativesSyntax allow_natives_syntax_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(
      "function f() { return function g(x) { }; }"
      // Create first closure, optimize it, and deoptimize it.
      "var g = f();"
      "%PrepareFunctionForOptimization(g);\n"
      "g(1);"
      "%OptimizeFunctionOnNextCall(g);"
      "g(1);"
      "%DeoptimizeFunction(g);"
      // Create second closure, and optimize it. This will create another
      // optimized code object and put in the (shared) type feedback vector.
      "var h = f();"
      "%PrepareFunctionForOptimization(h);\n"
      "h(1);"
      "%OptimizeFunctionOnNextCall(h);"
      "h(1);");

  // Start profiling.
  v8::CpuProfiler* cpu_profiler = v8::CpuProfiler::New(env->GetIsolate());
  v8::Local<v8::String> profile_name = v8_str("test");

  // Here we test that the heap iteration upon profiling start is not
  // confused by having a deoptimized code object for a closure while
  // having a different optimized code object in the type feedback vector.
  cpu_profiler->StartProfiling(profile_name);
  v8::CpuProfile* p = cpu_profiler->StopProfiling(profile_name);
  p->Delete();
  cpu_profiler->Dispose();
}

static const char* js_collect_sample_api_source =
    "%NeverOptimizeFunction(start);\n"
    "function start() {\n"
    "  CallStaticCollectSample();\n"
    "}";

static void CallStaticCollectSample(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::CpuProfiler::CollectSample(info.GetIsolate());
}

TEST(StaticCollectSampleAPI) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(env->GetIsolate(), CallStaticCollectSample);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  func->SetName(v8_str("CallStaticCollectSample"));
  env->Global()
      ->Set(env.local(), v8_str("CallStaticCollectSample"), func)
      .FromJust();

  CompileRun(js_collect_sample_api_source);
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");

  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* start_node = GetChild(env.local(), root, "start");
  GetChild(env.local(), start_node, "CallStaticCollectSample");

  profile->Delete();
}

TEST(CodeEntriesMemoryLeak) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  std::string source = "function start() {}\n";
  for (int i = 0; i < 1000; ++i) {
    source += "function foo" + std::to_string(i) + "() { return " +
              std::to_string(i) +
              "; }\n"
              "foo" +
              std::to_string(i) + "();\n";
  }
  CompileRun(source.c_str());
  v8::Local<v8::Function> function = GetFunction(env, "start");

  ProfilerHelper helper(env);

  for (int j = 0; j < 100; ++j) {
    v8::CpuProfile* profile = helper.Run(function, nullptr, 0);
    profile->Delete();
  }

  i::CpuProfiler* profiler =
      reinterpret_cast<i::CpuProfiler*>(helper.profiler());
  CHECK(!profiler->profiler_listener_for_test());
}

TEST(NativeFrameStackTrace) {
  // A test for issue https://crbug.com/768540
  // When a sample lands in a native function which has not EXIT frame
  // stack frame iterator used to bail out and produce an empty stack trace.
  // The source code below makes v8 call the
  // v8::internal::StringTable::LookupStringIfExists_NoAllocate native function
  // without producing an EXIT frame.
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext({PROFILER_EXTENSION_ID});
  v8::Context::Scope context_scope(env);

  const char* source = R"(
      function jsFunction() {
        var s = {};
        for (var i = 0; i < 1e4; ++i) {
          for (var j = 0; j < 100; j++) {
            s['item' + j] = 'alph';
          }
        }
      })";

  CompileRun(source);
  v8::Local<v8::Function> function = GetFunction(env, "jsFunction");

  ProfilerHelper helper(env);

  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 100, 0);

  // Count the fraction of samples landing in 'jsFunction' (valid stack)
  // vs '(program)' (no stack captured).
  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* js_function = FindChild(root, "jsFunction");
  const v8::CpuProfileNode* program = FindChild(root, "(program)");
  if (program) {
    unsigned js_function_samples = TotalHitCount(js_function);
    unsigned program_samples = TotalHitCount(program);
    double valid_samples_ratio =
        1. * js_function_samples / (js_function_samples + program_samples);
    i::PrintF("Ratio: %f\n", valid_samples_ratio);
    // TODO(alph): Investigate other causes of dropped frames. The ratio
    // should be close to 99%.
    CHECK_GE(valid_samples_ratio, 0.3);
  }

  profile->Delete();
}

TEST(SourcePositionTable) {
  i::SourcePositionTable info;

  // Newly created tables should return NoLineNumberInfo for any lookup.
  int no_info = v8::CpuProfileNode::kNoLineNumberInfo;
  CHECK_EQ(no_info, info.GetSourceLineNumber(std::numeric_limits<int>::min()));
  CHECK_EQ(no_info, info.GetSourceLineNumber(0));
  CHECK_EQ(SourcePosition::kNotInlined, info.GetInliningId(0));
  CHECK_EQ(no_info, info.GetSourceLineNumber(1));
  CHECK_EQ(no_info, info.GetSourceLineNumber(9));
  CHECK_EQ(no_info, info.GetSourceLineNumber(10));
  CHECK_EQ(no_info, info.GetSourceLineNumber(11));
  CHECK_EQ(no_info, info.GetSourceLineNumber(19));
  CHECK_EQ(no_info, info.GetSourceLineNumber(20));
  CHECK_EQ(no_info, info.GetSourceLineNumber(21));
  CHECK_EQ(no_info, info.GetSourceLineNumber(100));
  CHECK_EQ(SourcePosition::kNotInlined, info.GetInliningId(100));
  CHECK_EQ(no_info, info.GetSourceLineNumber(std::numeric_limits<int>::max()));

  info.SetPosition(10, 1, SourcePosition::kNotInlined);
  info.SetPosition(20, 2, SourcePosition::kNotInlined);

  // The only valid return values are 1 or 2 - every pc maps to a line
  // number.
  CHECK_EQ(1, info.GetSourceLineNumber(std::numeric_limits<int>::min()));
  CHECK_EQ(1, info.GetSourceLineNumber(0));
  CHECK_EQ(1, info.GetSourceLineNumber(1));
  CHECK_EQ(1, info.GetSourceLineNumber(9));
  CHECK_EQ(1, info.GetSourceLineNumber(10));
  CHECK_EQ(1, info.GetSourceLineNumber(11));
  CHECK_EQ(1, info.GetSourceLineNumber(19));
  CHECK_EQ(1, info.GetSourceLineNumber(20));
  CHECK_EQ(2, info.GetSourceLineNumber(21));
  CHECK_EQ(2, info.GetSourceLineNumber(100));
  CHECK_EQ(2, info.GetSourceLineNumber(std::numeric_limits<int>::max()));

  CHECK_EQ(SourcePosition::kNotInlined, info.GetInliningId(0));
  CHECK_EQ(SourcePosition::kNotInlined, info.GetInliningId(100));

  // Test SetPosition behavior.
  info.SetPosition(25, 3, 0);
  CHECK_EQ(2, info.GetSourceLineNumber(21));
  CHECK_EQ(3, info.GetSourceLineNumber(100));
  CHECK_EQ(3, info.GetSourceLineNumber(std::numeric_limits<int>::max()));

  CHECK_EQ(SourcePosition::kNotInlined, info.GetInliningId(21));
  CHECK_EQ(0, info.GetInliningId(100));
}

TEST(MultipleProfilers) {
  std::unique_ptr<CpuProfiler> profiler1(new CpuProfiler(CcTest::i_isolate()));
  std::unique_ptr<CpuProfiler> profiler2(new CpuProfiler(CcTest::i_isolate()));
  profiler1->StartProfiling("1");
  profiler2->StartProfiling("2");
  profiler1->StopProfiling("1");
  profiler2->StopProfiling("2");
}

// Tests that logged CodeCreateEvent calls do not crash a reused CpuProfiler.
// crbug.com/929928
TEST(CrashReusedProfiler) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  std::unique_ptr<CpuProfiler> profiler(new CpuProfiler(isolate));
  profiler->StartProfiling("1");
  profiler->StopProfiling("1");

  profiler->StartProfiling("2");
  CreateCode(&env);
  profiler->StopProfiling("2");
}

// Tests that samples from different profilers on the same isolate do not leak
// samples to each other. See crbug.com/v8/8835.
TEST(MultipleProfilersSampleIndependently) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  // Create two profilers- one slow ticking one, and one fast ticking one.
  // Ensure that the slow ticking profiler does not receive samples from the
  // fast ticking one.
  std::unique_ptr<CpuProfiler> slow_profiler(
      new CpuProfiler(CcTest::i_isolate()));
  slow_profiler->set_sampling_interval(base::TimeDelta::FromSeconds(1));
  slow_profiler->StartProfiling("1", {kLeafNodeLineNumbers});

  CompileRun(R"(
    function start() {
      let val = 1;
      for (let i = 0; i < 10e3; i++) {
        val = (val * 2) % 3;
      }
      return val;
    }
  )");
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile = helper.Run(function, nullptr, 0, 100, 0);

  auto slow_profile = slow_profiler->StopProfiling("1");
  CHECK_GT(profile->GetSamplesCount(), slow_profile->samples_count());
}

void ProfileSomeCode(v8::Isolate* isolate) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope scope(isolate);
  LocalContext context(isolate);

  v8::CpuProfiler* profiler = v8::CpuProfiler::New(isolate);

  v8::Local<v8::String> profile_name = v8_str("1");
  profiler->StartProfiling(profile_name);
  const char* source = R"(
      function foo() {
        var x = 0;
        for (var i = 0; i < 1e3; i++) {
          for (var j = 0; j < 1e3; j++) {
            x = i * j;
          }
        }
        return x;
      }
      foo();
    )";

  CompileRun(source);
  profiler->StopProfiling(profile_name);
  profiler->Dispose();
}

class IsolateThread : public v8::base::Thread {
 public:
  IsolateThread() : Thread(Options("IsolateThread")) {}

  void Run() override {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    ProfileSomeCode(isolate);
    isolate->Dispose();
  }
};

// Checking for crashes and TSAN issues with multiple isolates profiling.
TEST(MultipleIsolates) {
  IsolateThread thread1;
  IsolateThread thread2;

  thread1.Start();
  thread2.Start();

  thread1.Join();
  thread2.Join();
}

// Tests that StopProfiling doesn't wait for the next sample tick in order to
// stop, but rather exits early before a given wait threshold.
TEST(FastStopProfiling) {
  static const base::TimeDelta kLongInterval = base::TimeDelta::FromSeconds(10);
  static const base::TimeDelta kWaitThreshold = base::TimeDelta::FromSeconds(5);

  std::unique_ptr<CpuProfiler> profiler(new CpuProfiler(CcTest::i_isolate()));
  profiler->set_sampling_interval(kLongInterval);
  profiler->StartProfiling("", {kLeafNodeLineNumbers});

  v8::Platform* platform = v8::internal::V8::GetCurrentPlatform();
  double start = platform->CurrentClockTimeMillis();
  profiler->StopProfiling("");
  double duration = platform->CurrentClockTimeMillis() - start;

  CHECK_LT(duration, kWaitThreshold.InMillisecondsF());
}

TEST(LowPrecisionSamplingStartStopInternal) {
  i::Isolate* isolate = CcTest::i_isolate();
  CpuProfilesCollection profiles(isolate);
  ProfileGenerator generator(&profiles);
  std::unique_ptr<ProfilerEventsProcessor> processor(
      new SamplingEventsProcessor(isolate, &generator,
                                  v8::base::TimeDelta::FromMicroseconds(100),
                                  false));
  processor->Start();
  processor->StopSynchronously();
}

TEST(LowPrecisionSamplingStartStopPublic) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = v8::CpuProfiler::New(env->GetIsolate());
  cpu_profiler->SetUsePreciseSampling(false);
  v8::Local<v8::String> profile_name = v8_str("");
  cpu_profiler->StartProfiling(profile_name, true);
  cpu_profiler->StopProfiling(profile_name);
  cpu_profiler->Dispose();
}

const char* naming_test_source = R"(
  (function testAssignmentPropertyNamedFunction() {
    let object = {};
    object.propNamed = function () {
      CallCollectSample();
    };
    object.propNamed();
  })();
  )";

TEST(StandardNaming) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(env->GetIsolate(), CallCollectSample);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  func->SetName(v8_str("CallCollectSample"));
  env->Global()->Set(env.local(), v8_str("CallCollectSample"), func).FromJust();

  v8::CpuProfiler* profiler =
      v8::CpuProfiler::New(env->GetIsolate(), kStandardNaming);

  const auto profile_name = v8_str("");
  profiler->StartProfiling(profile_name);
  CompileRun(naming_test_source);
  auto* profile = profiler->StopProfiling(profile_name);

  auto* root = profile->GetTopDownRoot();
  auto* toplevel = FindChild(root, "");
  DCHECK(toplevel);

  auto* prop_assignment_named_test =
      GetChild(env.local(), toplevel, "testAssignmentPropertyNamedFunction");
  CHECK(FindChild(prop_assignment_named_test, ""));

  profiler->Dispose();
}

TEST(DebugNaming) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(env->GetIsolate(), CallCollectSample);
  v8::Local<v8::Function> func =
      func_template->GetFunction(env.local()).ToLocalChecked();
  func->SetName(v8_str("CallCollectSample"));
  env->Global()->Set(env.local(), v8_str("CallCollectSample"), func).FromJust();

  v8::CpuProfiler* profiler =
      v8::CpuProfiler::New(env->GetIsolate(), kDebugNaming);

  const auto profile_name = v8_str("");
  profiler->StartProfiling(profile_name);
  CompileRun(naming_test_source);
  auto* profile = profiler->StopProfiling(profile_name);

  auto* root = profile->GetTopDownRoot();
  auto* toplevel = FindChild(root, "");
  DCHECK(toplevel);

  auto* prop_assignment_named_test =
      GetChild(env.local(), toplevel, "testAssignmentPropertyNamedFunction");
  CHECK(FindChild(prop_assignment_named_test, "object.propNamed"));

  profiler->Dispose();
}

TEST(SampleLimit) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  CompileRun(R"(
    function start() {
      let val = 1;
      for (let i = 0; i < 10e3; i++) {
        val = (val * 2) % 3;
      }
      return val;
    }
  )");

  // Take 100 samples of `start`, but set the max samples to 50.
  v8::Local<v8::Function> function = GetFunction(env.local(), "start");
  ProfilerHelper helper(env.local());
  v8::CpuProfile* profile =
      helper.Run(function, nullptr, 0, 100, 0,
                 v8::CpuProfilingMode::kLeafNodeLineNumbers, 50);

  CHECK_EQ(profile->GetSamplesCount(), 50);
}

// Tests that a CpuProfile instance subsamples from a stream of tick samples
// appropriately.
TEST(ProflilerSubsampling) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      isolate, generator, v8::base::TimeDelta::FromMicroseconds(1),
      /* use_precise_sampling */ true);
  CpuProfiler profiler(isolate, kDebugNaming, profiles, generator, processor);

  // Create a new CpuProfile that wants samples at 8us.
  CpuProfile profile(&profiler, "",
                     {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                      v8::CpuProfilingOptions::kNoSampleLimit, 8});
  // Verify that the first sample is always included.
  CHECK(profile.CheckSubsample(base::TimeDelta::FromMicroseconds(10)));

  // 4 2us samples should result in one 8us sample.
  CHECK(!profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));
  CHECK(!profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));
  CHECK(!profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));
  CHECK(profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));

  // Profiles should expect the source sample interval to change, in which case
  // they should still take the first sample elapsed after their interval.
  CHECK(!profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));
  CHECK(!profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));
  CHECK(!profile.CheckSubsample(base::TimeDelta::FromMicroseconds(2)));
  CHECK(profile.CheckSubsample(base::TimeDelta::FromMicroseconds(4)));

  // Aligned samples (at 8us) are always included.
  CHECK(profile.CheckSubsample(base::TimeDelta::FromMicroseconds(8)));

  // Samples with a rate of 0 should always be included.
  CHECK(profile.CheckSubsample(base::TimeDelta::FromMicroseconds(0)));
}

// Tests that the base sampling rate of a CpuProfilesCollection is dynamically
// chosen based on the GCD of its child profiles.
TEST(DynamicResampling) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      isolate, generator, v8::base::TimeDelta::FromMicroseconds(1),
      /* use_precise_sampling */ true);
  CpuProfiler profiler(isolate, kDebugNaming, profiles, generator, processor);

  // Set a 1us base sampling rate, dividing all possible intervals.
  profiler.set_sampling_interval(base::TimeDelta::FromMicroseconds(1));

  // Verify that the sampling interval with no started profilers is unset.
  CHECK_EQ(profiles->GetCommonSamplingInterval(), base::TimeDelta());

  // Add a 10us profiler, verify that the base sampling interval is as high as
  // possible (10us).
  profiles->StartProfiling("10us",
                           {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                            v8::CpuProfilingOptions::kNoSampleLimit, 10});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(10));

  // Add a 5us profiler, verify that the base sampling interval is as high as
  // possible given a 10us and 5us profiler (5us).
  profiles->StartProfiling("5us", {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                                   v8::CpuProfilingOptions::kNoSampleLimit, 5});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(5));

  // Add a 3us profiler, verify that the base sampling interval is 1us (due to
  // coprime intervals).
  profiles->StartProfiling("3us", {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                                   v8::CpuProfilingOptions::kNoSampleLimit, 3});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(1));

  // Remove the 5us profiler, verify that the sample interval stays at 1us.
  profiles->StopProfiling("5us");
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(1));

  // Remove the 10us profiler, verify that the sample interval becomes 3us.
  profiles->StopProfiling("10us");
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(3));

  // Remove the 3us profiler, verify that the sample interval becomes unset.
  profiles->StopProfiling("3us");
  CHECK_EQ(profiles->GetCommonSamplingInterval(), base::TimeDelta());
}

// Ensures that when a non-unit base sampling interval is set on the profiler,
// that the sampling rate gets snapped to the nearest multiple prior to GCD
// computation.
TEST(DynamicResamplingWithBaseInterval) {
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate);
  ProfileGenerator* generator = new ProfileGenerator(profiles);
  ProfilerEventsProcessor* processor = new SamplingEventsProcessor(
      isolate, generator, v8::base::TimeDelta::FromMicroseconds(1),
      /* use_precise_sampling */ true);
  CpuProfiler profiler(isolate, kDebugNaming, profiles, generator, processor);

  profiler.set_sampling_interval(base::TimeDelta::FromMicroseconds(7));

  // Verify that the sampling interval with no started profilers is unset.
  CHECK_EQ(profiles->GetCommonSamplingInterval(), base::TimeDelta());

  // Add a profiler with an unset sampling interval, verify that the common
  // sampling interval is equal to the base.
  profiles->StartProfiling("unset", {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                                     v8::CpuProfilingOptions::kNoSampleLimit});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(7));
  profiles->StopProfiling("unset");

  // Adding a 8us sampling interval rounds to a 14us base interval.
  profiles->StartProfiling("8us", {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                                   v8::CpuProfilingOptions::kNoSampleLimit, 8});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(14));

  // Adding a 4us sampling interval should cause a lowering to a 7us interval.
  profiles->StartProfiling("4us", {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                                   v8::CpuProfilingOptions::kNoSampleLimit, 4});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(7));

  // Removing the 4us sampling interval should restore the 14us sampling
  // interval.
  profiles->StopProfiling("4us");
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(14));

  // Removing the 8us sampling interval should unset the common sampling
  // interval.
  profiles->StopProfiling("8us");
  CHECK_EQ(profiles->GetCommonSamplingInterval(), base::TimeDelta());

  // A sampling interval of 0us should enforce all profiles to have a sampling
  // interval of 0us (the only multiple of 0).
  profiler.set_sampling_interval(base::TimeDelta::FromMicroseconds(0));
  profiles->StartProfiling("5us", {v8::CpuProfilingMode::kLeafNodeLineNumbers,
                                   v8::CpuProfilingOptions::kNoSampleLimit, 5});
  CHECK_EQ(profiles->GetCommonSamplingInterval(),
           base::TimeDelta::FromMicroseconds(0));
  profiles->StopProfiling("5us");
}

enum class EntryCountMode { kAll, kOnlyInlined };

// Count the number of unique source positions.
int GetSourcePositionEntryCount(i::Isolate* isolate, const char* source,
                                EntryCountMode mode = EntryCountMode::kAll) {
  std::unordered_set<int64_t> raw_position_set;
  i::Handle<i::JSFunction> function = i::Handle<i::JSFunction>::cast(
      v8::Utils::OpenHandle(*CompileRun(source)));
  if (function->IsInterpreted()) return -1;
  i::Handle<i::Code> code(function->code(), isolate);
  i::SourcePositionTableIterator iterator(
      ByteArray::cast(code->source_position_table()));

  while (!iterator.done()) {
    if (mode == EntryCountMode::kAll ||
        iterator.source_position().isInlined()) {
      raw_position_set.insert(iterator.source_position().raw());
    }
    iterator.Advance();
  }
  return static_cast<int>(raw_position_set.size());
}

UNINITIALIZED_TEST(DetailedSourcePositionAPI) {
  i::FLAG_detailed_line_info = false;
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  const char* source =
      "function fib(i) {"
      "  if (i <= 1) return 1; "
      "  return fib(i - 1) +"
      "         fib(i - 2);"
      "}"
      "%PrepareFunctionForOptimization(fib);\n"
      "fib(5);"
      "%OptimizeFunctionOnNextCall(fib);"
      "fib(5);"
      "fib";
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

    CHECK(!i_isolate->NeedsDetailedOptimizedCodeLineInfo());

    int non_detailed_positions = GetSourcePositionEntryCount(i_isolate, source);

    v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);
    CHECK(i_isolate->NeedsDetailedOptimizedCodeLineInfo());

    int detailed_positions = GetSourcePositionEntryCount(i_isolate, source);

    CHECK((non_detailed_positions == -1 && detailed_positions == -1) ||
          non_detailed_positions < detailed_positions);
  }

  isolate->Dispose();
}

UNINITIALIZED_TEST(DetailedSourcePositionAPI_Inlining) {
  i::FLAG_detailed_line_info = false;
  i::FLAG_turbo_inlining = true;
  i::FLAG_stress_inline = true;
  i::FLAG_always_opt = false;
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  const char* source = R"(
    function foo(x) {
      return bar(x) + 1;
    }

    function bar(x) {
      var y = 1;
      for (var i = 0; i < x; ++i) {
        y = y * x;
      }
      return x;
    }

    %EnsureFeedbackVectorForFunction(bar);
    %PrepareFunctionForOptimization(foo);
    foo(5);
    %OptimizeFunctionOnNextCall(foo);
    foo(5);
    foo;
  )";

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

    CHECK(!i_isolate->NeedsDetailedOptimizedCodeLineInfo());

    int non_detailed_positions =
        GetSourcePositionEntryCount(i_isolate, source, EntryCountMode::kAll);
    int non_detailed_inlined_positions = GetSourcePositionEntryCount(
        i_isolate, source, EntryCountMode::kOnlyInlined);

    v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);
    CHECK(i_isolate->NeedsDetailedOptimizedCodeLineInfo());

    int detailed_positions =
        GetSourcePositionEntryCount(i_isolate, source, EntryCountMode::kAll);
    int detailed_inlined_positions = GetSourcePositionEntryCount(
        i_isolate, source, EntryCountMode::kOnlyInlined);

    if (non_detailed_positions == -1) {
      CHECK_EQ(non_detailed_positions, detailed_positions);
    } else {
      CHECK_LT(non_detailed_positions, detailed_positions);
      CHECK_LT(non_detailed_inlined_positions, detailed_inlined_positions);
    }
  }

  isolate->Dispose();
}

}  // namespace test_cpu_profiler
}  // namespace internal
}  // namespace v8
