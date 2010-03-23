// Copyright 2010 the V8 project authors. All rights reserved.
//
// Tests of profiles generator and utilities.

#include "v8.h"
#include "cpu-profiler-inl.h"
#include "cctest.h"

namespace i = v8::internal;

using i::CodeEntry;
using i::CpuProfilesCollection;
using i::ProfileGenerator;
using i::ProfileNode;
using i::ProfilerEventsProcessor;


TEST(StartStop) {
  CpuProfilesCollection profiles;
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();
  while (!processor.running()) {
    i::Thread::YieldCPU();
  }
  processor.Stop();
  processor.Join();
}

static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) env = v8::Context::New();
  v8::HandleScope scope;
  env->Enter();
}

static inline i::Address ToAddress(int n) {
  return reinterpret_cast<i::Address>(n);
}

static void EnqueueTickSampleEvent(ProfilerEventsProcessor* proc,
                                   i::Address frame1,
                                   i::Address frame2 = NULL,
                                   i::Address frame3 = NULL) {
  i::TickSample* sample = proc->TickSampleEvent();
  sample->pc = frame1;
  sample->function = frame1;
  sample->frames_count = 0;
  if (frame2 != NULL) {
    sample->stack[0] = frame2;
    sample->frames_count = 1;
  }
  if (frame3 != NULL) {
    sample->stack[1] = frame3;
    sample->frames_count = 2;
  }
}

TEST(CodeEvents) {
  InitializeVM();
  CpuProfilesCollection profiles;
  profiles.AddProfile(0);
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();
  processor.SetUpSamplesProducer();
  while (!processor.running()) {
    i::Thread::YieldCPU();
  }

  // Enqueue code creation events.
  i::HandleScope scope;
  const char* aaa_str = "aaa";
  i::Handle<i::String> aaa_name = i::Factory::NewStringFromAscii(
      i::Vector<const char>(aaa_str, strlen(aaa_str)));
  processor.CodeCreateEvent(i::Logger::FUNCTION_TAG,
                            *aaa_name,
                            i::Heap::empty_string(),
                            0,
                            ToAddress(0x1000),
                            0x100);
  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "bbb",
                            ToAddress(0x1200),
                            0x80);
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 5, ToAddress(0x1300), 0x10);
  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "ddd",
                            ToAddress(0x1400),
                            0x80);
  processor.CodeMoveEvent(ToAddress(0x1400), ToAddress(0x1500));
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 3, ToAddress(0x1600), 0x10);
  processor.CodeDeleteEvent(ToAddress(0x1600));
  processor.FunctionCreateEvent(ToAddress(0x1700), ToAddress(0x1000));
  // Enqueue a tick event to enable code events processing.
  EnqueueTickSampleEvent(&processor, ToAddress(0x1000));

  processor.Stop();
  processor.Join();

  // Check the state of profile generator.
  CodeEntry* entry1 = generator.code_map()->FindEntry(ToAddress(0x1000));
  CHECK_NE(NULL, entry1);
  CHECK_EQ(aaa_str, entry1->name());
  CodeEntry* entry2 = generator.code_map()->FindEntry(ToAddress(0x1200));
  CHECK_NE(NULL, entry2);
  CHECK_EQ("bbb", entry2->name());
  CodeEntry* entry3 = generator.code_map()->FindEntry(ToAddress(0x1300));
  CHECK_NE(NULL, entry3);
  CHECK_EQ("args_count: 5", entry3->name());
  CHECK_EQ(NULL, generator.code_map()->FindEntry(ToAddress(0x1400)));
  CodeEntry* entry4 = generator.code_map()->FindEntry(ToAddress(0x1500));
  CHECK_NE(NULL, entry4);
  CHECK_EQ("ddd", entry4->name());
  CHECK_EQ(NULL, generator.code_map()->FindEntry(ToAddress(0x1600)));
  CodeEntry* entry5 = generator.code_map()->FindEntry(ToAddress(0x1700));
  CHECK_NE(NULL, entry5);
  CHECK_EQ(aaa_str, entry5->name());

  processor.TearDownSamplesProducer();
}


template<typename T>
static int CompareProfileNodes(const T* p1, const T* p2) {
  return strcmp((*p1)->entry()->name(), (*p2)->entry()->name());
}

TEST(TickEvents) {
  CpuProfilesCollection profiles;
  profiles.AddProfile(0);
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();
  processor.SetUpSamplesProducer();
  while (!processor.running()) {
    i::Thread::YieldCPU();
  }

  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "bbb",
                            ToAddress(0x1200),
                            0x80);
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 5, ToAddress(0x1300), 0x10);
  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "ddd",
                            ToAddress(0x1400),
                            0x80);
  EnqueueTickSampleEvent(&processor, ToAddress(0x1210));
  EnqueueTickSampleEvent(&processor, ToAddress(0x1305), ToAddress(0x1220));
  EnqueueTickSampleEvent(&processor,
                         ToAddress(0x1404),
                         ToAddress(0x1305),
                         ToAddress(0x1230));

  processor.Stop();
  processor.Join();

  // Check call trees.
  i::List<ProfileNode*> top_down_root_children;
  profiles.profile()->top_down()->root()->GetChildren(&top_down_root_children);
  CHECK_EQ(1, top_down_root_children.length());
  CHECK_EQ("bbb", top_down_root_children.last()->entry()->name());
  i::List<ProfileNode*> top_down_bbb_children;
  top_down_root_children.last()->GetChildren(&top_down_bbb_children);
  CHECK_EQ(1, top_down_bbb_children.length());
  CHECK_EQ("args_count: 5", top_down_bbb_children.last()->entry()->name());
  i::List<ProfileNode*> top_down_stub_children;
  top_down_bbb_children.last()->GetChildren(&top_down_stub_children);
  CHECK_EQ(1, top_down_stub_children.length());
  CHECK_EQ("ddd", top_down_stub_children.last()->entry()->name());
  i::List<ProfileNode*> top_down_ddd_children;
  top_down_stub_children.last()->GetChildren(&top_down_ddd_children);
  CHECK_EQ(0, top_down_ddd_children.length());

  i::List<ProfileNode*> bottom_up_root_children;
  profiles.profile()->bottom_up()->root()->GetChildren(
      &bottom_up_root_children);
  CHECK_EQ(3, bottom_up_root_children.length());
  bottom_up_root_children.Sort(&CompareProfileNodes);
  CHECK_EQ("args_count: 5", bottom_up_root_children[0]->entry()->name());
  CHECK_EQ("bbb", bottom_up_root_children[1]->entry()->name());
  CHECK_EQ("ddd", bottom_up_root_children[2]->entry()->name());
  i::List<ProfileNode*> bottom_up_stub_children;
  bottom_up_root_children[0]->GetChildren(&bottom_up_stub_children);
  CHECK_EQ(1, bottom_up_stub_children.length());
  CHECK_EQ("bbb", bottom_up_stub_children.last()->entry()->name());
  i::List<ProfileNode*> bottom_up_bbb_children;
  bottom_up_root_children[1]->GetChildren(&bottom_up_bbb_children);
  CHECK_EQ(0, bottom_up_bbb_children.length());
  i::List<ProfileNode*> bottom_up_ddd_children;
  bottom_up_root_children[2]->GetChildren(&bottom_up_ddd_children);
  CHECK_EQ(1, bottom_up_ddd_children.length());
  CHECK_EQ("args_count: 5", bottom_up_ddd_children.last()->entry()->name());
  i::List<ProfileNode*> bottom_up_ddd_stub_children;
  bottom_up_ddd_children.last()->GetChildren(&bottom_up_ddd_stub_children);
  CHECK_EQ(1, bottom_up_ddd_stub_children.length());
  CHECK_EQ("bbb", bottom_up_ddd_stub_children.last()->entry()->name());

  processor.TearDownSamplesProducer();
}
