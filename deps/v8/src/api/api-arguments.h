// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_ARGUMENTS_H_
#define V8_API_API_ARGUMENTS_H_

#include "src/api/api.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

// Custom arguments replicate a small segment of stack that can be
// accessed through an Arguments object the same way the actual stack
// can.
class CustomArgumentsBase : public Relocatable {
 protected:
  explicit inline CustomArgumentsBase(Isolate* isolate);
};

template <typename T>
class CustomArguments : public CustomArgumentsBase {
 public:
  static const int kReturnValueOffset = T::kReturnValueIndex;

  ~CustomArguments() override;

  inline void IterateInstance(RootVisitor* v) override {
    v->VisitRootPointers(Root::kRelocatable, nullptr, slot_at(0),
                         slot_at(T::kArgsLength));
  }

 protected:
  explicit inline CustomArguments(Isolate* isolate)
      : CustomArgumentsBase(isolate) {}

  template <typename V>
  Handle<V> GetReturnValue(Isolate* isolate);

  inline Isolate* isolate() {
    return reinterpret_cast<Isolate*>((*slot_at(T::kIsolateIndex)).ptr());
  }

  inline FullObjectSlot slot_at(int index) {
    // This allows index == T::kArgsLength so "one past the end" slots
    // can be retrieved for iterating purposes.
    DCHECK_LE(static_cast<unsigned>(index),
              static_cast<unsigned>(T::kArgsLength));
    return FullObjectSlot(values_ + index);
  }
  Address values_[T::kArgsLength];
};

// Note: Calling args.Call() sets the return value on args. For multiple
// Call()'s, a new args should be used every time.
class PropertyCallbackArguments
    : public CustomArguments<PropertyCallbackInfo<Value> > {
 public:
  using T = PropertyCallbackInfo<Value>;
  using Super = CustomArguments<T>;
  static const int kArgsLength = T::kArgsLength;
  static const int kThisIndex = T::kThisIndex;
  static const int kHolderIndex = T::kHolderIndex;
  static const int kDataIndex = T::kDataIndex;
  static const int kReturnValueDefaultValueIndex =
      T::kReturnValueDefaultValueIndex;
  static const int kIsolateIndex = T::kIsolateIndex;
  static const int kShouldThrowOnErrorIndex = T::kShouldThrowOnErrorIndex;

  PropertyCallbackArguments(Isolate* isolate, Object data, Object self,
                            JSObject holder, Maybe<ShouldThrow> should_throw);

  // Don't copy PropertyCallbackArguments, because they would both have the
  // same prev_ pointer.
  PropertyCallbackArguments(const PropertyCallbackArguments&) = delete;
  PropertyCallbackArguments& operator=(const PropertyCallbackArguments&) =
      delete;

  // -------------------------------------------------------------------------
  // Accessor Callbacks
  // Also used for AccessorSetterCallback.
  inline Handle<Object> CallAccessorSetter(Handle<AccessorInfo> info,
                                           Handle<Name> name,
                                           Handle<Object> value);
  // Also used for AccessorGetterCallback, AccessorNameGetterCallback.
  inline Handle<Object> CallAccessorGetter(Handle<AccessorInfo> info,
                                           Handle<Name> name);

  // -------------------------------------------------------------------------
  // Named Interceptor Callbacks
  inline Handle<Object> CallNamedQuery(Handle<InterceptorInfo> interceptor,
                                       Handle<Name> name);
  inline Handle<Object> CallNamedGetter(Handle<InterceptorInfo> interceptor,
                                        Handle<Name> name);
  inline Handle<Object> CallNamedSetter(Handle<InterceptorInfo> interceptor,
                                        Handle<Name> name,
                                        Handle<Object> value);
  inline Handle<Object> CallNamedDefiner(Handle<InterceptorInfo> interceptor,
                                         Handle<Name> name,
                                         const v8::PropertyDescriptor& desc);
  inline Handle<Object> CallNamedDeleter(Handle<InterceptorInfo> interceptor,
                                         Handle<Name> name);
  inline Handle<Object> CallNamedDescriptor(Handle<InterceptorInfo> interceptor,
                                            Handle<Name> name);
  inline Handle<JSObject> CallNamedEnumerator(
      Handle<InterceptorInfo> interceptor);

  // -------------------------------------------------------------------------
  // Indexed Interceptor Callbacks
  inline Handle<Object> CallIndexedQuery(Handle<InterceptorInfo> interceptor,
                                         uint32_t index);
  inline Handle<Object> CallIndexedGetter(Handle<InterceptorInfo> interceptor,
                                          uint32_t index);
  inline Handle<Object> CallIndexedSetter(Handle<InterceptorInfo> interceptor,
                                          uint32_t index, Handle<Object> value);
  inline Handle<Object> CallIndexedDefiner(Handle<InterceptorInfo> interceptor,
                                           uint32_t index,
                                           const v8::PropertyDescriptor& desc);
  inline Handle<Object> CallIndexedDeleter(Handle<InterceptorInfo> interceptor,
                                           uint32_t index);
  inline Handle<Object> CallIndexedDescriptor(
      Handle<InterceptorInfo> interceptor, uint32_t index);
  inline Handle<JSObject> CallIndexedEnumerator(
      Handle<InterceptorInfo> interceptor);

 private:
  /*
   * The following Call functions wrap the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
  inline Handle<JSObject> CallPropertyEnumerator(
      Handle<InterceptorInfo> interceptor);

  inline Handle<Object> BasicCallIndexedGetterCallback(
      IndexedPropertyGetterCallback f, uint32_t index, Handle<Object> info);
  inline Handle<Object> BasicCallNamedGetterCallback(
      GenericNamedPropertyGetterCallback f, Handle<Name> name,
      Handle<Object> info, Handle<Object> receiver = Handle<Object>());

  inline JSObject holder();
  inline Object receiver();
};

class FunctionCallbackArguments
    : public CustomArguments<FunctionCallbackInfo<Value> > {
 public:
  using T = FunctionCallbackInfo<Value>;
  using Super = CustomArguments<T>;
  static const int kArgsLength = T::kArgsLength;
  static const int kHolderIndex = T::kHolderIndex;
  static const int kDataIndex = T::kDataIndex;
  static const int kReturnValueDefaultValueIndex =
      T::kReturnValueDefaultValueIndex;
  static const int kIsolateIndex = T::kIsolateIndex;
  static const int kNewTargetIndex = T::kNewTargetIndex;

  FunctionCallbackArguments(Isolate* isolate, Object data, HeapObject callee,
                            Object holder, HeapObject new_target, Address* argv,
                            int argc);

  /*
   * The following Call function wraps the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
  inline Handle<Object> Call(CallHandlerInfo handler);

 private:
  inline JSReceiver holder();

  internal::Address* argv_;
  int argc_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_ARGUMENTS_H_
