// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-memory.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace wasm_heap_unittest {

class DisjointAllocationPoolTest : public ::testing::Test {
 public:
  void CheckPool(const DisjointAllocationPool& mem,
                 std::initializer_list<base::AddressRegion> expected_regions);
  void CheckRange(base::AddressRegion region1, base::AddressRegion region2);
  DisjointAllocationPool Make(
      std::initializer_list<base::AddressRegion> regions);
};

void DisjointAllocationPoolTest::CheckPool(
    const DisjointAllocationPool& mem,
    std::initializer_list<base::AddressRegion> expected_regions) {
  const auto& regions = mem.regions();
  CHECK_EQ(regions.size(), expected_regions.size());
  auto iter = expected_regions.begin();
  for (auto it = regions.begin(), e = regions.end(); it != e; ++it, ++iter) {
    CHECK_EQ(*it, *iter);
  }
}

void DisjointAllocationPoolTest::CheckRange(base::AddressRegion region1,
                                            base::AddressRegion region2) {
  CHECK_EQ(region1, region2);
}

DisjointAllocationPool DisjointAllocationPoolTest::Make(
    std::initializer_list<base::AddressRegion> regions) {
  DisjointAllocationPool ret;
  for (auto& region : regions) {
    ret.Merge(region);
  }
  return ret;
}

TEST_F(DisjointAllocationPoolTest, ConstructEmpty) {
  DisjointAllocationPool a;
  CHECK(a.IsEmpty());
  CheckPool(a, {});
  a.Merge({1, 4});
  CheckPool(a, {{1, 4}});
}

TEST_F(DisjointAllocationPoolTest, ConstructWithRange) {
  DisjointAllocationPool a({1, 4});
  CHECK(!a.IsEmpty());
  CheckPool(a, {{1, 4}});
}

TEST_F(DisjointAllocationPoolTest, SimpleExtract) {
  DisjointAllocationPool a = Make({{1, 4}});
  base::AddressRegion b = a.Allocate(2);
  CheckPool(a, {{3, 2}});
  CheckRange(b, {1, 2});
  a.Merge(b);
  CheckPool(a, {{1, 4}});
  CHECK_EQ(a.regions().size(), 1);
  CHECK_EQ(a.regions().front().begin(), 1);
  CHECK_EQ(a.regions().front().end(), 5);
}

TEST_F(DisjointAllocationPoolTest, ExtractAll) {
  DisjointAllocationPool a({1, 4});
  base::AddressRegion b = a.Allocate(4);
  CheckRange(b, {1, 4});
  CHECK(a.IsEmpty());
  a.Merge(b);
  CheckPool(a, {{1, 4}});
}

TEST_F(DisjointAllocationPoolTest, FailToExtract) {
  DisjointAllocationPool a = Make({{1, 4}});
  base::AddressRegion b = a.Allocate(5);
  CheckPool(a, {{1, 4}});
  CHECK(b.is_empty());
}

TEST_F(DisjointAllocationPoolTest, FailToExtractExact) {
  DisjointAllocationPool a = Make({{1, 4}, {10, 4}});
  base::AddressRegion b = a.Allocate(5);
  CheckPool(a, {{1, 4}, {10, 4}});
  CHECK(b.is_empty());
}

TEST_F(DisjointAllocationPoolTest, ExtractExact) {
  DisjointAllocationPool a = Make({{1, 4}, {10, 5}});
  base::AddressRegion b = a.Allocate(5);
  CheckPool(a, {{1, 4}});
  CheckRange(b, {10, 5});
}

TEST_F(DisjointAllocationPoolTest, Merging) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}});
  a.Merge({15, 5});
  CheckPool(a, {{10, 15}});
}

TEST_F(DisjointAllocationPoolTest, MergingMore) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({15, 5});
  a.Merge({25, 5});
  CheckPool(a, {{10, 25}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkip) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({25, 5});
  CheckPool(a, {{10, 5}, {20, 15}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrc) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({25, 5});
  a.Merge({35, 5});
  CheckPool(a, {{10, 5}, {20, 20}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrcWithGap) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({25, 5});
  a.Merge({36, 4});
  CheckPool(a, {{10, 5}, {20, 15}, {36, 4}});
}

enum ModuleStyle : int { Fixed = 0, Growable = 1 };

std::string PrintWasmCodeManageTestParam(
    ::testing::TestParamInfo<ModuleStyle> info) {
  switch (info.param) {
    case Fixed:
      return "Fixed";
    case Growable:
      return "Growable";
  }
  UNREACHABLE();
}

class WasmCodeManagerTest : public TestWithContext,
                            public ::testing::WithParamInterface<ModuleStyle> {
 public:
  static constexpr uint32_t kNumFunctions = 10;
  static size_t allocate_page_size;
  static size_t commit_page_size;

  WasmCodeManagerTest() {
    CHECK_EQ(allocate_page_size == 0, commit_page_size == 0);
    if (allocate_page_size == 0) {
      allocate_page_size = AllocatePageSize();
      commit_page_size = CommitPageSize();
    }
    CHECK_NE(0, allocate_page_size);
    CHECK_NE(0, commit_page_size);
    manager()->DisableImplicitAllocationsForTesting();
  }

  using NativeModulePtr = std::shared_ptr<NativeModule>;

  NativeModulePtr AllocModule(size_t size, ModuleStyle style) {
    std::shared_ptr<WasmModule> module(new WasmModule);
    module->num_declared_functions = kNumFunctions;
    bool can_request_more = style == Growable;
    return engine()->NewNativeModule(i_isolate(), kAllWasmFeatures, size,
                                     can_request_more, std::move(module));
  }

  WasmCode* AddCode(NativeModule* native_module, uint32_t index, size_t size) {
    CodeDesc desc;
    memset(reinterpret_cast<void*>(&desc), 0, sizeof(CodeDesc));
    std::unique_ptr<byte[]> exec_buff(new byte[size]);
    desc.buffer = exec_buff.get();
    desc.instr_size = static_cast<int>(size);
    std::unique_ptr<WasmCode> code = native_module->AddCode(
        index, desc, 0, 0, {}, {}, WasmCode::kFunction, ExecutionTier::kNone);
    return native_module->PublishCode(std::move(code));
  }

  WasmEngine* engine() { return i_isolate()->wasm_engine(); }

  WasmCodeManager* manager() { return engine()->code_manager(); }

  void SetMaxCommittedMemory(size_t limit) {
    manager()->SetMaxCommittedMemoryForTesting(limit);
  }
};

// static
size_t WasmCodeManagerTest::allocate_page_size = 0;
size_t WasmCodeManagerTest::commit_page_size = 0;

INSTANTIATE_TEST_SUITE_P(Parameterized, WasmCodeManagerTest,
                         ::testing::Values(Fixed, Growable),
                         PrintWasmCodeManageTestParam);

TEST_P(WasmCodeManagerTest, EmptyCase) {
  SetMaxCommittedMemory(0);
  CHECK_EQ(0, manager()->committed_code_space());

  NativeModulePtr native_module = AllocModule(allocate_page_size, GetParam());
  ASSERT_DEATH_IF_SUPPORTED(AddCode(native_module.get(), 0, kCodeAlignment),
                            "OOM in wasm code commit");
}

TEST_P(WasmCodeManagerTest, AllocateAndGoOverLimit) {
  SetMaxCommittedMemory(allocate_page_size);

  CHECK_EQ(0, manager()->committed_code_space());
  NativeModulePtr native_module = AllocModule(allocate_page_size, GetParam());
  CHECK(native_module);
  CHECK_EQ(0, manager()->committed_code_space());
  WasmCodeRefScope code_ref_scope;
  uint32_t index = 0;
  WasmCode* code = AddCode(native_module.get(), index++, 1 * kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(commit_page_size, manager()->committed_code_space());

  code = AddCode(native_module.get(), index++, 3 * kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(commit_page_size, manager()->committed_code_space());

  code = AddCode(native_module.get(), index++,
                 allocate_page_size - 4 * kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(allocate_page_size, manager()->committed_code_space());

  // This fails in "reservation" if we cannot extend the code space, or in
  // "commit" it we can (since we hit the allocation limit in the
  // WasmCodeManager). Hence don't check for that part of the OOM message.
  ASSERT_DEATH_IF_SUPPORTED(
      AddCode(native_module.get(), index++, 1 * kCodeAlignment),
      "OOM in wasm code");
}

TEST_P(WasmCodeManagerTest, TotalLimitIrrespectiveOfModuleCount) {
  SetMaxCommittedMemory(3 * allocate_page_size);

  NativeModulePtr nm1 = AllocModule(2 * allocate_page_size, GetParam());
  NativeModulePtr nm2 = AllocModule(2 * allocate_page_size, GetParam());
  CHECK(nm1);
  CHECK(nm2);
  WasmCodeRefScope code_ref_scope;
  WasmCode* code = AddCode(nm1.get(), 0, 2 * allocate_page_size);
  CHECK_NOT_NULL(code);
  ASSERT_DEATH_IF_SUPPORTED(AddCode(nm2.get(), 0, 2 * allocate_page_size),
                            "OOM in wasm code commit");
}

TEST_P(WasmCodeManagerTest, GrowingVsFixedModule) {
  SetMaxCommittedMemory(3 * allocate_page_size);

  NativeModulePtr nm = AllocModule(allocate_page_size, GetParam());
  size_t module_size =
      GetParam() == Fixed ? kMaxWasmCodeMemory : allocate_page_size;
  size_t remaining_space_in_module = module_size;
  if (GetParam() == Fixed) {
    // Requesting more than the remaining space fails because the module cannot
    // grow.
    ASSERT_DEATH_IF_SUPPORTED(
        AddCode(nm.get(), 0, remaining_space_in_module + kCodeAlignment),
        "OOM in wasm code reservation");
  } else {
    // The module grows by one page. One page remains uncommitted.
    WasmCodeRefScope code_ref_scope;
    CHECK_NOT_NULL(
        AddCode(nm.get(), 0, remaining_space_in_module + kCodeAlignment));
    CHECK_EQ(commit_page_size + allocate_page_size,
             manager()->committed_code_space());
  }
}

TEST_P(WasmCodeManagerTest, CommitIncrements) {
  SetMaxCommittedMemory(10 * allocate_page_size);

  NativeModulePtr nm = AllocModule(3 * allocate_page_size, GetParam());
  WasmCodeRefScope code_ref_scope;
  WasmCode* code = AddCode(nm.get(), 0, kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(commit_page_size, manager()->committed_code_space());
  code = AddCode(nm.get(), 1, 2 * allocate_page_size);
  CHECK_NOT_NULL(code);
  CHECK_EQ(commit_page_size + 2 * allocate_page_size,
           manager()->committed_code_space());
  code = AddCode(nm.get(), 2, allocate_page_size - kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(3 * allocate_page_size, manager()->committed_code_space());
}

TEST_P(WasmCodeManagerTest, Lookup) {
  SetMaxCommittedMemory(2 * allocate_page_size);

  NativeModulePtr nm1 = AllocModule(allocate_page_size, GetParam());
  NativeModulePtr nm2 = AllocModule(allocate_page_size, GetParam());
  Address mid_code1_1;
  {
    // The {WasmCodeRefScope} needs to die before {nm1} dies.
    WasmCodeRefScope code_ref_scope;
    WasmCode* code1_0 = AddCode(nm1.get(), 0, kCodeAlignment);
    CHECK_EQ(nm1.get(), code1_0->native_module());
    WasmCode* code1_1 = AddCode(nm1.get(), 1, kCodeAlignment);
    WasmCode* code2_0 = AddCode(nm2.get(), 0, kCodeAlignment);
    WasmCode* code2_1 = AddCode(nm2.get(), 1, kCodeAlignment);
    CHECK_EQ(nm2.get(), code2_1->native_module());

    CHECK_EQ(0, code1_0->index());
    CHECK_EQ(1, code1_1->index());
    CHECK_EQ(0, code2_0->index());
    CHECK_EQ(1, code2_1->index());

    // we know the manager object is allocated here, so we shouldn't
    // find any WasmCode* associated with that ptr.
    WasmCode* not_found =
        manager()->LookupCode(reinterpret_cast<Address>(manager()));
    CHECK_NULL(not_found);
    WasmCode* found = manager()->LookupCode(code1_0->instruction_start());
    CHECK_EQ(found, code1_0);
    found = manager()->LookupCode(code2_1->instruction_start() +
                                  (code2_1->instructions().size() / 2));
    CHECK_EQ(found, code2_1);
    found = manager()->LookupCode(code2_1->instruction_start() +
                                  code2_1->instructions().size() - 1);
    CHECK_EQ(found, code2_1);
    found = manager()->LookupCode(code2_1->instruction_start() +
                                  code2_1->instructions().size());
    CHECK_NULL(found);
    mid_code1_1 =
        code1_1->instruction_start() + (code1_1->instructions().size() / 2);
    CHECK_EQ(code1_1, manager()->LookupCode(mid_code1_1));
  }
  nm1.reset();
  CHECK_NULL(manager()->LookupCode(mid_code1_1));
}

TEST_P(WasmCodeManagerTest, LookupWorksAfterRewrite) {
  SetMaxCommittedMemory(2 * allocate_page_size);

  NativeModulePtr nm1 = AllocModule(allocate_page_size, GetParam());

  WasmCodeRefScope code_ref_scope;
  WasmCode* code0 = AddCode(nm1.get(), 0, kCodeAlignment);
  WasmCode* code1 = AddCode(nm1.get(), 1, kCodeAlignment);
  CHECK_EQ(0, code0->index());
  CHECK_EQ(1, code1->index());
  CHECK_EQ(code1, manager()->LookupCode(code1->instruction_start()));
  WasmCode* code1_1 = AddCode(nm1.get(), 1, kCodeAlignment);
  CHECK_EQ(1, code1_1->index());
  CHECK_EQ(code1, manager()->LookupCode(code1->instruction_start()));
  CHECK_EQ(code1_1, manager()->LookupCode(code1_1->instruction_start()));
}

}  // namespace wasm_heap_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
