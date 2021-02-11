// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

TEST(CodeLayoutWithoutUnwindingInfo) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());

  // "Hello, World!" in ASCII, padded to kCodeAlignment.
  byte buffer_array[16] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x57,
                           0x6F, 0x72, 0x6C, 0x64, 0x21, 0xcc, 0xcc, 0xcc};

  byte* buffer = &buffer_array[0];
  int buffer_size = sizeof(buffer_array);

  CodeDesc code_desc;
  code_desc.buffer = buffer;
  code_desc.buffer_size = buffer_size;
  code_desc.instr_size = buffer_size;
  code_desc.safepoint_table_offset = buffer_size;
  code_desc.safepoint_table_size = 0;
  code_desc.handler_table_offset = buffer_size;
  code_desc.handler_table_size = 0;
  code_desc.constant_pool_offset = buffer_size;
  code_desc.constant_pool_size = 0;
  code_desc.code_comments_offset = buffer_size;
  code_desc.code_comments_size = 0;
  code_desc.reloc_offset = buffer_size;
  code_desc.reloc_size = 0;
  code_desc.unwinding_info = nullptr;
  code_desc.unwinding_info_size = 0;
  code_desc.origin = nullptr;

  Handle<Code> code = Factory::CodeBuilder(CcTest::i_isolate(), code_desc,
                                           CodeKind::FOR_TESTING)
                          .Build();

  CHECK(!code->has_unwinding_info());
  CHECK_EQ(code->raw_instruction_size(), buffer_size);
  CHECK_EQ(0, memcmp(reinterpret_cast<void*>(code->raw_instruction_start()),
                     buffer, buffer_size));
  CHECK_EQ(code->raw_instruction_end() - code->raw_instruction_start(),
           buffer_size);
}

TEST(CodeLayoutWithUnwindingInfo) {
  CcTest::InitializeVM();
  HandleScope handle_scope(CcTest::i_isolate());

  // "Hello, World!" in ASCII, padded to kCodeAlignment.
  byte buffer_array[16] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x57,
                           0x6F, 0x72, 0x6C, 0x64, 0x21, 0xcc, 0xcc, 0xcc};

  // "JavaScript" in ASCII.
  byte unwinding_info_array[10] = {0x4A, 0x61, 0x76, 0x61, 0x53,
                                   0x63, 0x72, 0x69, 0x70, 0x74};

  byte* buffer = &buffer_array[0];
  int buffer_size = sizeof(buffer_array);
  byte* unwinding_info = &unwinding_info_array[0];
  int unwinding_info_size = sizeof(unwinding_info_array);

  CodeDesc code_desc;
  code_desc.buffer = buffer;
  code_desc.buffer_size = buffer_size;
  code_desc.instr_size = buffer_size;
  code_desc.safepoint_table_offset = buffer_size;
  code_desc.safepoint_table_size = 0;
  code_desc.handler_table_offset = buffer_size;
  code_desc.handler_table_size = 0;
  code_desc.constant_pool_offset = buffer_size;
  code_desc.constant_pool_size = 0;
  code_desc.code_comments_offset = buffer_size;
  code_desc.code_comments_size = 0;
  code_desc.reloc_offset = buffer_size;
  code_desc.reloc_size = 0;
  code_desc.unwinding_info = unwinding_info;
  code_desc.unwinding_info_size = unwinding_info_size;
  code_desc.origin = nullptr;

  Handle<Code> code = Factory::CodeBuilder(CcTest::i_isolate(), code_desc,
                                           CodeKind::FOR_TESTING)
                          .Build();

  CHECK(code->has_unwinding_info());
  CHECK_EQ(code->raw_body_size(), buffer_size + unwinding_info_size);
  CHECK_EQ(0, memcmp(reinterpret_cast<void*>(code->raw_instruction_start()),
                     buffer, buffer_size));
  CHECK_EQ(code->unwinding_info_size(), unwinding_info_size);
  CHECK_EQ(memcmp(reinterpret_cast<void*>(code->unwinding_info_start()),
                  unwinding_info, unwinding_info_size),
           0);
  CHECK_EQ(code->unwinding_info_end() - code->raw_instruction_start(),
           buffer_size + unwinding_info_size);
}

}  // namespace internal
}  // namespace v8
