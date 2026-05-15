// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <json/json.h>

#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator.h"
#include "test/unittests/profiler/heap-snapshot-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

template <typename TMixin>
class WithHeapSnapshot : public TMixin {
 public:
  HeapSnapshot* TakeHeapSnapshot() {
    HeapProfiler* heap_profiler = TMixin::i_isolate()->heap()->heap_profiler();
    v8::HeapProfiler::HeapSnapshotOptions options;
    return heap_profiler->TakeSnapshot(options);
  }

  const Json::Value TakeHeapSnapshotJson() {
    HeapProfiler* heap_profiler = TMixin::i_isolate()->heap()->heap_profiler();
    v8::HeapProfiler::HeapSnapshotOptions options;
    std::string raw_json = heap_profiler->TakeSnapshotToString(options);

    Json::Value root;
    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    CHECK(reader->parse(raw_json.c_str(), raw_json.c_str() + raw_json.size(),
                        &root, nullptr));
    return root;
  }

 protected:
  void TestMergedWrapperNode(v8::HeapProfiler::HeapSnapshotMode snapshot_mode);
};

using HeapSnapshotTest = WithHeapSnapshot<TestWithContext>;

TEST_F(HeapSnapshotTest, Empty) {
  Json::Value root = TakeHeapSnapshotJson();

  Json::Value meta = root["snapshot"]["meta"];
  CHECK_EQ(meta["node_fields"].size(), meta["node_types"].size());
  CHECK_EQ(meta["edge_fields"].size(), meta["edge_types"].size());
}

TEST_F(HeapSnapshotTest, StringTableRootsAreWeak) {
  HeapSnapshot* snapshot = TakeHeapSnapshot();
  const HeapEntry* string_table_entry =
      snapshot->gc_subroot(Root::kStringTable);
  ASSERT_NE(nullptr, string_table_entry);
  ASSERT_GT(string_table_entry->children_count(), 0);
  // All edges from the string table should be weak.
  for (int i = 0; i < string_table_entry->children_count(); ++i) {
    const HeapGraphEdge* edge = string_table_entry->child(i);
    EXPECT_EQ(HeapGraphEdge::kWeak, edge->type());
  }
}

namespace {}  // namespace

TEST_F(HeapSnapshotTest, PositionOnSharedFunctionInfo) {
  RunJS(
      "globalThis.lazy = function lazy(x) { return x - 1; }\n"
      "globalThis.compiled = function compiled(x) { ()=>x; return x + 1; }\n"
      "compiled(1)");
  HeapSnapshot* snapshot = TakeHeapSnapshot();

  const HeapEntry* compiled = nullptr;
  const HeapEntry* lazy = nullptr;
  for (const HeapEntry& entry : snapshot->entries()) {
    if (entry.type() == HeapEntry::kClosure) {
      if (strcmp(entry.name(), "compiled") == 0)
        compiled = &entry;
      else if (strcmp(entry.name(), "lazy") == 0)
        lazy = &entry;
    }
  }
  CHECK_NOT_NULL(compiled);
  CHECK_NOT_NULL(lazy);

  // Find references to shared function info.
  const HeapGraphEdge* compiled_sfi_edge = GetNamedEdge(*compiled, "shared");
  CHECK_NOT_NULL(compiled_sfi_edge);
  const HeapEntry* compiled_sfi = compiled_sfi_edge->to();

  const HeapGraphEdge* lazy_sfi_edge = GetNamedEdge(*lazy, "shared");
  CHECK_NOT_NULL(lazy_sfi_edge);
  const HeapEntry* lazy_sfi = lazy_sfi_edge->to();

  // Check that start_position and end_position edges exist on compiled_sfi.
  std::optional<int> start_position =
      GetIntEdge(compiled_sfi, "start_position");
  EXPECT_TRUE(start_position.has_value());
  EXPECT_EQ(92, start_position.value());

  std::optional<int> end_position = GetIntEdge(compiled_sfi, "end_position");
  EXPECT_TRUE(end_position.has_value());
  EXPECT_EQ(120, end_position.value());

  std::optional<int> lazy_start_position =
      GetIntEdge(lazy_sfi, "start_position");
  EXPECT_TRUE(lazy_start_position.has_value());
  EXPECT_EQ(31, lazy_start_position.value());

  std::optional<int> lazy_end_position = GetIntEdge(lazy_sfi, "end_position");
  EXPECT_TRUE(lazy_end_position.has_value());
  EXPECT_EQ(52, lazy_end_position.value());
}

}  // namespace v8::internal
