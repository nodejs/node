// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8.h"

#include "src/codegen/code-desc.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_factory {

namespace {

// This needs to be large enough to create a new nosnap Isolate, but smaller
// than kMaximalCodeRangeSize so we can recover from the OOM.
constexpr int kInstructionSize = 100 * MB;
STATIC_ASSERT(kInstructionSize < kMaximalCodeRangeSize || !kRequiresCodeRange);

size_t NearHeapLimitCallback(void* raw_bool, size_t current_heap_limit,
                             size_t initial_heap_limit) {
  bool* oom_triggered = static_cast<bool*>(raw_bool);
  *oom_triggered = true;
  return kInstructionSize * 2;
}

class SetupIsolateWithSmallHeap {
 public:
  SetupIsolateWithSmallHeap() {
    FLAG_max_old_space_size = kInstructionSize / MB / 2;  // In MB.
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    isolate_ = reinterpret_cast<Isolate*>(v8::Isolate::New(create_params));
    isolate_->heap()->AddNearHeapLimitCallback(NearHeapLimitCallback,
                                               &oom_triggered_);
  }

  ~SetupIsolateWithSmallHeap() {
    reinterpret_cast<v8::Isolate*>(isolate_)->Dispose();
  }

  Isolate* isolate() { return isolate_; }
  bool oom_triggered() const { return oom_triggered_; }

 private:
  Isolate* isolate_;
  bool oom_triggered_ = false;
};

}  // namespace

TEST(Factory_CodeBuilder) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size = kMaxRegularHeapObjectSize + 1;
  std::unique_ptr<byte[]> instructions(new byte[instruction_size]);

  CodeDesc desc;
  desc.buffer = instructions.get();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;
  desc.reloc_size = 0;
  desc.constant_pool_size = 0;
  desc.unwinding_info = nullptr;
  desc.unwinding_info_size = 0;
  desc.origin = nullptr;
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, Code::WASM_FUNCTION).Build();

  CHECK(isolate->heap()->InSpace(*code, CODE_LO_SPACE));
#if VERIFY_HEAP
  code->ObjectVerify(isolate);
#endif
}

UNINITIALIZED_TEST(Factory_CodeBuilder_BuildOOM) {
  SetupIsolateWithSmallHeap isolate_scope;
  HandleScope scope(isolate_scope.isolate());
  std::unique_ptr<byte[]> instructions(new byte[kInstructionSize]);
  CodeDesc desc;
  desc.instr_size = kInstructionSize;
  desc.buffer = instructions.get();

  const Handle<Code> code =
      Factory::CodeBuilder(isolate_scope.isolate(), desc, Code::WASM_FUNCTION)
          .Build();

  CHECK(!code.is_null());
  CHECK(isolate_scope.oom_triggered());
}

UNINITIALIZED_TEST(Factory_CodeBuilder_TryBuildOOM) {
  SetupIsolateWithSmallHeap isolate_scope;
  HandleScope scope(isolate_scope.isolate());
  std::unique_ptr<byte[]> instructions(new byte[kInstructionSize]);
  CodeDesc desc;
  desc.instr_size = kInstructionSize;
  desc.buffer = instructions.get();

  const MaybeHandle<Code> code =
      Factory::CodeBuilder(isolate_scope.isolate(), desc, Code::WASM_FUNCTION)
          .TryBuild();

  CHECK(code.is_null());
  CHECK(!isolate_scope.oom_triggered());
}

}  // namespace test_factory
}  // namespace internal
}  // namespace v8
