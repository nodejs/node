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
class PromiseCapability;
class PromiseReaction;
class MicrotaskQueueBuiltinsAssembler;
class SandboxTesting;

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
V8_OBJECT class PromiseReactionJobTask : public Microtask {
 public:
  inline Tagged<Object> argument() const;
  inline void set_argument(Tagged<Object> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Context> context() const;
  inline void set_context(Tagged<Context> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSCallable, Undefined>> handler() const;
  inline void set_handler(Tagged<UnionOf<JSCallable, Undefined>> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>>
  promise_or_capability() const;
  inline void set_promise_or_capability(
      Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(PromiseReactionJobTask)
  DECL_PRINTER(PromiseReactionJobTask)

 private:
  friend class TorqueGeneratedPromiseReactionJobTaskAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;
  friend class JSPromise;
  friend struct ObjectTraits<PromiseReactionJobTask>;

  TaggedMember<Object> argument_;
  TaggedMember<Context> context_;
  TaggedMember<UnionOf<JSCallable, Undefined>> handler_;
  TaggedMember<UnionOf<JSPromise, PromiseCapability, Undefined>>
      promise_or_capability_;
} V8_OBJECT_END;

// Struct to hold state required for a PromiseReactionJob of type "Fulfill".
V8_OBJECT class PromiseFulfillReactionJobTask : public PromiseReactionJobTask {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(PromiseFulfillReactionJobTask)
  DECL_PRINTER(PromiseFulfillReactionJobTask)
} V8_OBJECT_END;

// Struct to hold state required for a PromiseReactionJob of type "Reject".
V8_OBJECT class PromiseRejectReactionJobTask : public PromiseReactionJobTask {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(PromiseRejectReactionJobTask)
  DECL_PRINTER(PromiseRejectReactionJobTask)
} V8_OBJECT_END;

// A container struct to hold state required for PromiseResolveThenableJob.
V8_OBJECT class PromiseResolveThenableJobTask : public Microtask {
 public:
  inline Tagged<Context> context() const;
  inline void set_context(Tagged<Context> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSPromise> promise_to_resolve() const;
  inline void set_promise_to_resolve(
      Tagged<JSPromise> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSReceiver> thenable() const;
  inline void set_thenable(Tagged<JSReceiver> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSReceiver> then() const;
  inline void set_then(Tagged<JSReceiver> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(PromiseResolveThenableJobTask)
  DECL_PRINTER(PromiseResolveThenableJobTask)

 private:
  friend class TorqueGeneratedPromiseResolveThenableJobTaskAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;

  TaggedMember<Context> context_;
  TaggedMember<JSPromise> promise_to_resolve_;
  TaggedMember<JSReceiver> thenable_;
  TaggedMember<JSReceiver> then_;
} V8_OBJECT_END;

// Struct to hold the state of a PromiseCapability.
V8_OBJECT class PromiseCapability : public StructLayout {
 public:
  inline Tagged<UnionOf<JSReceiver, Undefined>> promise() const;
  inline void set_promise(Tagged<UnionOf<JSReceiver, Undefined>> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> resolve() const;
  inline void set_resolve(Tagged<Object> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<Object> reject() const;
  inline void set_reject(Tagged<Object> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(PromiseCapability)
  DECL_PRINTER(PromiseCapability)

 private:
  friend class TorqueGeneratedPromiseCapabilityAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;

  TaggedMember<UnionOf<JSReceiver, Undefined>> promise_;
  TaggedMember<Object> resolve_;
  TaggedMember<Object> reject_;
} V8_OBJECT_END;

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
V8_OBJECT class PromiseReaction : public StructLayout {
 public:
  enum Type { kFulfill, kReject };

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  inline Tagged<Object> continuation_preserved_embedder_data() const;
  inline void set_continuation_preserved_embedder_data(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
#endif

  inline Tagged<UnionOf<PromiseReaction, Smi>> next() const;
  inline void set_next(Tagged<UnionOf<PromiseReaction, Smi>> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSCallable, Undefined>> reject_handler() const;
  inline void set_reject_handler(Tagged<UnionOf<JSCallable, Undefined>> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSCallable, Undefined>> fulfill_handler() const;
  inline void set_fulfill_handler(Tagged<UnionOf<JSCallable, Undefined>> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>>
  promise_or_capability() const;
  inline void set_promise_or_capability(
      Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(PromiseReaction)
  DECL_PRINTER(PromiseReaction)

 private:
  friend class TorqueGeneratedPromiseReactionAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;
  friend class JSPromise;
  friend class SandboxTesting;
  friend struct ObjectTraits<PromiseReaction>;

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  TaggedMember<Object> continuation_preserved_embedder_data_;
#endif
  TaggedMember<UnionOf<PromiseReaction, Smi>> next_;
  TaggedMember<UnionOf<JSCallable, Undefined>> reject_handler_;
  TaggedMember<UnionOf<JSCallable, Undefined>> fulfill_handler_;
  TaggedMember<UnionOf<JSPromise, PromiseCapability, Undefined>>
      promise_or_capability_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<PromiseReaction> {
  static constexpr int kFulfillHandlerOffset =
      offsetof(PromiseReaction, fulfill_handler_);
  static constexpr int kPromiseOrCapabilityOffset =
      offsetof(PromiseReaction, promise_or_capability_);
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  static constexpr int kContinuationPreservedEmbedderDataOffset =
      offsetof(PromiseReaction, continuation_preserved_embedder_data_);
#endif
};

template <>
struct ObjectTraits<PromiseReactionJobTask> : public ObjectTraits<Microtask> {
  static constexpr int kHandlerOffset =
      offsetof(PromiseReactionJobTask, handler_);
  static constexpr int kPromiseOrCapabilityOffset =
      offsetof(PromiseReactionJobTask, promise_or_capability_);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_H_
