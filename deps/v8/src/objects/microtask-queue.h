// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MICROTASK_QUEUE_H_
#define V8_OBJECTS_MICROTASK_QUEUE_H_

#include "src/objects.h"
#include "src/objects/microtask.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE MicrotaskQueue : public Struct {
 public:
  DECL_CAST(MicrotaskQueue)
  DECL_VERIFIER(MicrotaskQueue)
  DECL_PRINTER(MicrotaskQueue)

  // A FixedArray that the queued microtasks are stored.
  // The first |pending_microtask_count| slots contains Microtask instance
  // for each, and followings are undefined_value if any.
  DECL_ACCESSORS(queue, FixedArray)

  // The number of microtasks queued in |queue|. This must be less or equal to
  // the length of |queue|.
  DECL_INT_ACCESSORS(pending_microtask_count)

  // Enqueues |microtask| to |microtask_queue|.
  static void EnqueueMicrotask(Isolate* isolate,
                               Handle<MicrotaskQueue> microtask_queue,
                               Handle<Microtask> microtask);

  // Runs all enqueued microtasks.
  static void RunMicrotasks(Handle<MicrotaskQueue> microtask_queue);

  static constexpr int kMinimumQueueCapacity = 8;

  static const int kQueueOffset = HeapObject::kHeaderSize;
  static const int kPendingMicrotaskCountOffset = kQueueOffset + kPointerSize;
  static const int kSize = kPendingMicrotaskCountOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MicrotaskQueue);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_QUEUE_H_
