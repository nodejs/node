// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STACK_FRAME_INFO_H_
#define V8_OBJECTS_STACK_FRAME_INFO_H_

#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class MessageLocation;
class WasmInstanceObject;

#include "torque-generated/src/objects/stack-frame-info-tq.inc"

class StackFrameInfo
    : public TorqueGeneratedStackFrameInfo<StackFrameInfo, Struct> {
 public:
  NEVER_READ_ONLY_SPACE

  inline bool IsWasm() const;
  inline bool IsAsmJsWasm() const;
  inline bool IsStrict() const;
  inline bool IsConstructor() const;
  inline bool IsAsmJsAtNumberConversion() const;
  inline bool IsAsync() const;
  bool IsEval() const;
  bool IsUserJavaScript() const;
  bool IsMethodCall() const;
  bool IsToplevel() const;
  bool IsPromiseAll() const;
  bool IsPromiseAny() const;
  bool IsNative() const;

  // Dispatched behavior.
  DECL_PRINTER(StackFrameInfo)
  DECL_VERIFIER(StackFrameInfo)

  // Used to signal that the requested field is unknown.
  static constexpr int kUnknown = kNoSourcePosition;

  static int GetLineNumber(Handle<StackFrameInfo> info);
  static int GetColumnNumber(Handle<StackFrameInfo> info);

  static int GetEnclosingLineNumber(Handle<StackFrameInfo> info);
  static int GetEnclosingColumnNumber(Handle<StackFrameInfo> info);

  // Returns the script ID if one is attached,
  // Message::kNoScriptIdInfo otherwise.
  int GetScriptId() const;
  Object GetScriptName() const;
  Object GetScriptNameOrSourceURL() const;

  static Handle<PrimitiveHeapObject> GetEvalOrigin(Handle<StackFrameInfo> info);
  static Handle<Object> GetFunctionName(Handle<StackFrameInfo> info);
  static Handle<Object> GetMethodName(Handle<StackFrameInfo> info);
  static Handle<Object> GetTypeName(Handle<StackFrameInfo> info);

  // These methods are only valid for Wasm and asm.js Wasm frames.
  uint32_t GetWasmFunctionIndex() const;
  WasmInstanceObject GetWasmInstance() const;
  static Handle<Object> GetWasmModuleName(Handle<StackFrameInfo> info);

  // Returns the 0-based source position, which is the offset into the
  // Script in case of JavaScript and Asm.js, and the bytecode offset
  // in the module in case of actual Wasm. In case of async promise
  // combinator frames, this returns the index of the promise.
  static int GetSourcePosition(Handle<StackFrameInfo> info);

  // Attempts to fill the |location| based on the |info|, and avoids
  // triggering source position table building for JavaScript frames.
  static bool ComputeLocation(Handle<StackFrameInfo> info,
                              MessageLocation* location);

 private:
  // Bit position in the flag, from least significant bit position.
  DEFINE_TORQUE_GENERATED_STACK_FRAME_INFO_FLAGS()
  friend class StackTraceBuilder;

  static int ComputeSourcePosition(Handle<StackFrameInfo> info, int offset);

  base::Optional<Script> GetScript() const;
  SharedFunctionInfo GetSharedFunctionInfo() const;

  static MaybeHandle<Script> GetScript(Isolate* isolate,
                                       Handle<StackFrameInfo> info);

  TQ_OBJECT_CONSTRUCTORS(StackFrameInfo)
};

class IncrementalStringBuilder;
void SerializeStackFrameInfo(Isolate* isolate, Handle<StackFrameInfo> frame,
                             IncrementalStringBuilder* builder);
V8_EXPORT_PRIVATE
MaybeHandle<String> SerializeStackFrameInfo(Isolate* isolate,
                                            Handle<StackFrameInfo> frame);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_H_
