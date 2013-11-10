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

#include "v8.h"
#include "cpu-profiler-inl.h"
#include "cctest.h"
#include "platform.h"
#include "smart-pointers.h"
#include "utils.h"
#include "../include/v8-profiler.h"
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
using i::TimeDelta;
using i::Vector;


TEST(StartStop) {
  i::Isolate* isolate = CcTest::i_isolate();
  CpuProfilesCollection profiles(isolate->heap());
  ProfileGenerator generator(&profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, TimeDelta::FromMicroseconds(100)));
  processor->Start();
  processor->StopSynchronously();
}


static inline i::Address ToAddress(int n) {
  return reinterpret_cast<i::Address>(n);
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

  i::OS::SNPrintF(name, "function_%d", ++counter);
  const char* name_start = name.start();
  i::OS::SNPrintF(script,
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
  profiles->StartProfiling("", 1, false);
  ProfileGenerator generator(profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, TimeDelta::FromMicroseconds(100)));
  processor->Start();
  CpuProfiler profiler(isolate, profiles, &generator, *processor);

  // Enqueue code creation events.
  const char* aaa_str = "aaa";
  i::Handle<i::String> aaa_name = factory->NewStringFromAscii(
      i::Vector<const char>(aaa_str, i::StrLength(aaa_str)));
  profiler.CodeCreateEvent(i::Logger::FUNCTION_TAG, aaa_code, *aaa_name);
  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, comment_code, "comment");
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, args5_code, 5);
  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, comment2_code, "comment2");
  profiler.CodeMoveEvent(comment2_code->address(), moved_code->address());
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, args3_code, 3);
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, args4_code, 4);

  // Enqueue a tick event to enable code events processing.
  EnqueueTickSampleEvent(*processor, aaa_code->address());

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
  profiles->StartProfiling("", 1, false);
  ProfileGenerator generator(profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, TimeDelta::FromMicroseconds(100)));
  processor->Start();
  CpuProfiler profiler(isolate, profiles, &generator, *processor);

  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, frame1_code, "bbb");
  profiler.CodeCreateEvent(i::Logger::STUB_TAG, frame2_code, 5);
  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, frame3_code, "ddd");

  EnqueueTickSampleEvent(*processor, frame1_code->instruction_start());
  EnqueueTickSampleEvent(
      *processor,
      frame2_code->instruction_start() + frame2_code->ExecutableSize() / 2,
      frame1_code->instruction_start() + frame2_code->ExecutableSize() / 2);
  EnqueueTickSampleEvent(
      *processor,
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
  profiles->StartProfiling("", 1, false);
  ProfileGenerator generator(profiles);
  SmartPointer<ProfilerEventsProcessor> processor(new ProfilerEventsProcessor(
          &generator, NULL, TimeDelta::FromMicroseconds(100)));
  processor->Start();
  CpuProfiler profiler(isolate, profiles, &generator, *processor);

  profiler.CodeCreateEvent(i::Logger::BUILTIN_TAG, code, "bbb");

  i::TickSample* sample = processor->StartTickSample();
  sample->pc = code->address();
  sample->tos = 0;
  sample->frames_count = i::TickSample::kMaxFramesCount;
  for (int i = 0; i < sample->frames_count; ++i) {
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


static const v8::CpuProfile* FindCpuProfile(v8::CpuProfiler* profiler,
                                            unsigned uid) {
  int length = profiler->GetProfileCount();
  for (int i = 0; i < length; i++) {
    const v8::CpuProfile* profile = profiler->GetCpuProfile(i);
    if (profile->GetUid() == uid) {
      return profile;
    }
  }
  return NULL;
}


TEST(DeleteCpuProfile) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();

  CHECK_EQ(0, cpu_profiler->GetProfileCount());
  v8::Local<v8::String> name1 = v8::String::New("1");
  cpu_profiler->StartCpuProfiling(name1);
  const v8::CpuProfile* p1 = cpu_profiler->StopCpuProfiling(name1);
  CHECK_NE(NULL, p1);
  CHECK_EQ(1, cpu_profiler->GetProfileCount());
  unsigned uid1 = p1->GetUid();
  CHECK_EQ(p1, FindCpuProfile(cpu_profiler, uid1));
  const_cast<v8::CpuProfile*>(p1)->Delete();
  CHECK_EQ(0, cpu_profiler->GetProfileCount());
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid1));

  v8::Local<v8::String> name2 = v8::String::New("2");
  cpu_profiler->StartCpuProfiling(name2);
  const v8::CpuProfile* p2 = cpu_profiler->StopCpuProfiling(name2);
  CHECK_NE(NULL, p2);
  CHECK_EQ(1, cpu_profiler->GetProfileCount());
  unsigned uid2 = p2->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid2));
  CHECK_EQ(p2, FindCpuProfile(cpu_profiler, uid2));
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid1));
  v8::Local<v8::String> name3 = v8::String::New("3");
  cpu_profiler->StartCpuProfiling(name3);
  const v8::CpuProfile* p3 = cpu_profiler->StopCpuProfiling(name3);
  CHECK_NE(NULL, p3);
  CHECK_EQ(2, cpu_profiler->GetProfileCount());
  unsigned uid3 = p3->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid3));
  CHECK_EQ(p3, FindCpuProfile(cpu_profiler, uid3));
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid1));
  const_cast<v8::CpuProfile*>(p2)->Delete();
  CHECK_EQ(1, cpu_profiler->GetProfileCount());
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid2));
  CHECK_EQ(p3, FindCpuProfile(cpu_profiler, uid3));
  const_cast<v8::CpuProfile*>(p3)->Delete();
  CHECK_EQ(0, cpu_profiler->GetProfileCount());
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid3));
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid2));
  CHECK_EQ(NULL, FindCpuProfile(cpu_profiler, uid1));
}


TEST(ProfileStartEndTime) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();

  v8::Local<v8::String> profile_name = v8::String::New("test");
  cpu_profiler->StartCpuProfiling(profile_name);
  const v8::CpuProfile* profile = cpu_profiler->StopCpuProfiling(profile_name);
  CHECK(profile->GetStartTime() <= profile->GetEndTime());
}


static const v8::CpuProfile* RunProfiler(
    LocalContext& env, v8::Handle<v8::Function> function,
    v8::Handle<v8::Value> argv[], int argc,
    unsigned min_js_samples) {
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  v8::Local<v8::String> profile_name = v8::String::New("my_profile");

  cpu_profiler->StartCpuProfiling(profile_name);

  i::Sampler* sampler =
      reinterpret_cast<i::Isolate*>(env->GetIsolate())->logger()->sampler();
  sampler->StartCountingSamples();
  do {
    function->Call(env->Global(), argc, argv);
  } while (sampler->js_and_external_sample_count() < min_js_samples);

  const v8::CpuProfile* profile = cpu_profiler->StopCpuProfiling(profile_name);

  CHECK_NE(NULL, profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(
      const_cast<v8::CpuProfile*>(profile))->Print();

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


static const v8::CpuProfileNode* FindChild(const v8::CpuProfileNode* node,
                                           const char* name) {
  int count = node->GetChildrenCount();
  v8::Handle<v8::String> nameHandle = v8::String::New(name);
  for (int i = 0; i < count; i++) {
    const v8::CpuProfileNode* child = node->GetChild(i);
    if (nameHandle->Equals(child->GetFunctionName())) return child;
  }
  return NULL;
}


static const v8::CpuProfileNode* GetChild(const v8::CpuProfileNode* node,
                                          const char* name) {
  const v8::CpuProfileNode* result = FindChild(node, name);
  if (!result) {
    char buffer[100];
    i::OS::SNPrintF(Vector<char>(buffer, ARRAY_SIZE(buffer)),
                    "Failed to GetChild: %s", name);
    FATAL(buffer);
  }
  return result;
}


static void CheckSimpleBranch(const v8::CpuProfileNode* node,
                              const char* names[], int length) {
  for (int i = 0; i < length; i++) {
    const char* name = names[i];
    node = GetChild(node, name);
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

  v8::Script::Compile(v8::String::New(cpu_profiler_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t profiling_interval_ms = 200;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(profiling_interval_ms) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 200);
  function->Call(env->Global(), ARRAY_SIZE(args), args);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();

  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::New("start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());

  const v8::CpuProfileNode* fooNode = GetChild(startNode, "foo");
  CHECK_EQ(3, fooNode->GetChildrenCount());

  const char* barBranch[] = { "bar", "delay", "loop" };
  CheckSimpleBranch(fooNode, barBranch, ARRAY_SIZE(barBranch));
  const char* bazBranch[] = { "baz", "delay", "loop" };
  CheckSimpleBranch(fooNode, bazBranch, ARRAY_SIZE(bazBranch));
  const char* delayBranch[] = { "delay", "loop" };
  CheckSimpleBranch(fooNode, delayBranch, ARRAY_SIZE(delayBranch));

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
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

  v8::Script::Compile(v8::String::New(cpu_profiler_test_source2))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t repeat_count = 100;
#if defined(USE_SIMULATOR)
  // Simulators are much slower.
  repeat_count = 1;
#endif
  v8::Handle<v8::Value> args[] = { v8::Integer::New(repeat_count) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();

  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::New("start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode = FindChild(root, "start");
  // On slow machines there may be no meaningfull samples at all, skip the
  // check there.
  if (startNode && startNode->GetChildrenCount() > 0) {
    CHECK_EQ(1, startNode->GetChildrenCount());
    const v8::CpuProfileNode* delayNode = GetChild(startNode, "delay");
    if (delayNode->GetChildrenCount() > 0) {
      CHECK_EQ(1, delayNode->GetChildrenCount());
      GetChild(delayNode, "loop");
    }
  }

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
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
    double start = i::OS::TimeCurrentMillis();
    double duration = 0;
    while (duration < min_duration_ms_) {
      i::OS::Sleep(1);
      duration = i::OS::TimeCurrentMillis() - start;
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
  v8::HandleScope scope(env->GetIsolate());


  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New();
  v8::Local<v8::ObjectTemplate> instance_template =
      func_template->InstanceTemplate();

  TestApiCallbacks accessors(100);
  v8::Local<v8::External> data = v8::External::New(&accessors);
  instance_template->SetAccessor(
      v8::String::New("foo"), &TestApiCallbacks::Getter,
      &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::New("instance"), instance);

  v8::Script::Compile(v8::String::New(native_accessor_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t repeat_count = 1;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(repeat_count) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 180);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  GetChild(startNode, "get foo");
  GetChild(startNode, "set foo");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


// Test that native accessors are properly reported in the CPU profile.
// This test makes sure that the accessors are called enough times to become
// hot and to trigger optimizations.
TEST(NativeAccessorMonomorphicIC) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());


  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New();
  v8::Local<v8::ObjectTemplate> instance_template =
      func_template->InstanceTemplate();

  TestApiCallbacks accessors(1);
  v8::Local<v8::External> data = v8::External::New(&accessors);
  instance_template->SetAccessor(
      v8::String::New("foo"), &TestApiCallbacks::Getter,
      &TestApiCallbacks::Setter, data);
  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::New("instance"), instance);

  v8::Script::Compile(v8::String::New(native_accessor_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  {
    // Make sure accessors ICs are in monomorphic state before starting
    // profiling.
    accessors.set_warming_up(true);
    int32_t warm_up_iterations = 3;
    v8::Handle<v8::Value> args[] = { v8::Integer::New(warm_up_iterations) };
    function->Call(env->Global(), ARRAY_SIZE(args), args);
    accessors.set_warming_up(false);
  }

  int32_t repeat_count = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(repeat_count) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 200);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  GetChild(startNode, "get foo");
  GetChild(startNode, "set foo");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


static const char* native_method_test_source = "function start(count) {\n"
"  for (var i = 0; i < count; i++) {\n"
"    instance.fooMethod();\n"
"  }\n"
"}\n";


TEST(NativeMethodUninitializedIC) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  TestApiCallbacks callbacks(100);
  v8::Local<v8::External> data = v8::External::New(&callbacks);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New();
  func_template->SetClassName(v8::String::New("Test_InstanceCostructor"));
  v8::Local<v8::ObjectTemplate> proto_template =
      func_template->PrototypeTemplate();
  v8::Local<v8::Signature> signature = v8::Signature::New(func_template);
  proto_template->Set(v8::String::New("fooMethod"), v8::FunctionTemplate::New(
      &TestApiCallbacks::Callback, data, signature, 0));

  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::New("instance"), instance);

  v8::Script::Compile(v8::String::New(native_method_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t repeat_count = 1;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(repeat_count) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  GetChild(startNode, "fooMethod");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


TEST(NativeMethodMonomorphicIC) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  TestApiCallbacks callbacks(1);
  v8::Local<v8::External> data = v8::External::New(&callbacks);

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New();
  func_template->SetClassName(v8::String::New("Test_InstanceCostructor"));
  v8::Local<v8::ObjectTemplate> proto_template =
      func_template->PrototypeTemplate();
  v8::Local<v8::Signature> signature = v8::Signature::New(func_template);
  proto_template->Set(v8::String::New("fooMethod"), v8::FunctionTemplate::New(
      &TestApiCallbacks::Callback, data, signature, 0));

  v8::Local<v8::Function> func = func_template->GetFunction();
  v8::Local<v8::Object> instance = func->NewInstance();
  env->Global()->Set(v8::String::New("instance"), instance);

  v8::Script::Compile(v8::String::New(native_method_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));
  {
    // Make sure method ICs are in monomorphic state before starting
    // profiling.
    callbacks.set_warming_up(true);
    int32_t warm_up_iterations = 3;
    v8::Handle<v8::Value> args[] = { v8::Integer::New(warm_up_iterations) };
    function->Call(env->Global(), ARRAY_SIZE(args), args);
    callbacks.set_warming_up(false);
  }

  int32_t repeat_count = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(repeat_count) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  GetChild(root, "start");
  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  GetChild(startNode, "fooMethod");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


static const char* bound_function_test_source = "function foo(iterations) {\n"
"  var r = 0;\n"
"  for (var i = 0; i < iterations; i++) { r += i; }\n"
"  return r;\n"
"}\n"
"function start(duration) {\n"
"  var callback = foo.bind(this);\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < duration) {\n"
"    callback(10 * 1000);\n"
"  }\n"
"}";


TEST(BoundFunctionCall) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Script::Compile(v8::String::New(bound_function_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t duration_ms = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(duration_ms) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::New("start");
  // Don't allow |foo| node to be at the top level.
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  GetChild(startNode, "foo");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
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

  v8::Script::Compile(v8::String::New(call_function_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t duration_ms = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(duration_ms) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(4);
    names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::New("start");
    names[3] = v8::String::New(i::ProfileGenerator::kUnresolvedFunctionName);
    // Don't allow |bar| and |call| nodes to be at the top level.
    CheckChildrenNames(root, names);
  }

  // In case of GC stress tests all samples may be in GC phase and there
  // won't be |start| node in the profiles.
  bool is_gc_stress_testing =
      (i::FLAG_gc_interval != -1) || i::FLAG_stress_compaction;
  const v8::CpuProfileNode* startNode = FindChild(root, "start");
  CHECK(is_gc_stress_testing || startNode);
  if (startNode) {
    ScopedVector<v8::Handle<v8::String> > names(2);
    names[0] = v8::String::New("bar");
    names[1] = v8::String::New("call");
    CheckChildrenNames(startNode, names);
  }

  const v8::CpuProfileNode* unresolvedNode =
      FindChild(root, i::ProfileGenerator::kUnresolvedFunctionName);
  if (unresolvedNode) {
    ScopedVector<v8::Handle<v8::String> > names(1);
    names[0] = v8::String::New("call");
    CheckChildrenNames(unresolvedNode, names);
  }

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
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

  v8::Script::Compile(v8::String::New(function_apply_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t duration_ms = 100;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(duration_ms) };

  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 100);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(3);
    names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::New("start");
    // Don't allow |test|, |bar| and |apply| nodes to be at the top level.
    CheckChildrenNames(root, names);
  }

  const v8::CpuProfileNode* startNode = FindChild(root, "start");
  if (startNode) {
    {
      ScopedVector<v8::Handle<v8::String> > names(2);
      names[0] = v8::String::New("test");
      names[1] = v8::String::New(ProfileGenerator::kUnresolvedFunctionName);
      CheckChildrenNames(startNode, names);
    }

    const v8::CpuProfileNode* testNode = FindChild(startNode, "test");
    if (testNode) {
      ScopedVector<v8::Handle<v8::String> > names(2);
      names[0] = v8::String::New("bar");
      names[1] = v8::String::New("apply");
      CheckChildrenNames(testNode, names);
    }

    if (const v8::CpuProfileNode* unresolvedNode =
            FindChild(startNode, ProfileGenerator::kUnresolvedFunctionName)) {
      ScopedVector<v8::Handle<v8::String> > names(1);
      names[0] = v8::String::New("apply");
      CheckChildrenNames(unresolvedNode, names);
      GetChild(unresolvedNode, "apply");
    }
  }

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


static const char* js_native_js_test_source =
"var is_profiling = false;\n"
"function foo(iterations) {\n"
"  if (!is_profiling) {\n"
"    is_profiling = true;\n"
"    startProfiling('my_profile');\n"
"  }\n"
"  var r = 0;\n"
"  for (var i = 0; i < iterations; i++) { r += i; }\n"
"  return r;\n"
"}\n"
"function bar(iterations) {\n"
"  try { foo(iterations); } catch(e) {}\n"
"}\n"
"function start(duration) {\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < duration) {\n"
"    try {\n"
"      CallJsFunction(bar, 10 * 1000);\n"
"    } catch(e) {}\n"
"  }\n"
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
  const char* extensions[] = { "v8/profiler" };
  v8::ExtensionConfiguration config(1, extensions);
  LocalContext env(&config);
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      CallJsFunction);
  v8::Local<v8::Function> func = func_template->GetFunction();
  func->SetName(v8::String::New("CallJsFunction"));
  env->Global()->Set(v8::String::New("CallJsFunction"), func);

  v8::Script::Compile(v8::String::New(js_native_js_test_source))->Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t duration_ms = 20;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(duration_ms) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 10);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  {
    ScopedVector<v8::Handle<v8::String> > names(3);
    names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
    names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
    names[2] = v8::String::New("start");
    CheckChildrenNames(root, names);
  }

  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeFunctionNode =
      GetChild(startNode, "CallJsFunction");

  CHECK_EQ(1, nativeFunctionNode->GetChildrenCount());
  const v8::CpuProfileNode* barNode = GetChild(nativeFunctionNode, "bar");

  CHECK_EQ(1, barNode->GetChildrenCount());
  GetChild(barNode, "foo");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


static const char* js_native_js_runtime_js_test_source =
"var is_profiling = false;\n"
"function foo(iterations) {\n"
"  if (!is_profiling) {\n"
"    is_profiling = true;\n"
"    startProfiling('my_profile');\n"
"  }\n"
"  var r = 0;\n"
"  for (var i = 0; i < iterations; i++) { r += i; }\n"
"  return r;\n"
"}\n"
"var bound = foo.bind(this);\n"
"function bar(iterations) {\n"
"  try { bound(iterations); } catch(e) {}\n"
"}\n"
"function start(duration) {\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < duration) {\n"
"    try {\n"
"      CallJsFunction(bar, 10 * 1000);\n"
"    } catch(e) {}\n"
"  }\n"
"}";


// [Top down]:
//    57     0   (root) #0 1
//    55     1    start #16 3
//    54     0      CallJsFunction #0 4
//    54     3        bar #16 5
//    51    51          foo #16 6
//     2     2    (program) #0 2
TEST(JsNativeJsRuntimeJsSample) {
  const char* extensions[] = { "v8/profiler" };
  v8::ExtensionConfiguration config(1, extensions);
  LocalContext env(&config);
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      CallJsFunction);
  v8::Local<v8::Function> func = func_template->GetFunction();
  func->SetName(v8::String::New("CallJsFunction"));
  env->Global()->Set(v8::String::New("CallJsFunction"), func);

  v8::Script::Compile(v8::String::New(js_native_js_runtime_js_test_source))->
      Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t duration_ms = 20;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(duration_ms) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 10);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::New("start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeFunctionNode =
      GetChild(startNode, "CallJsFunction");

  CHECK_EQ(1, nativeFunctionNode->GetChildrenCount());
  const v8::CpuProfileNode* barNode = GetChild(nativeFunctionNode, "bar");

  CHECK_EQ(1, barNode->GetChildrenCount());
  GetChild(barNode, "foo");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


static void CallJsFunction2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CallJsFunction(info);
}


static const char* js_native1_js_native2_js_test_source =
"var is_profiling = false;\n"
"function foo(iterations) {\n"
"  if (!is_profiling) {\n"
"    is_profiling = true;\n"
"    startProfiling('my_profile');\n"
"  }\n"
"  var r = 0;\n"
"  for (var i = 0; i < iterations; i++) { r += i; }\n"
"  return r;\n"
"}\n"
"function bar(iterations) {\n"
"  CallJsFunction2(foo, iterations);\n"
"}\n"
"function start(duration) {\n"
"  var start = Date.now();\n"
"  while (Date.now() - start < duration) {\n"
"    try {\n"
"      CallJsFunction1(bar, 10 * 1000);\n"
"    } catch(e) {}\n"
"  }\n"
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
  const char* extensions[] = { "v8/profiler" };
  v8::ExtensionConfiguration config(1, extensions);
  LocalContext env(&config);
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(
      CallJsFunction);
  v8::Local<v8::Function> func1 = func_template->GetFunction();
  func1->SetName(v8::String::New("CallJsFunction1"));
  env->Global()->Set(v8::String::New("CallJsFunction1"), func1);

  v8::Local<v8::Function> func2 = v8::FunctionTemplate::New(
      CallJsFunction2)->GetFunction();
  func2->SetName(v8::String::New("CallJsFunction2"));
  env->Global()->Set(v8::String::New("CallJsFunction2"), func2);

  v8::Script::Compile(v8::String::New(js_native1_js_native2_js_test_source))->
      Run();
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      env->Global()->Get(v8::String::New("start")));

  int32_t duration_ms = 20;
  v8::Handle<v8::Value> args[] = { v8::Integer::New(duration_ms) };
  const v8::CpuProfile* profile =
      RunProfiler(env, function, args, ARRAY_SIZE(args), 10);

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::New("start");
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* startNode = GetChild(root, "start");
  CHECK_EQ(1, startNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeNode1 =
      GetChild(startNode, "CallJsFunction1");

  CHECK_EQ(1, nativeNode1->GetChildrenCount());
  const v8::CpuProfileNode* barNode = GetChild(nativeNode1, "bar");

  CHECK_EQ(1, barNode->GetChildrenCount());
  const v8::CpuProfileNode* nativeNode2 = GetChild(barNode, "CallJsFunction2");

  CHECK_EQ(1, nativeNode2->GetChildrenCount());
  GetChild(nativeNode2, "foo");

  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();
  cpu_profiler->DeleteAllCpuProfiles();
}


// [Top down]:
//     6     0   (root) #0 1
//     3     3    (program) #0 2
//     3     3    (idle) #0 3
TEST(IdleTime) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::CpuProfiler* cpu_profiler = env->GetIsolate()->GetCpuProfiler();

  v8::Local<v8::String> profile_name = v8::String::New("my_profile");
  cpu_profiler->StartCpuProfiling(profile_name);

  i::Isolate* isolate = CcTest::i_isolate();
  i::ProfilerEventsProcessor* processor = isolate->cpu_profiler()->processor();
  processor->AddCurrentStack(isolate);

  cpu_profiler->SetIdle(true);

  for (int i = 0; i < 3; i++) {
    processor->AddCurrentStack(isolate);
  }

  cpu_profiler->SetIdle(false);
  processor->AddCurrentStack(isolate);


  const v8::CpuProfile* profile = cpu_profiler->StopCpuProfiling(profile_name);
  CHECK_NE(NULL, profile);
  // Dump collected profile to have a better diagnostic in case of failure.
  reinterpret_cast<i::CpuProfile*>(
      const_cast<v8::CpuProfile*>(profile))->Print();

  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  ScopedVector<v8::Handle<v8::String> > names(3);
  names[0] = v8::String::New(ProfileGenerator::kGarbageCollectorEntryName);
  names[1] = v8::String::New(ProfileGenerator::kProgramEntryName);
  names[2] = v8::String::New(ProfileGenerator::kIdleEntryName);
  CheckChildrenNames(root, names);

  const v8::CpuProfileNode* programNode =
      GetChild(root, ProfileGenerator::kProgramEntryName);
  CHECK_EQ(0, programNode->GetChildrenCount());
  CHECK_GE(programNode->GetHitCount(), 3);

  const v8::CpuProfileNode* idleNode =
      GetChild(root, ProfileGenerator::kIdleEntryName);
  CHECK_EQ(0, idleNode->GetChildrenCount());
  CHECK_GE(idleNode->GetHitCount(), 3);

  cpu_profiler->DeleteAllCpuProfiles();
}


static void CheckFunctionDetails(const v8::CpuProfileNode* node,
    const char* name, const char* script_name, int script_id,
    int line, int column) {
  CHECK_EQ(v8::String::New(name), node->GetFunctionName());
  CHECK_EQ(v8::String::New(script_name), node->GetScriptResourceName());
  CHECK_EQ(script_id, node->GetScriptId());
  CHECK_EQ(line, node->GetLineNumber());
  CHECK_EQ(column, node->GetColumnNumber());
}


TEST(FunctionDetails) {
  const char* extensions[] = { "v8/profiler" };
  v8::ExtensionConfiguration config(1, extensions);
  LocalContext env(&config);
  v8::HandleScope handleScope(env->GetIsolate());

  v8::CpuProfiler* profiler = env->GetIsolate()->GetCpuProfiler();
  CHECK_EQ(0, profiler->GetProfileCount());
  v8::Handle<v8::Script> script_a = v8::Script::Compile(v8::String::New(
      "    function foo\n() { try { bar(); } catch(e) {} }\n"
      " function bar() { startProfiling(); }\n"), v8::String::New("script_a"));
  script_a->Run();
  v8::Handle<v8::Script> script_b = v8::Script::Compile(v8::String::New(
      "\n\n   function baz() { try { foo(); } catch(e) {} }\n"
      "\n\nbaz();\n"
      "stopProfiling();\n"), v8::String::New("script_b"));
  script_b->Run();
  CHECK_EQ(1, profiler->GetProfileCount());
  const v8::CpuProfile* profile = profiler->GetCpuProfile(0);
  const v8::CpuProfileNode* current = profile->GetTopDownRoot();
  reinterpret_cast<ProfileNode*>(
      const_cast<v8::CpuProfileNode*>(current))->Print(0);
  // The tree should look like this:
  //  0   (root) 0 #1
  //  0    (anonymous function) 19 #2 no reason script_b:1
  //  0      baz 19 #3 TryCatchStatement script_b:3
  //  0        foo 18 #4 TryCatchStatement script_a:2
  //  1          bar 18 #5 no reason script_a:3
  const v8::CpuProfileNode* root = profile->GetTopDownRoot();
  const v8::CpuProfileNode* script = GetChild(root,
      ProfileGenerator::kAnonymousFunctionName);
  CheckFunctionDetails(script, ProfileGenerator::kAnonymousFunctionName,
      "script_b", script_b->GetId(), 1, 1);
  const v8::CpuProfileNode* baz = GetChild(script, "baz");
  CheckFunctionDetails(baz, "baz", "script_b", script_b->GetId(), 3, 16);
  const v8::CpuProfileNode* foo = GetChild(baz, "foo");
  CheckFunctionDetails(foo, "foo", "script_a", script_a->GetId(), 2, 1);
  const v8::CpuProfileNode* bar = GetChild(foo, "bar");
  CheckFunctionDetails(bar, "bar", "script_a", script_a->GetId(), 3, 14);
}
