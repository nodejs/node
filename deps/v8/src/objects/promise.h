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
class StructBodyDescriptor;

#include "torque-generated/src/objects/promise-tq.inc"

// Struct to hold state required for PromiseReactionJob. See the comment on the
// PromiseReaction below for details on how this is being managed to reduce the
// memory and allocation overhead. This is the base class for the concrete
//
//   - PromiseFulfillReactionJobTask
//   - PromiseRejectReactionJobTask
//
// classes, which are used to represent either reactions, and we distinguish
// them by their instance types.
class PromiseReactionJobTask
    : public TorqueGeneratedPromiseReactionJobTask<PromiseReactionJobTask,
                                                   Microtask> {
 public:
  static const int kSizeOfAllPromiseReactionJobTasks = kHeaderSize;

  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(PromiseReactionJobTask)
};

// Struct to hold state required for a PromiseReactionJob of type "Fulfill".
class PromiseFulfillReactionJobTask
    : public TorqueGeneratedPromiseFulfillReactionJobTask<
          PromiseFulfillReactionJobTask, PromiseReactionJobTask> {
 public:
  static_assert(kSize == kSizeOfAllPromiseReactionJobTasks);

  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(PromiseFulfillReactionJobTask)
};

// Struct to hold state required for a PromiseReactionJob of type "Reject".
class PromiseRejectReactionJobTask
    : public TorqueGeneratedPromiseRejectReactionJobTask<
          PromiseRejectReactionJobTask, PromiseReactionJobTask> {
 public:
  static_assert(kSize == kSizeOfAllPromiseReactionJobTasks);

  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(PromiseRejectReactionJobTask)
};

// A container struct to hold state required for PromiseResolveThenableJob.
class PromiseResolveThenableJobTask
    : public TorqueGeneratedPromiseResolveThenableJobTask<
          PromiseResolveThenableJobTask, Microtask> {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(PromiseResolveThenableJobTask)
};

// Struct to hold the state of a PromiseCapability.
class PromiseCapability
    : public TorqueGeneratedPromiseCapability<PromiseCapability, Struct> {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(PromiseCapability)
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
class PromiseReaction
    : public TorqueGeneratedPromiseReaction<PromiseReaction, Struct> {
 public:
  enum Type { kFulfill, kReject };

  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(PromiseReaction)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_H_
