// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "src/wasm/wasm-heap.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace wasm_heap_unittest {

class DisjointAllocationPoolTest : public ::testing::Test {
 public:
  Address A(size_t n) { return reinterpret_cast<Address>(n); }
  void CheckLooksLike(const DisjointAllocationPool& mem,
                      std::vector<std::pair<size_t, size_t>> expectation);
  DisjointAllocationPool Make(std::vector<std::pair<size_t, size_t>> model);
};

void DisjointAllocationPoolTest::CheckLooksLike(
    const DisjointAllocationPool& mem,
    std::vector<std::pair<size_t, size_t>> expectation) {
  const auto& ranges = mem.ranges();
  CHECK_EQ(ranges.size(), expectation.size());
  auto iter = expectation.begin();
  for (auto it = ranges.begin(), e = ranges.end(); it != e; ++it, ++iter) {
    CHECK_EQ(it->first, A(iter->first));
    CHECK_EQ(it->second, A(iter->second));
  }
}

DisjointAllocationPool DisjointAllocationPoolTest::Make(
    std::vector<std::pair<size_t, size_t>> model) {
  DisjointAllocationPool ret;
  for (auto& pair : model) {
    ret.Merge(DisjointAllocationPool(A(pair.first), A(pair.second)));
  }
  return ret;
}

TEST_F(DisjointAllocationPoolTest, Construct) {
  DisjointAllocationPool a;
  CHECK(a.IsEmpty());
  CHECK_EQ(a.ranges().size(), 0);
  DisjointAllocationPool b = Make({{1, 5}});
  CHECK(!b.IsEmpty());
  CHECK_EQ(b.ranges().size(), 1);
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}});
  DisjointAllocationPool c;
  a.Merge(std::move(c));
  CheckLooksLike(a, {{1, 5}});
  DisjointAllocationPool e, f;
  e.Merge(std::move(f));
  CHECK(e.IsEmpty());
}

TEST_F(DisjointAllocationPoolTest, SimpleExtract) {
  DisjointAllocationPool a = Make({{1, 5}});
  DisjointAllocationPool b = a.AllocatePool(2);
  CheckLooksLike(a, {{3, 5}});
  CheckLooksLike(b, {{1, 3}});
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}});
  CHECK_EQ(a.ranges().size(), 1);
  CHECK_EQ(a.ranges().front().first, A(1));
  CHECK_EQ(a.ranges().front().second, A(5));
}

TEST_F(DisjointAllocationPoolTest, ExtractAll) {
  DisjointAllocationPool a(A(1), A(5));
  DisjointAllocationPool b = a.AllocatePool(4);
  CheckLooksLike(b, {{1, 5}});
  CHECK(a.IsEmpty());
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}});
}

TEST_F(DisjointAllocationPoolTest, ExtractAccross) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 20}});
  DisjointAllocationPool b = a.AllocatePool(5);
  CheckLooksLike(a, {{11, 20}});
  CheckLooksLike(b, {{1, 5}, {10, 11}});
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}, {10, 20}});
}

TEST_F(DisjointAllocationPoolTest, ReassembleOutOfOrder) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 15}});
  DisjointAllocationPool b = Make({{7, 8}, {20, 22}});
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}, {7, 8}, {10, 15}, {20, 22}});

  DisjointAllocationPool c = Make({{1, 5}, {10, 15}});
  DisjointAllocationPool d = Make({{7, 8}, {20, 22}});
  d.Merge(std::move(c));
  CheckLooksLike(d, {{1, 5}, {7, 8}, {10, 15}, {20, 22}});
}

TEST_F(DisjointAllocationPoolTest, FailToExtract) {
  DisjointAllocationPool a = Make({{1, 5}});
  DisjointAllocationPool b = a.AllocatePool(5);
  CheckLooksLike(a, {{1, 5}});
  CHECK(b.IsEmpty());
}

TEST_F(DisjointAllocationPoolTest, FailToExtractExact) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 14}});
  DisjointAllocationPool b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}, {10, 14}});
  CHECK(b.IsEmpty());
}

TEST_F(DisjointAllocationPoolTest, ExtractExact) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 15}});
  DisjointAllocationPool b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}});
  CheckLooksLike(b, {{10, 15}});
}

TEST_F(DisjointAllocationPoolTest, Merging) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}});
  a.Merge(Make({{15, 20}}));
  CheckLooksLike(a, {{10, 25}});
}

TEST_F(DisjointAllocationPoolTest, MergingMore) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{15, 20}, {25, 30}}));
  CheckLooksLike(a, {{10, 35}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkip) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{25, 30}}));
  CheckLooksLike(a, {{10, 15}, {20, 35}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrc) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{25, 30}, {35, 40}}));
  CheckLooksLike(a, {{10, 15}, {20, 40}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrcWithGap) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{25, 30}, {36, 40}}));
  CheckLooksLike(a, {{10, 15}, {20, 35}, {36, 40}});
}

class WasmCodeManagerTest : public TestWithIsolate {
 public:
  using NativeModulePtr = std::unique_ptr<NativeModule>;
  enum ModuleStyle : int { Fixed = 0, Growable = 1 };

  const std::vector<ModuleStyle> styles() const {
    return std::vector<ModuleStyle>({Fixed, Growable});
  }
  // We pretend all our modules have 10 functions and no imports, just so
  // we can size up the code_table.
  NativeModulePtr AllocFixedModule(WasmCodeManager* manager, size_t size) {
    return manager->NewNativeModule(size, 10, 0, false);
  }

  NativeModulePtr AllocGrowableModule(WasmCodeManager* manager, size_t size) {
    return manager->NewNativeModule(size, 10, 0, true);
  }

  NativeModulePtr AllocModule(WasmCodeManager* manager, size_t size,
                              ModuleStyle style) {
    switch (style) {
      case Fixed:
        return AllocFixedModule(manager, size);
      case Growable:
        return AllocGrowableModule(manager, size);
      default:
        UNREACHABLE();
    }
  }

  WasmCode* AddCode(NativeModule* native_module, uint32_t index, size_t size) {
    CodeDesc desc;
    memset(reinterpret_cast<void*>(&desc), 0, sizeof(CodeDesc));
    std::unique_ptr<byte[]> exec_buff(new byte[size]);
    desc.buffer = exec_buff.get();
    desc.instr_size = static_cast<int>(size);
    return native_module->AddCode(desc, 0, index, 0, {}, false);
  }

  size_t page() const { return base::OS::AllocatePageSize(); }
  v8::Isolate* v8_isolate() const {
    return reinterpret_cast<v8::Isolate*>(isolate());
  }
};

TEST_F(WasmCodeManagerTest, EmptyCase) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 0 * page());
    CHECK_EQ(0, manager.remaining_uncommitted());

    NativeModulePtr native_module = AllocModule(&manager, 1 * page(), style);
    CHECK(native_module);
    WasmCode* code = AddCode(native_module.get(), 0, 10);
    CHECK_NULL(code);
    CHECK_EQ(0, manager.remaining_uncommitted());
    native_module.reset();
    CHECK_EQ(0, manager.remaining_uncommitted());
  }
}

TEST_F(WasmCodeManagerTest, AllocateAndGoOverLimit) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 1 * page());
    CHECK_EQ(1 * page(), manager.remaining_uncommitted());
    NativeModulePtr native_module = AllocModule(&manager, 1 * page(), style);
    CHECK(native_module);
    CHECK_EQ(1 * page(), manager.remaining_uncommitted());
    uint32_t index = 0;
    WasmCode* code = AddCode(native_module.get(), index++, 1 * kCodeAlignment);
    CHECK_NOT_NULL(code);
    CHECK_EQ(0, manager.remaining_uncommitted());

    code = AddCode(native_module.get(), index++, 3 * kCodeAlignment);
    CHECK_NOT_NULL(code);
    CHECK_EQ(0, manager.remaining_uncommitted());

    code = AddCode(native_module.get(), index++, page() - 4 * kCodeAlignment);
    CHECK_NOT_NULL(code);
    CHECK_EQ(0, manager.remaining_uncommitted());

    code = AddCode(native_module.get(), index++, 1 * kCodeAlignment);
    CHECK_NULL(code);
    CHECK_EQ(0, manager.remaining_uncommitted());

    native_module.reset();
    CHECK_EQ(1 * page(), manager.remaining_uncommitted());
  }
}

TEST_F(WasmCodeManagerTest, TotalLimitIrrespectiveOfModuleCount) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 1 * page());
    NativeModulePtr nm1 = AllocModule(&manager, 1 * page(), style);
    NativeModulePtr nm2 = AllocModule(&manager, 1 * page(), style);
    CHECK(nm1);
    CHECK(nm2);
    WasmCode* code = AddCode(nm1.get(), 0, 1 * page());
    CHECK_NOT_NULL(code);
    code = AddCode(nm2.get(), 0, 1 * page());
    CHECK_NULL(code);
  }
}

TEST_F(WasmCodeManagerTest, DifferentHeapsApplyLimitsIndependently) {
  for (auto style : styles()) {
    WasmCodeManager manager1(v8_isolate(), 1 * page());
    WasmCodeManager manager2(v8_isolate(), 2 * page());
    NativeModulePtr nm1 = AllocModule(&manager1, 1 * page(), style);
    NativeModulePtr nm2 = AllocModule(&manager2, 1 * page(), style);
    CHECK(nm1);
    CHECK(nm2);
    WasmCode* code = AddCode(nm1.get(), 0, 1 * page());
    CHECK_NOT_NULL(code);
    CHECK_EQ(0, manager1.remaining_uncommitted());
    code = AddCode(nm2.get(), 0, 1 * page());
    CHECK_NOT_NULL(code);
  }
}

TEST_F(WasmCodeManagerTest, GrowingVsFixedModule) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 3 * page());
    NativeModulePtr nm = AllocModule(&manager, 1 * page(), style);
    WasmCode* code = AddCode(nm.get(), 0, 1 * page() + kCodeAlignment);
    if (style == Fixed) {
      CHECK_NULL(code);
      CHECK_EQ(manager.remaining_uncommitted(), 3 * page());
    } else {
      CHECK_NOT_NULL(code);
      CHECK_EQ(manager.remaining_uncommitted(), 1 * page());
    }
  }
}

TEST_F(WasmCodeManagerTest, CommitIncrements) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 10 * page());
    NativeModulePtr nm = AllocModule(&manager, 3 * page(), style);
    WasmCode* code = AddCode(nm.get(), 0, kCodeAlignment);
    CHECK_NOT_NULL(code);
    CHECK_EQ(manager.remaining_uncommitted(), 9 * page());
    code = AddCode(nm.get(), 1, 2 * page());
    CHECK_NOT_NULL(code);
    CHECK_EQ(manager.remaining_uncommitted(), 7 * page());
    code = AddCode(nm.get(), 2, page() - kCodeAlignment);
    CHECK_NOT_NULL(code);
    CHECK_EQ(manager.remaining_uncommitted(), 7 * page());
  }
}

TEST_F(WasmCodeManagerTest, Lookup) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 2 * page());

    NativeModulePtr nm1 = AllocModule(&manager, 1 * page(), style);
    NativeModulePtr nm2 = AllocModule(&manager, 1 * page(), style);
    WasmCode* code1_0 = AddCode(nm1.get(), 0, kCodeAlignment);
    CHECK_EQ(nm1.get(), code1_0->owner());
    WasmCode* code1_1 = AddCode(nm1.get(), 1, kCodeAlignment);
    WasmCode* code2_0 = AddCode(nm2.get(), 0, kCodeAlignment);
    WasmCode* code2_1 = AddCode(nm2.get(), 1, kCodeAlignment);
    CHECK_EQ(nm2.get(), code2_1->owner());

    CHECK_EQ(0, code1_0->index());
    CHECK_EQ(1, code1_1->index());
    CHECK_EQ(0, code2_0->index());
    CHECK_EQ(1, code2_1->index());

    // we know the manager object is allocated here, so we shouldn't
    // find any WasmCode* associated with that ptr.
    WasmCode* not_found =
        manager.LookupCode(reinterpret_cast<Address>(&manager));
    CHECK_NULL(not_found);
    WasmCode* found = manager.LookupCode(code1_0->instructions().start());
    CHECK_EQ(found, code1_0);
    found = manager.LookupCode(code2_1->instructions().start() +
                               (code2_1->instructions().size() / 2));
    CHECK_EQ(found, code2_1);
    found = manager.LookupCode(code2_1->instructions().start() +
                               code2_1->instructions().size() - 1);
    CHECK_EQ(found, code2_1);
    found = manager.LookupCode(code2_1->instructions().start() +
                               code2_1->instructions().size());
    CHECK_NULL(found);
    Address mid_code1_1 =
        code1_1->instructions().start() + (code1_1->instructions().size() / 2);
    CHECK_EQ(code1_1, manager.LookupCode(mid_code1_1));
    nm1.reset();
    CHECK_NULL(manager.LookupCode(mid_code1_1));
  }
}

TEST_F(WasmCodeManagerTest, MultiManagerLookup) {
  for (auto style : styles()) {
    WasmCodeManager manager1(v8_isolate(), 2 * page());
    WasmCodeManager manager2(v8_isolate(), 2 * page());

    NativeModulePtr nm1 = AllocModule(&manager1, 1 * page(), style);
    NativeModulePtr nm2 = AllocModule(&manager2, 1 * page(), style);

    WasmCode* code1_0 = AddCode(nm1.get(), 0, kCodeAlignment);
    CHECK_EQ(nm1.get(), code1_0->owner());
    WasmCode* code1_1 = AddCode(nm1.get(), 1, kCodeAlignment);
    WasmCode* code2_0 = AddCode(nm2.get(), 0, kCodeAlignment);
    WasmCode* code2_1 = AddCode(nm2.get(), 1, kCodeAlignment);
    CHECK_EQ(nm2.get(), code2_1->owner());

    CHECK_EQ(0, code1_0->index());
    CHECK_EQ(1, code1_1->index());
    CHECK_EQ(0, code2_0->index());
    CHECK_EQ(1, code2_1->index());

    CHECK_EQ(code1_0, manager1.LookupCode(code1_0->instructions().start()));
    CHECK_NULL(manager2.LookupCode(code1_0->instructions().start()));
  }
}

TEST_F(WasmCodeManagerTest, LookupWorksAfterRewrite) {
  for (auto style : styles()) {
    WasmCodeManager manager(v8_isolate(), 2 * page());

    NativeModulePtr nm1 = AllocModule(&manager, 1 * page(), style);

    WasmCode* code0 = AddCode(nm1.get(), 0, kCodeAlignment);
    WasmCode* code1 = AddCode(nm1.get(), 1, kCodeAlignment);
    CHECK_EQ(0, code0->index());
    CHECK_EQ(1, code1->index());
    CHECK_EQ(code1, manager.LookupCode(code1->instructions().start()));
    WasmCode* code1_1 = AddCode(nm1.get(), 1, kCodeAlignment);
    CHECK_EQ(1, code1_1->index());
    CHECK_EQ(code1, manager.LookupCode(code1->instructions().start()));
    CHECK_EQ(code1_1, manager.LookupCode(code1_1->instructions().start()));
  }
}

}  // namespace wasm_heap_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
