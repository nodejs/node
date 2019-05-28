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

class FrameArray;

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

  DECL_CAST(StackFrameInfo)

  // Dispatched behavior.
  DECL_PRINTER(StackFrameInfo)
  DECL_VERIFIER(StackFrameInfo)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_STACK_FRAME_INFO_FIELDS)

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kIsEvalBit = 0;
  static const int kIsConstructorBit = 1;
  static const int kIsWasmBit = 2;

  OBJECT_CONSTRUCTORS(StackFrameInfo, Struct);
};

// This class is used to lazily initialize a StackFrameInfo object from
// a FrameArray plus an index.
// The first time any of the Get* or Is* methods is called, a
// StackFrameInfo object is allocated and all necessary information
// retrieved.
class StackTraceFrame : public Struct {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_ACCESSORS(frame_array, Object)
  DECL_INT_ACCESSORS(frame_index)
  DECL_ACCESSORS(frame_info, Object)
  DECL_INT_ACCESSORS(id)

  DECL_CAST(StackTraceFrame)

  // Dispatched behavior.
  DECL_PRINTER(StackTraceFrame)
  DECL_VERIFIER(StackTraceFrame)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_STACK_TRACE_FRAME_FIELDS)

  static int GetLineNumber(Handle<StackTraceFrame> frame);
  static int GetColumnNumber(Handle<StackTraceFrame> frame);
  static int GetScriptId(Handle<StackTraceFrame> frame);

  static Handle<Object> GetFileName(Handle<StackTraceFrame> frame);
  static Handle<Object> GetScriptNameOrSourceUrl(Handle<StackTraceFrame> frame);
  static Handle<Object> GetFunctionName(Handle<StackTraceFrame> frame);

  static bool IsEval(Handle<StackTraceFrame> frame);
  static bool IsConstructor(Handle<StackTraceFrame> frame);
  static bool IsWasm(Handle<StackTraceFrame> frame);

 private:
  OBJECT_CONSTRUCTORS(StackTraceFrame, Struct);

  static Handle<StackFrameInfo> GetFrameInfo(Handle<StackTraceFrame> frame);
  static void InitializeFrameInfo(Handle<StackTraceFrame> frame);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_H_
