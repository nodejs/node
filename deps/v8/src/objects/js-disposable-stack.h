// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DISPOSABLE_STACK_H_
#define V8_OBJECTS_JS_DISPOSABLE_STACK_H_

#include "src/base/bit-field.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/contexts.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-promise.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-disposable-stack-tq.inc"

// Valid states for a DisposableStack.
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack-objects
enum class DisposableStackState { kDisposed, kPending };

// kValueIsReceiver: Call the method with no argument
// kValueIsArgument: Pass the value as the argument to the dispose method,
// `disposablestack.prototype.adopt` is the only method that uses
// kValueIsArgument as DisposeMethodCallType.
enum class DisposeMethodCallType { kValueIsReceiver = 0, kValueIsArgument = 1 };

// Valid hints for a DisposableStack.
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposableresource-records
enum class DisposeMethodHint { kSyncDispose = 0, kAsyncDispose = 1 };

// Types of disposable resources in a DisposableStack.
enum class DisposableStackResourcesType { kAllSync, kAtLeastOneAsync };

using DisposeCallTypeBit =
    base::BitField<DisposeMethodCallType, 0, 1, uint32_t>;
using DisposeHintBit = DisposeCallTypeBit::Next<DisposeMethodHint, 1>;

V8_OBJECT class JSDisposableStackBase : public JSObject {
 public:
  inline Tagged<FixedArray> stack() const;
  inline void set_stack(Tagged<FixedArray> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int status() const;
  inline void set_status(int value);

  inline Tagged<UnionOf<Object, Hole>> error() const;
  inline void set_error(Tagged<UnionOf<Object, Hole>> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Object, Hole>> error_message() const;
  inline void set_error_message(Tagged<UnionOf<Object, Hole>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSDisposableStackBase)
  DECL_VERIFIER(JSDisposableStackBase)

  // Bit layout for status_.
  using StateBit = base::BitField<DisposableStackState, 0, 1>;
  using NeedsAwaitBit = base::BitField<bool, 1, 1>;
  using HasAwaitedBit = base::BitField<bool, 2, 1>;
  using SuppressedErrorCreatedBit = base::BitField<bool, 3, 1>;
  using LengthBits = base::BitField<int32_t, 4, 27>;

  inline DisposableStackState state() const;
  inline void set_state(DisposableStackState value);
  DECL_BOOLEAN_ACCESSORS(needs_await)
  DECL_BOOLEAN_ACCESSORS(has_awaited)
  DECL_BOOLEAN_ACCESSORS(suppressed_error_created)
  DECL_INT_ACCESSORS(length)

  enum class AsyncDisposableStackContextSlots {
    kStack = Context::MIN_CONTEXT_SLOTS,
    kOuterPromise,
    kLength,
  };

  enum class AsyncDisposeFromSyncDisposeContextSlots {
    kMethod = Context::MIN_CONTEXT_SLOTS,
    kLength,
  };

  static void InitializeJSDisposableStackBase(
      Isolate* isolate, DirectHandle<JSDisposableStackBase> stack);
  static void Add(Isolate* isolate,
                  DirectHandle<JSDisposableStackBase> disposable_stack,
                  DirectHandle<Object> value, DirectHandle<Object> method,
                  DisposeMethodCallType type, DisposeMethodHint hint);
  static MaybeDirectHandle<Object> CheckValueAndGetDisposeMethod(
      Isolate* isolate, DirectHandle<JSAny> value, DisposeMethodHint hint);
  static MaybeDirectHandle<Object> DisposeResources(
      Isolate* isolate, DirectHandle<JSDisposableStackBase> disposable_stack,
      DisposableStackResourcesType resources_type);
  static MaybeDirectHandle<JSReceiver> ResolveAPromiseWithValueAndReturnIt(
      Isolate* isolate, DirectHandle<Object> value);
  static void HandleErrorInDisposal(
      Isolate* isolate, DirectHandle<JSDisposableStackBase> disposable_stack,
      DirectHandle<Object> current_error,
      DirectHandle<Object> current_error_message);

 public:
  TaggedMember<FixedArray> stack_;
  // SmiTagged<DisposableStackStatus>.
  TaggedMember<Smi> status_;
  TaggedMember<UnionOf<Object, Hole>> error_;
  TaggedMember<UnionOf<Object, Hole>> error_message_;
} V8_OBJECT_END;

V8_OBJECT class JSSyncDisposableStack final : public JSDisposableStackBase {
 public:
  DECL_PRINTER(JSSyncDisposableStack)
  DECL_VERIFIER(JSSyncDisposableStack)
} V8_OBJECT_END;

V8_OBJECT class JSAsyncDisposableStack final : public JSDisposableStackBase {
 public:
  DECL_PRINTER(JSAsyncDisposableStack)
  DECL_VERIFIER(JSAsyncDisposableStack)

  static Maybe<bool> NextDisposeAsyncIteration(
      Isolate* isolate,
      DirectHandle<JSDisposableStackBase> async_disposable_stack,
      DirectHandle<JSPromise> outer_promise);
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DISPOSABLE_STACK_H_
