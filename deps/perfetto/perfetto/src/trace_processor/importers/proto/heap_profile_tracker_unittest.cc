/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/heap_profile_tracker.h"

#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

struct Packet {
  uint32_t mapping_name_id;
  uint32_t build_id;
  uint32_t frame_name_id;
  uint32_t mapping_id;
  uint32_t frame_id;
};

constexpr Packet kFirstPacket{1, 2, 3, 1, 1};
constexpr Packet kSecondPacket{3, 2, 1, 2, 2};

constexpr auto kMappingExactOffset = 123;
constexpr auto kMappingStartOffset = 1231;
constexpr auto kMappingStart = 234;
constexpr auto kMappingEnd = 345;
constexpr auto kMappingLoadBias = 456;
constexpr auto kDefaultSequence = 1;

// heapprofd on Android Q has large callstack ideas, explicitly test large
// values.
constexpr auto kCallstackId = 1ull << 34;

static constexpr auto kFrameRelPc = 567;
static constexpr char kBuildIDName[] = "[build id]";
static constexpr char kBuildIDHexName[] = "5b6275696c642069645d";

using ::testing::ElementsAre;

class HeapProfileTrackerDupTest : public ::testing::Test {
 public:
  HeapProfileTrackerDupTest() {
    context.storage.reset(new TraceStorage());
    stack_profile_tracker.reset(new StackProfileTracker(&context));
    context.heap_profile_tracker.reset(new HeapProfileTracker(&context));

    mapping_name = context.storage->InternString("[mapping]");
    fully_qualified_mapping_name = context.storage->InternString("/[mapping]");
    build = context.storage->InternString(kBuildIDName);
    frame_name = context.storage->InternString("[frame]");
  }

 protected:
  void InsertMapping(const Packet& packet) {
    stack_profile_tracker->AddString(packet.mapping_name_id, "[mapping]");

    stack_profile_tracker->AddString(packet.build_id, kBuildIDName);

    StackProfileTracker::SourceMapping first_frame;
    first_frame.build_id = packet.build_id;
    first_frame.exact_offset = kMappingExactOffset;
    first_frame.start_offset = kMappingStartOffset;
    first_frame.start = kMappingStart;
    first_frame.end = kMappingEnd;
    first_frame.load_bias = kMappingLoadBias;
    first_frame.name_ids = {packet.mapping_name_id};

    stack_profile_tracker->AddMapping(packet.mapping_id, first_frame);
  }

  void InsertFrame(const Packet& packet) {
    InsertMapping(packet);
    stack_profile_tracker->AddString(packet.frame_name_id, "[frame]");

    StackProfileTracker::SourceFrame first_frame;
    first_frame.name_id = packet.frame_name_id;
    first_frame.mapping_id = packet.mapping_id;
    first_frame.rel_pc = kFrameRelPc;

    stack_profile_tracker->AddFrame(packet.frame_id, first_frame);
  }

  void InsertCallsite(const Packet& packet) {
    InsertFrame(packet);

    StackProfileTracker::SourceCallstack first_callsite = {packet.frame_id,
                                                           packet.frame_id};
    stack_profile_tracker->AddCallstack(kCallstackId, first_callsite);
  }

  StringId mapping_name;
  StringId fully_qualified_mapping_name;
  StringId build;
  StringId frame_name;
  TraceProcessorContext context;
  std::unique_ptr<StackProfileTracker> stack_profile_tracker;
};

// Insert the same mapping from two different packets, with different strings
// interned, and assert we only store one.
TEST_F(HeapProfileTrackerDupTest, Mapping) {
  InsertMapping(kFirstPacket);
  context.heap_profile_tracker->FinalizeProfile(
      kDefaultSequence, stack_profile_tracker.get(), nullptr);
  InsertMapping(kSecondPacket);
  context.heap_profile_tracker->FinalizeProfile(
      kDefaultSequence, stack_profile_tracker.get(), nullptr);

  EXPECT_THAT(context.storage->stack_profile_mapping_table().build_id()[0],
              context.storage->InternString({kBuildIDHexName}));
  EXPECT_THAT(context.storage->stack_profile_mapping_table().exact_offset()[0],
              kMappingExactOffset);
  EXPECT_THAT(context.storage->stack_profile_mapping_table().start_offset()[0],
              kMappingStartOffset);
  EXPECT_THAT(context.storage->stack_profile_mapping_table().start()[0],
              kMappingStart);
  EXPECT_THAT(context.storage->stack_profile_mapping_table().end()[0],
              kMappingEnd);
  EXPECT_THAT(context.storage->stack_profile_mapping_table().load_bias()[0],
              kMappingLoadBias);
  EXPECT_THAT(context.storage->stack_profile_mapping_table().name()[0],
              fully_qualified_mapping_name);
}

// Insert the same mapping from two different packets, with different strings
// interned, and assert we only store one.
TEST_F(HeapProfileTrackerDupTest, Frame) {
  InsertFrame(kFirstPacket);
  context.heap_profile_tracker->FinalizeProfile(
      kDefaultSequence, stack_profile_tracker.get(), nullptr);
  InsertFrame(kSecondPacket);
  context.heap_profile_tracker->FinalizeProfile(
      kDefaultSequence, stack_profile_tracker.get(), nullptr);

  const auto& frames = context.storage->stack_profile_frame_table();
  EXPECT_THAT(frames.name()[0], frame_name);
  EXPECT_THAT(frames.mapping()[0], MappingId{0});
  EXPECT_THAT(frames.rel_pc()[0], kFrameRelPc);
}

// Insert the same callstack from two different packets, assert it is only
// stored once.
TEST_F(HeapProfileTrackerDupTest, Callstack) {
  InsertCallsite(kFirstPacket);
  context.heap_profile_tracker->FinalizeProfile(
      kDefaultSequence, stack_profile_tracker.get(), nullptr);
  InsertCallsite(kSecondPacket);
  context.heap_profile_tracker->FinalizeProfile(
      kDefaultSequence, stack_profile_tracker.get(), nullptr);

  const auto& callsite_table = context.storage->stack_profile_callsite_table();
  const auto& depth = callsite_table.depth();
  const auto& parent_id = callsite_table.parent_id();
  const auto& frame_id = callsite_table.frame_id();

  EXPECT_EQ(depth[0], 0u);
  EXPECT_EQ(depth[1], 1u);

  EXPECT_EQ(parent_id[0], base::nullopt);
  EXPECT_EQ(parent_id[1], CallsiteId{0});

  EXPECT_EQ(frame_id[0], FrameId{0});
  EXPECT_EQ(frame_id[1], FrameId{0});
}

base::Optional<CallsiteId> FindCallstack(const TraceStorage& storage,
                                         int64_t depth,
                                         base::Optional<CallsiteId> parent,
                                         FrameId frame_id) {
  const auto& callsites = storage.stack_profile_callsite_table();
  for (uint32_t i = 0; i < callsites.row_count(); ++i) {
    if (callsites.depth()[i] == depth && callsites.parent_id()[i] == parent &&
        callsites.frame_id()[i] == frame_id) {
      return callsites.id()[i];
    }
  }
  return base::nullopt;
}

TEST(HeapProfileTrackerTest, SourceMappingPath) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  context.heap_profile_tracker.reset(new HeapProfileTracker(&context));

  HeapProfileTracker* hpt = context.heap_profile_tracker.get();
  std::unique_ptr<StackProfileTracker> spt(new StackProfileTracker(&context));

  constexpr auto kBuildId = 1u;
  constexpr auto kMappingNameId1 = 2u;
  constexpr auto kMappingNameId2 = 3u;

  spt->AddString(kBuildId, "buildid");
  spt->AddString(kMappingNameId1, "foo");
  spt->AddString(kMappingNameId2, "bar");

  StackProfileTracker::SourceMapping mapping;
  mapping.build_id = kBuildId;
  mapping.exact_offset = 1;
  mapping.start_offset = 1;
  mapping.start = 2;
  mapping.end = 3;
  mapping.load_bias = 0;
  mapping.name_ids = {kMappingNameId1, kMappingNameId2};
  spt->AddMapping(0, mapping);
  hpt->CommitAllocations(kDefaultSequence, spt.get(), nullptr);
  auto foo_bar_id = context.storage->string_pool().GetId("/foo/bar");
  ASSERT_NE(foo_bar_id, base::nullopt);
  EXPECT_THAT(context.storage->stack_profile_mapping_table().name()[0],
              *foo_bar_id);
}

// Insert multiple mappings, frames and callstacks and check result.
TEST(HeapProfileTrackerTest, Functional) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  context.heap_profile_tracker.reset(new HeapProfileTracker(&context));

  HeapProfileTracker* hpt = context.heap_profile_tracker.get();
  std::unique_ptr<StackProfileTracker> spt(new StackProfileTracker(&context));

  uint32_t next_string_intern_id = 1;

  const std::string build_ids[] = {"build1", "build2", "build3"};
  uint32_t build_id_ids[base::ArraySize(build_ids)];
  for (size_t i = 0; i < base::ArraySize(build_ids); ++i)
    build_id_ids[i] = next_string_intern_id++;

  const std::string mapping_names[] = {"map1", "map2", "map3"};
  uint32_t mapping_name_ids[base::ArraySize(mapping_names)];
  for (size_t i = 0; i < base::ArraySize(mapping_names); ++i)
    mapping_name_ids[i] = next_string_intern_id++;

  StackProfileTracker::SourceMapping mappings[base::ArraySize(mapping_names)] =
      {};
  mappings[0].build_id = build_id_ids[0];
  mappings[0].exact_offset = 1;
  mappings[0].start_offset = 1;
  mappings[0].start = 2;
  mappings[0].end = 3;
  mappings[0].load_bias = 0;
  mappings[0].name_ids = {mapping_name_ids[0], mapping_name_ids[1]};

  mappings[1].build_id = build_id_ids[1];
  mappings[1].exact_offset = 1;
  mappings[1].start_offset = 1;
  mappings[1].start = 2;
  mappings[1].end = 3;
  mappings[1].load_bias = 1;
  mappings[1].name_ids = {mapping_name_ids[1]};

  mappings[2].build_id = build_id_ids[2];
  mappings[2].exact_offset = 1;
  mappings[2].start_offset = 1;
  mappings[2].start = 2;
  mappings[2].end = 3;
  mappings[2].load_bias = 2;
  mappings[2].name_ids = {mapping_name_ids[2]};

  const std::string function_names[] = {"fun1", "fun2", "fun3", "fun4"};
  uint32_t function_name_ids[base::ArraySize(function_names)];
  for (size_t i = 0; i < base::ArraySize(function_names); ++i)
    function_name_ids[i] = next_string_intern_id++;

  StackProfileTracker::SourceFrame frames[base::ArraySize(function_names)];
  frames[0].name_id = function_name_ids[0];
  frames[0].mapping_id = 0;
  frames[0].rel_pc = 123;

  frames[1].name_id = function_name_ids[1];
  frames[1].mapping_id = 0;
  frames[1].rel_pc = 123;

  frames[2].name_id = function_name_ids[2];
  frames[2].mapping_id = 1;
  frames[2].rel_pc = 123;

  frames[3].name_id = function_name_ids[3];
  frames[3].mapping_id = 2;
  frames[3].rel_pc = 123;

  StackProfileTracker::SourceCallstack callstacks[3];
  callstacks[0] = {2, 1, 0};
  callstacks[1] = {2, 1, 0, 1, 0};
  callstacks[2] = {0, 2, 0, 1, 2};

  for (size_t i = 0; i < base::ArraySize(build_ids); ++i) {
    auto interned = base::StringView(build_ids[i].data(), build_ids[i].size());
    spt->AddString(build_id_ids[i], interned);
  }
  for (size_t i = 0; i < base::ArraySize(mapping_names); ++i) {
    auto interned =
        base::StringView(mapping_names[i].data(), mapping_names[i].size());
    spt->AddString(mapping_name_ids[i], interned);
  }
  for (size_t i = 0; i < base::ArraySize(function_names); ++i) {
    auto interned =
        base::StringView(function_names[i].data(), function_names[i].size());
    spt->AddString(function_name_ids[i], interned);
  }

  for (uint32_t i = 0; i < base::ArraySize(mappings); ++i)
    spt->AddMapping(i, mappings[i]);
  for (uint32_t i = 0; i < base::ArraySize(frames); ++i)
    spt->AddFrame(i, frames[i]);
  for (uint32_t i = 0; i < base::ArraySize(callstacks); ++i)
    spt->AddCallstack(i, callstacks[i]);

  hpt->CommitAllocations(kDefaultSequence, spt.get(), nullptr);

  for (size_t i = 0; i < base::ArraySize(callstacks); ++i) {
    base::Optional<CallsiteId> parent;
    const StackProfileTracker::SourceCallstack& callstack = callstacks[i];
    for (size_t depth = 0; depth < callstack.size(); ++depth) {
      auto frame_id = spt->GetDatabaseFrameIdForTesting(callstack[depth]);
      base::Optional<CallsiteId> self = FindCallstack(
          *context.storage, static_cast<int64_t>(depth), parent, frame_id);
      ASSERT_TRUE(self.has_value());
      parent = self;
    }
  }

  hpt->FinalizeProfile(kDefaultSequence, spt.get(), nullptr);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
