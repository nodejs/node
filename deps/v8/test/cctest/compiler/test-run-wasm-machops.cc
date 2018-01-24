// Copyright 2016 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <cmath>
#include <functional>
#include <limits>

#include "src/base/bits.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

static void UpdateFunctionTableSizeReferences(Handle<Code> code,
                                              uint32_t old_size,
                                              uint32_t new_size) {
  Isolate* isolate = CcTest::i_isolate();
  // TODO(6792): No longer needed once WebAssembly code is off heap.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  bool modified = false;
  int mode_mask =
      RelocInfo::ModeMask(RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmFunctionTableSizeReference(mode)) {
      it.rinfo()->update_wasm_function_table_size_reference(isolate, old_size,
                                                            new_size);
      modified = true;
    }
  }
  if (modified) {
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
  }
}

template <typename CType>
static void RunLoadStoreRelocation(MachineType rep) {
  const int kNumElems = 2;
  CType buffer[kNumElems];
  CType new_buffer[kNumElems];
  byte* raw = reinterpret_cast<byte*>(buffer);
  byte* new_raw = reinterpret_cast<byte*>(new_buffer);
  WasmContext wasm_context;
  wasm_context.SetRawMemory(raw, sizeof(buffer));
  for (size_t i = 0; i < sizeof(buffer); i++) {
    raw[i] = static_cast<byte>((i + sizeof(CType)) ^ 0xAA);
    new_raw[i] = static_cast<byte>((i + sizeof(CType)) ^ 0xAA);
  }
  uint32_t OK = 0x29000;
  RawMachineAssemblerTester<uint32_t> m;
  Node* wasm_context_node =
      m.RelocatableIntPtrConstant(reinterpret_cast<uintptr_t>(&wasm_context),
                                  RelocInfo::WASM_CONTEXT_REFERENCE);
  Node* offset = m.Int32Constant(offsetof(WasmContext, mem_start));
  Node* base = m.Load(MachineType::UintPtr(), wasm_context_node, offset);
  Node* base1 = m.IntPtrAdd(base, m.Int32Constant(sizeof(CType)));
  Node* index = m.Int32Constant(0);
  Node* load = m.Load(rep, base, index);
  m.Store(rep.representation(), base1, index, load, kNoWriteBarrier);
  m.Return(m.Int32Constant(OK));
  CHECK(buffer[0] != buffer[1]);
  CHECK_EQ(OK, m.Call());
  CHECK(buffer[0] == buffer[1]);
  wasm_context.SetRawMemory(new_raw, sizeof(new_buffer));
  CHECK(new_buffer[0] != new_buffer[1]);
  CHECK_EQ(OK, m.Call());
  CHECK(new_buffer[0] == new_buffer[1]);
}

TEST(RunLoadStoreRelocation) {
  RunLoadStoreRelocation<int8_t>(MachineType::Int8());
  RunLoadStoreRelocation<uint8_t>(MachineType::Uint8());
  RunLoadStoreRelocation<int16_t>(MachineType::Int16());
  RunLoadStoreRelocation<uint16_t>(MachineType::Uint16());
  RunLoadStoreRelocation<int32_t>(MachineType::Int32());
  RunLoadStoreRelocation<uint32_t>(MachineType::Uint32());
  RunLoadStoreRelocation<void*>(MachineType::AnyTagged());
  RunLoadStoreRelocation<float>(MachineType::Float32());
  RunLoadStoreRelocation<double>(MachineType::Float64());
}

template <typename CType>
static void RunLoadStoreRelocationOffset(MachineType rep) {
  RawMachineAssemblerTester<int32_t> r(MachineType::Int32());
  const int kNumElems = 4;
  CType buffer[kNumElems];
  CType new_buffer[kNumElems + 1];
  WasmContext wasm_context;

  for (int32_t x = 0; x < kNumElems; x++) {
    int32_t y = kNumElems - x - 1;
    // initialize the buffer with raw data.
    byte* raw = reinterpret_cast<byte*>(buffer);
    wasm_context.SetRawMemory(raw, sizeof(buffer));
    for (size_t i = 0; i < sizeof(buffer); i++) {
      raw[i] = static_cast<byte>((i + sizeof(buffer)) ^ 0xAA);
    }

    RawMachineAssemblerTester<int32_t> m;
    int32_t OK = 0x29000 + x;
    Node* wasm_context_node =
        m.RelocatableIntPtrConstant(reinterpret_cast<uintptr_t>(&wasm_context),
                                    RelocInfo::WASM_CONTEXT_REFERENCE);
    Node* offset = m.Int32Constant(offsetof(WasmContext, mem_start));
    Node* base = m.Load(MachineType::UintPtr(), wasm_context_node, offset);
    Node* index0 = m.IntPtrConstant(x * sizeof(buffer[0]));
    Node* load = m.Load(rep, base, index0);
    Node* index1 = m.IntPtrConstant(y * sizeof(buffer[0]));
    m.Store(rep.representation(), base, index1, load, kNoWriteBarrier);
    m.Return(m.Int32Constant(OK));

    CHECK(buffer[x] != buffer[y]);
    CHECK_EQ(OK, m.Call());
    CHECK(buffer[x] == buffer[y]);

    // Initialize new buffer and set old_buffer to 0
    byte* new_raw = reinterpret_cast<byte*>(new_buffer);
    for (size_t i = 0; i < sizeof(buffer); i++) {
      raw[i] = 0;
      new_raw[i] = static_cast<byte>((i + sizeof(buffer)) ^ 0xAA);
    }

    wasm_context.SetRawMemory(new_raw, sizeof(new_buffer));

    CHECK(new_buffer[x] != new_buffer[y]);
    CHECK_EQ(OK, m.Call());
    CHECK(new_buffer[x] == new_buffer[y]);
  }
}

TEST(RunLoadStoreRelocationOffset) {
  RunLoadStoreRelocationOffset<int8_t>(MachineType::Int8());
  RunLoadStoreRelocationOffset<uint8_t>(MachineType::Uint8());
  RunLoadStoreRelocationOffset<int16_t>(MachineType::Int16());
  RunLoadStoreRelocationOffset<uint16_t>(MachineType::Uint16());
  RunLoadStoreRelocationOffset<int32_t>(MachineType::Int32());
  RunLoadStoreRelocationOffset<uint32_t>(MachineType::Uint32());
  RunLoadStoreRelocationOffset<void*>(MachineType::AnyTagged());
  RunLoadStoreRelocationOffset<float>(MachineType::Float32());
  RunLoadStoreRelocationOffset<double>(MachineType::Float64());
}

TEST(Uint32LessThanMemoryRelocation) {
  RawMachineAssemblerTester<uint32_t> m;
  RawMachineLabel within_bounds, out_of_bounds;
  WasmContext wasm_context;
  wasm_context.SetRawMemory(reinterpret_cast<void*>(1234), 0x200);
  Node* index = m.Int32Constant(0x200);
  Node* wasm_context_node =
      m.RelocatableIntPtrConstant(reinterpret_cast<uintptr_t>(&wasm_context),
                                  RelocInfo::WASM_CONTEXT_REFERENCE);
  Node* offset = m.Int32Constant(offsetof(WasmContext, mem_size));
  Node* limit = m.Load(MachineType::Uint32(), wasm_context_node, offset);
  Node* cond = m.AddNode(m.machine()->Uint32LessThan(), index, limit);
  m.Branch(cond, &within_bounds, &out_of_bounds);
  m.Bind(&within_bounds);
  m.Return(m.Int32Constant(0xaced));
  m.Bind(&out_of_bounds);
  m.Return(m.Int32Constant(0xdeadbeef));
  // Check that index is out of bounds with current size
  CHECK_EQ(0xdeadbeef, m.Call());
  wasm_context.SetRawMemory(wasm_context.mem_start, 0x400);
  // Check that after limit is increased, index is within bounds.
  CHECK_EQ(0xacedu, m.Call());
}

TEST(Uint32LessThanFunctionTableRelocation) {
  RawMachineAssemblerTester<uint32_t> m;
  RawMachineLabel within_bounds, out_of_bounds;
  Node* index = m.Int32Constant(0x200);
  Node* limit = m.RelocatableInt32Constant(
      0x200, RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE);
  Node* cond = m.AddNode(m.machine()->Uint32LessThan(), index, limit);
  m.Branch(cond, &within_bounds, &out_of_bounds);
  m.Bind(&within_bounds);
  m.Return(m.Int32Constant(0xaced));
  m.Bind(&out_of_bounds);
  m.Return(m.Int32Constant(0xdeadbeef));
  // Check that index is out of bounds with current size
  CHECK_EQ(0xdeadbeef, m.Call());
  m.GenerateCode();

  Handle<Code> code = m.GetCode();
  UpdateFunctionTableSizeReferences(code, 0x200, 0x400);
  // Check that after limit is increased, index is within bounds.
  CHECK_EQ(0xaced, m.Call());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
