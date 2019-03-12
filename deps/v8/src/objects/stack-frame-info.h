// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STACK_FRAME_INFO_H_
#define V8_OBJECTS_STACK_FRAME_INFO_H_

#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StackFrameInfo : public Struct {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_INT_ACCESSORS(line_number)
  DECL_INT_ACCESSORS(column_number)
  DECL_INT_ACCESSORS(script_id)
  DECL_ACCESSORS(script_name, Object)
  DECL_ACCESSORS(script_name_or_source_url, Object)
  DECL_ACCESSORS(function_name, Object)
  DECL_BOOLEAN_ACCESSORS(is_eval)
  DECL_BOOLEAN_ACCESSORS(is_constructor)
  DECL_BOOLEAN_ACCESSORS(is_wasm)
  DECL_INT_ACCESSORS(flag)
  DECL_INT_ACCESSORS(id)

  DECL_CAST(StackFrameInfo)

  // Dispatched behavior.
  DECL_PRINTER(StackFrameInfo)
  DECL_VERIFIER(StackFrameInfo)

  // Layout description.
#define STACK_FRAME_INFO_FIELDS(V)            \
  V(kLineNumberIndex, kTaggedSize)            \
  V(kColumnNumberIndex, kTaggedSize)          \
  V(kScriptIdIndex, kTaggedSize)              \
  V(kScriptNameIndex, kTaggedSize)            \
  V(kScriptNameOrSourceUrlIndex, kTaggedSize) \
  V(kFunctionNameIndex, kTaggedSize)          \
  V(kFlagIndex, kTaggedSize)                  \
  V(kIdIndex, kTaggedSize)                    \
  /* Total size. */                           \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize, STACK_FRAME_INFO_FIELDS)
#undef STACK_FRAME_INFO_FIELDS

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kIsEvalBit = 0;
  static const int kIsConstructorBit = 1;
  static const int kIsWasmBit = 2;

  OBJECT_CONSTRUCTORS(StackFrameInfo, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_H_
