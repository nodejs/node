// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_ARGUMENTS_H_
#define V8_API_API_ARGUMENTS_H_

#include "include/v8-template.h"
#include "src/base/small-vector.h"
#include "src/builtins/builtins-utils.h"
#include "src/execution/isolate.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

// This class also serves as a side effects detection scope (JavaScript code
// execution). It is used for ensuring correctness of the interceptor callback
// implementations. The idea is that the interceptor callback that does not
// intercept an operation must not produce side effects. If the callback
// signals that it has handled the operation (by either returning a respective
// result or by throwing an exception) then the AcceptSideEffects() method
// must be called to "accept" the side effects that have happened during the
// lifetime of the PropertyCallbackArguments object.
class PropertyCallbackArguments final : public Relocatable {
 public:
  using T = PropertyCallbackInfo<Value>;
  using Super = CustomArguments<T>;
  static constexpr int kMandatoryArgsLength = T::kMandatoryArgsLength;
  static constexpr int kFullArgsLength = T::kFullArgsLength;
  static constexpr int kMandatoryApiArgsLength = T::kMandatoryApiArgsLength;
  static constexpr int kFullApiArgsLength = T::kFullApiArgsLength;

  static constexpr int kGetterApiArgsLength = T::kMandatoryApiArgsLength;
  static constexpr int kSetterApiArgsLength = T::kFullApiArgsLength;

  static constexpr int kFrameTypeIndex = T::kFrameTypeIndex;
  static constexpr int kThisIndex = T::kThisIndex;
  static constexpr int kUnusedIndex = T::kUnusedIndex;
  static constexpr int kCallbackInfoIndex = T::kCallbackInfoIndex;
  static constexpr int kHolderIndex = T::kHolderIndex;
  static constexpr int kIsolateIndex = T::kIsolateIndex;
  static constexpr int kReturnValueIndex = T::kReturnValueIndex;
  static constexpr int kShouldThrowOnErrorIndex = T::kShouldThrowOnErrorIndex;
  static constexpr int kPropertyKeyIndex = T::kPropertyKeyIndex;
  static constexpr int kValueIndex = T::kValueIndex;

  // Helper for converting Api arguments indices to [0..kFullApiArgsLength)
  // value.
  static constexpr uint32_t ApiArgIndex(uint32_t index) {
    DCHECK_GE(index, T::kFirstApiArgumentIndex);
    return index - T::kFirstApiArgumentIndex;
  }

  // This constructor leaves kPropertyKeyIndex, kReturnValueIndex and
  // kCallbackInfoIndex slots uninitialized in order to let them be
  // initialized by the subsequent CallXXX(..) and avoid double initialization.
  // As a consequence, there must be no GC call between this constructor and
  // CallXXX(..). In debug mode these slots are zapped, so GC should be able
  // to detect misuse of this object.
  inline PropertyCallbackArguments(Isolate* isolate, Tagged<Object> receiver,
                                   Tagged<JSObject> holder);
  inline PropertyCallbackArguments(Isolate* isolate, Tagged<Object> receiver,
                                   Tagged<JSObject> holder,
                                   Maybe<ShouldThrow> should_throw);
  inline ~PropertyCallbackArguments();

  // Don't copy PropertyCallbackArguments, because they would both have the
  // same prev_ pointer.
  PropertyCallbackArguments(const PropertyCallbackArguments&) = delete;
  PropertyCallbackArguments& operator=(const PropertyCallbackArguments&) =
      delete;

  // -------------------------------------------------------------------------
  // Accessor Callbacks

  // Returns the result of [[Get]] operation or throws an exception.
  // In case of exception empty handle is returned.
  // TODO(ishell, 328490288): stop returning empty handles.
  inline DirectHandle<JSAny> CallAccessorGetter(Isolate* isolate,
                                                DirectHandle<AccessorInfo> info,
                                                DirectHandle<Name> name);
  // Returns the result of [[Set]] operation or throws an exception.
  V8_WARN_UNUSED_RESULT
  inline bool CallAccessorSetter(Isolate* isolate,
                                 DirectHandle<AccessorInfo> info,
                                 DirectHandle<Name> name,
                                 DirectHandle<Object> value);

  // -------------------------------------------------------------------------
  // Named Interceptor Callbacks

  // Empty handle means that the request was not intercepted.
  // Pending exception handling should be done by the caller.
  inline DirectHandle<Object> CallNamedQuery(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      DirectHandle<Name> name);
  inline DirectHandle<JSAny> CallNamedGetter(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      DirectHandle<Name> name);

  // Calls Setter/Definer/Deleter callback and returns whether the request
  // was intercepted.
  // Pending exception handling and interpretation of the result should be
  // done by the caller using GetBooleanReturnValue(..).
  inline v8::Intercepted CallNamedSetter(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      DirectHandle<Name> name, DirectHandle<Object> value);
  inline v8::Intercepted CallNamedDefiner(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      DirectHandle<Name> name, const v8::PropertyDescriptor& desc);
  inline v8::Intercepted CallNamedDeleter(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      DirectHandle<Name> name);

  // Empty handle means that the request was not intercepted.
  // Pending exception handling should be done by the caller.
  inline Handle<JSAny> CallNamedDescriptor(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      DirectHandle<Name> name);
  // Returns JSArray-like object with property names or undefined.
  inline DirectHandle<JSObjectOrUndefined> CallNamedEnumerator(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor);

  // -------------------------------------------------------------------------
  // Indexed Interceptor Callbacks

  // Empty handle means that the request was not intercepted.
  // Pending exception handling should be done by the caller.
  inline DirectHandle<Object> CallIndexedQuery(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      uint32_t index);
  inline DirectHandle<JSAny> CallIndexedGetter(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      uint32_t index);

  // Calls Setter/Definer/Deleter callback and returns whether the request
  // was intercepted.
  // Pending exception handling and interpretation of the result should be
  // done by the caller using GetBooleanReturnValue(..).
  inline v8::Intercepted CallIndexedSetter(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      uint32_t index, DirectHandle<Object> value);
  inline v8::Intercepted CallIndexedDefiner(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      uint32_t index, const v8::PropertyDescriptor& desc);
  inline v8::Intercepted CallIndexedDeleter(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      uint32_t index);

  // Empty handle means that the request was not intercepted.
  // Pending exception handling should be done by the caller.
  inline Handle<JSAny> CallIndexedDescriptor(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
      uint32_t index);
  // Returns JSArray-like object with property names or undefined.
  inline DirectHandle<JSObjectOrUndefined> CallIndexedEnumerator(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor);

  // Accept potential JavaScript side effects that might occur during life
  // time of this object.
  inline void AcceptSideEffects() {
#ifdef DEBUG
    javascript_execution_counter_ = 0;
#endif  // DEBUG
  }

  // Converts the result of Setter/Definer/Deleter interceptor callback to
  // Maybe<InterceptorResult>.
  // Currently, in certain scenarios the actual boolean result returned by
  // the Setter/Definer operation is ignored and thus we don't need to process
  // the actual return value.
  inline Maybe<InterceptorResult> GetBooleanReturnValue(
      Isolate* isolate, v8::Intercepted intercepted,
      const char* callback_kind_for_error_message,
      bool ignore_return_value = false);

  // TODO(ishell): cleanup this hack by embedding the PropertyCallbackInfo
  // into PropertyCallbackArguments object.
  template <typename T>
  const v8::PropertyCallbackInfo<T>& GetPropertyCallbackInfo() {
    return *(reinterpret_cast<PropertyCallbackInfo<T>*>(&values_[0]));
  }

  // Forwards ShouldThrowOnError() request to the underlying
  // v8::PropertyCallbackInfo<> object.
  bool ShouldThrowOnError() {
    return GetPropertyCallbackInfo<Value>().ShouldThrowOnError();
  }

  // Returns AccessorInfo stored in v8::PropertyCallbackInfo<T>.
  template <typename T>
  static DirectHandle<AccessorInfo> GetAccessorInfo(
      const PropertyCallbackInfo<T>& info) {
    return Cast<AccessorInfo>(
        DirectHandle<Object>::FromSlot(&info.args_[kCallbackInfoIndex]));
  }

  // Returns whether given v8::PropertyCallbackInfo<T> object is named/indexed.
  template <typename T>
  static bool IsNamed(const PropertyCallbackInfo<T>& info) {
    return info.IsNamed();
  }

  // Returns property name stored in v8::PropertyCallbackInfo<T> (for named
  // accessors/interceptors).
  template <typename T>
  static DirectHandle<Name> GetPropertyName(
      const PropertyCallbackInfo<T>& info) {
    DCHECK(info.IsNamed());
    return Cast<Name>(
        DirectHandle<Object>::FromSlot(&info.args_[kPropertyKeyIndex]));
  }

  // Returns property index stored in v8::PropertyCallbackInfo<T> (for indexed
  // interceptors).
  template <typename T>
  static uint32_t GetPropertyIndex(const PropertyCallbackInfo<T>& info) {
    DCHECK(!info.IsNamed());
    return static_cast<uint32_t>(info.args_[kPropertyKeyIndex]);
  }

  // Returns true if it's an arguments object for named callback, otherwise
  // it's one for an indexed callback.
  inline bool is_named() const;

  // Set property key and a respective frame type (named vs. indexed).
  inline void set_property_key(Tagged<Name> name);
  inline void set_property_key(uint32_t index);

  inline DirectHandle<JSObject> holder() const;

 private:
  inline void Initialize(Isolate* isolate, Tagged<Object> self,
                         Tagged<JSObject> holder);
  // Returns JSArray-like object with property names or undefined.
  inline DirectHandle<JSObjectOrUndefined> CallPropertyEnumerator(
      Isolate* isolate, DirectHandle<InterceptorInfo> interceptor);

  inline DirectHandle<Object> receiver() const;

  void IterateInstance(RootVisitor* v) override;

  template <typename V>
  Handle<V> GetReturnValue() const;

  inline FullObjectSlot slot_at(int index) const {
    // This allows index == kFullArgsLength so "one past the end" slots
    // can be retrieved for iterating purposes.
    DCHECK_LE(static_cast<unsigned>(index),
              static_cast<unsigned>(kFullArgsLength));
    return FullObjectSlot(values_ + index);
  }

#ifdef DEBUG
  // Used for checking that the way this object was constructed matches the
  // following CallXxx(..).
  const bool is_setter_definer_deleter_;

  // This stores current value of Isolate::javascript_execution_counter().
  // It's used for detecting whether JavaScript code was executed between
  // PropertyCallbackArguments's constructor and destructor.
  uint32_t javascript_execution_counter_ = 0;
#endif  // DEBUG

  Address values_[kFullArgsLength];
};

class FunctionCallbackArguments : public Relocatable {
 public:
  using T = FunctionCallbackInfo<Value>;
  using Super = CustomArguments<T>;
  static constexpr int kArgsLength = T::kArgsLength;

  // Frame arguments block, the values are located on stack in the frame.
  static constexpr int kArgcIndex = T::kArgcIndex;
  static constexpr int kNewTargetIndex = T::kNewTargetIndex;
  static constexpr int kFrameSPIndex = T::kFrameSPIndex;
  static constexpr int kFrameTypeIndex = T::kFrameTypeIndex;

  // Api arguments block, the values are located on stack right above PC.
  static constexpr int kIsolateIndex = T::kIsolateIndex;
  static constexpr int kReturnValueIndex = T::kReturnValueIndex;
  static constexpr int kContextIndex = T::kContextIndex;
  static constexpr int kTargetIndex = T::kTargetIndex;
  static constexpr int kApiArgsLength = T::kApiArgsLength;

  // JS arguments block, follows Api arguments.
  static constexpr int kReceiverIndex = T::kReceiverIndex;
  static constexpr int kFirstJSArgumentIndex = T::kFirstJSArgumentIndex;

  // Helper for converting Api arguments indices to [0..kApiArgsLength) value.
  static constexpr uint32_t ApiArgIndex(uint32_t index) {
    DCHECK_GE(index, T::kFirstApiArgumentIndex);
    return index - T::kFirstApiArgumentIndex;
  }

  // Arguments for [[Call]] operation.
  template <typename ArgT>
  inline FunctionCallbackArguments(Isolate* isolate,
                                   Tagged<FunctionTemplateInfo> target,
                                   Tagged<Object> receiver,
                                   const base::Vector<const ArgT> args);
  // Arguments for [[Construct]] operation.
  template <typename ArgT>
  inline FunctionCallbackArguments(Isolate* isolate,
                                   Tagged<FunctionTemplateInfo> target,
                                   Tagged<HeapObject> new_target,
                                   Tagged<Object> receiver,
                                   const base::Vector<const ArgT> args);
  inline ~FunctionCallbackArguments();

  // Performs [[Call]] of [[Construct]] operation for a given function
  // and new_target.
  // Exception is supposed to be checked by the caller.
  // It explicitly returns raw value in order to enforce the caller to create
  // a handle if necessary.
  inline Tagged<JSAny> CallOrConstruct(Isolate* isolate,
                                       Tagged<FunctionTemplateInfo> function,
                                       bool is_construct);

  // Unofficial way of getting target FunctionTemplateInfo from
  // v8::FunctionCallbackInfo<T>.
  template <typename T>
  static Tagged<Object> GetTarget(const FunctionCallbackInfo<T>& info) {
    return Tagged<Object>(info.values_[kTargetIndex]);
  }

 private:
  template <bool is_construct, typename ArgT>
    requires(std::is_same_v<ArgT, DirectHandle<Object>> ||
             std::is_same_v<ArgT, Address>)
  inline void Initialize(Isolate* isolate, Tagged<FunctionTemplateInfo> target,
                         Tagged<Object> new_target, Tagged<Object> receiver,
                         const base::Vector<const ArgT> args);

  inline FullObjectSlot slot_at(uint32_t index) const {
    // Shift index to accommodate for unconditionally allocated "optional" part.
    index += T::kOptionalArgsLength;
    // This allows index == values_.size() so "one past the end" slots
    // can be retrieved for iterating purposes.
    DCHECK_LE(index, values_.size());
    // Don't use operator[] because it doesn't allow one past end index.
    return FullObjectSlot(&values_.data()[index]);
  }

  void IterateInstance(RootVisitor* v) override;

  // This default size is enough for passing up to 4 JS arguments.
  base::SmallVector<Address, 16> values_;
};

static_assert(BuiltinArguments::kNumExtraArgs ==
              BuiltinExitFrameConstants::kNumExtraArgs);
static_assert(BuiltinArguments::kNumExtraArgsWithReceiver ==
              BuiltinExitFrameConstants::kNumExtraArgsWithReceiver);

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_ARGUMENTS_H_
