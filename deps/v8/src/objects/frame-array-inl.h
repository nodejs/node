// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FRAME_ARRAY_INL_H_
#define V8_OBJECTS_FRAME_ARRAY_INL_H_

#include "src/objects/frame-array.h"

#include "src/objects/foreign-inl.h"
#include "src/wasm/wasm-objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(FrameArray, FixedArray)
CAST_ACCESSOR(FrameArray)

#define DEFINE_FRAME_ARRAY_ACCESSORS(name, type)                              \
  type FrameArray::name(int frame_ix) const {                                 \
    Object obj =                                                              \
        get(kFirstIndex + frame_ix * kElementsPerFrame + k##name##Offset);    \
    return type::cast(obj);                                                   \
  }                                                                           \
                                                                              \
  void FrameArray::Set##name(int frame_ix, type value) {                      \
    set(kFirstIndex + frame_ix * kElementsPerFrame + k##name##Offset, value); \
  }
FRAME_ARRAY_FIELD_LIST(DEFINE_FRAME_ARRAY_ACCESSORS)
#undef DEFINE_FRAME_ARRAY_ACCESSORS

bool FrameArray::IsWasmFrame(int frame_ix) const {
  const int flags = Flags(frame_ix)->value();
  return (flags & kIsWasmFrame) != 0;
}

bool FrameArray::IsWasmInterpretedFrame(int frame_ix) const {
  const int flags = Flags(frame_ix)->value();
  return (flags & kIsWasmInterpretedFrame) != 0;
}

bool FrameArray::IsAsmJsWasmFrame(int frame_ix) const {
  const int flags = Flags(frame_ix)->value();
  return (flags & kIsAsmJsWasmFrame) != 0;
}

int FrameArray::FrameCount() const {
  const int frame_count = Smi::ToInt(get(kFrameCountIndex));
  DCHECK_LE(0, frame_count);
  return frame_count;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FRAME_ARRAY_INL_H_
