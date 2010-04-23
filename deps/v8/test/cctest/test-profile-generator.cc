// Copyright 2010 the V8 project authors. All rights reserved.
//
// Tests of profiles generator and utilities.

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"
#include "profile-generator-inl.h"
#include "cctest.h"

namespace i = v8::internal;

using i::CodeEntry;
using i::CodeMap;
using i::CpuProfile;
using i::CpuProfilesCollection;
using i::ProfileNode;
using i::ProfileTree;
using i::ProfileGenerator;
using i::SampleRateCalculator;
using i::TickSample;
using i::Vector;


TEST(ProfileNodeFindOrAddChild) {
  ProfileNode node(NULL, NULL);
  CodeEntry entry1(i::Logger::FUNCTION_TAG, "", "aaa", "", 0);
  ProfileNode* childNode1 = node.FindOrAddChild(&entry1);
  CHECK_NE(NULL, childNode1);
  CHECK_EQ(childNode1, node.FindOrAddChild(&entry1));
  CodeEntry entry2(i::Logger::FUNCTION_TAG, "", "bbb", "", 0);
  ProfileNode* childNode2 = node.FindOrAddChild(&entry2);
  CHECK_NE(NULL, childNode2);
  CHECK_NE(childNode1, childNode2);
  CHECK_EQ(childNode1, node.FindOrAddChild(&entry1));
  CHECK_EQ(childNode2, node.FindOrAddChild(&entry2));
  CodeEntry entry3(i::Logger::FUNCTION_TAG, "", "ccc", "", 0);
  ProfileNode* childNode3 = node.FindOrAddChild(&entry3);
  CHECK_NE(NULL, childNode3);
  CHECK_NE(childNode1, childNode3);
  CHECK_NE(childNode2, childNode3);
  CHECK_EQ(childNode1, node.FindOrAddChild(&entry1));
  CHECK_EQ(childNode2, node.FindOrAddChild(&entry2));
  CHECK_EQ(childNode3, node.FindOrAddChild(&entry3));
}


namespace {

class ProfileTreeTestHelper {
 public:
  explicit ProfileTreeTestHelper(const ProfileTree* tree)
      : tree_(tree) { }

  ProfileNode* Walk(CodeEntry* entry1,
                    CodeEntry* entry2 = NULL,
                    CodeEntry* entry3 = NULL) {
    ProfileNode* node = tree_->root();
    node = node->FindChild(entry1);
    if (node == NULL) return NULL;
    if (entry2 != NULL) {
      node = node->FindChild(entry2);
      if (node == NULL) return NULL;
    }
    if (entry3 != NULL) {
      node = node->FindChild(entry3);
    }
    return node;
  }

 private:
  const ProfileTree* tree_;
};

}  // namespace

TEST(ProfileTreeAddPathFromStart) {
  CodeEntry entry1(i::Logger::FUNCTION_TAG, "", "aaa", "", 0);
  CodeEntry entry2(i::Logger::FUNCTION_TAG, "", "bbb", "", 0);
  CodeEntry entry3(i::Logger::FUNCTION_TAG, "", "ccc", "", 0);
  ProfileTree tree;
  ProfileTreeTestHelper helper(&tree);
  CHECK_EQ(NULL, helper.Walk(&entry1));
  CHECK_EQ(NULL, helper.Walk(&entry2));
  CHECK_EQ(NULL, helper.Walk(&entry3));

  CodeEntry* path[] = {NULL, &entry1, NULL, &entry2, NULL, NULL, &entry3, NULL};
  Vector<CodeEntry*> path_vec(path, sizeof(path) / sizeof(path[0]));
  tree.AddPathFromStart(path_vec);
  CHECK_EQ(NULL, helper.Walk(&entry2));
  CHECK_EQ(NULL, helper.Walk(&entry3));
  ProfileNode* node1 = helper.Walk(&entry1);
  CHECK_NE(NULL, node1);
  CHECK_EQ(0, node1->total_ticks());
  CHECK_EQ(0, node1->self_ticks());
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry3));
  ProfileNode* node2 = helper.Walk(&entry1, &entry2);
  CHECK_NE(NULL, node2);
  CHECK_NE(node1, node2);
  CHECK_EQ(0, node2->total_ticks());
  CHECK_EQ(0, node2->self_ticks());
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry2, &entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry2, &entry2));
  ProfileNode* node3 = helper.Walk(&entry1, &entry2, &entry3);
  CHECK_NE(NULL, node3);
  CHECK_NE(node1, node3);
  CHECK_NE(node2, node3);
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(1, node3->self_ticks());

  tree.AddPathFromStart(path_vec);
  CHECK_EQ(node1, helper.Walk(&entry1));
  CHECK_EQ(node2, helper.Walk(&entry1, &entry2));
  CHECK_EQ(node3, helper.Walk(&entry1, &entry2, &entry3));
  CHECK_EQ(0, node1->total_ticks());
  CHECK_EQ(0, node1->self_ticks());
  CHECK_EQ(0, node2->total_ticks());
  CHECK_EQ(0, node2->self_ticks());
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(2, node3->self_ticks());

  CodeEntry* path2[] = {&entry1, &entry2, &entry2};
  Vector<CodeEntry*> path2_vec(path2, sizeof(path2) / sizeof(path2[0]));
  tree.AddPathFromStart(path2_vec);
  CHECK_EQ(NULL, helper.Walk(&entry2));
  CHECK_EQ(NULL, helper.Walk(&entry3));
  CHECK_EQ(node1, helper.Walk(&entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry3));
  CHECK_EQ(node2, helper.Walk(&entry1, &entry2));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry2, &entry1));
  CHECK_EQ(node3, helper.Walk(&entry1, &entry2, &entry3));
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(2, node3->self_ticks());
  ProfileNode* node4 = helper.Walk(&entry1, &entry2, &entry2);
  CHECK_NE(NULL, node4);
  CHECK_NE(node3, node4);
  CHECK_EQ(0, node4->total_ticks());
  CHECK_EQ(1, node4->self_ticks());
}


TEST(ProfileTreeAddPathFromEnd) {
  CodeEntry entry1(i::Logger::FUNCTION_TAG, "", "aaa", "", 0);
  CodeEntry entry2(i::Logger::FUNCTION_TAG, "", "bbb", "", 0);
  CodeEntry entry3(i::Logger::FUNCTION_TAG, "", "ccc", "", 0);
  ProfileTree tree;
  ProfileTreeTestHelper helper(&tree);
  CHECK_EQ(NULL, helper.Walk(&entry1));
  CHECK_EQ(NULL, helper.Walk(&entry2));
  CHECK_EQ(NULL, helper.Walk(&entry3));

  CodeEntry* path[] = {NULL, &entry3, NULL, &entry2, NULL, NULL, &entry1, NULL};
  Vector<CodeEntry*> path_vec(path, sizeof(path) / sizeof(path[0]));
  tree.AddPathFromEnd(path_vec);
  CHECK_EQ(NULL, helper.Walk(&entry2));
  CHECK_EQ(NULL, helper.Walk(&entry3));
  ProfileNode* node1 = helper.Walk(&entry1);
  CHECK_NE(NULL, node1);
  CHECK_EQ(0, node1->total_ticks());
  CHECK_EQ(0, node1->self_ticks());
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry3));
  ProfileNode* node2 = helper.Walk(&entry1, &entry2);
  CHECK_NE(NULL, node2);
  CHECK_NE(node1, node2);
  CHECK_EQ(0, node2->total_ticks());
  CHECK_EQ(0, node2->self_ticks());
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry2, &entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry2, &entry2));
  ProfileNode* node3 = helper.Walk(&entry1, &entry2, &entry3);
  CHECK_NE(NULL, node3);
  CHECK_NE(node1, node3);
  CHECK_NE(node2, node3);
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(1, node3->self_ticks());

  tree.AddPathFromEnd(path_vec);
  CHECK_EQ(node1, helper.Walk(&entry1));
  CHECK_EQ(node2, helper.Walk(&entry1, &entry2));
  CHECK_EQ(node3, helper.Walk(&entry1, &entry2, &entry3));
  CHECK_EQ(0, node1->total_ticks());
  CHECK_EQ(0, node1->self_ticks());
  CHECK_EQ(0, node2->total_ticks());
  CHECK_EQ(0, node2->self_ticks());
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(2, node3->self_ticks());

  CodeEntry* path2[] = {&entry2, &entry2, &entry1};
  Vector<CodeEntry*> path2_vec(path2, sizeof(path2) / sizeof(path2[0]));
  tree.AddPathFromEnd(path2_vec);
  CHECK_EQ(NULL, helper.Walk(&entry2));
  CHECK_EQ(NULL, helper.Walk(&entry3));
  CHECK_EQ(node1, helper.Walk(&entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry1));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry3));
  CHECK_EQ(node2, helper.Walk(&entry1, &entry2));
  CHECK_EQ(NULL, helper.Walk(&entry1, &entry2, &entry1));
  CHECK_EQ(node3, helper.Walk(&entry1, &entry2, &entry3));
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(2, node3->self_ticks());
  ProfileNode* node4 = helper.Walk(&entry1, &entry2, &entry2);
  CHECK_NE(NULL, node4);
  CHECK_NE(node3, node4);
  CHECK_EQ(0, node4->total_ticks());
  CHECK_EQ(1, node4->self_ticks());
}


TEST(ProfileTreeCalculateTotalTicks) {
  ProfileTree empty_tree;
  CHECK_EQ(0, empty_tree.root()->total_ticks());
  CHECK_EQ(0, empty_tree.root()->self_ticks());
  empty_tree.CalculateTotalTicks();
  CHECK_EQ(0, empty_tree.root()->total_ticks());
  CHECK_EQ(0, empty_tree.root()->self_ticks());
  empty_tree.root()->IncrementSelfTicks();
  CHECK_EQ(0, empty_tree.root()->total_ticks());
  CHECK_EQ(1, empty_tree.root()->self_ticks());
  empty_tree.CalculateTotalTicks();
  CHECK_EQ(1, empty_tree.root()->total_ticks());
  CHECK_EQ(1, empty_tree.root()->self_ticks());

  CodeEntry entry1(i::Logger::FUNCTION_TAG, "", "aaa", "", 0);
  CodeEntry entry2(i::Logger::FUNCTION_TAG, "", "bbb", "", 0);
  CodeEntry* e1_path[] = {&entry1};
  Vector<CodeEntry*> e1_path_vec(
      e1_path, sizeof(e1_path) / sizeof(e1_path[0]));
  CodeEntry* e1_e2_path[] = {&entry1, &entry2};
  Vector<CodeEntry*> e1_e2_path_vec(
      e1_e2_path, sizeof(e1_e2_path) / sizeof(e1_e2_path[0]));

  ProfileTree flat_tree;
  ProfileTreeTestHelper flat_helper(&flat_tree);
  flat_tree.AddPathFromStart(e1_path_vec);
  flat_tree.AddPathFromStart(e1_path_vec);
  flat_tree.AddPathFromStart(e1_e2_path_vec);
  flat_tree.AddPathFromStart(e1_e2_path_vec);
  flat_tree.AddPathFromStart(e1_e2_path_vec);
  // Results in {root,0,0} -> {entry1,0,2} -> {entry2,0,3}
  CHECK_EQ(0, flat_tree.root()->total_ticks());
  CHECK_EQ(0, flat_tree.root()->self_ticks());
  ProfileNode* node1 = flat_helper.Walk(&entry1);
  CHECK_NE(NULL, node1);
  CHECK_EQ(0, node1->total_ticks());
  CHECK_EQ(2, node1->self_ticks());
  ProfileNode* node2 = flat_helper.Walk(&entry1, &entry2);
  CHECK_NE(NULL, node2);
  CHECK_EQ(0, node2->total_ticks());
  CHECK_EQ(3, node2->self_ticks());
  flat_tree.CalculateTotalTicks();
  // Must calculate {root,5,0} -> {entry1,5,2} -> {entry2,3,3}
  CHECK_EQ(5, flat_tree.root()->total_ticks());
  CHECK_EQ(0, flat_tree.root()->self_ticks());
  CHECK_EQ(5, node1->total_ticks());
  CHECK_EQ(2, node1->self_ticks());
  CHECK_EQ(3, node2->total_ticks());
  CHECK_EQ(3, node2->self_ticks());

  CodeEntry* e2_path[] = {&entry2};
  Vector<CodeEntry*> e2_path_vec(
      e2_path, sizeof(e2_path) / sizeof(e2_path[0]));
  CodeEntry entry3(i::Logger::FUNCTION_TAG, "", "ccc", "", 0);
  CodeEntry* e3_path[] = {&entry3};
  Vector<CodeEntry*> e3_path_vec(
      e3_path, sizeof(e3_path) / sizeof(e3_path[0]));

  ProfileTree wide_tree;
  ProfileTreeTestHelper wide_helper(&wide_tree);
  wide_tree.AddPathFromStart(e1_path_vec);
  wide_tree.AddPathFromStart(e1_path_vec);
  wide_tree.AddPathFromStart(e1_e2_path_vec);
  wide_tree.AddPathFromStart(e2_path_vec);
  wide_tree.AddPathFromStart(e2_path_vec);
  wide_tree.AddPathFromStart(e2_path_vec);
  wide_tree.AddPathFromStart(e3_path_vec);
  wide_tree.AddPathFromStart(e3_path_vec);
  wide_tree.AddPathFromStart(e3_path_vec);
  wide_tree.AddPathFromStart(e3_path_vec);
  // Results in            -> {entry1,0,2} -> {entry2,0,1}
  //            {root,0,0} -> {entry2,0,3}
  //                       -> {entry3,0,4}
  CHECK_EQ(0, wide_tree.root()->total_ticks());
  CHECK_EQ(0, wide_tree.root()->self_ticks());
  node1 = wide_helper.Walk(&entry1);
  CHECK_NE(NULL, node1);
  CHECK_EQ(0, node1->total_ticks());
  CHECK_EQ(2, node1->self_ticks());
  ProfileNode* node1_2 = wide_helper.Walk(&entry1, &entry2);
  CHECK_NE(NULL, node1_2);
  CHECK_EQ(0, node1_2->total_ticks());
  CHECK_EQ(1, node1_2->self_ticks());
  node2 = wide_helper.Walk(&entry2);
  CHECK_NE(NULL, node2);
  CHECK_EQ(0, node2->total_ticks());
  CHECK_EQ(3, node2->self_ticks());
  ProfileNode* node3 = wide_helper.Walk(&entry3);
  CHECK_NE(NULL, node3);
  CHECK_EQ(0, node3->total_ticks());
  CHECK_EQ(4, node3->self_ticks());
  wide_tree.CalculateTotalTicks();
  // Calculates             -> {entry1,3,2} -> {entry2,1,1}
  //            {root,10,0} -> {entry2,3,3}
  //                        -> {entry3,4,4}
  CHECK_EQ(10, wide_tree.root()->total_ticks());
  CHECK_EQ(0, wide_tree.root()->self_ticks());
  CHECK_EQ(3, node1->total_ticks());
  CHECK_EQ(2, node1->self_ticks());
  CHECK_EQ(1, node1_2->total_ticks());
  CHECK_EQ(1, node1_2->self_ticks());
  CHECK_EQ(3, node2->total_ticks());
  CHECK_EQ(3, node2->self_ticks());
  CHECK_EQ(4, node3->total_ticks());
  CHECK_EQ(4, node3->self_ticks());
}


static inline i::Address ToAddress(int n) {
  return reinterpret_cast<i::Address>(n);
}

TEST(CodeMapAddCode) {
  CodeMap code_map;
  CodeEntry entry1(i::Logger::FUNCTION_TAG, "", "aaa", "", 0);
  CodeEntry entry2(i::Logger::FUNCTION_TAG, "", "bbb", "", 0);
  CodeEntry entry3(i::Logger::FUNCTION_TAG, "", "ccc", "", 0);
  CodeEntry entry4(i::Logger::FUNCTION_TAG, "", "ddd", "", 0);
  code_map.AddCode(ToAddress(0x1500), &entry1, 0x200);
  code_map.AddCode(ToAddress(0x1700), &entry2, 0x100);
  code_map.AddCode(ToAddress(0x1900), &entry3, 0x50);
  code_map.AddCode(ToAddress(0x1950), &entry4, 0x10);
  CHECK_EQ(NULL, code_map.FindEntry(0));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0x1500 - 1)));
  CHECK_EQ(&entry1, code_map.FindEntry(ToAddress(0x1500)));
  CHECK_EQ(&entry1, code_map.FindEntry(ToAddress(0x1500 + 0x100)));
  CHECK_EQ(&entry1, code_map.FindEntry(ToAddress(0x1500 + 0x200 - 1)));
  CHECK_EQ(&entry2, code_map.FindEntry(ToAddress(0x1700)));
  CHECK_EQ(&entry2, code_map.FindEntry(ToAddress(0x1700 + 0x50)));
  CHECK_EQ(&entry2, code_map.FindEntry(ToAddress(0x1700 + 0x100 - 1)));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0x1700 + 0x100)));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0x1900 - 1)));
  CHECK_EQ(&entry3, code_map.FindEntry(ToAddress(0x1900)));
  CHECK_EQ(&entry3, code_map.FindEntry(ToAddress(0x1900 + 0x28)));
  CHECK_EQ(&entry4, code_map.FindEntry(ToAddress(0x1950)));
  CHECK_EQ(&entry4, code_map.FindEntry(ToAddress(0x1950 + 0x7)));
  CHECK_EQ(&entry4, code_map.FindEntry(ToAddress(0x1950 + 0x10 - 1)));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0x1950 + 0x10)));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0xFFFFFFFF)));
}


TEST(CodeMapMoveAndDeleteCode) {
  CodeMap code_map;
  CodeEntry entry1(i::Logger::FUNCTION_TAG, "", "aaa", "", 0);
  CodeEntry entry2(i::Logger::FUNCTION_TAG, "", "bbb", "", 0);
  code_map.AddCode(ToAddress(0x1500), &entry1, 0x200);
  code_map.AddCode(ToAddress(0x1700), &entry2, 0x100);
  CHECK_EQ(&entry1, code_map.FindEntry(ToAddress(0x1500)));
  CHECK_EQ(&entry2, code_map.FindEntry(ToAddress(0x1700)));
  code_map.MoveCode(ToAddress(0x1500), ToAddress(0x1800));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0x1500)));
  CHECK_EQ(&entry2, code_map.FindEntry(ToAddress(0x1700)));
  CHECK_EQ(&entry1, code_map.FindEntry(ToAddress(0x1800)));
  code_map.DeleteCode(ToAddress(0x1700));
  CHECK_EQ(NULL, code_map.FindEntry(ToAddress(0x1700)));
  CHECK_EQ(&entry1, code_map.FindEntry(ToAddress(0x1800)));
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

TEST(RecordTickSample) {
  TestSetup test_setup;
  CpuProfilesCollection profiles;
  profiles.StartProfiling("", 1);
  ProfileGenerator generator(&profiles);
  CodeEntry* entry1 = generator.NewCodeEntry(i::Logger::FUNCTION_TAG, "aaa");
  CodeEntry* entry2 = generator.NewCodeEntry(i::Logger::FUNCTION_TAG, "bbb");
  CodeEntry* entry3 = generator.NewCodeEntry(i::Logger::FUNCTION_TAG, "ccc");
  generator.code_map()->AddCode(ToAddress(0x1500), entry1, 0x200);
  generator.code_map()->AddCode(ToAddress(0x1700), entry2, 0x100);
  generator.code_map()->AddCode(ToAddress(0x1900), entry3, 0x50);

  // We are building the following calls tree:
  //      -> aaa         - sample1
  //  aaa -> bbb -> ccc  - sample2
  //      -> ccc -> aaa  - sample3
  TickSample sample1;
  sample1.pc = ToAddress(0x1600);
  sample1.function = ToAddress(0x1500);
  sample1.stack[0] = ToAddress(0x1510);
  sample1.frames_count = 1;
  generator.RecordTickSample(sample1);
  TickSample sample2;
  sample2.pc = ToAddress(0x1925);
  sample2.function = ToAddress(0x1900);
  sample2.stack[0] = ToAddress(0x1780);
  sample2.stack[1] = ToAddress(0x10000);  // non-existent.
  sample2.stack[2] = ToAddress(0x1620);
  sample2.frames_count = 3;
  generator.RecordTickSample(sample2);
  TickSample sample3;
  sample3.pc = ToAddress(0x1510);
  sample3.function = ToAddress(0x1500);
  sample3.stack[0] = ToAddress(0x1910);
  sample3.stack[1] = ToAddress(0x1610);
  sample3.frames_count = 2;
  generator.RecordTickSample(sample3);

  CpuProfile* profile = profiles.StopProfiling("", 1);
  CHECK_NE(NULL, profile);
  ProfileTreeTestHelper top_down_test_helper(profile->top_down());
  CHECK_EQ(NULL, top_down_test_helper.Walk(entry2));
  CHECK_EQ(NULL, top_down_test_helper.Walk(entry3));
  ProfileNode* node1 = top_down_test_helper.Walk(entry1);
  CHECK_NE(NULL, node1);
  CHECK_EQ(entry1, node1->entry());
  ProfileNode* node2 = top_down_test_helper.Walk(entry1, entry1);
  CHECK_NE(NULL, node2);
  CHECK_EQ(entry1, node2->entry());
  ProfileNode* node3 = top_down_test_helper.Walk(entry1, entry2, entry3);
  CHECK_NE(NULL, node3);
  CHECK_EQ(entry3, node3->entry());
  ProfileNode* node4 = top_down_test_helper.Walk(entry1, entry3, entry1);
  CHECK_NE(NULL, node4);
  CHECK_EQ(entry1, node4->entry());
}


TEST(SampleRateCalculator) {
  const double kSamplingIntervalMs = i::Logger::kSamplingIntervalMs;

  // Verify that ticking exactly in query intervals results in the
  // initial sampling interval.
  double time = 0.0;
  SampleRateCalculator calc1;
  CHECK_EQ(kSamplingIntervalMs, calc1.ticks_per_ms());
  calc1.UpdateMeasurements(time);
  CHECK_EQ(kSamplingIntervalMs, calc1.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs;
  calc1.UpdateMeasurements(time);
  CHECK_EQ(kSamplingIntervalMs, calc1.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs;
  calc1.UpdateMeasurements(time);
  CHECK_EQ(kSamplingIntervalMs, calc1.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs;
  calc1.UpdateMeasurements(time);
  CHECK_EQ(kSamplingIntervalMs, calc1.ticks_per_ms());

  SampleRateCalculator calc2;
  time = 0.0;
  CHECK_EQ(kSamplingIntervalMs, calc2.ticks_per_ms());
  calc2.UpdateMeasurements(time);
  CHECK_EQ(kSamplingIntervalMs, calc2.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs * 0.5;
  calc2.UpdateMeasurements(time);
  // (1.0 + 2.0) / 2
  CHECK_EQ(kSamplingIntervalMs * 1.5, calc2.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs * 0.75;
  calc2.UpdateMeasurements(time);
  // (1.0 + 2.0 + 2.0) / 3
  CHECK_EQ(kSamplingIntervalMs * 1.66666, calc2.ticks_per_ms());

  SampleRateCalculator calc3;
  time = 0.0;
  CHECK_EQ(kSamplingIntervalMs, calc3.ticks_per_ms());
  calc3.UpdateMeasurements(time);
  CHECK_EQ(kSamplingIntervalMs, calc3.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs * 2;
  calc3.UpdateMeasurements(time);
  // (1.0 + 0.5) / 2
  CHECK_EQ(kSamplingIntervalMs * 0.75, calc3.ticks_per_ms());
  time += SampleRateCalculator::kWallTimeQueryIntervalMs * 1.5;
  calc3.UpdateMeasurements(time);
  // (1.0 + 0.5 + 0.5) / 3
  CHECK_EQ(kSamplingIntervalMs * 0.66666, calc3.ticks_per_ms());
}

#endif  // ENABLE_LOGGING_AND_PROFILING
