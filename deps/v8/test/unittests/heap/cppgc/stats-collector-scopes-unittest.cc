// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if CPPGC_IS_STANDALONE

#include "src/heap/cppgc/stats-collector.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class DelegatingTracingControllerImpl : public TracingController {
 public:
  virtual uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) {
    if (!check_expectations) return 0;
    static char phases[2] = {'B', 'E'};
    EXPECT_EQ(phases[AddTraceEvent_callcount], phase);
    EXPECT_TRUE(*category_enabled_flag);
    if (expected_name) {
      EXPECT_EQ(0, strcmp(expected_name, name));
    }
    stored_num_args += num_args;
    for (int i = 0; i < num_args; ++i) {
      stored_arg_names.push_back(arg_names[i]);
      stored_arg_types.push_back(arg_types[i]);
      stored_arg_values.push_back(arg_values[i]);
    }
    AddTraceEvent_callcount++;
    return 0;
  }

  static bool check_expectations;
  static size_t AddTraceEvent_callcount;
  static const char* expected_name;
  static int32_t stored_num_args;
  static std::vector<std::string> stored_arg_names;
  static std::vector<uint8_t> stored_arg_types;
  static std::vector<uint64_t> stored_arg_values;
};

bool DelegatingTracingControllerImpl::check_expectations = false;
size_t DelegatingTracingControllerImpl::AddTraceEvent_callcount = 0u;
const char* DelegatingTracingControllerImpl::expected_name = nullptr;
int32_t DelegatingTracingControllerImpl::stored_num_args = 0;
std::vector<std::string> DelegatingTracingControllerImpl::stored_arg_names;
std::vector<uint8_t> DelegatingTracingControllerImpl::stored_arg_types;
std::vector<uint64_t> DelegatingTracingControllerImpl::stored_arg_values;

class V8_NODISCARD CppgcTracingScopesTest : public testing::TestWithHeap {
  using Config = Marker::MarkingConfig;

 public:
  CppgcTracingScopesTest() {
    SetTracingController(std::make_unique<DelegatingTracingControllerImpl>());
  }

  void StartGC() {
    Config config = {Config::CollectionType::kMajor,
                     Config::StackState::kNoHeapPointers,
                     Config::MarkingType::kIncremental};
    GetMarkerRef() = MarkerFactory::CreateAndStartMarking<Marker>(
        Heap::From(GetHeap())->AsBase(), GetPlatformHandle().get(), config);
    DelegatingTracingControllerImpl::check_expectations = true;
  }

  void EndGC() {
    DelegatingTracingControllerImpl::check_expectations = false;
    GetMarkerRef()->FinishMarking(Config::StackState::kNoHeapPointers);
    GetMarkerRef().reset();
    Heap::From(GetHeap())->stats_collector()->NotifySweepingCompleted();
  }

  void ResetDelegatingTracingController(const char* expected_name = nullptr) {
    DelegatingTracingControllerImpl::AddTraceEvent_callcount = 0u;
    DelegatingTracingControllerImpl::stored_num_args = 0;
    DelegatingTracingControllerImpl::stored_arg_names.clear();
    DelegatingTracingControllerImpl::stored_arg_types.clear();
    DelegatingTracingControllerImpl::stored_arg_values.clear();
    DelegatingTracingControllerImpl::expected_name = expected_name;
  }

  void FindArgument(std::string name, uint8_t type, uint64_t value) {
    int i = 0;
    for (; i < DelegatingTracingControllerImpl::stored_num_args; ++i) {
      if (name.compare(DelegatingTracingControllerImpl::stored_arg_names[i]) ==
          0)
        break;
    }
    EXPECT_LT(i, DelegatingTracingControllerImpl::stored_num_args);
    EXPECT_EQ(type, DelegatingTracingControllerImpl::stored_arg_types[i]);
    EXPECT_EQ(value, DelegatingTracingControllerImpl::stored_arg_values[i]);
  }
};

}  // namespace

TEST_F(CppgcTracingScopesTest, DisabledScope) {
  StartGC();
  ResetDelegatingTracingController();
  {
    StatsCollector::DisabledScope scope(
        Heap::From(GetHeap())->stats_collector(),
        StatsCollector::kMarkProcessMarkingWorklist);
  }
  EXPECT_EQ(0u, DelegatingTracingControllerImpl::AddTraceEvent_callcount);
  EndGC();
}

TEST_F(CppgcTracingScopesTest, EnabledScope) {
  {
    StartGC();
    ResetDelegatingTracingController("CppGC.MarkProcessMarkingWorklist");
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist);
    }
    EXPECT_EQ(2u, DelegatingTracingControllerImpl::AddTraceEvent_callcount);
    EndGC();
  }
  {
    StartGC();
    ResetDelegatingTracingController("CppGC.MarkProcessWriteBarrierWorklist");
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessWriteBarrierWorklist);
    }
    EXPECT_EQ(2u, DelegatingTracingControllerImpl::AddTraceEvent_callcount);
    EndGC();
  }
}

TEST_F(CppgcTracingScopesTest, EnabledScopeWithArgs) {
  // Scopes always add 2 arguments: epoch and is_forced_gc.
  {
    StartGC();
    ResetDelegatingTracingController();
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist);
    }
    EXPECT_EQ(2, DelegatingTracingControllerImpl::stored_num_args);
    EndGC();
  }
  {
    StartGC();
    ResetDelegatingTracingController();
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist, "arg1", 1);
    }
    EXPECT_EQ(3, DelegatingTracingControllerImpl::stored_num_args);
    EndGC();
  }
  {
    StartGC();
    ResetDelegatingTracingController();
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist, "arg1", 1, "arg2", 2);
    }
    EXPECT_EQ(4, DelegatingTracingControllerImpl::stored_num_args);
    EndGC();
  }
}

TEST_F(CppgcTracingScopesTest, CheckScopeArgs) {
  {
    StartGC();
    ResetDelegatingTracingController();
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist, "uint_arg", 13u,
          "bool_arg", false);
    }
    FindArgument("uint_arg", TRACE_VALUE_TYPE_UINT, 13);
    FindArgument("bool_arg", TRACE_VALUE_TYPE_BOOL, false);
    EndGC();
  }
  {
    StartGC();
    ResetDelegatingTracingController();
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist, "neg_int_arg", -5,
          "pos_int_arg", 7);
    }
    FindArgument("neg_int_arg", TRACE_VALUE_TYPE_INT, -5);
    FindArgument("pos_int_arg", TRACE_VALUE_TYPE_INT, 7);
    EndGC();
  }
  {
    StartGC();
    ResetDelegatingTracingController();
    double double_value = 1.2;
    const char* string_value = "test";
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist, "string_arg",
          string_value, "double_arg", double_value);
    }
    FindArgument("string_arg", TRACE_VALUE_TYPE_STRING,
                 reinterpret_cast<uint64_t>(string_value));
    FindArgument("double_arg", TRACE_VALUE_TYPE_DOUBLE,
                 *reinterpret_cast<uint64_t*>(&double_value));
    EndGC();
  }
}

TEST_F(CppgcTracingScopesTest, InitalScopesAreZero) {
  StatsCollector* stats_collector = Heap::From(GetHeap())->stats_collector();
  stats_collector->NotifyMarkingStarted(
      GarbageCollector::Config::CollectionType::kMajor,
      GarbageCollector::Config::IsForcedGC::kNotForced);
  stats_collector->NotifyMarkingCompleted(0);
  stats_collector->NotifySweepingCompleted();
  const StatsCollector::Event& event =
      stats_collector->GetPreviousEventForTesting();
  for (int i = 0; i < StatsCollector::kNumHistogramScopeIds; ++i) {
    EXPECT_TRUE(event.scope_data[i].IsZero());
  }
  for (int i = 0; i < StatsCollector::kNumHistogramConcurrentScopeIds; ++i) {
    EXPECT_EQ(0, event.concurrent_scope_data[i]);
  }
}

TEST_F(CppgcTracingScopesTest, TestIndividualScopes) {
  for (int scope_id = 0; scope_id < StatsCollector::kNumHistogramScopeIds;
       ++scope_id) {
    StatsCollector* stats_collector = Heap::From(GetHeap())->stats_collector();
    stats_collector->NotifyMarkingStarted(
        GarbageCollector::Config::CollectionType::kMajor,
        GarbageCollector::Config::IsForcedGC::kNotForced);
    DelegatingTracingControllerImpl::check_expectations = false;
    {
      StatsCollector::EnabledScope scope(
          Heap::From(GetHeap())->stats_collector(),
          static_cast<StatsCollector::ScopeId>(scope_id));
      v8::base::TimeTicks time = v8::base::TimeTicks::Now();
      while (time == v8::base::TimeTicks::Now()) {
        // Force time to progress before destroying scope.
      }
    }
    stats_collector->NotifyMarkingCompleted(0);
    stats_collector->NotifySweepingCompleted();
    const StatsCollector::Event& event =
        stats_collector->GetPreviousEventForTesting();
    for (int i = 0; i < StatsCollector::kNumHistogramScopeIds; ++i) {
      if (i == scope_id)
        EXPECT_LT(v8::base::TimeDelta(), event.scope_data[i]);
      else
        EXPECT_TRUE(event.scope_data[i].IsZero());
    }
    for (int i = 0; i < StatsCollector::kNumHistogramConcurrentScopeIds; ++i) {
      EXPECT_EQ(0, event.concurrent_scope_data[i]);
    }
  }
}

TEST_F(CppgcTracingScopesTest, TestIndividualConcurrentScopes) {
  for (int scope_id = 0;
       scope_id < StatsCollector::kNumHistogramConcurrentScopeIds; ++scope_id) {
    StatsCollector* stats_collector = Heap::From(GetHeap())->stats_collector();
    stats_collector->NotifyMarkingStarted(
        GarbageCollector::Config::CollectionType::kMajor,
        GarbageCollector::Config::IsForcedGC::kNotForced);
    DelegatingTracingControllerImpl::check_expectations = false;
    {
      StatsCollector::EnabledConcurrentScope scope(
          Heap::From(GetHeap())->stats_collector(),
          static_cast<StatsCollector::ConcurrentScopeId>(scope_id));
      v8::base::TimeTicks time = v8::base::TimeTicks::Now();
      while (time == v8::base::TimeTicks::Now()) {
        // Force time to progress before destroying scope.
      }
    }
    stats_collector->NotifyMarkingCompleted(0);
    stats_collector->NotifySweepingCompleted();
    const StatsCollector::Event& event =
        stats_collector->GetPreviousEventForTesting();
    for (int i = 0; i < StatsCollector::kNumHistogramScopeIds; ++i) {
      EXPECT_TRUE(event.scope_data[i].IsZero());
    }
    for (int i = 0; i < StatsCollector::kNumHistogramConcurrentScopeIds; ++i) {
      if (i == scope_id)
        EXPECT_LT(0, event.concurrent_scope_data[i]);
      else
        EXPECT_EQ(0, event.concurrent_scope_data[i]);
    }
  }
}

}  // namespace internal
}  // namespace cppgc

#endif  // CPPGC_IS_STANDALONE
