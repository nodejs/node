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

class JSPromise;

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
  // [promise_or_capability]: Either a JSPromise (in case of native promises),
  // a PromiseCapability (general case), or undefined (in case of await).
  DECL_ACCESSORS(promise_or_capability, HeapObject)

  DEFINE_FIELD_OFFSET_CONSTANTS(
      Microtask::kHeaderSize, TORQUE_GENERATED_PROMISE_REACTION_JOB_TASK_FIELDS)

  // Dispatched behavior.
  DECL_CAST(PromiseReactionJobTask)
  DECL_VERIFIER(PromiseReactionJobTask)
  static const int kSizeOfAllPromiseReactionJobTasks = kHeaderSize;
  OBJECT_CONSTRUCTORS(PromiseReactionJobTask, Microtask);
};

// Struct to hold state required for a PromiseReactionJob of type "Fulfill".
class PromiseFulfillReactionJobTask : public PromiseReactionJobTask {
 public:
  // Dispatched behavior.
  DECL_CAST(PromiseFulfillReactionJobTask)
  DECL_PRINTER(PromiseFulfillReactionJobTask)
  DECL_VERIFIER(PromiseFulfillReactionJobTask)

  DEFINE_FIELD_OFFSET_CONSTANTS(
      PromiseReactionJobTask::kHeaderSize,
      TORQUE_GENERATED_PROMISE_FULFILL_REACTION_JOB_TASK_FIELDS)
  STATIC_ASSERT(kSize == kSizeOfAllPromiseReactionJobTasks);

  OBJECT_CONSTRUCTORS(PromiseFulfillReactionJobTask, PromiseReactionJobTask);
};

// Struct to hold state required for a PromiseReactionJob of type "Reject".
class PromiseRejectReactionJobTask : public PromiseReactionJobTask {
 public:
  // Dispatched behavior.
  DECL_CAST(PromiseRejectReactionJobTask)
  DECL_PRINTER(PromiseRejectReactionJobTask)
  DECL_VERIFIER(PromiseRejectReactionJobTask)

  DEFINE_FIELD_OFFSET_CONSTANTS(
      PromiseReactionJobTask::kHeaderSize,
      TORQUE_GENERATED_PROMISE_REJECT_REACTION_JOB_TASK_FIELDS)
  STATIC_ASSERT(kSize == kSizeOfAllPromiseReactionJobTasks);

  OBJECT_CONSTRUCTORS(PromiseRejectReactionJobTask, PromiseReactionJobTask);
};

// A container struct to hold state required for PromiseResolveThenableJob.
class PromiseResolveThenableJobTask : public Microtask {
 public:
  DECL_ACCESSORS(context, Context)
  DECL_ACCESSORS(promise_to_resolve, JSPromise)
  DECL_ACCESSORS(then, JSReceiver)
  DECL_ACCESSORS(thenable, JSReceiver)

  DEFINE_FIELD_OFFSET_CONSTANTS(
      Microtask::kHeaderSize,
      TORQUE_GENERATED_PROMISE_RESOLVE_THENABLE_JOB_TASK_FIELDS)

  // Dispatched behavior.
  DECL_CAST(PromiseResolveThenableJobTask)
  DECL_PRINTER(PromiseResolveThenableJobTask)
  DECL_VERIFIER(PromiseResolveThenableJobTask)

  OBJECT_CONSTRUCTORS(PromiseResolveThenableJobTask, Microtask);
};

// Struct to hold the state of a PromiseCapability.
class PromiseCapability : public Struct {
 public:
  DECL_ACCESSORS(promise, HeapObject)
  DECL_ACCESSORS(resolve, Object)
  DECL_ACCESSORS(reject, Object)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_PROMISE_CAPABILITY_FIELDS)

  // Dispatched behavior.
  DECL_CAST(PromiseCapability)
  DECL_PRINTER(PromiseCapability)
  DECL_VERIFIER(PromiseCapability)

  OBJECT_CONSTRUCTORS(PromiseCapability, Struct);
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
// case of a Promise subclass. In case of await it can also be undefined if
// PromiseHooks are disabled (see https://github.com/tc39/ecma262/pull/1146).
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
  // [promise_or_capability]: Either a JSPromise (in case of native promises),
  // a PromiseCapability (general case), or undefined (in case of await).
  DECL_ACCESSORS(promise_or_capability, HeapObject)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_PROMISE_REACTION_FIELDS)

  // Dispatched behavior.
  DECL_CAST(PromiseReaction)
  DECL_PRINTER(PromiseReaction)
  DECL_VERIFIER(PromiseReaction)

  OBJECT_CONSTRUCTORS(PromiseReaction, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_H_
