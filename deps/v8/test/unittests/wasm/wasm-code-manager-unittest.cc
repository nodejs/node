// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-memory.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace wasm_heap_unittest {

class DisjointAllocationPoolTest : public ::testing::Test {
 public:
  Address A(size_t n) { return static_cast<Address>(n); }
  void CheckLooksLike(const DisjointAllocationPool& mem,
                      std::vector<std::pair<size_t, size_t>> expectation);
  void CheckLooksLike(AddressRange range,
                      std::pair<size_t, size_t> expectation);
  DisjointAllocationPool Make(std::vector<std::pair<size_t, size_t>> model);
};

void DisjointAllocationPoolTest::CheckLooksLike(
    const DisjointAllocationPool& mem,
    std::vector<std::pair<size_t, size_t>> expectation) {
  const auto& ranges = mem.ranges();
  CHECK_EQ(ranges.size(), expectation.size());
  auto iter = expectation.begin();
  for (auto it = ranges.begin(), e = ranges.end(); it != e; ++it, ++iter) {
    CheckLooksLike(*it, *iter);
  }
}

void DisjointAllocationPoolTest::CheckLooksLike(
    AddressRange range, std::pair<size_t, size_t> expectation) {
  CHECK_EQ(range.start, A(expectation.first));
  CHECK_EQ(range.end, A(expectation.second));
}

DisjointAllocationPool DisjointAllocationPoolTest::Make(
    std::vector<std::pair<size_t, size_t>> model) {
  DisjointAllocationPool ret;
  for (auto& pair : model) {
    ret.Merge({A(pair.first), A(pair.second)});
  }
  return ret;
}

TEST_F(DisjointAllocationPoolTest, ConstructEmpty) {
  DisjointAllocationPool a;
  CHECK(a.IsEmpty());
  CheckLooksLike(a, {});
  a.Merge({1, 5});
  CheckLooksLike(a, {{1, 5}});
}

TEST_F(DisjointAllocationPoolTest, ConstructWithRange) {
  DisjointAllocationPool a({1, 5});
  CHECK(!a.IsEmpty());
  CheckLooksLike(a, {{1, 5}});
}

TEST_F(DisjointAllocationPoolTest, SimpleExtract) {
  DisjointAllocationPool a = Make({{1, 5}});
  AddressRange b = a.Allocate(2);
  CheckLooksLike(a, {{3, 5}});
  CheckLooksLike(b, {1, 3});
  a.Merge(b);
  CheckLooksLike(a, {{1, 5}});
  CHECK_EQ(a.ranges().size(), 1);
  CHECK_EQ(a.ranges().front().start, A(1));
  CHECK_EQ(a.ranges().front().end, A(5));
}

TEST_F(DisjointAllocationPoolTest, ExtractAll) {
  DisjointAllocationPool a({A(1), A(5)});
  AddressRange b = a.Allocate(4);
  CheckLooksLike(b, {1, 5});
  CHECK(a.IsEmpty());
  a.Merge(b);
  CheckLooksLike(a, {{1, 5}});
}

TEST_F(DisjointAllocationPoolTest, FailToExtract) {
  DisjointAllocationPool a = Make({{1, 5}});
  AddressRange b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}});
  CHECK(b.is_empty());
}

TEST_F(DisjointAllocationPoolTest, FailToExtractExact) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 14}});
  AddressRange b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}, {10, 14}});
  CHECK(b.is_empty());
}

TEST_F(DisjointAllocationPoolTest, ExtractExact) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 15}});
  AddressRange b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}});
  CheckLooksLike(b, {10, 15});
}

TEST_F(DisjointAllocationPoolTest, Merging) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}});
  a.Merge({15, 20});
  CheckLooksLike(a, {{10, 25}});
}

TEST_F(DisjointAllocationPoolTest, MergingMore) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge({15, 20});
  a.Merge({25, 30});
  CheckLooksLike(a, {{10, 35}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkip) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge({25, 30});
  CheckLooksLike(a, {{10, 15}, {20, 35}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrc) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge({25, 30});
  a.Merge({35, 40});
  CheckLooksLike(a, {{10, 15}, {20, 40}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrcWithGap) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge({25, 30});
  a.Merge({36, 40});
  CheckLooksLike(a, {{10, 15}, {20, 35}, {36, 40}});
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
  static constexpr uint32_t kJumpTableSize = RoundUp<kCodeAlignment>(
      JumpTableAssembler::SizeForNumberOfSlots(kNumFunctions));

  using NativeModulePtr = std::unique_ptr<NativeModule>;

  NativeModulePtr AllocModule(WasmCodeManager* manager, size_t size,
                              ModuleStyle style) {
    std::shared_ptr<WasmModule> module(new WasmModule);
    module->num_declared_functions = kNumFunctions;
    bool can_request_more = style == Growable;
    ModuleEnv env(module.get(), UseTrapHandler::kNoTrapHandler,
                  RuntimeExceptionSupport::kNoRuntimeExceptionSupport);
    return manager->NewNativeModule(i_isolate(), kAllWasmFeatures, size,
                                    can_request_more, std::move(module), env);
  }

  WasmCode* AddCode(NativeModule* native_module, uint32_t index, size_t size) {
    CodeDesc desc;
    memset(reinterpret_cast<void*>(&desc), 0, sizeof(CodeDesc));
    std::unique_ptr<byte[]> exec_buff(new byte[size]);
    desc.buffer = exec_buff.get();
    desc.instr_size = static_cast<int>(size);
    return native_module->AddCode(index, desc, 0, 0, 0, {}, OwnedVector<byte>(),
                                  WasmCode::kOther);
  }

  size_t page() const { return AllocatePageSize(); }

  WasmMemoryTracker* memory_tracker() { return &memory_tracker_; }

 private:
  WasmMemoryTracker memory_tracker_;
};

INSTANTIATE_TEST_CASE_P(Parameterized, WasmCodeManagerTest,
                        ::testing::Values(Fixed, Growable),
                        PrintWasmCodeManageTestParam);

TEST_P(WasmCodeManagerTest, EmptyCase) {
  WasmCodeManager manager(memory_tracker(), 0 * page());
  CHECK_EQ(0, manager.remaining_uncommitted_code_space());

  ASSERT_DEATH_IF_SUPPORTED(AllocModule(&manager, 1 * page(), GetParam()),
                            "OOM in NativeModule::AddOwnedCode");
}

TEST_P(WasmCodeManagerTest, AllocateAndGoOverLimit) {
  WasmCodeManager manager(memory_tracker(), 1 * page());
  CHECK_EQ(1 * page(), manager.remaining_uncommitted_code_space());
  NativeModulePtr native_module = AllocModule(&manager, 1 * page(), GetParam());
  CHECK(native_module);
  CHECK_EQ(0, manager.remaining_uncommitted_code_space());
  uint32_t index = 0;
  WasmCode* code = AddCode(native_module.get(), index++, 1 * kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(0, manager.remaining_uncommitted_code_space());

  code = AddCode(native_module.get(), index++, 3 * kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(0, manager.remaining_uncommitted_code_space());

  code = AddCode(native_module.get(), index++,
                 page() - 4 * kCodeAlignment - kJumpTableSize);
  CHECK_NOT_NULL(code);
  CHECK_EQ(0, manager.remaining_uncommitted_code_space());

  ASSERT_DEATH_IF_SUPPORTED(
      AddCode(native_module.get(), index++, 1 * kCodeAlignment),
      "OOM in NativeModule::AddOwnedCode");
}

TEST_P(WasmCodeManagerTest, TotalLimitIrrespectiveOfModuleCount) {
  WasmCodeManager manager(memory_tracker(), 3 * page());
  NativeModulePtr nm1 = AllocModule(&manager, 2 * page(), GetParam());
  NativeModulePtr nm2 = AllocModule(&manager, 2 * page(), GetParam());
  CHECK(nm1);
  CHECK(nm2);
  WasmCode* code = AddCode(nm1.get(), 0, 2 * page() - kJumpTableSize);
  CHECK_NOT_NULL(code);
  ASSERT_DEATH_IF_SUPPORTED(AddCode(nm2.get(), 0, 2 * page() - kJumpTableSize),
                            "OOM in NativeModule::AddOwnedCode");
}

TEST_P(WasmCodeManagerTest, DifferentHeapsApplyLimitsIndependently) {
  WasmCodeManager manager1(memory_tracker(), 1 * page());
  WasmCodeManager manager2(memory_tracker(), 2 * page());
  NativeModulePtr nm1 = AllocModule(&manager1, 1 * page(), GetParam());
  NativeModulePtr nm2 = AllocModule(&manager2, 1 * page(), GetParam());
  CHECK(nm1);
  CHECK(nm2);
  WasmCode* code = AddCode(nm1.get(), 0, 1 * page() - kJumpTableSize);
  CHECK_NOT_NULL(code);
  CHECK_EQ(0, manager1.remaining_uncommitted_code_space());
  code = AddCode(nm2.get(), 0, 1 * page() - kJumpTableSize);
  CHECK_NOT_NULL(code);
}

TEST_P(WasmCodeManagerTest, GrowingVsFixedModule) {
  WasmCodeManager manager(memory_tracker(), 3 * page());
  NativeModulePtr nm = AllocModule(&manager, 1 * page(), GetParam());
  size_t module_size = GetParam() == Fixed ? kMaxWasmCodeMemory : 1 * page();
  size_t remaining_space_in_module = module_size - kJumpTableSize;
  if (GetParam() == Fixed) {
    // Requesting more than the remaining space fails because the module cannot
    // grow.
    ASSERT_DEATH_IF_SUPPORTED(
        AddCode(nm.get(), 0, remaining_space_in_module + kCodeAlignment),
        "OOM in NativeModule::AddOwnedCode");
  } else {
    // The module grows by one page. One page remains uncommitted.
    CHECK_NOT_NULL(
        AddCode(nm.get(), 0, remaining_space_in_module + kCodeAlignment));
    CHECK_EQ(manager.remaining_uncommitted_code_space(), 1 * page());
  }
}

TEST_P(WasmCodeManagerTest, CommitIncrements) {
  WasmCodeManager manager(memory_tracker(), 10 * page());
  NativeModulePtr nm = AllocModule(&manager, 3 * page(), GetParam());
  WasmCode* code = AddCode(nm.get(), 0, kCodeAlignment);
  CHECK_NOT_NULL(code);
  CHECK_EQ(manager.remaining_uncommitted_code_space(), 9 * page());
  code = AddCode(nm.get(), 1, 2 * page());
  CHECK_NOT_NULL(code);
  CHECK_EQ(manager.remaining_uncommitted_code_space(), 7 * page());
  code = AddCode(nm.get(), 2, page() - kCodeAlignment - kJumpTableSize);
  CHECK_NOT_NULL(code);
  CHECK_EQ(manager.remaining_uncommitted_code_space(), 7 * page());
}

TEST_P(WasmCodeManagerTest, Lookup) {
  WasmCodeManager manager(memory_tracker(), 2 * page());

  NativeModulePtr nm1 = AllocModule(&manager, 1 * page(), GetParam());
  NativeModulePtr nm2 = AllocModule(&manager, 1 * page(), GetParam());
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
  WasmCode* not_found = manager.LookupCode(reinterpret_cast<Address>(&manager));
  CHECK_NULL(not_found);
  WasmCode* found = manager.LookupCode(code1_0->instruction_start());
  CHECK_EQ(found, code1_0);
  found = manager.LookupCode(code2_1->instruction_start() +
                             (code2_1->instructions().size() / 2));
  CHECK_EQ(found, code2_1);
  found = manager.LookupCode(code2_1->instruction_start() +
                             code2_1->instructions().size() - 1);
  CHECK_EQ(found, code2_1);
  found = manager.LookupCode(code2_1->instruction_start() +
                             code2_1->instructions().size());
  CHECK_NULL(found);
  Address mid_code1_1 =
      code1_1->instruction_start() + (code1_1->instructions().size() / 2);
  CHECK_EQ(code1_1, manager.LookupCode(mid_code1_1));
  nm1.reset();
  CHECK_NULL(manager.LookupCode(mid_code1_1));
}

TEST_P(WasmCodeManagerTest, MultiManagerLookup) {
  WasmCodeManager manager1(memory_tracker(), 2 * page());
  WasmCodeManager manager2(memory_tracker(), 2 * page());

  NativeModulePtr nm1 = AllocModule(&manager1, 1 * page(), GetParam());
  NativeModulePtr nm2 = AllocModule(&manager2, 1 * page(), GetParam());

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

  CHECK_EQ(code1_0, manager1.LookupCode(code1_0->instruction_start()));
  CHECK_NULL(manager2.LookupCode(code1_0->instruction_start()));
}

TEST_P(WasmCodeManagerTest, LookupWorksAfterRewrite) {
  WasmCodeManager manager(memory_tracker(), 2 * page());

  NativeModulePtr nm1 = AllocModule(&manager, 1 * page(), GetParam());

  WasmCode* code0 = AddCode(nm1.get(), 0, kCodeAlignment);
  WasmCode* code1 = AddCode(nm1.get(), 1, kCodeAlignment);
  CHECK_EQ(0, code0->index());
  CHECK_EQ(1, code1->index());
  CHECK_EQ(code1, manager.LookupCode(code1->instruction_start()));
  WasmCode* code1_1 = AddCode(nm1.get(), 1, kCodeAlignment);
  CHECK_EQ(1, code1_1->index());
  CHECK_EQ(code1, manager.LookupCode(code1->instruction_start()));
  CHECK_EQ(code1_1, manager.LookupCode(code1_1->instruction_start()));
}

}  // namespace wasm_heap_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
