// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_STATE_H_
#define V8_HEAP_MARKING_STATE_H_

#include "src/common/globals.h"
#include "src/heap/marking.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

class BasePage;
class MutablePage;

template <typename ConcreteState, AccessMode access_mode>
class MarkingStateBase {
 public:
  explicit MarkingStateBase(const Isolate* isolate);

  V8_INLINE bool TryMark(Tagged<HeapObject> obj);
  // Helper method for fully marking an object and accounting its live bytes.
  // Should be used to mark individual objects in one-off cases.
  V8_INLINE bool TryMarkAndAccountLiveBytes(Tagged<HeapObject> obj);
  // Same, but does not require the object to be initialized.
  V8_INLINE bool TryMarkAndAccountLiveBytes(Tagged<HeapObject> obj,
                                            int object_size);
  V8_INLINE bool IsMarked(const Tagged<HeapObject> obj) const;
  V8_INLINE bool IsUnmarked(const Tagged<HeapObject> obj) const;

 private:
  const Isolate* isolate_;
};

// This is used by marking visitors.
class MarkingState final
    : public MarkingStateBase<MarkingState, AccessMode::ATOMIC> {
 public:
  explicit MarkingState(const Isolate* isolate) : MarkingStateBase(isolate) {}
};

class NonAtomicMarkingState final
    : public MarkingStateBase<NonAtomicMarkingState, AccessMode::NON_ATOMIC> {
 public:
  explicit NonAtomicMarkingState(const Isolate* isolate)
      : MarkingStateBase(isolate) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_STATE_H_
