// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"

#include "src/api/api-inl.h"
#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"
#include "src/heap/spaces.h"
#include "src/objects/code-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_unwinder {

static const void* fake_stack_base = nullptr;

// Ignore deprecation warnings so that we can keep the tests for now.
// TODO(petermarshall): Delete all the tests here when the old API is removed to
// reduce the duplication.
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
#endif

TEST(Unwind_BadState_Fail) {
  UnwindState unwind_state;  // Fields are intialized to nullptr.
  RegisterState register_state;

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 fake_stack_base);
  CHECK(!unwound);
  // The register state should not change when unwinding fails.
  CHECK_NULL(register_state.fp);
  CHECK_NULL(register_state.sp);
  CHECK_NULL(register_state.pc);
}

TEST(Unwind_BuiltinPCInMiddle_Success) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  uintptr_t stack[3];
  void* stack_base = stack + arraysize(stack);
  stack[0] = reinterpret_cast<uintptr_t>(stack + 2);  // saved FP (rbp).
  stack[1] = 202;  // Return address into C++ code.
  stack[2] = 303;  // The SP points here in the caller's frame.

  register_state.sp = stack;
  register_state.fp = stack;

  // Put the current PC inside of a valid builtin.
  Code builtin = i_isolate->builtins()->builtin(Builtins::kStringEqual);
  const uintptr_t offset = 40;
  CHECK_LT(offset, builtin.InstructionSize());
  register_state.pc =
      reinterpret_cast<void*>(builtin.InstructionStart() + offset);

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);
  CHECK(unwound);
  CHECK_EQ(reinterpret_cast<void*>(stack + 2), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 2), register_state.sp);
  CHECK_EQ(reinterpret_cast<void*>(202), register_state.pc);
}

// The unwinder should be able to unwind even if we haven't properly set up the
// current frame, as long as there is another JS frame underneath us (i.e. as
// long as the PC isn't in JSEntry). This test puts the PC at the start
// of a JS builtin and creates a fake JSEntry frame before it on the stack. The
// unwinder should be able to unwind to the C++ frame before the JSEntry frame.
TEST(Unwind_BuiltinPCAtStart_Success) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  const size_t code_length = 40;
  uintptr_t code[code_length] = {0};
  unwind_state.code_range.start = code;
  unwind_state.code_range.length_in_bytes = code_length * sizeof(uintptr_t);

  uintptr_t stack[6];
  void* stack_base = stack + arraysize(stack);
  stack[0] = 101;
  // Return address into JS code. It doesn't matter that this is not actually in
  // JSEntry, because we only check that for the top frame.
  stack[1] = reinterpret_cast<uintptr_t>(code + 10);
  stack[2] = reinterpret_cast<uintptr_t>(stack + 5);  // saved FP (rbp).
  stack[3] = 303;  // Return address into C++ code.
  stack[4] = 404;
  stack[5] = 505;

  register_state.sp = stack;
  register_state.fp = stack + 2;  // FP to the JSEntry frame.

  // Put the current PC at the start of a valid builtin, so that we are setting
  // up the frame.
  Code builtin = i_isolate->builtins()->builtin(Builtins::kStringEqual);
  register_state.pc = reinterpret_cast<void*>(builtin.InstructionStart());

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);

  CHECK(unwound);
  CHECK_EQ(reinterpret_cast<void*>(stack + 5), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 4), register_state.sp);
  CHECK_EQ(reinterpret_cast<void*>(303), register_state.pc);
}

const char* foo_source = R"(
  function foo(a, b) {
    let x = a * b;
    let y = x ^ b;
    let z = y / a;
    return x + y - z;
  };
  %PrepareFunctionForOptimization(foo);
  foo(1, 2);
  foo(1, 2);
  %OptimizeFunctionOnNextCall(foo);
  foo(1, 2);
)";

// Check that we can unwind when the pc is within an optimized code object on
// the V8 heap.
TEST(Unwind_CodeObjectPCInMiddle_Success) {
  FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  HandleScope scope(i_isolate);

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  uintptr_t stack[3];
  void* stack_base = stack + arraysize(stack);
  stack[0] = reinterpret_cast<uintptr_t>(stack + 2);  // saved FP (rbp).
  stack[1] = 202;  // Return address into C++ code.
  stack[2] = 303;  // The SP points here in the caller's frame.

  register_state.sp = stack;
  register_state.fp = stack;

  // Create an on-heap code object. Make sure we run the function so that it is
  // compiled and not just marked for lazy compilation.
  CompileRun(foo_source);
  v8::Local<v8::Function> local_foo = v8::Local<v8::Function>::Cast(
      env.local()->Global()->Get(env.local(), v8_str("foo")).ToLocalChecked());
  Handle<JSFunction> foo =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(*local_foo));

  // Put the current PC inside of the created code object.
  AbstractCode abstract_code = foo->abstract_code();
  // We don't produce optimized code when run with --no-opt.
  if (!abstract_code.IsCode() && FLAG_opt == false) return;
  CHECK(abstract_code.IsCode());

  Code code = abstract_code.GetCode();
  // We don't want the offset too early or it could be the `push rbp`
  // instruction (which is not at the start of generated code, because the lazy
  // deopt check happens before frame setup).
  const uintptr_t offset = code.InstructionSize() - 20;
  CHECK_LT(offset, code.InstructionSize());
  Address pc = code.InstructionStart() + offset;
  register_state.pc = reinterpret_cast<void*>(pc);

  // Check that the created code is within the code range that we get from the
  // API.
  Address start = reinterpret_cast<Address>(unwind_state.code_range.start);
  CHECK(pc >= start && pc < start + unwind_state.code_range.length_in_bytes);

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);
  CHECK(unwound);
  CHECK_EQ(reinterpret_cast<void*>(stack + 2), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 2), register_state.sp);
  CHECK_EQ(reinterpret_cast<void*>(202), register_state.pc);
}

// If the PC is within JSEntry but we haven't set up the frame yet, then we
// cannot unwind.
TEST(Unwind_JSEntryBeforeFrame_Fail) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  const size_t code_length = 40;
  uintptr_t code[code_length] = {0};
  unwind_state.code_range.start = code;
  unwind_state.code_range.length_in_bytes = code_length * sizeof(uintptr_t);

  // Pretend that it takes 5 instructions to set up the frame in JSEntry.
  unwind_state.js_entry_stub.code.start = code + 10;
  unwind_state.js_entry_stub.code.length_in_bytes = 10 * sizeof(uintptr_t);

  uintptr_t stack[10];
  void* stack_base = stack + arraysize(stack);
  stack[0] = 101;
  stack[1] = 111;
  stack[2] = 121;
  stack[3] = 131;
  stack[4] = 141;
  stack[5] = 151;
  stack[6] = 100;  // Return address into C++ code.
  stack[7] = 303;  // The SP points here in the caller's frame.
  stack[8] = 404;
  stack[9] = 505;

  register_state.sp = stack + 5;
  register_state.fp = stack + 9;

  // Put the current PC inside of JSEntry, before the frame is set up.
  register_state.pc = code + 12;
  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);
  CHECK(!unwound);
  // The register state should not change when unwinding fails.
  CHECK_EQ(reinterpret_cast<void*>(stack + 9), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 5), register_state.sp);
  CHECK_EQ(code + 12, register_state.pc);

  // Change the PC to a few instructions later, after the frame is set up.
  register_state.pc = code + 16;
  unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                            stack_base);
  // TODO(petermarshall): More precisely check position within JSEntry rather
  // than just assuming the frame is unreadable.
  CHECK(!unwound);
  // The register state should not change when unwinding fails.
  CHECK_EQ(reinterpret_cast<void*>(stack + 9), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 5), register_state.sp);
  CHECK_EQ(code + 16, register_state.pc);
}

TEST(Unwind_OneJSFrame_Success) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  // Use a fake code range so that we can initialize it to 0s.
  const size_t code_length = 40;
  uintptr_t code[code_length] = {0};
  unwind_state.code_range.start = code;
  unwind_state.code_range.length_in_bytes = code_length * sizeof(uintptr_t);

  // Our fake stack has two frames - one C++ frame and one JS frame (on top).
  // The stack grows from high addresses to low addresses.
  uintptr_t stack[10];
  void* stack_base = stack + arraysize(stack);
  stack[0] = 101;
  stack[1] = 111;
  stack[2] = 121;
  stack[3] = 131;
  stack[4] = 141;
  stack[5] = reinterpret_cast<uintptr_t>(stack + 9);  // saved FP (rbp).
  stack[6] = 100;  // Return address into C++ code.
  stack[7] = 303;  // The SP points here in the caller's frame.
  stack[8] = 404;
  stack[9] = 505;

  register_state.sp = stack;
  register_state.fp = stack + 5;

  // Put the current PC inside of the code range so it looks valid.
  register_state.pc = code + 30;

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);

  CHECK(unwound);
  CHECK_EQ(reinterpret_cast<void*>(stack + 9), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 7), register_state.sp);
  CHECK_EQ(reinterpret_cast<void*>(100), register_state.pc);
}

// Creates a fake stack with two JS frames on top of a C++ frame and checks that
// the unwinder correctly unwinds past the JS frames and returns the C++ frame's
// details.
TEST(Unwind_TwoJSFrames_Success) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  // Use a fake code range so that we can initialize it to 0s.
  const size_t code_length = 40;
  uintptr_t code[code_length] = {0};
  unwind_state.code_range.start = code;
  unwind_state.code_range.length_in_bytes = code_length * sizeof(uintptr_t);

  // Our fake stack has three frames - one C++ frame and two JS frames (on top).
  // The stack grows from high addresses to low addresses.
  uintptr_t stack[10];
  void* stack_base = stack + arraysize(stack);
  stack[0] = 101;
  stack[1] = 111;
  stack[2] = reinterpret_cast<uintptr_t>(stack + 5);  // saved FP (rbp).
  // The fake return address is in the JS code range.
  stack[3] = reinterpret_cast<uintptr_t>(code + 10);
  stack[4] = 141;
  stack[5] = reinterpret_cast<uintptr_t>(stack + 9);  // saved FP (rbp).
  stack[6] = 100;  // Return address into C++ code.
  stack[7] = 303;  // The SP points here in the caller's frame.
  stack[8] = 404;
  stack[9] = 505;

  register_state.sp = stack;
  register_state.fp = stack + 2;

  // Put the current PC inside of the code range so it looks valid.
  register_state.pc = code + 30;

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);

  CHECK(unwound);
  CHECK_EQ(reinterpret_cast<void*>(stack + 9), register_state.fp);
  CHECK_EQ(reinterpret_cast<void*>(stack + 7), register_state.sp);
  CHECK_EQ(reinterpret_cast<void*>(100), register_state.pc);
}

// If the PC is in JSEntry then the frame might not be set up correctly, meaning
// we can't unwind the stack properly.
TEST(Unwind_JSEntry_Fail) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  Code js_entry = i_isolate->heap()->builtin(Builtins::kJSEntry);
  byte* start = reinterpret_cast<byte*>(js_entry.InstructionStart());
  register_state.pc = start + 10;

  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 fake_stack_base);
  CHECK(!unwound);
  // The register state should not change when unwinding fails.
  CHECK_NULL(register_state.fp);
  CHECK_NULL(register_state.sp);
  CHECK_EQ(start + 10, register_state.pc);
}

TEST(Unwind_StackBounds_Basic) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  const size_t code_length = 10;
  uintptr_t code[code_length] = {0};
  unwind_state.code_range.start = code;
  unwind_state.code_range.length_in_bytes = code_length * sizeof(uintptr_t);

  uintptr_t stack[3];
  stack[0] = reinterpret_cast<uintptr_t>(stack + 2);  // saved FP (rbp).
  stack[1] = 202;  // Return address into C++ code.
  stack[2] = 303;  // The SP points here in the caller's frame.

  register_state.sp = stack;
  register_state.fp = stack;
  register_state.pc = code;

  void* wrong_stack_base = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(stack) - sizeof(uintptr_t));
  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 wrong_stack_base);
  CHECK(!unwound);

  // Correct the stack base and unwinding should succeed.
  void* correct_stack_base = stack + arraysize(stack);
  unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                            correct_stack_base);
  CHECK(unwound);
}

TEST(Unwind_StackBounds_WithUnwinding) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();
  RegisterState register_state;

  // Use a fake code range so that we can initialize it to 0s.
  const size_t code_length = 40;
  uintptr_t code[code_length] = {0};
  unwind_state.code_range.start = code;
  unwind_state.code_range.length_in_bytes = code_length * sizeof(uintptr_t);

  // Our fake stack has two frames - one C++ frame and one JS frame (on top).
  // The stack grows from high addresses to low addresses.
  uintptr_t stack[11];
  void* stack_base = stack + arraysize(stack);
  stack[0] = 101;
  stack[1] = 111;
  stack[2] = 121;
  stack[3] = 131;
  stack[4] = 141;
  stack[5] = reinterpret_cast<uintptr_t>(stack + 9);  // saved FP (rbp).
  stack[6] = reinterpret_cast<uintptr_t>(code + 20);  // JS code.
  stack[7] = 303;  // The SP points here in the caller's frame.
  stack[8] = 404;
  stack[9] = reinterpret_cast<uintptr_t>(stack) +
             (12 * sizeof(uintptr_t));                 // saved FP (OOB).
  stack[10] = reinterpret_cast<uintptr_t>(code + 20);  // JS code.

  register_state.sp = stack;
  register_state.fp = stack + 5;

  // Put the current PC inside of the code range so it looks valid.
  register_state.pc = code + 30;

  // Unwind will fail because stack[9] FP points outside of the stack.
  bool unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                                 stack_base);
  CHECK(!unwound);

  // Change the return address so that it is not in range. We will not range
  // check the stack[9] FP value because we have finished unwinding and the
  // contents of rbp does not necessarily have to be the FP in this case.
  stack[10] = 202;
  unwound = v8::Unwinder::TryUnwindV8Frames(unwind_state, &register_state,
                                            stack_base);
  CHECK(unwound);
}

TEST(PCIsInV8_BadState_Fail) {
  UnwindState unwind_state;
  void* pc = nullptr;

  CHECK(!v8::Unwinder::PCIsInV8(unwind_state, pc));
}

TEST(PCIsInV8_ValidStateNullPC_Fail) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();
  void* pc = nullptr;

  CHECK(!v8::Unwinder::PCIsInV8(unwind_state, pc));
}

void TestRangeBoundaries(const UnwindState& unwind_state, byte* range_start,
                         size_t range_length) {
  void* pc = range_start - 1;
  CHECK(!v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = range_start;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = range_start + 1;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = range_start + range_length - 1;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = range_start + range_length;
  CHECK(!v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = range_start + range_length + 1;
  CHECK(!v8::Unwinder::PCIsInV8(unwind_state, pc));
}

TEST(PCIsInV8_InCodeOrEmbeddedRange) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();

  UnwindState unwind_state = isolate->GetUnwindState();

  byte* code_range_start = const_cast<byte*>(
      reinterpret_cast<const byte*>(unwind_state.code_range.start));
  size_t code_range_length = unwind_state.code_range.length_in_bytes;
  TestRangeBoundaries(unwind_state, code_range_start, code_range_length);

  byte* embedded_range_start = const_cast<byte*>(
      reinterpret_cast<const byte*>(unwind_state.embedded_code_range.start));
  size_t embedded_range_length =
      unwind_state.embedded_code_range.length_in_bytes;
  TestRangeBoundaries(unwind_state, embedded_range_start,
                      embedded_range_length);
}

// PCIsInV8 doesn't check if the PC is in JSEntry directly. It's assumed that
// the CodeRange or EmbeddedCodeRange contain JSEntry.
TEST(PCIsInV8_InJSEntryRange) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  UnwindState unwind_state = isolate->GetUnwindState();

  Code js_entry = i_isolate->heap()->builtin(Builtins::kJSEntry);
  byte* start = reinterpret_cast<byte*>(js_entry.InstructionStart());
  size_t length = js_entry.InstructionSize();

  void* pc = start;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = start + 1;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
  pc = start + length - 1;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
}

// Large code objects can be allocated in large object space. Check that this is
// inside the CodeRange.
TEST(PCIsInV8_LargeCodeObject) {
  FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  HandleScope scope(i_isolate);

  UnwindState unwind_state = isolate->GetUnwindState();

  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size = Page::kPageSize + 1;
  STATIC_ASSERT(instruction_size > kMaxRegularHeapObjectSize);
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
  Handle<Code> foo_code =
      Factory::CodeBuilder(i_isolate, desc, Code::WASM_FUNCTION).Build();

  CHECK(i_isolate->heap()->InSpace(*foo_code, CODE_LO_SPACE));
  byte* start = reinterpret_cast<byte*>(foo_code->InstructionStart());

  void* pc = start;
  CHECK(v8::Unwinder::PCIsInV8(unwind_state, pc));
}

#if __clang__
#pragma clang diagnostic pop
#endif

}  // namespace test_unwinder
}  // namespace internal
}  // namespace v8
