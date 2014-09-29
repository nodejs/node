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

#include "src/v8.h"

#include "include/v8-profiler.h"
#include "src/base/platform/platform.h"
#include "src/cpu-profiler-inl.h"
#include "src/smart-pointers.h"
#include "src/utils.h"
#include "test/cctest/cctest.h"
#include "test/cctest/profiler-extension.h"
using i::CodeEntry;
using i::CpuProfile;
using i::CpuProfiler;
using i::CpuProfilesCollection;
using i::Heap;
using i::ProfileGenerator;
using i::ProfileNode;
using i::ProfilerEventsProcessor;
using i::ScopedVector;
using i::SmartPointer;
using i::Vector;


TEST(StartStop) {
  i::Isolate* isolate = CcTest::i_isolate();
  CpuProfilesCollection profiles(isolate->heap());
  ProfileGenerator generator(&profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, v8::base::TimeDelta::FromMicroseconds(100)));
  processor->Start();
  processor->StopSynchronously();
}


static void EnqueueTickSampleEvent(ProfilerEventsProcessor* proc,
                                   i::Address frame1,
                                   i::Address frame2 = NULL,
                                   i::Address frame3 = NULL) {
  i::TickSample* sample = proc->StartTickSample();
  sample->pc = frame1;
  sample->tos = frame1;
  sample->frames_count = 0;
  if (frame2 != NULL) {
    sample->stack[0] = frame2;
    sample->frames_count = 1;
  }
  if (frame3 != NULL) {
    sample->stack[1] = frame3;
    sample->frames_count = 2;
  }
  proc->FinishTickSample();
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


i::Code* CreateCode(LocalContext* env) {
  static int counter = 0;
  i::EmbeddedVector<char, 256> script;
  i::EmbeddedVector<char, 32> name;

  i::SNPrintF(name, "function_%d", ++counter);
  const char* name_start = name.start();
  i::SNPrintF(script,
      "function %s() {\n"
           "var counter = 0;\n"
           "for (var i = 0; i < %d; ++i) counter += i;\n"
           "return '%s_' + counter;\n"
       "}\n"
       "%s();\n", name_start, counter, name_start, name_start);
  CompileRun(script.start());
  i::Handle<i::JSFunction> fun = v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(
          (*env)->Global()->Get(v8_str(name_start))));
  return fun->code();
}


TEST(CodeEvents) {
  CcTest::InitializeVM();
  LocalContext env;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  TestSetup test_setup;

  i::HandleScope scope(isolate);

  i::Code* aaa_code = CreateCode(&env);
  i::Code* comment_code = CreateCode(&env);
  i::Code* args5_code = CreateCode(&env);
  i::Code* comment2_code = CreateCode(&env);
  i::Code* moved_code = CreateCode(&env);
  i::Code* args3_code = CreateCode(&env);
  i::Code* args4_code = CreateCode(&env);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate->heap());
  profiles->StartProfiling("", false);
  ProfileGenerator generator(profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, v8::base::TimeDelta::FromMicroseconds(100)));
  processor->Start();
  CpuProfiler profiler(isolate, profiles, &generator, processor.get());

  // Enqueue code creation events.
  const char* aaa_str = "aaa";
  i::Handle<i::String> aaa_name = factory->NewStringFromAsciiChecked(aaa_str);
  profiler.CodeCreateEvent(i::Logger::FUNCTION_TAG, aaa_code, *aaa_name);
  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, comment_code, "comment");
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, args5_code, 5);
  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, comment2_code, "comment2");
  profiler.CodeMoveEvent(comment2_code->address(), moved_code->address());
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, args3_code, 3);
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, args4_code, 4);

  // Enqueue a tick event to enable code events processing.
  EnqueueTickSampleEvent(processor.get(), aaa_code->address());

  processor->StopSynchronously();

  // Check the state of profile generator.
  CodeEntry* aaa = generator.code_map()->FindEntry(aaa_code->address());
  CHECK_NE(NULL, aaa);
  CHECK_EQ(aaa_str, aaa->name());

  CodeEntry* comment = generator.code_map()->FindEntry(comment_code->address());
  CHECK_NE(NULL, comment);
  CHECK_EQ("comment", comment->name());

  CodeEntry* args5 = generator.code_map()->FindEntry(args5_code->address());
  CHECK_NE(NULL, args5);
  CHECK_EQ("5", args5->name());

  CHECK_EQ(NULL, generator.code_map()->FindEntry(comment2_code->address()));

  CodeEntry* comment2 = generator.code_map()->FindEntry(moved_code->address());
  CHECK_NE(NULL, comment2);
  CHECK_EQ("comment2", comment2->name());
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

  i::Code* frame1_code = CreateCode(&env);
  i::Code* frame2_code = CreateCode(&env);
  i::Code* frame3_code = CreateCode(&env);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate->heap());
  profiles->StartProfiling("", false);
  ProfileGenerator generator(profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, v8::base::TimeDelta::FromMicroseconds(100)));
  processor->Start();
  CpuProfiler profiler(isolate, profiles, &generator, processor.get());

  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, frame1_code, "bbb");
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, frame2_code, 5);
  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, frame3_code, "ddd");

  EnqueueTickSampleEvent(processor.get(), frame1_code->instruction_start());
  EnqueueTickSampleEvent(
      processor.get(),
      frame2_code->instruction_start() + frame2_code->ExecutableSize() / 2,
      frame1_code->instruction_start() + frame2_code->ExecutableSize() / 2);
  EnqueueTickSampleEvent(
      processor.get(),
      frame3_code->instruction_end() - 1,
      frame2_code->instruction_end() - 1,
      frame1_code->instruction_end() - 1);

  processor->StopSynchronously();
  CpuProfile* profile = profiles->StopProfiling("");
  CHECK_NE(NULL, profile);

  // Check call trees.
  const i::List<ProfileNode*>* top_down_root_children =
      profile->top_down()->root()->children();
  CHECK_EQ(1, top_down_root_children->length());
  CHECK_EQ("bbb", top_down_root_children->last()->entry()->name());
  const i::List<ProfileNode*>* top_down_bbb_children =
      top_down_root_children->last()->children();
  CHECK_EQ(1, top_down_bbb_children->length());
  CHECK_EQ("5", top_down_bbb_children->last()->entry()->name());
  const i::List<ProfileNode*>* top_down_stub_children =
      top_down_bbb_children->last()->children();
  CHECK_EQ(1, top_down_stub_children->length());
  CHECK_EQ("ddd", top_down_stub_children->last()->entry()->name());
  const i::List<ProfileNode*>* top_down_ddd_children =
      top_down_stub_children->last()->children();
  CHECK_EQ(0, top_down_ddd_children->length());
}


// http://crbug/51594
// This test must not crash.
TEST(CrashIfStoppingLastNonExistentProfile) {
  CcTest::InitializeVM();
  TestSetup test_setup;
  CpuProfiler* profiler = CcTest::i_isolate()->cpu_profiler();
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

  i::Code* code = CreateCode(&env);

  CpuProfilesCollection* profiles = new CpuProfilesCollection(isolate->heap());
  profiles->StartProfiling("", false);
  ProfileGenerator generator(profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, v8::base::TimeDelta::FromMicroseconds(100)));
  processor->Start();
  CpuProfiler profiler(isolate, profiles, &generator, processor.get());

  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, code, "bbb");

  i::TickSample* sample = processor->StartTickSample();
  sample->pc = code->address();
  sample->tos = 0;
  sample->frames_count = i::TickSample::kMaxFramesCount;
  for (unsigned i = 0; i < sample->frames_count; ++i) {
    sample->stack[i] = code->address();
  }
  processor->FinishTickSample();

  processor->StopSynchronously();
  CpuProfile* profile = profiles->StopProfiling("");
  CHECK_NE(NULL, profile);

  int actual_depth = 0;
  const ProfileNode* node = profile->top_down()->root();
  while (node->children()->length() > 0) {
    node = node->children()->last();
    ++actual_depth;
  }

  CHECK_EQ(1 + i::TickSample::kMaxFramesCount, actual_depth);  // +1 for PC.
}


TEST(DeleteAllCpuProfiles) {
  CcTest::InitializeVM();
  TestSetup test_setup;
  CpuProfiler* profiler = CcTest::i_isolate()->cpu_profiler();
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
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  i::CpuProfiler* iprofiler = reinterpret_cast<i::CpuProfiler*>(cpu_profiler);

  CHECK_EQ(0, iprofiler->GetProfilesCount());
  v8::Local<v8::String> name1 = v8::String::NewFromUtf8(env->GetIsolate(), "1");
  cpu_profiler->StartProfiling(name1);
  v8::CpuProfile* p1 = cpu_profiler->StopProfiling(name1);
  CHECK_NE(NULL, p1);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  CHECK(FindCpuProfile(cpu_profiler, p1));
  p1->Delete();
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::Local<v8::String> name2 = v8::String::NewFromUtf8(env->GetIsolate(), "2");
  cpu_profiler->StartProfiling(name2);
  v8::CpuProfile* p2 = cpu_profiler->StopProfiling(name2);
  CHECK_NE(NULL, p2);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  CHECK(FindCpuProfile(cpu_profiler, p2));
  v8::Local<v8::String> name3 = v8::String::NewFromUtf8(env->GetIsolate(), "3");
  cpu_profiler->StartProfiling(name3);
  v8::CpuProfile* p3 = cpu_profiler->StopProfiling(name3);
  CHECK_NE(NULL, p3);
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
}


TEST(ProfileStartEndTime) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();

  v8::Local<v8::String> profile_name =
      v8::String::NewFromUtf8(env->GetIsolate(), "test");
  cpu_profiler->StartProfiling(profile_name);
  const v8::CpuProfile* profile = cpu_profiler->StopProfiling(profile_name);
  CHECK(profile->GetStartTime() <= profile->GetEndTime());
}


static v8::CpuProfile* RunProfiler(
    v8::Handle<v8::Context> env, v8::Handle<v8::Function> function,
    v8::Handle<v8::Value> argv[], int argc,
    unsigned min_js_samples, bool collect_samples = false) {
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  v8::Local<v8::String> profile_name =
      v8::String::NewFromUtf8(env->GetIsolate(), "my_profile");

  cpu_profiler->StartProfiling(profile_name, collect_samples);

  i::Sampler* sampler =
      reinterpret_cast<i::Isolate*>(env->GetIsolate())->logger()->sampler();
  sampler->StartCountingSamples();
  do {
    function->Call(env->Global(), argc, argv);
  } while (sampler->js_and_external_sample_count() < min_js_samples);

  v8::CpuProfile* profile = cpu_profiler->StopProfiling(profile_name);

  CHECK_NE(NULL, profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  return profile;
}


static bool ContainsString(v8::Handle<v8::String> string,
                           const Vector<v8::Handle<v8::String> >& vector) {
  for (int i = 0; i < vector.length(); i++) {
    if (string->Equals(vector[i]))
      return true;
  }
  return false;
}


static void CheckChildrenNames(const v8::CpuProfileNode* node,
                               const Vector<v8::Handle<v8::String> >& names) {
  int count = node->GetChildrenCount();
  for (int i = 0; i < count; i++) {
    v8::Handle<v8::String> name = node->GetChild(i)->GetFunctionName();
    CHECK(ContainsString(name, names));
    // Check that there are no duplicates.
    for (int j = 0; j < count; j++) {
      if (j == i) continue;
      CHECK_NE(name, node->GetChild(j)->GetFunctionName());
    }
  }
}


static const v8::CpuProfileNode* FindChild(v8::Isolate* isolate,
                                           const v8::CpuProfileNode* node,
                                           const char* name) {
  int count = node->GetChildrenCount();
  v8::Handle<v8::String> nameHandle = v8::String::NewFromUtf8(isolate, name);
  for (int i = 0; i < count; i++) {
    const v8::CpuProfileNode* child = node->GetChild(i);
    if (nameHandle->Equals(child->GetFunctionName())) return child;
  }
  return NULL;
}


static const v8::CpuProfileNode* GetChild(v8::Isolate* isolate,
                                          const v8::CpuProfileNode* node,
                                          const char* name) {
  const v8::CpuProfileNode* result = FindChild(isolate, node, name);
  if (!result) {
    char buffer[100];
    i::SNPrintF(Vector<char>(buffer, ARRAY_SIZE(buffer)),
                "Failed to GetChild: %s", name);
    FATAL(buffer);
  }
  return result;
}


static void CheckSimpleBranch(v8::Isolate* isolate,
                              const v8::CpuProfileNode* node,
                              const char* names[], int length) {
  for (int i = 0; i < length; i++) {
    const char* name = names[i];
    node = GetChild(isolate, node, name);
    int expectedChildrenCount = (i == length - 1) ? 0 : 1;
    CHECK_EQ(expectedChildrenCount, node->GetChildrenCount());
  }
}


static const char* cpu_profiler_test_source = "function loop(timeout) {\n"
"  this.mmm = 0;\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < timeout) {\n"
"    var n = 100*1000;\n"
"    while(n > 1) {\n"
"      n--;\n"
"      this.mmm += n * n * n;\n"
"    }\n"
"  }\n"
"}\n"
"function delay() { try { loop(10); } catch(e) { } }\n"
"function bar() { delay(); }\n"
"function baz() { delay(); }\n"
"function foo() {\n"
"    try {\n"
"       delay();\n"
"       bar();\n"
"       delay();\n"
"       baz();\n"
"    } catch (e) { }\n"
"}\n"
"function start(timeout) {\n"
"  var start = Date.now();\n"
"  do {\n"
"    foo();\n"
"    var duration = Date.now() - start;\n"
"  } while (duration < timeout);\n"
"  return duration;\n"
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Script::Compile(v8::String::NewFromUtf8(env->GetIsolate(),
                                              cpu_profiler_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  int32_t profiling_interval_ms = 200;
  v8::Handle<v8::Value> args[] = {
    v8::Integer::New(env->GetIsolate(), profiling_interval_ms)
  };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 200);
  function->Call(env->Global(), ARRAY_SIZE(args), args);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();

  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode =
      GetChild(env->GetIsolate(), root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());

  const v8::CpuProfileNode* fooNode =
      GetChild(env->GetIsolate(), startNode, "foo");
  CHECK_EQ(3, fooNode->GetChildrenCount());

  const char* barBranch[] = { "bar", "delay", "loop" };
  CheckSimpleBranch(env->GetIsolate(), fooNode, barBranch,
                    ARRAY_SIZE(barBranch));
  const char* bazBranch[] = { "baz", "delay", "loop" };
  CheckSimpleBranch(env->GetIsolate(), fooNode, bazBranch,
                    ARRAY_SIZE(bazBranch));
  const char* delayBranch[] = { "delay", "loop" };
  CheckSimpleBranch(env->GetIsolate(), fooNode, delayBranch,
                    ARRAY_SIZE(delayBranch));

  profile->Delete();
}


static const char* hot_deopt_no_frame_entry_test_source =
"function foo(a, b) {\n"
"    try {\n"
"      return a + b;\n"
"    } catch (e) { }\n"
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
// The test checks no FP ranges are present in a deoptimized funcion.
// If 'foo' has no ranges the samples falling into the prologue will miss the
// 'start' function on the stack, so 'foo' will be attached to the (root).
TEST(HotDeoptNoFrameEntry) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Script::Compile(v8::String::NewFromUtf8(
      env->GetIsolate(),
      hot_deopt_no_frame_entry_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  int32_t profiling_interval_ms = 200;
  v8::Handle<v8::Value> args[] = {
    v8::Integer::New(env->GetIsolate(), profiling_interval_ms)
  };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 200);
  function->Call(env->Global(), ARRAY_SIZE(args), args);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();

  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode =
      GetChild(env->GetIsolate(), root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());

  GetChild(env->GetIsolate(), startNode, "foo");

  profile->Delete();
}


TEST(CollectCpuProfileSamples) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Script::Compile(v8::String::NewFromUtf8(env->GetIsolate(),
                                              cpu_profiler_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  int32_t profiling_interval_ms = 200;
  v8::Handle<v8::Value> args[] = {
    v8::Integer::New(env->GetIsolate(), profiling_interval_ms)
  };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 200, true);

  CHECK_LE(200, profile->GetSamplesCount());
  uint64_t end_time = profile->GetEndTime();
  uint64_t current_time = profile->GetStartTime();
  CHECK_LE(current_time, end_time);
  for (int i = 0; i < profile->GetSamplesCount(); i++) {
    CHECK_NE(NULL, profile->GetSample(i));
    uint64_t timestamp = profile->GetSampleTimestamp(i);
    CHECK_LE(current_time, timestamp);
    CHECK_LE(timestamp, end_time);
    current_time = timestamp;
  }

  profile->Delete();
}


static const char* cpu_profiler_test_source2 = "function loop() {}\n"
"function delay() { loop(); }\n"
"function start(count) {\n"
"  var k = 0;\n"
"  do {\n"
"    delay();\n"
"  } while (++k < count*100*1000);\n"
"}\n";

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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Script::Compile(v8::String::NewFromUtf8(
                          env->GetIsolate(), cpu_profiler_test_source2))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  int32_t repeat_count = 100;
#if defined(USE_SIMULATOR)
  // Simulators are much slower.
  repeat_count = 1;
#endif
  v8::Handle<v8::Value> args[] = {
    v8::Integer::New(env->GetIsolate(), repeat_count)
  };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();

  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode =
      FindChild(env->GetIsolate(), root, "start");
  // On slow machines there may be no meaningfull samples at all, skip the
  // check there.
  if (startNode && startNode->GetChildrenCount() > 0) {
    CHECK_EQ(1, startNode->GetChildrenCount());
    const v8::CpuProfileNode* delayNode =
        GetChild(env->GetIsolate(), startNode, "delay");
    if (delayNode->GetChildrenCount() > 0) {
      CHECK_EQ(1, delayNode->GetChildrenCount());
      GetChild(env->GetIsolate(), delayNode, "loop");
    }
  }

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
    TestApiCallbacks* data = fromInfo(info);
    data->Wait();
  }

  static void Setter(v8::Local<v8::String> name,
                     v8::Local<v8::Value> value,
                     const v8::PropertyCallbackInfo<void>& info) {
    TestApiCallbacks* data = fromInfo(info);
    data->Wait();
  }

  static void Callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
    TestApiCallbacks* data = fromInfo(info);
    data->Wait();
  }

  void set_warming_up(bool value) { is_warming_up_ = value; }

 private:
  void Wait() {
    if (is_warming_up_) return;
    double start = v8::base::OS::TimeCurrentMillis();
    double duration = 0;
    while (duration < min_duration_ms_) {
      v8::base::OS::Sleep(1);
      duration = v8::base::OS::TimeCurrentMillis() - start;
    }
  }

  template<typename T>
  static TestApiCallbacks* fromInfo(const T& info) {
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
  v8::Local<v8::External> data =
      v8::External::New(isolate, &accessors);
  instance_template->SetAccessor(
      v8::String::NewFromUtf8(isolate, "foo"),
      &TestApiCallbacks::Getter, &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::NewFromUtf8(isolate, "instance"),
                     instance);

  v8::Script::Compile(
      v8::String::NewFromUtf8(isolate, native_accessor_test_source))
      ->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(isolate, "start")));

  int32_t repeat_count = 1;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(isolate, repeat_count) };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 180);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* startNode =
      GetChild(isolate, root, "start");
  GetChild(isolate, startNode, "get foo");
  GetChild(isolate, startNode, "set foo");

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
  instance_template->SetAccessor(
      v8::String::NewFromUtf8(isolate, "foo"),
      &TestApiCallbacks::Getter, &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::NewFromUtf8(isolate, "instance"),
                     instance);

  v8::Script::Compile(
      v8::String::NewFromUtf8(isolate, native_accessor_test_source))
      ->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(isolate, "start")));

  {
    // Make sure accessors ICs are in monomorphic state before starting
    // profiling.
    accessors.set_warming_up(true);
    int32_t warm_up_iterations = 3;
    v8::Handle<v8::Value> args[] = {
      v8::Integer::New(isolate, warm_up_iterations)
    };
    function->Call(env->Global(), ARRAY_SIZE(args), args);
    accessors.set_warming_up(false);
  }

  int32_t repeat_count = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(isolate, repeat_count) };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 200);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* startNode =
      GetChild(isolate, root, "start");
  GetChild(isolate, startNode, "get foo");
  GetChild(isolate, startNode, "set foo");

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
  v8::Local<v8::External> data =
      v8::External::New(isolate, &callbacks);

  v8::Local<v8::FunctionTemplate> func_template =
      v8::FunctionTemplate::New(isolate);
  func_template->SetClassName(
      v8::String::NewFromUtf8(isolate, "Test_InstanceCostructor"));
  v8::Local<v8::ObjectTemplate> proto_template =
      func_template->PrototypeTemplate();
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, func_template);
  proto_template->Set(v8::String::NewFromUtf8(isolate, "fooMethod"),
                      v8::FunctionTemplate::New(isolate,
                                                &TestApiCallbacks::Callback,
                                                data, signature, 0));

  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::NewFromUtf8(isolate, "instance"),
                     instance);

  v8::Script::Compile(v8::String::NewFromUtf8(
                          isolate, native_method_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(isolate, "start")));

  int32_t repeat_count = 1;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(isolate, repeat_count) };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* startNode =
      GetChild(isolate, root, "start");
  GetChild(isolate, startNode, "fooMethod");

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
  func_template->SetClassName(
      v8::String::NewFromUtf8(isolate, "Test_InstanceCostructor"));
  v8::Local<v8::ObjectTemplate> proto_template =
      func_template->PrototypeTemplate();
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, func_template);
  proto_template->Set(v8::String::NewFromUtf8(isolate, "fooMethod"),
                      v8::FunctionTemplate::New(isolate,
                                                &TestApiCallbacks::Callback,
                                                data, signature, 0));

  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::NewFromUtf8(isolate, "instance"),
                     instance);

  v8::Script::Compile(v8::String::NewFromUtf8(
                          isolate, native_method_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(isolate, "start")));
  {
    // Make sure method ICs are in monomorphic state before starting
    // profiling.
    callbacks.set_warming_up(true);
    int32_t warm_up_iterations = 3;
    v8::Handle<v8::Value> args[] = {
      v8::Integer::New(isolate, warm_up_iterations)
    };
    function->Call(env->Global(), ARRAY_SIZE(args), args);
    callbacks.set_warming_up(false);
  }

  int32_t repeat_count = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(isolate, repeat_count) };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  GetChild(isolate, root, "start");
  const v8::CpuProfileNode* startNode =
      GetChild(isolate, root, "start");
  GetChild(isolate, startNode, "fooMethod");

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
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);

  v8::Script::Compile(
      v8::String::NewFromUtf8(env->GetIsolate(), bound_function_test_source))
      ->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  v8::CpuProfile* profile = RunProfiler(env, function, NULL, 0, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
  // Don't allow |foo| node to be at the top level.
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode =
      GetChild(env->GetIsolate(), root, "start");
  GetChild(env->GetIsolate(), startNode, "foo");

  profile->Delete();
}


static const char* call_function_test_source = "function bar(iterations) {\n"
"}\n"
"function start(duration) {\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < duration) {\n"
"    try {\n"
"      bar.call(this, 10 * 1000);\n"
"    } catch(e) {}\n"
"  }\n"
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Collect garbage that might have be generated while installing extensions.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  v8::Script::Compile(v8::String::NewFromUtf8(
                          env->GetIsolate(), call_function_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  int32_t duration_ms = 100;
  v8::Handle<v8::Value> args[] = {
    v8::Integer::New(env->GetIsolate(), duration_ms)
  };
  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(4);
    names[0] = v8::String::NewFromUtf8(
        env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                       ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
    names[3] = v8::String::NewFromUtf8(
        env->GetIsolate(), i::ProfileGenerator::kUnresolvedFunctionName);
    // Don't allow |bar| and |call| nodes to be at the top level.
    CheckChildrenNames(root, names);
  }

  // In case of GC stress tests all samples may be in GC phase and there
  // won't be |start| node in the profiles.
  bool is_gc_stress_testing =
      (i::FLAG_gc_interval != -1) || i::FLAG_stress_compaction;
  const v8::CpuProfileNode* startNode =
      FindChild(env->GetIsolate(), root, "start");
  CHECK(is_gc_stress_testing || startNode);
  if (startNode) {
    ScopedVector<v8::Handle<v8::String> > names(2);
    names[0] = v8::String::NewFromUtf8(env->GetIsolate(), "bar");
    names[1] = v8::String::NewFromUtf8(env->GetIsolate(), "call");
    CheckChildrenNames(startNode, names);
  }

  const v8::CpuProfileNode* unresolvedNode = FindChild(
      env->GetIsolate(), root, i::ProfileGenerator::kUnresolvedFunctionName);
  if (unresolvedNode) {
    ScopedVector<v8::Handle<v8::String> > names(1);
    names[0] = v8::String::NewFromUtf8(env->GetIsolate(), "call");
    CheckChildrenNames(unresolvedNode, names);
  }

  profile->Delete();
}


static const char* function_apply_test_source = "function bar(iterations) {\n"
"}\n"
"function test() {\n"
"  bar.apply(this, [10 * 1000]);\n"
"}\n"
"function start(duration) {\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < duration) {\n"
"    try {\n"
"      test();\n"
"    } catch(e) {}\n"
"  }\n"
"}";


// [Top down]:
//    94     0   (root) [-1] #0 1
//     2     2    (garbage collector) [-1] #0 7
//    82    49    start [-1] #16 3
//     1     0      (unresolved function) [-1] #0 8
//     1     1        apply [-1] #0 9
//    32    21      test [-1] #16 4
//     2     2        bar [-1] #16 6
//     9     9        apply [-1] #0 5
//    10    10    (program) [-1] #0 2
TEST(FunctionApplySample) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Script::Compile(
      v8::String::NewFromUtf8(env->GetIsolate(), function_apply_test_source))
      ->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  int32_t duration_ms = 100;
  v8::Handle<v8::Value> args[] = {
    v8::Integer::New(env->GetIsolate(), duration_ms)
  };

  v8::CpuProfile* profile =
      RunProfiler(env.local(), function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(3);
    names[0] = v8::String::NewFromUtf8(
        env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                       ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
    // Don't allow |test|, |bar| and |apply| nodes to be at the top level.
    CheckChildrenNames(root, names);
  }

  const v8::CpuProfileNode* startNode =
      FindChild(env->GetIsolate(), root, "start");
  if (startNode) {
    {
      ScopedVector<v8::Handle<v8::String> > names(2);
      names[0] = v8::String::NewFromUtf8(env->GetIsolate(), "test");
      names[1] = v8::String::NewFromUtf8(
          env->GetIsolate(), ProfileGenerator::kUnresolvedFunctionName);
      CheckChildrenNames(startNode, names);
    }

    const v8::CpuProfileNode* testNode =
        FindChild(env->GetIsolate(), startNode, "test");
    if (testNode) {
      ScopedVector<v8::Handle<v8::String> > names(3);
      names[0] = v8::String::NewFromUtf8(env->GetIsolate(), "bar");
      names[1] = v8::String::NewFromUtf8(env->GetIsolate(), "apply");
      // apply calls "get length" before invoking the function itself
      // and we may get hit into it.
      names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "get length");
      CheckChildrenNames(testNode, names);
    }

    if (const v8::CpuProfileNode* unresolvedNode =
            FindChild(env->GetIsolate(), startNode,
                      ProfileGenerator::kUnresolvedFunctionName)) {
      ScopedVector<v8::Handle<v8::String> > names(1);
      names[0] = v8::String::NewFromUtf8(env->GetIsolate(), "apply");
      CheckChildrenNames(unresolvedNode, names);
      GetChild(env->GetIsolate(), unresolvedNode, "apply");
    }
  }

  profile->Delete();
}


static const char* cpu_profiler_deep_stack_test_source =
"function foo(n) {\n"
"  if (n)\n"
"    foo(n - 1);\n"
"  else\n"
"    startProfiling('my_profile');\n"
"}\n"
"function start() {\n"
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
//    0          foo 21 #253 no reason
//    1            startProfiling 0 #254
TEST(CpuProfileDeepStack) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);

  v8::Script::Compile(v8::String::NewFromUtf8(
      env->GetIsolate(), cpu_profiler_deep_stack_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  v8::Local<v8::String> profile_name =
      v8::String::NewFromUtf8(env->GetIsolate(), "my_profile");
  function->Call(env->Global(), 0, NULL);
  v8::CpuProfile* profile = cpu_profiler->StopProfiling(profile_name);
  CHECK_NE(NULL, profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(3);
    names[0] = v8::String::NewFromUtf8(
        env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                       ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
    CheckChildrenNames(root, names);
  }

  const v8::CpuProfileNode* node =
      GetChild(env->GetIsolate(), root, "start");
  for (int i = 0; i < 250; ++i) {
    node = GetChild(env->GetIsolate(), node, "foo");
  }
  // TODO(alph):
  // In theory there must be one more 'foo' and a 'startProfiling' nodes,
  // but due to unstable top frame extraction these might be missing.

  profile->Delete();
}


static const char* js_native_js_test_source =
    "function foo() {\n"
    "  startProfiling('my_profile');\n"
    "}\n"
    "function bar() {\n"
    "  try { foo(); } catch(e) {}\n"
    "}\n"
    "function start() {\n"
    "  try {\n"
    "    CallJsFunction(bar);\n"
    "  } catch(e) {}\n"
    "}";

static void CallJsFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Handle<v8::Function> function = info[0].As<v8::Function>();
  v8::Handle<v8::Value> argv[] = { info[1] };
  function->Call(info.This(), ARRAY_SIZE(argv), argv);
}


// [Top down]:
//    58     0   (root) #0 1
//     2     2    (program) #0 2
//    56     1    start #16 3
//    55     0      CallJsFunction #0 4
//    55     1        bar #16 5
//    54    54          foo #16 6
TEST(JsNativeJsSample) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      env->GetIsolate(), CallJsFunction);
  v8::Local<v8::Function> func = func_template->GetFunction();
  func->SetName(v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction"));
  env->Global()->Set(
      v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction"), func);

  v8::Script::Compile(v8::String::NewFromUtf8(env->GetIsolate(),
                                              js_native_js_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  v8::CpuProfile* profile = RunProfiler(env, function, NULL, 0, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(3);
    names[0] = v8::String::NewFromUtf8(
        env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                       ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
    CheckChildrenNames(root, names);
  }

  const v8::CpuProfileNode* startNode =
      GetChild(env->GetIsolate(), root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeFunctionNode =
      GetChild(env->GetIsolate(), startNode, "CallJsFunction");

  CHECK_EQ(1, nativeFunctionNode->GetChildrenCount());
  const v8::CpuProfileNode* barNode =
      GetChild(env->GetIsolate(), nativeFunctionNode, "bar");

  CHECK_EQ(1, barNode->GetChildrenCount());
  GetChild(env->GetIsolate(), barNode, "foo");

  profile->Delete();
}


static const char* js_native_js_runtime_js_test_source =
    "function foo() {\n"
    "  startProfiling('my_profile');\n"
    "}\n"
    "var bound = foo.bind(this);\n"
    "function bar() {\n"
    "  try { bound(); } catch(e) {}\n"
    "}\n"
    "function start() {\n"
    "  try {\n"
    "    CallJsFunction(bar);\n"
    "  } catch(e) {}\n"
    "}";


// [Top down]:
//    57     0   (root) #0 1
//    55     1    start #16 3
//    54     0      CallJsFunction #0 4
//    54     3        bar #16 5
//    51    51          foo #16 6
//     2     2    (program) #0 2
TEST(JsNativeJsRuntimeJsSample) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      env->GetIsolate(), CallJsFunction);
  v8::Local<v8::Function> func = func_template->GetFunction();
  func->SetName(v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction"));
  env->Global()->Set(
      v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction"), func);

  v8::Script::Compile(
      v8::String::NewFromUtf8(env->GetIsolate(),
                              js_native_js_runtime_js_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  v8::CpuProfile* profile = RunProfiler(env, function, NULL, 0, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode =
      GetChild(env->GetIsolate(), root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeFunctionNode =
      GetChild(env->GetIsolate(), startNode, "CallJsFunction");

  CHECK_EQ(1, nativeFunctionNode->GetChildrenCount());
  const v8::CpuProfileNode* barNode =
      GetChild(env->GetIsolate(), nativeFunctionNode, "bar");

  // The child is in fact a bound foo.
  // A bound function has a wrapper that may make calls to
  // other functions e.g. "get length".
  CHECK_LE(1, barNode->GetChildrenCount());
  CHECK_GE(2, barNode->GetChildrenCount());
  GetChild(env->GetIsolate(), barNode, "foo");

  profile->Delete();
}


static void CallJsFunction2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::base::OS::Print("In CallJsFunction2\n");
  CallJsFunction(info);
}


static const char* js_native1_js_native2_js_test_source =
    "function foo() {\n"
    "  try {\n"
    "    startProfiling('my_profile');\n"
    "  } catch(e) {}\n"
    "}\n"
    "function bar() {\n"
    "  CallJsFunction2(foo);\n"
    "}\n"
    "function start() {\n"
    "  try {\n"
    "    CallJsFunction1(bar);\n"
    "  } catch(e) {}\n"
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
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      env->GetIsolate(), CallJsFunction);
  v8::Local<v8::Function> func1 = func_template->GetFunction();
  func1->SetName(v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction1"));
  env->Global()->Set(
      v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction1"), func1);

  v8::Local<v8::Function> func2 = v8::FunctionTemplate::New(
      env->GetIsolate(), CallJsFunction2)->GetFunction();
  func2->SetName(v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction2"));
  env->Global()->Set(
      v8::String::NewFromUtf8(env->GetIsolate(), "CallJsFunction2"), func2);

  v8::Script::Compile(
      v8::String::NewFromUtf8(env->GetIsolate(),
                              js_native1_js_native2_js_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "start")));

  v8::CpuProfile* profile = RunProfiler(env, function, NULL, 0, 0);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(), "start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode =
      GetChild(env->GetIsolate(), root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeNode1 =
      GetChild(env->GetIsolate(), startNode, "CallJsFunction1");

  CHECK_EQ(1, nativeNode1->GetChildrenCount());
  const v8::CpuProfileNode* barNode =
      GetChild(env->GetIsolate(), nativeNode1, "bar");

  CHECK_EQ(1, barNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeNode2 =
      GetChild(env->GetIsolate(), barNode, "CallJsFunction2");

  CHECK_EQ(1, nativeNode2->GetChildrenCount());
  GetChild(env->GetIsolate(), nativeNode2, "foo");

  profile->Delete();
}


// [Top down]:
//     6     0   (root) #0 1
//     3     3    (program) #0 2
//     3     3    (idle) #0 3
TEST(IdleTime) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();

  v8::Local<v8::String> profile_name =
      v8::String::NewFromUtf8(env->GetIsolate(), "my_profile");
  cpu_profiler->StartProfiling(profile_name);

  i::Isolate* isolate = CcTest::i_isolate();
  i::ProfilerEventsProcessor* processor = isolate->cpu_profiler()->processor();
  processor->AddCurrentStack(isolate);

  cpu_profiler->SetIdle(true);

  for (int i = 0; i < 3; i++) {
    processor->AddCurrentStack(isolate);
  }

  cpu_profiler->SetIdle(false);
  processor->AddCurrentStack(isolate);


  v8::CpuProfile* profile = cpu_profiler->StopProfiling(profile_name);
  CHECK_NE(NULL, profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(profile)->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::NewFromUtf8(
      env->GetIsolate(), ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::NewFromUtf8(env->GetIsolate(),
                                     ProfileGenerator::kIdleEntryName);
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* programNode =
      GetChild(env->GetIsolate(), root, ProfileGenerator::kProgramEntryName);
  CHECK_EQ(0, programNode->GetChildrenCount());
  CHECK_GE(programNode->GetHitCount(), 3);

  const v8::CpuProfileNode* idleNode =
      GetChild(env->GetIsolate(), root, ProfileGenerator::kIdleEntryName);
  CHECK_EQ(0, idleNode->GetChildrenCount());
  CHECK_GE(idleNode->GetHitCount(), 3);

  profile->Delete();
}


static void CheckFunctionDetails(v8::Isolate* isolate,
                                 const v8::CpuProfileNode* node,
                                 const char* name, const char* script_name,
                                 int script_id, int line, int column) {
  CHECK_EQ(v8::String::NewFromUtf8(isolate, name),
           node->GetFunctionName());
  CHECK_EQ(v8::String::NewFromUtf8(isolate, script_name),
           node->GetScriptResourceName());
  CHECK_EQ(script_id, node->GetScriptId());
  CHECK_EQ(line, node->GetLineNumber());
  CHECK_EQ(column, node->GetColumnNumber());
}


TEST(FunctionDetails) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);

  v8::Handle<v8::Script> script_a = CompileWithOrigin(
          "    function foo\n() { try { bar(); } catch(e) {} }\n"
          " function bar() { startProfiling(); }\n",
          "script_a");
  script_a->Run();
  v8::Handle<v8::Script> script_b = CompileWithOrigin(
          "\n\n   function baz() { try { foo(); } catch(e) {} }\n"
          "\n\nbaz();\n"
          "stopProfiling();\n",
          "script_b");
  script_b->Run();
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
  const v8::CpuProfileNode* script = GetChild(env->GetIsolate(), root, "");
  CheckFunctionDetails(env->GetIsolate(), script, "", "script_b",
                       script_b->GetUnboundScript()->GetId(), 1, 1);
  const v8::CpuProfileNode* baz = GetChild(env->GetIsolate(), script, "baz");
  CheckFunctionDetails(env->GetIsolate(), baz, "baz", "script_b",
                       script_b->GetUnboundScript()->GetId(), 3, 16);
  const v8::CpuProfileNode* foo = GetChild(env->GetIsolate(), baz, "foo");
  CheckFunctionDetails(env->GetIsolate(), foo, "foo", "script_a",
                       script_a->GetUnboundScript()->GetId(), 2, 1);
  const v8::CpuProfileNode* bar = GetChild(env->GetIsolate(), foo, "bar");
  CheckFunctionDetails(env->GetIsolate(), bar, "bar", "script_a",
                       script_a->GetUnboundScript()->GetId(), 3, 14);
}


TEST(DontStopOnFinishedProfileDelete) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = CcTest::NewContext(PROFILER_EXTENSION);
  v8::Context::Scope context_scope(env);
  v8::Isolate* isolate = env->GetIsolate();

  v8::CpuProfiler* profiler = env->GetIsolate()->GetCpuProfiler();
  i::CpuProfiler* iprofiler = reinterpret_cast<i::CpuProfiler*>(profiler);

  CHECK_EQ(0, iprofiler->GetProfilesCount());
  v8::Handle<v8::String> outer = v8::String::NewFromUtf8(isolate, "outer");
  profiler->StartProfiling(outer);
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::Handle<v8::String> inner = v8::String::NewFromUtf8(isolate, "inner");
  profiler->StartProfiling(inner);
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::CpuProfile* inner_profile = profiler->StopProfiling(inner);
  CHECK(inner_profile);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  inner_profile->Delete();
  inner_profile = NULL;
  CHECK_EQ(0, iprofiler->GetProfilesCount());

  v8::CpuProfile* outer_profile = profiler->StopProfiling(outer);
  CHECK(outer_profile);
  CHECK_EQ(1, iprofiler->GetProfilesCount());
  outer_profile->Delete();
  outer_profile = NULL;
  CHECK_EQ(0, iprofiler->GetProfilesCount());
}
