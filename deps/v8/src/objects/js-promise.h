// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PROMISE_H_
#define V8_OBJECTS_JS_PROMISE_H_

#include "src/objects/js-objects.h"
#include "src/objects/promise.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Representation of promise objects in the specification. Our layout of
// JSPromise differs a bit from the layout in the specification, for example
// there's only a single list of PromiseReaction objects, instead of separate
// lists for fulfill and reject reactions. The PromiseReaction carries both
// callbacks from the start, and is eventually morphed into the proper kind of
// PromiseReactionJobTask when the JSPromise is settled.
//
// We also overlay the result and reactions fields on the JSPromise, since
// the reactions are only necessary for pending promises, whereas the result
// is only meaningful for settled promises.
class JSPromise : public TorqueGeneratedJSPromise<JSPromise, JSObject> {
 public:
  // [result]: Checks that the promise is settled and returns the result.
  inline Object result() const;

  // [reactions]: Checks that the promise is pending and returns the reactions.
  inline Object reactions() const;

  // [has_handler]: Whether this promise has a reject handler or not.
  DECL_BOOLEAN_ACCESSORS(has_handler)

  // [handled_hint]: Whether this promise will be handled by a catch
  // block in an async function.
  DECL_BOOLEAN_ACCESSORS(handled_hint)

  int async_task_id() const;
  void set_async_task_id(int id);

  static const char* Status(Promise::PromiseState status);
  V8_EXPORT_PRIVATE Promise::PromiseState status() const;
  void set_status(Promise::PromiseState status);

  // ES section #sec-fulfillpromise
  V8_EXPORT_PRIVATE static Handle<Object> Fulfill(Handle<JSPromise> promise,
                                                  Handle<Object> value);
  // ES section #sec-rejectpromise
  static Handle<Object> Reject(Handle<JSPromise> promise, Handle<Object> reason,
                               bool debug_event = true);
  // ES section #sec-promise-resolve-functions
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Resolve(
      Handle<JSPromise> promise, Handle<Object> resolution);

  // Dispatched behavior.
  DECL_PRINTER(JSPromise)
  DECL_VERIFIER(JSPromise)

  static const int kSizeWithEmbedderFields =
      kHeaderSize + v8::Promise::kEmbedderFieldCount * kEmbedderDataSlotSize;

  // Flags layout.
  DEFINE_TORQUE_GENERATED_JS_PROMISE_FLAGS()

  STATIC_ASSERT(v8::Promise::kPending == 0);
  STATIC_ASSERT(v8::Promise::kFulfilled == 1);
  STATIC_ASSERT(v8::Promise::kRejected == 2);

 private:
  // ES section #sec-triggerpromisereactions
  static Handle<Object> TriggerPromiseReactions(Isolate* isolate,
                                                Handle<Object> reactions,
                                                Handle<Object> argument,
                                                PromiseReaction::Type type);

  TQ_OBJECT_CONSTRUCTORS(JSPromise)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PROMISE_H_
