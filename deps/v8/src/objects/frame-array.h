// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FRAME_ARRAY_H_
#define V8_OBJECTS_FRAME_ARRAY_H_

#include "src/objects.h"
#include "src/wasm/wasm-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

#define FRAME_ARRAY_FIELD_LIST(V)     \
  V(WasmInstance, WasmInstanceObject) \
  V(WasmFunctionIndex, Smi)           \
  V(WasmCodeObject, Object)           \
  V(Receiver, Object)                 \
  V(Function, JSFunction)             \
  V(Code, AbstractCode)               \
  V(Offset, Smi)                      \
  V(Flags, Smi)                       \
  V(Parameters, FixedArray)

// Container object for data collected during simple stack trace captures.
class FrameArray : public FixedArray {
 public:
#define DECL_FRAME_ARRAY_ACCESSORS(name, type) \
  inline type name(int frame_ix) const;        \
  inline void Set##name(int frame_ix, type value);
  FRAME_ARRAY_FIELD_LIST(DECL_FRAME_ARRAY_ACCESSORS)
#undef DECL_FRAME_ARRAY_ACCESSORS

  inline bool IsWasmFrame(int frame_ix) const;
  inline bool IsWasmInterpretedFrame(int frame_ix) const;
  inline bool IsAsmJsWasmFrame(int frame_ix) const;
  inline bool IsAnyWasmFrame(int frame_ix) const;
  inline int FrameCount() const;

  void ShrinkToFit(Isolate* isolate);

  // Flags.
  enum Flag {
    kIsWasmFrame = 1 << 0,
    kIsWasmInterpretedFrame = 1 << 1,
    kIsAsmJsWasmFrame = 1 << 2,
    kIsStrict = 1 << 3,
    kIsConstructor = 1 << 4,
    kAsmJsAtNumberConversion = 1 << 5,
    kIsAsync = 1 << 6,
    kIsPromiseAll = 1 << 7
  };

  static Handle<FrameArray> AppendJSFrame(Handle<FrameArray> in,
                                          Handle<Object> receiver,
                                          Handle<JSFunction> function,
                                          Handle<AbstractCode> code, int offset,
                                          int flags,
                                          Handle<FixedArray> parameters);
  static Handle<FrameArray> AppendWasmFrame(
      Handle<FrameArray> in, Handle<WasmInstanceObject> wasm_instance,
      int wasm_function_index, wasm::WasmCode* code, int offset, int flags);

  DECL_CAST(FrameArray)

 private:
  // The underlying fixed array embodies a captured stack trace. Frame i
  // occupies indices
  //
  // kFirstIndex + 1 + [i * kElementsPerFrame, (i + 1) * kElementsPerFrame[,
  //
  // with internal offsets as below:

  static const int kWasmInstanceOffset = 0;
  static const int kWasmFunctionIndexOffset = 1;
  static const int kWasmCodeObjectOffset = 2;

  static const int kReceiverOffset = 0;
  static const int kFunctionOffset = 1;

  static const int kCodeOffset = 2;
  static const int kOffsetOffset = 3;

  static const int kFlagsOffset = 4;

  static const int kParametersOffset = 5;

  static const int kElementsPerFrame = 6;

  // Array layout indices.

  static const int kFrameCountIndex = 0;
  static const int kFirstIndex = 1;

  static int LengthFor(int frame_count) {
    return kFirstIndex + frame_count * kElementsPerFrame;
  }

  static Handle<FrameArray> EnsureSpace(Isolate* isolate,
                                        Handle<FrameArray> array, int length);

  friend class Factory;
  OBJECT_CONSTRUCTORS(FrameArray, FixedArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FRAME_ARRAY_H_
