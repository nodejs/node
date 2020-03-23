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
class WasmInstanceObject;

class StackFrameInfo : public Struct {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_INT_ACCESSORS(line_number)
  DECL_INT_ACCESSORS(column_number)
  DECL_INT_ACCESSORS(script_id)
  DECL_INT_ACCESSORS(wasm_function_index)
  DECL_INT_ACCESSORS(promise_all_index)
  // Wasm frames only: function_offset instead of promise_all_index.
  DECL_INT_ACCESSORS(function_offset)
  DECL_ACCESSORS(script_name, Object)
  DECL_ACCESSORS(script_name_or_source_url, Object)
  DECL_ACCESSORS(function_name, Object)
  DECL_ACCESSORS(method_name, Object)
  DECL_ACCESSORS(type_name, Object)
  DECL_ACCESSORS(eval_origin, Object)
  DECL_ACCESSORS(wasm_module_name, Object)
  DECL_ACCESSORS(wasm_instance, Object)
  DECL_BOOLEAN_ACCESSORS(is_eval)
  DECL_BOOLEAN_ACCESSORS(is_constructor)
  DECL_BOOLEAN_ACCESSORS(is_wasm)
  DECL_BOOLEAN_ACCESSORS(is_asmjs_wasm)
  DECL_BOOLEAN_ACCESSORS(is_user_java_script)
  DECL_BOOLEAN_ACCESSORS(is_toplevel)
  DECL_BOOLEAN_ACCESSORS(is_async)
  DECL_BOOLEAN_ACCESSORS(is_promise_all)
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
  static const int kIsAsmJsWasmBit = 3;
  static const int kIsUserJavaScriptBit = 4;
  static const int kIsToplevelBit = 5;
  static const int kIsAsyncBit = 6;
  static const int kIsPromiseAllBit = 7;

  OBJECT_CONSTRUCTORS(StackFrameInfo, Struct);
};

// This class is used to lazily initialize a StackFrameInfo object from
// a FrameArray plus an index.
// The first time any of the Get* or Is* methods is called, a
// StackFrameInfo object is allocated and all necessary information
// retrieved.
class StackTraceFrame
    : public TorqueGeneratedStackTraceFrame<StackTraceFrame, Struct> {
 public:
  NEVER_READ_ONLY_SPACE

  // Dispatched behavior.
  DECL_PRINTER(StackTraceFrame)

  static int GetLineNumber(Handle<StackTraceFrame> frame);
  static int GetOneBasedLineNumber(Handle<StackTraceFrame> frame);
  static int GetColumnNumber(Handle<StackTraceFrame> frame);
  static int GetOneBasedColumnNumber(Handle<StackTraceFrame> frame);
  static int GetScriptId(Handle<StackTraceFrame> frame);
  static int GetPromiseAllIndex(Handle<StackTraceFrame> frame);
  static int GetFunctionOffset(Handle<StackTraceFrame> frame);
  static int GetWasmFunctionIndex(Handle<StackTraceFrame> frame);

  static Handle<Object> GetFileName(Handle<StackTraceFrame> frame);
  static Handle<Object> GetScriptNameOrSourceUrl(Handle<StackTraceFrame> frame);
  static Handle<Object> GetFunctionName(Handle<StackTraceFrame> frame);
  static Handle<Object> GetMethodName(Handle<StackTraceFrame> frame);
  static Handle<Object> GetTypeName(Handle<StackTraceFrame> frame);
  static Handle<Object> GetEvalOrigin(Handle<StackTraceFrame> frame);
  static Handle<Object> GetWasmModuleName(Handle<StackTraceFrame> frame);
  static Handle<WasmInstanceObject> GetWasmInstance(
      Handle<StackTraceFrame> frame);

  static bool IsEval(Handle<StackTraceFrame> frame);
  static bool IsConstructor(Handle<StackTraceFrame> frame);
  static bool IsWasm(Handle<StackTraceFrame> frame);
  static bool IsAsmJsWasm(Handle<StackTraceFrame> frame);
  static bool IsUserJavaScript(Handle<StackTraceFrame> frame);
  static bool IsToplevel(Handle<StackTraceFrame> frame);
  static bool IsAsync(Handle<StackTraceFrame> frame);
  static bool IsPromiseAll(Handle<StackTraceFrame> frame);

 private:
  static Handle<StackFrameInfo> GetFrameInfo(Handle<StackTraceFrame> frame);
  static void InitializeFrameInfo(Handle<StackTraceFrame> frame);

  TQ_OBJECT_CONSTRUCTORS(StackTraceFrame)
};

// Small helper that retrieves the FrameArray from a stack-trace
// consisting of a FixedArray of StackTraceFrame objects.
// This helper is only temporary until all FrameArray use-sites have
// been converted to use StackTraceFrame and StackFrameInfo objects.
V8_EXPORT_PRIVATE
Handle<FrameArray> GetFrameArrayFromStackTrace(Isolate* isolate,
                                               Handle<FixedArray> stack_trace);

class IncrementalStringBuilder;
void SerializeStackTraceFrame(Isolate* isolate, Handle<StackTraceFrame> frame,
                              IncrementalStringBuilder* builder);
V8_EXPORT_PRIVATE
MaybeHandle<String> SerializeStackTraceFrame(Isolate* isolate,
                                             Handle<StackTraceFrame> frame);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_H_
