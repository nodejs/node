// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/feedback-vector.h ->
// src/feedback-vector-inl.h
#include "src/feedback-vector-inl.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

TEST(CodeLayoutWithoutUnwindingInfo) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());

  // "Hello, World!" in ASCII.
  byte buffer_array[13] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20,
                           0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21};

  byte* buffer = &buffer_array[0];
  int buffer_size = sizeof(buffer_array);

  CodeDesc code_desc;
  code_desc.buffer = buffer;
  code_desc.buffer_size = buffer_size;
  code_desc.constant_pool_size = 0;
  code_desc.instr_size = buffer_size;
  code_desc.reloc_size = 0;
  code_desc.origin = nullptr;
  code_desc.unwinding_info = nullptr;
  code_desc.unwinding_info_size = 0;

  Handle<Code> code = CcTest::i_isolate()->factory()->NewCode(
      code_desc, 0, Handle<Object>::null());

  CHECK(!code->has_unwinding_info());
  CHECK_EQ(code->instruction_size(), buffer_size);
  CHECK_EQ(0, memcmp(code->instruction_start(), buffer, buffer_size));
  CHECK_EQ(code->instruction_end() - reinterpret_cast<byte*>(*code),
           Code::kHeaderSize + buffer_size - kHeapObjectTag);
}

TEST(CodeLayoutWithUnwindingInfo) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());

  // "Hello, World!" in ASCII.
  byte buffer_array[13] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20,
                           0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21};

  // "JavaScript" in ASCII.
  byte unwinding_info_array[10] = {0x4a, 0x61, 0x76, 0x61, 0x53,
                                   0x63, 0x72, 0x69, 0x70, 0x74};

  byte* buffer = &buffer_array[0];
  int buffer_size = sizeof(buffer_array);
  byte* unwinding_info = &unwinding_info_array[0];
  int unwinding_info_size = sizeof(unwinding_info_array);

  CodeDesc code_desc;
  code_desc.buffer = buffer;
  code_desc.buffer_size = buffer_size;
  code_desc.constant_pool_size = 0;
  code_desc.instr_size = buffer_size;
  code_desc.reloc_size = 0;
  code_desc.origin = nullptr;
  code_desc.unwinding_info = unwinding_info;
  code_desc.unwinding_info_size = unwinding_info_size;

  Handle<Code> code = CcTest::i_isolate()->factory()->NewCode(
      code_desc, 0, Handle<Object>::null());

  CHECK(code->has_unwinding_info());
  CHECK_EQ(code->instruction_size(), buffer_size);
  CHECK_EQ(0, memcmp(code->instruction_start(), buffer, buffer_size));
  CHECK(IsAligned(code->GetUnwindingInfoSizeOffset(), 8));
  CHECK_EQ(code->unwinding_info_size(), unwinding_info_size);
  CHECK(
      IsAligned(reinterpret_cast<uintptr_t>(code->unwinding_info_start()), 8));
  CHECK_EQ(
      memcmp(code->unwinding_info_start(), unwinding_info, unwinding_info_size),
      0);
  CHECK_EQ(code->unwinding_info_end() - reinterpret_cast<byte*>(*code),
           Code::kHeaderSize + RoundUp(buffer_size, kInt64Size) + kInt64Size +
               unwinding_info_size - kHeapObjectTag);
}
