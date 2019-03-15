// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_DESC_H_
#define V8_CODE_DESC_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

// A CodeDesc describes a buffer holding instructions and relocation
// information. The instructions start at the beginning of the buffer
// and grow forward, the relocation information starts at the end of
// the buffer and grows backward. Inlined metadata sections may exist
// at the end of the instructions.
//
//  │<--------------- buffer_size ----------------------------------->│
//  │<---------------- instr_size ------------->│      │<-reloc_size->│
//  ├───────────────────────────────────────────┼──────┼──────────────┤
//  │ instructions │         data               │ free │  reloc info  │
//  ├───────────────────────────────────────────┴──────┴──────────────┘

// TODO(jgruber): Add a single chokepoint for specifying the instruction area
// layout (i.e. the order of inlined metadata fields).
// TODO(jgruber): Systematically maintain inlined metadata offsets and sizes
// to simplify CodeDesc initialization.

class CodeDesc {
 public:
  static void Initialize(CodeDesc* desc, Assembler* assembler,
                         int safepoint_table_offset, int handler_table_offset,
                         int constant_pool_offset, int code_comments_offset,
                         int reloc_info_offset);

#ifdef DEBUG
  static void Verify(const CodeDesc* desc);
#else
  inline static void Verify(const CodeDesc* desc) {}
#endif

 public:
  byte* buffer = nullptr;
  int buffer_size = 0;

  // The instruction area contains executable code plus inlined metadata.

  int instr_size = 0;

  // Metadata packed into the instructions area.

  int safepoint_table_offset = 0;
  int safepoint_table_size = 0;

  int handler_table_offset = 0;
  int handler_table_size = 0;

  int constant_pool_offset = 0;
  int constant_pool_size = 0;

  int code_comments_offset = 0;
  int code_comments_size = 0;

  // Relocation info is located at the end of the buffer and not part of the
  // instructions area.

  int reloc_offset = 0;
  int reloc_size = 0;

  // Unwinding information.
  // TODO(jgruber,mstarzinger): Pack this into the inlined metadata section.

  byte* unwinding_info = nullptr;
  int unwinding_info_size = 0;

  Assembler* origin = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_DESC_H_
