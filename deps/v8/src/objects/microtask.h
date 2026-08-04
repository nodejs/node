// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MICROTASK_H_
#define V8_OBJECTS_MICROTASK_H_

#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StructBodyDescriptor;
class MicrotaskQueueBuiltinsAssembler;

#include "torque-generated/src/objects/microtask-tq.inc"

// Abstract base class for all microtasks that can be scheduled on the
// microtask queue. This class merely serves the purpose of a marker
// interface.
V8_OBJECT class Microtask : public StructLayout {
 public:
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  inline Tagged<Object> continuation_preserved_embedder_data() const;
  inline void set_continuation_preserved_embedder_data(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
#endif

  DECL_PRINTER(Microtask)
  DECL_VERIFIER(Microtask)

 private:
  friend class TorqueGeneratedMicrotaskAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;
  friend class JSPromise;
  friend struct ObjectTraits<Microtask>;

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  TaggedMember<Object> continuation_preserved_embedder_data_;
#endif
} V8_OBJECT_END;

// A CallbackTask is a special Microtask that allows us to schedule
// C++ microtask callbacks on the microtask queue. This is heavily
// used by Blink for example.
V8_OBJECT class CallbackTask : public Microtask {
 public:
  inline Tagged<Foreign> callback() const;
  inline void set_callback(Tagged<Foreign> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Foreign> data() const;
  inline void set_data(Tagged<Foreign> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(CallbackTask)
  DECL_PRINTER(CallbackTask)

 private:
  friend class TorqueGeneratedCallbackTaskAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;

  TaggedMember<Foreign> callback_;
  TaggedMember<Foreign> data_;
} V8_OBJECT_END;

// A CallableTask is a special (internal) Microtask that allows us to
// schedule arbitrary callables on the microtask queue. We use this
// for various tests of the microtask queue.
V8_OBJECT class CallableTask : public Microtask {
 public:
  inline Tagged<JSReceiver> callable() const;
  inline void set_callable(Tagged<JSReceiver> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Context> context() const;
  inline void set_context(Tagged<Context> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Dispatched behavior.
  DECL_VERIFIER(CallableTask)
  DECL_PRINTER(CallableTask)
  void BriefPrintDetails(std::ostream& os);

 private:
  friend class TorqueGeneratedCallableTaskAsserts;
  friend class MicrotaskQueueBuiltinsAssembler;

  TaggedMember<JSReceiver> callable_;
  TaggedMember<Context> context_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<Microtask> {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  static constexpr int kContinuationPreservedEmbedderDataOffset =
      offsetof(Microtask, continuation_preserved_embedder_data_);
#endif
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_H_
