// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROMISE_H_
#define V8_OBJECTS_PROMISE_H_

#include "src/objects/microtask.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Struct to hold state required for PromiseReactionJob. See the comment on the
// PromiseReaction below for details on how this is being managed to reduce the
// memory and allocation overhead. This is the base class for the concrete
//
//   - PromiseFulfillReactionJobTask
//   - PromiseRejectReactionJobTask
//
// classes, which are used to represent either reactions, and we distinguish
// them by their instance types.
class PromiseReactionJobTask : public Microtask {
 public:
  DECL_ACCESSORS(argument, Object)
  DECL_ACCESSORS(context, Context)
  DECL_ACCESSORS(handler, HeapObject)
  // [promise_or_capability]: Either a JSPromise or a PromiseCapability.
  DECL_ACCESSORS(promise_or_capability, HeapObject)

  static const int kArgumentOffset = Microtask::kHeaderSize;
  static const int kContextOffset = kArgumentOffset + kPointerSize;
  static const int kHandlerOffset = kContextOffset + kPointerSize;
  static const int kPromiseOrCapabilityOffset = kHandlerOffset + kPointerSize;
  static const int kSize = kPromiseOrCapabilityOffset + kPointerSize;

  // Dispatched behavior.
  DECL_CAST(PromiseReactionJobTask)
  DECL_VERIFIER(PromiseReactionJobTask)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseReactionJobTask);
};

// Struct to hold state required for a PromiseReactionJob of type "Fulfill".
class PromiseFulfillReactionJobTask : public PromiseReactionJobTask {
 public:
  // Dispatched behavior.
  DECL_CAST(PromiseFulfillReactionJobTask)
  DECL_PRINTER(PromiseFulfillReactionJobTask)
  DECL_VERIFIER(PromiseFulfillReactionJobTask)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseFulfillReactionJobTask);
};

// Struct to hold state required for a PromiseReactionJob of type "Reject".
class PromiseRejectReactionJobTask : public PromiseReactionJobTask {
 public:
  // Dispatched behavior.
  DECL_CAST(PromiseRejectReactionJobTask)
  DECL_PRINTER(PromiseRejectReactionJobTask)
  DECL_VERIFIER(PromiseRejectReactionJobTask)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseRejectReactionJobTask);
};

// A container struct to hold state required for PromiseResolveThenableJob.
class PromiseResolveThenableJobTask : public Microtask {
 public:
  DECL_ACCESSORS(context, Context)
  DECL_ACCESSORS(promise_to_resolve, JSPromise)
  DECL_ACCESSORS(then, JSReceiver)
  DECL_ACCESSORS(thenable, JSReceiver)

  static const int kContextOffset = Microtask::kHeaderSize;
  static const int kPromiseToResolveOffset = kContextOffset + kPointerSize;
  static const int kThenOffset = kPromiseToResolveOffset + kPointerSize;
  static const int kThenableOffset = kThenOffset + kPointerSize;
  static const int kSize = kThenableOffset + kPointerSize;

  // Dispatched behavior.
  DECL_CAST(PromiseResolveThenableJobTask)
  DECL_PRINTER(PromiseResolveThenableJobTask)
  DECL_VERIFIER(PromiseResolveThenableJobTask)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseResolveThenableJobTask);
};

// Struct to hold the state of a PromiseCapability.
class PromiseCapability : public Struct {
 public:
  DECL_ACCESSORS(promise, HeapObject)
  DECL_ACCESSORS(resolve, Object)
  DECL_ACCESSORS(reject, Object)

  static const int kPromiseOffset = Struct::kHeaderSize;
  static const int kResolveOffset = kPromiseOffset + kPointerSize;
  static const int kRejectOffset = kResolveOffset + kPointerSize;
  static const int kSize = kRejectOffset + kPointerSize;

  // Dispatched behavior.
  DECL_CAST(PromiseCapability)
  DECL_PRINTER(PromiseCapability)
  DECL_VERIFIER(PromiseCapability)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseCapability);
};

// A representation of promise reaction. This differs from the specification
// in that the PromiseReaction here holds both handlers for the fulfill and
// the reject case. When a JSPromise is eventually resolved (either via
// fulfilling it or rejecting it), we morph this PromiseReaction object in
// memory into a proper PromiseReactionJobTask and schedule it on the queue
// of microtasks. So the size of PromiseReaction and the size of the
// PromiseReactionJobTask has to be same for this to work.
//
// The PromiseReaction::promise_or_capability field can either hold a JSPromise
// instance (in the fast case of a native promise) or a PromiseCapability in
// case of a Promise subclass.
//
// We need to keep the context in the PromiseReaction so that we can run
// the default handlers (in case they are undefined) in the proper context.
//
// The PromiseReaction objects form a singly-linked list, terminated by
// Smi 0. On the JSPromise instance they are linked in reverse order,
// and are turned into the proper order again when scheduling them on
// the microtask queue.
class PromiseReaction : public Struct {
 public:
  enum Type { kFulfill, kReject };

  DECL_ACCESSORS(next, Object)
  DECL_ACCESSORS(reject_handler, HeapObject)
  DECL_ACCESSORS(fulfill_handler, HeapObject)
  DECL_ACCESSORS(promise_or_capability, HeapObject)

  static const int kNextOffset = Struct::kHeaderSize;
  static const int kRejectHandlerOffset = kNextOffset + kPointerSize;
  static const int kFulfillHandlerOffset = kRejectHandlerOffset + kPointerSize;
  static const int kPromiseOrCapabilityOffset =
      kFulfillHandlerOffset + kPointerSize;
  static const int kSize = kPromiseOrCapabilityOffset + kPointerSize;

  // Dispatched behavior.
  DECL_CAST(PromiseReaction)
  DECL_PRINTER(PromiseReaction)
  DECL_VERIFIER(PromiseReaction)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseReaction);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_H_
