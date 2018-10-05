// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STACK_FRAME_INFO_H_
#define V8_OBJECTS_STACK_FRAME_INFO_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StackFrameInfo : public Struct, public NeverReadOnlySpaceObject {
 public:
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

  static const int kLineNumberIndex = Struct::kHeaderSize;
  static const int kColumnNumberIndex = kLineNumberIndex + kPointerSize;
  static const int kScriptIdIndex = kColumnNumberIndex + kPointerSize;
  static const int kScriptNameIndex = kScriptIdIndex + kPointerSize;
  static const int kScriptNameOrSourceUrlIndex =
      kScriptNameIndex + kPointerSize;
  static const int kFunctionNameIndex =
      kScriptNameOrSourceUrlIndex + kPointerSize;
  static const int kFlagIndex = kFunctionNameIndex + kPointerSize;
  static const int kIdIndex = kFlagIndex + kPointerSize;
  static const int kSize = kIdIndex + kPointerSize;

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kIsEvalBit = 0;
  static const int kIsConstructorBit = 1;
  static const int kIsWasmBit = 2;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackFrameInfo);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_H_
