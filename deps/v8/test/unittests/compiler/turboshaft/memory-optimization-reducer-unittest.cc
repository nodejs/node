// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/memory-optimization-reducer.h"

#include "src/compiler/turboshaft/operations.h"
#include "src/execution/isolate-data.h"
#include "src/roots/roots.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

using MemoryOptimizationReducerTest = ReducerTest;

TEST_F(MemoryOptimizationReducerTest,
       SkipWriteBarrierForImmutableReadOnlyRootLoad) {
  TestInstance test = CreateFromGraph(0, [](TestInstance& t) {
    V<WordPtr> root_reg = t.Asm().LoadRootRegister();
    V<Object> ro_first = t.Asm().Load(
        root_reg, LoadOp::Kind::RawAligned().Immutable(),
        MemoryRepresentation::AnyUncompressedTagged(),
        IsolateData::root_slot_offset(RootIndex::kFirstReadOnlyRoot));
    V<Object> ro_null =
        t.Asm().Load(root_reg, LoadOp::Kind::RawAligned().Immutable(),
                     MemoryRepresentation::AnyUncompressedTagged(),
                     IsolateData::root_slot_offset(RootIndex::kNullValue));
    V<Object> ro_last = t.Asm().Load(
        root_reg, LoadOp::Kind::RawAligned().Immutable(),
        MemoryRepresentation::UncompressedTaggedPointer(),
        IsolateData::root_slot_offset(RootIndex::kLastReadOnlyRoot));
#if V8_ENABLE_WEBASSEMBLY
    V<Object> ro_wasm_null =
        t.Asm().Load(root_reg, LoadOp::Kind::RawAligned().Immutable(),
                     MemoryRepresentation::AnyUncompressedTagged(),
                     IsolateData::root_slot_offset(RootIndex::kWasmNull));
#endif
    V<Object> below_ro = t.Asm().Load(
        root_reg, LoadOp::Kind::RawAligned().Immutable(),
        MemoryRepresentation::AnyUncompressedTagged(),
        IsolateData::root_slot_offset(RootIndex::kFirstReadOnlyRoot) -
            kSystemPointerSize);
    V<Object> non_ro = t.Asm().Load(
        root_reg, LoadOp::Kind::RawAligned().Immutable(),
        MemoryRepresentation::AnyUncompressedTagged(),
        IsolateData::root_slot_offset(RootIndex::kFirstStrongRoot));
    V<Object> mutable_ro_slot =
        t.Asm().Load(root_reg, LoadOp::Kind::RawAligned(),
                     MemoryRepresentation::AnyUncompressedTagged(),
                     IsolateData::root_slot_offset(RootIndex::kNullValue));
    V<Object> indexed_ro_slot =
        t.Asm().Load(root_reg, t.Asm().IntPtrConstant(0),
                     LoadOp::Kind::RawAligned().Immutable(),
                     MemoryRepresentation::AnyUncompressedTagged(),
                     IsolateData::root_slot_offset(RootIndex::kNullValue), 0);
    V<Object> compressed_ro_slot =
        t.Asm().Load(root_reg, LoadOp::Kind::RawAligned().Immutable(),
                     MemoryRepresentation::TaggedPointer(),
                     IsolateData::root_slot_offset(RootIndex::kNullValue));
    V<Object> tagged_base_load =
        t.Asm().Load(root_reg, LoadOp::Kind::TaggedBase().Immutable(),
                     MemoryRepresentation::AnyUncompressedTagged(),
                     IsolateData::root_slot_offset(RootIndex::kNullValue));
    V<WordPtr> non_root_base = t.Asm().IntPtrConstant(0);
    V<Object> non_root_load =
        t.Asm().Load(non_root_base, LoadOp::Kind::RawAligned().Immutable(),
                     MemoryRepresentation::AnyUncompressedTagged(),
                     IsolateData::root_slot_offset(RootIndex::kNullValue));

    t.Capture(ro_first, "ro_first");
    t.Capture(ro_null, "ro_null");
    t.Capture(ro_last, "ro_last");
#if V8_ENABLE_WEBASSEMBLY
    t.Capture(ro_wasm_null, "ro_wasm_null");
#endif
    t.Capture(below_ro, "below_ro");
    t.Capture(non_ro, "non_ro");
    t.Capture(mutable_ro_slot, "mutable_ro_slot");
    t.Capture(indexed_ro_slot, "indexed_ro_slot");
    t.Capture(compressed_ro_slot, "compressed_ro_slot");
    t.Capture(tagged_base_load, "tagged_base_load");
    t.Capture(non_root_load, "non_root_load");

    t.Asm().Return(t.Asm().Word32Constant(0));
  });

  auto needs_barrier = [&](const char* name, Isolate* iso) {
    OpIndex idx = test.GetCapture(name).input;
    return ValueNeedsWriteBarrier(&test.graph(), test.graph().Get(idx), iso);
  };

  for (Isolate* iso : {isolate(), static_cast<Isolate*>(nullptr)}) {
    EXPECT_FALSE(needs_barrier("ro_first", iso));
    EXPECT_FALSE(needs_barrier("ro_null", iso));
    EXPECT_FALSE(needs_barrier("ro_last", iso));
#if V8_ENABLE_WEBASSEMBLY
    EXPECT_FALSE(needs_barrier("ro_wasm_null", iso));
#endif
    EXPECT_TRUE(needs_barrier("below_ro", iso));
    EXPECT_TRUE(needs_barrier("non_ro", iso));
    EXPECT_TRUE(needs_barrier("mutable_ro_slot", iso));
    EXPECT_TRUE(needs_barrier("indexed_ro_slot", iso));
    EXPECT_TRUE(needs_barrier("compressed_ro_slot", iso));
    EXPECT_TRUE(needs_barrier("tagged_base_load", iso));
    EXPECT_TRUE(needs_barrier("non_root_load", iso));
  }
}

}  // namespace v8::internal::compiler::turboshaft
