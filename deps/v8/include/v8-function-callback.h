// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_FUNCTION_CALLBACK_H_
#define INCLUDE_V8_FUNCTION_CALLBACK_H_

#include <cstdint>
#include <limits>

#include "v8-internal.h"      // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-primitive.h"     // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

template <typename T>
class BasicTracedReference;
template <typename T>
class Global;
class Object;
class Value;

namespace internal {
class FunctionCallbackArguments;
class PropertyCallbackArguments;
class Builtins;
}  // namespace internal

namespace debug {
class ConsoleCallArguments;
}  // namespace debug

namespace api_internal {
V8_EXPORT v8::Local<v8::Value> GetFunctionTemplateData(
    v8::Isolate* isolate, v8::Local<v8::Data> raw_target);
}  // namespace api_internal

template <typename T>
class ReturnValue {
 public:
  template <class S>
  V8_INLINE ReturnValue(const ReturnValue<S>& that) : value_(that.value_) {
    static_assert(std::is_base_of_v<T, S>, "type check");
  }
  // Handle-based setters.
  template <typename S>
  V8_INLINE void Set(const Global<S>& handle);
  template <typename S>
  V8_INLINE void SetNonEmpty(const Global<S>& handle);
  template <typename S>
  V8_INLINE void Set(const BasicTracedReference<S>& handle);
  template <typename S>
  V8_INLINE void SetNonEmpty(const BasicTracedReference<S>& handle);
  template <typename S>
  V8_INLINE void Set(const Local<S> handle);
  template <typename S>
  V8_INLINE void SetNonEmpty(const Local<S> handle);

  // Fast primitive number setters.
  V8_INLINE void Set(bool value);
  V8_INLINE void Set(double i);
  V8_INLINE void Set(int16_t i);
  V8_INLINE void Set(int32_t i);
  V8_INLINE void Set(int64_t i);
  V8_INLINE void Set(uint16_t i);
  V8_INLINE void Set(uint32_t i);
  V8_INLINE void Set(uint64_t i);

  // Fast JS primitive setters.
  V8_INLINE void SetNull();
  V8_INLINE void SetUndefined();
  V8_INLINE void SetFalse();
  V8_INLINE void SetEmptyString();

  // Convenience getter for the Isolate.
  V8_INLINE Isolate* GetIsolate() const;

  // Pointer setter: Uncompilable to prevent inadvertent misuse.
  template <typename S>
  V8_INLINE void Set(S* whatever);

  // Getter. Creates a new Local<> so it comes with a certain performance
  // hit. If the ReturnValue was not yet set, this will return the undefined
  // value.
  V8_INLINE Local<Value> Get() const;

 private:
  template <class F>
  friend class ReturnValue;
  template <class F>
  friend class FunctionCallbackInfo;
  template <class F>
  friend class PropertyCallbackInfo;
  template <class F, class G, class H>
  friend class PersistentValueMapBase;
  V8_INLINE void SetInternal(internal::Address value);
  // Default value depends on <T>:
  //  - <void> -> true_value,
  //  - <v8::Boolean> -> true_value,
  //  - <v8::Integer> -> 0,
  //  - <v8::Value> -> undefined_value,
  //  - <v8::Array> -> undefined_value.
  V8_INLINE void SetDefaultValue();
  V8_INLINE explicit ReturnValue(internal::Address* slot);

  // See FunctionCallbackInfo.
  static constexpr int kIsolateValueIndex = -1;

  internal::Address* value_;
};

/**
 * The argument information given to function call callbacks.  This
 * class provides access to information about the context of the call,
 * including the receiver, the number and values of arguments, and
 * the holder of the function.
 */
template <typename T>
class FunctionCallbackInfo {
 public:
  /** The number of available arguments. */
  V8_INLINE int Length() const;
  /**
   * Accessor for the available arguments. Returns `undefined` if the index
   * is out of bounds.
   */
  V8_INLINE Local<Value> operator[](int i) const;
  /** Returns the receiver. This corresponds to the "this" value. */
  V8_INLINE Local<Object> This() const;
  /** For construct calls, this returns the "new.target" value. */
  V8_INLINE Local<Value> NewTarget() const;
  /** Indicates whether this is a regular call or a construct call. */
  V8_INLINE bool IsConstructCall() const;
  /** The data argument specified when creating the callback. */
  V8_INLINE Local<Value> Data() const;
  /** The current Isolate. */
  V8_INLINE Isolate* GetIsolate() const;
  /** The ReturnValue for the call. */
  V8_INLINE ReturnValue<T> GetReturnValue() const;

 private:
  friend class internal::FunctionCallbackArguments;
  friend class internal::CustomArguments<FunctionCallbackInfo>;
  friend class debug::ConsoleCallArguments;
  friend void internal::PrintFunctionCallbackInfo(void*);
  using I = internal::Internals;

  // Frame block, matches the layout of ApiCallbackExitFrame.
  // See ApiCallbackExitFrameConstants.
  enum {
    //
    // Optional frame arguments block (exists only for API_CONSTRUCT_EXIT
    // frame).

    // Frame arguments block.
    kNewTargetIndex = -1,

    //
    // Mandatory part, exists for both API_CALLBACK_EXIT and API_CONSTRUCT_EXIT
    // frames.
    //

    // Frame arguments block.
    kArgcIndex,

    // Regular ExitFrame structure.
    kFrameSPIndex,
    kFrameTypeIndex,
    kFrameConstantPoolIndex,  // Optional, see I::kFrameCPSlotCount.
    kFrameFPIndex = kFrameConstantPoolIndex + I::kFrameCPSlotCount,
    kFramePCIndex,

    // Api arguments block, starts at kFirstArgumentIndex.
    kFirstApiArgumentIndex,
    kIsolateIndex = kFirstApiArgumentIndex,
    kReturnValueIndex,
    kContextIndex,
    kTargetIndex,

    // JS args block, starts at kFrameFirstImplicitArgsIndex.
    kReceiverIndex,
    kFirstJSArgumentIndex,

    // Mandatory part includes receiver.
    kArgsLength = kReceiverIndex + 1,
    // Optional part size (exists only for API_CONSTRUCT_EXIT frame).
    kOptionalArgsLength = 1,

    // The length of just Api arguments part.
    kApiArgsLength = kReceiverIndex - kFirstApiArgumentIndex,
  };

  static_assert(kArgcIndex == 0);
  static_assert(ReturnValue<Value>::kIsolateValueIndex ==
                kIsolateIndex - kReturnValueIndex);

  internal::Address* address_of_first_argument() const {
    return &values_[kFirstJSArgumentIndex];
  }

  V8_INLINE FunctionCallbackInfo() = default;

  // FunctionCallbackInfo object provides a view of the stack area where the
  // data is stored and thus it's not supposed to be copyable/movable.
  FunctionCallbackInfo(const FunctionCallbackInfo&) = delete;
  FunctionCallbackInfo& operator=(const FunctionCallbackInfo&) = delete;
  FunctionCallbackInfo(FunctionCallbackInfo&&) = delete;
  FunctionCallbackInfo& operator=(FunctionCallbackInfo&&) = delete;

  // Declare as mutable to let GC modify the contents of the slots even though
  // it's not possible to change values via this class.
  // Define the array size as 1 to make it clear that we are going to access
  // it out-of-bounds from both sides anyway.
  mutable internal::Address values_[1];
};

/**
 * The information passed to a property callback about the context
 * of the property access.
 */
template <typename T>
class PropertyCallbackInfo {
 public:
  /**
   * \return The isolate of the property access.
   */
  V8_INLINE Isolate* GetIsolate() const;

  /**
   * \return The data set in the configuration, i.e., in
   * `NamedPropertyHandlerConfiguration` or
   * `IndexedPropertyHandlerConfiguration.`
   */
  V8_INLINE Local<Value> Data() const;

  /**
   * \return The receiver. In many cases, this is the object on which the
   * property access was intercepted. When using
   * `Reflect.get`, `Function.prototype.call`, or similar functions, it is the
   * object passed in as receiver or thisArg.
   *
   * \code
   *  void GetterCallback(Local<Name> name,
   *                      const v8::PropertyCallbackInfo<v8::Value>& info) {
   *     auto context = info.GetIsolate()->GetCurrentContext();
   *
   *     v8::Local<v8::Value> a_this =
   *         info.This()
   *             ->GetRealNamedProperty(context, v8_str("a"))
   *             .ToLocalChecked();
   *     v8::Local<v8::Value> a_holder =
   *         info.Holder()
   *             ->GetRealNamedProperty(context, v8_str("a"))
   *             .ToLocalChecked();
   *
   *    CHECK(v8_str("r")->Equals(context, a_this).FromJust());
   *    CHECK(v8_str("obj")->Equals(context, a_holder).FromJust());
   *
   *    info.GetReturnValue().Set(name);
   *  }
   *
   *  v8::Local<v8::FunctionTemplate> templ =
   *  v8::FunctionTemplate::New(isolate);
   *  templ->InstanceTemplate()->SetHandler(
   *      v8::NamedPropertyHandlerConfiguration(GetterCallback));
   *  LocalContext env;
   *  env->Global()
   *      ->Set(env.local(), v8_str("obj"), templ->GetFunction(env.local())
   *                                           .ToLocalChecked()
   *                                           ->NewInstance(env.local())
   *                                           .ToLocalChecked())
   *      .FromJust();
   *
   *  CompileRun("obj.a = 'obj'; var r = {a: 'r'}; Reflect.get(obj, 'x', r)");
   * \endcode
   */
  V8_DEPRECATED(
      "Access to receiver will be deprecated soon. Use HolderV2() instead. \n"
      "See http://crbug.com/455600234. ")
  V8_INLINE Local<Object> This() const;

  /**
   * \return The object in the prototype chain of the receiver that has the
   * interceptor. Suppose you have `x` and its prototype is `y`, and `y`
   * has an interceptor. Then `info.This()` is `x` and `info.Holder()` is `y`.
   * In case the property is installed on the global object the Holder()
   * would return the global proxy.
   * TODO(http://crbug.com/333672197): rename back to Holder().
   */
  V8_INLINE Local<Object> HolderV2() const;

  /**
   * \return The return value of the callback.
   * Can be changed by calling Set().
   * \code
   * info.GetReturnValue().Set(...)
   * \endcode
   *
   */
  V8_INLINE ReturnValue<T> GetReturnValue() const;

  /**
   * For [[Set]], [[DefineOwnProperty]] and [[Delete]] operations (i.e.
   * for setter/definer/deleter callbacks) indicates whether TypeError
   * should be thrown upon operation failure. The callback should throw
   * TypeError only if it's necessary to provide more details than a default
   * error thrown by V8 contains in this case.
   *
   * \return True if the intercepted function should throw if an error occurs.
   * Usually, `true` corresponds to `'use strict'` execution mode.
   *
   * \note Always `false` when the operation was initiated by respecive
   * `Reflect` call (i.e. `Reflect.set()`, `Reflect.defineProperty()` and
   * `Reflect.deleteProperty()`).
   */
  V8_INLINE bool ShouldThrowOnError() const;

 private:
  template <typename U>
  friend class PropertyCallbackInfo;
  friend class MacroAssembler;
  friend class internal::PropertyCallbackArguments;
  friend class internal::CustomArguments<PropertyCallbackInfo>;
  friend void internal::PrintPropertyCallbackInfo(void*);
  using I = internal::Internals;

  // ShouldThrowOnError() can return true only for setter/definer/deleter
  // callbacks which match [[Set]]/[[DefineOwnProperty]]/[[Delete]]
  // operations. We detect these operations by return value type - they
  // all return boolean value, even though setter/deleter callbacks are
  // still using v8::PropertyCallbackInfo<void>.
  // TODO(https://crbug.com/348660658): cleanup this, once the callbacks are
  // migrated to a new return type.
  static constexpr bool HasShouldThrowOnError() {
    return std::is_same_v<T, v8::Boolean> || std::is_same_v<T, void>;
  }

  // Indicates whether this is a named accessor/interceptor callback call
  // or an indexed one.
  V8_INLINE bool IsNamed() const;

  // Frame block, matches the layout of ApiAccessorExitFrame.
  // See ApiAccessorExitFrameConstants.
  enum {
    // Frame arguments block.
    kPropertyKeyIndex,

    // Regular ExitFrame structure.
    kFrameSPIndex,
    kFrameTypeIndex,
    kFrameConstantPoolIndex,  // Optional, see I::kFrameCPSlotCount.
    kFrameFPIndex = kFrameConstantPoolIndex + I::kFrameCPSlotCount,
    kFramePCIndex,

    // Other arguments block, starts at kFirstArgumentIndex.
    kFirstApiArgumentIndex,
    kIsolateIndex = kFirstApiArgumentIndex,
    kReturnValueIndex,
    kCallbackInfoIndex,
    // TODO(http://crbug.com/455600234): drop this once This() is removed.
    kUnusedIndex,  // Optional, see I::kSPAlignmentSlotCount.
    kHolderIndex = kUnusedIndex + I::kSPAlignmentSlotCount,
    // TODO(http://crbug.com/455600234): drop this once This() is removed.
    kThisIndex,

    //
    // Optional part, used only by setter/definer/deleter callbacks.
    //
    kFirstOptionalArgument,
    kShouldThrowOnErrorIndex = kFirstOptionalArgument,

    // Used as value handle storage when called via CallApiSetter builtin.
    kValueIndex,

    kFullArgsLength,
    kMandatoryArgsLength = kFirstOptionalArgument,
    kOptionalArgsLength = kFullArgsLength - kFirstOptionalArgument,

    // Various lengths of just Api arguments part.
    kMandatoryApiArgsLength = kMandatoryArgsLength - kFirstApiArgumentIndex,
    kFullApiArgsLength = kFullArgsLength - kFirstApiArgumentIndex,
  };

  // PropertyCallbackInfo object provides a view of the stack area where the
  // data is stored and thus it's not supposed to be copyable/movable.
  PropertyCallbackInfo(const PropertyCallbackInfo&) = delete;
  PropertyCallbackInfo& operator=(const PropertyCallbackInfo&) = delete;
  PropertyCallbackInfo(PropertyCallbackInfo&&) = delete;
  PropertyCallbackInfo& operator=(PropertyCallbackInfo&&) = delete;

  PropertyCallbackInfo() = default;

  // Declare as mutable to let GC modify the contents of the slots even though
  // it's not possible to change values via this class.
  // Define the array size as 1 to make it clear that we are going to access
  // it out-of-bounds anyway.
  mutable internal::Address args_[1];
};

using FunctionCallback = void (*)(const FunctionCallbackInfo<Value>& info);

// --- Implementation ---

template <typename T>
ReturnValue<T>::ReturnValue(internal::Address* slot) : value_(slot) {}

template <typename T>
void ReturnValue<T>::SetInternal(internal::Address value) {
#if V8_STATIC_ROOTS_BOOL
  using I = internal::Internals;
  // Ensure that the upper 32-bits are not modified. Compiler should be
  // able to optimize this to a store of a lower 32-bits of the value.
  // This is fine since the callback can return only JavaScript values which
  // are either Smis or heap objects allocated in the main cage.
  *value_ = I::DecompressTaggedField(*value_, I::CompressTagged(value));
#else
  *value_ = value;
#endif  // V8_STATIC_ROOTS_BOOL
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Global<S>& handle) {
  static_assert(std::is_base_of_v<T, S>, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    SetDefaultValue();
  } else {
    SetInternal(handle.ptr());
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::SetNonEmpty(const Global<S>& handle) {
  static_assert(std::is_base_of_v<T, S>, "type check");
#ifdef V8_ENABLE_CHECKS
  internal::VerifyHandleIsNonEmpty(handle.IsEmpty());
#endif  // V8_ENABLE_CHECKS
  SetInternal(handle.ptr());
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const BasicTracedReference<S>& handle) {
  static_assert(std::is_base_of_v<T, S>, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    SetDefaultValue();
  } else {
    SetInternal(handle.ptr());
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::SetNonEmpty(const BasicTracedReference<S>& handle) {
  static_assert(std::is_base_of_v<T, S>, "type check");
#ifdef V8_ENABLE_CHECKS
  internal::VerifyHandleIsNonEmpty(handle.IsEmpty());
#endif  // V8_ENABLE_CHECKS
  SetInternal(handle.ptr());
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Local<S> handle) {
  static_assert(std::is_base_of_v<T, S>, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    SetDefaultValue();
  } else {
    SetInternal(handle.ptr());
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::SetNonEmpty(const Local<S> handle) {
  static_assert(std::is_base_of_v<T, S>, "type check");
#ifdef V8_ENABLE_CHECKS
  internal::VerifyHandleIsNonEmpty(handle.IsEmpty());
#endif  // V8_ENABLE_CHECKS
  SetInternal(handle.ptr());
}

template <typename T>
void ReturnValue<T>::Set(double i) {
  static_assert(std::is_base_of_v<T, Number>, "type check");
  SetNonEmpty(Number::New(GetIsolate(), i));
}

template <typename T>
void ReturnValue<T>::Set(int16_t i) {
  static_assert(std::is_base_of_v<T, Integer>, "type check");
  using I = internal::Internals;
  static_assert(I::IsValidSmi(std::numeric_limits<int16_t>::min()));
  static_assert(I::IsValidSmi(std::numeric_limits<int16_t>::max()));
  SetInternal(I::IntegralToSmi(i));
}

template <typename T>
void ReturnValue<T>::Set(int32_t i) {
  static_assert(std::is_base_of_v<T, Integer>, "type check");
  if (const auto result = internal::Internals::TryIntegralToSmi(i)) {
    SetInternal(*result);
    return;
  }
  SetNonEmpty(Integer::New(GetIsolate(), i));
}

template <typename T>
void ReturnValue<T>::Set(int64_t i) {
  static_assert(std::is_base_of_v<T, Integer>, "type check");
  if (const auto result = internal::Internals::TryIntegralToSmi(i)) {
    SetInternal(*result);
    return;
  }
  SetNonEmpty(Number::New(GetIsolate(), static_cast<double>(i)));
}

template <typename T>
void ReturnValue<T>::Set(uint16_t i) {
  static_assert(std::is_base_of_v<T, Integer>, "type check");
  using I = internal::Internals;
  static_assert(I::IsValidSmi(std::numeric_limits<uint16_t>::min()));
  static_assert(I::IsValidSmi(std::numeric_limits<uint16_t>::max()));
  SetInternal(I::IntegralToSmi(i));
}

template <typename T>
void ReturnValue<T>::Set(uint32_t i) {
  static_assert(std::is_base_of_v<T, Integer>, "type check");
  if (const auto result = internal::Internals::TryIntegralToSmi(i)) {
    SetInternal(*result);
    return;
  }
  SetNonEmpty(Integer::NewFromUnsigned(GetIsolate(), i));
}

template <typename T>
void ReturnValue<T>::Set(uint64_t i) {
  static_assert(std::is_base_of_v<T, Integer>, "type check");
  if (const auto result = internal::Internals::TryIntegralToSmi(i)) {
    SetInternal(*result);
    return;
  }
  SetNonEmpty(Number::New(GetIsolate(), static_cast<double>(i)));
}

template <typename T>
void ReturnValue<T>::Set(bool value) {
  static_assert(std::is_void_v<T> || std::is_base_of_v<T, Boolean>,
                "type check");
  using I = internal::Internals;
#if V8_STATIC_ROOTS_BOOL
#ifdef V8_ENABLE_CHECKS
  internal::PerformCastCheck(
      internal::ValueHelper::SlotAsValue<Value, true>(value_));
#endif  // V8_ENABLE_CHECKS
  SetInternal(value ? I::StaticReadOnlyRoot::kTrueValue
                    : I::StaticReadOnlyRoot::kFalseValue);
#else
  int root_index;
  if (value) {
    root_index = I::kTrueValueRootIndex;
  } else {
    root_index = I::kFalseValueRootIndex;
  }
  *value_ = I::GetRoot(GetIsolate(), root_index);
#endif  // V8_STATIC_ROOTS_BOOL
}

template <typename T>
void ReturnValue<T>::SetDefaultValue() {
  using I = internal::Internals;
  if constexpr (std::is_same_v<void, T> || std::is_same_v<v8::Boolean, T>) {
    Set(true);
  } else if constexpr (std::is_same_v<v8::Integer, T>) {
    SetInternal(I::IntegralToSmi(0));
  } else {
    static_assert(std::is_same_v<v8::Value, T> || std::is_same_v<v8::Array, T>);
#if V8_STATIC_ROOTS_BOOL
    SetInternal(I::StaticReadOnlyRoot::kUndefinedValue);
#else
    *value_ = I::GetRoot(GetIsolate(), I::kUndefinedValueRootIndex);
#endif  // V8_STATIC_ROOTS_BOOL
  }
}

template <typename T>
void ReturnValue<T>::SetNull() {
  static_assert(std::is_base_of_v<T, Primitive>, "type check");
  using I = internal::Internals;
#if V8_STATIC_ROOTS_BOOL
#ifdef V8_ENABLE_CHECKS
  internal::PerformCastCheck(
      internal::ValueHelper::SlotAsValue<Value, true>(value_));
#endif  // V8_ENABLE_CHECKS
  SetInternal(I::StaticReadOnlyRoot::kNullValue);
#else
  *value_ = I::GetRoot(GetIsolate(), I::kNullValueRootIndex);
#endif  // V8_STATIC_ROOTS_BOOL
}

template <typename T>
void ReturnValue<T>::SetUndefined() {
  static_assert(std::is_base_of_v<T, Primitive>, "type check");
  using I = internal::Internals;
#if V8_STATIC_ROOTS_BOOL
#ifdef V8_ENABLE_CHECKS
  internal::PerformCastCheck(
      internal::ValueHelper::SlotAsValue<Value, true>(value_));
#endif  // V8_ENABLE_CHECKS
  SetInternal(I::StaticReadOnlyRoot::kUndefinedValue);
#else
  *value_ = I::GetRoot(GetIsolate(), I::kUndefinedValueRootIndex);
#endif  // V8_STATIC_ROOTS_BOOL
}

template <typename T>
void ReturnValue<T>::SetFalse() {
  static_assert(std::is_void_v<T> || std::is_base_of_v<T, Boolean>,
                "type check");
  using I = internal::Internals;
#if V8_STATIC_ROOTS_BOOL
#ifdef V8_ENABLE_CHECKS
  internal::PerformCastCheck(
      internal::ValueHelper::SlotAsValue<Value, true>(value_));
#endif  // V8_ENABLE_CHECKS
  SetInternal(I::StaticReadOnlyRoot::kFalseValue);
#else
  *value_ = I::GetRoot(GetIsolate(), I::kFalseValueRootIndex);
#endif  // V8_STATIC_ROOTS_BOOL
}

template <typename T>
void ReturnValue<T>::SetEmptyString() {
  static_assert(std::is_base_of_v<T, String>, "type check");
  using I = internal::Internals;
#if V8_STATIC_ROOTS_BOOL
#ifdef V8_ENABLE_CHECKS
  internal::PerformCastCheck(
      internal::ValueHelper::SlotAsValue<Value, true>(value_));
#endif  // V8_ENABLE_CHECKS
  SetInternal(I::StaticReadOnlyRoot::kEmptyString);
#else
  *value_ = I::GetRoot(GetIsolate(), I::kEmptyStringRootIndex);
#endif  // V8_STATIC_ROOTS_BOOL
}

template <typename T>
Isolate* ReturnValue<T>::GetIsolate() const {
  return *reinterpret_cast<Isolate**>(&value_[kIsolateValueIndex]);
}

template <typename T>
Local<Value> ReturnValue<T>::Get() const {
  return Local<Value>::New(GetIsolate(),
                           internal::ValueHelper::SlotAsValue<Value>(value_));
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(S* whatever) {
  static_assert(sizeof(S) < 0, "incompilable to prevent inadvertent misuse");
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::operator[](int i) const {
  if (i < 0 || Length() <= i) return Undefined(GetIsolate());
  return Local<Value>::FromSlot(&values_[kFirstJSArgumentIndex + i]);
}

template <typename T>
Local<Object> FunctionCallbackInfo<T>::This() const {
  return Local<Object>::FromSlot(&values_[kReceiverIndex]);
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::NewTarget() const {
  if (IsConstructCall()) {
    // Can't use &values_[kNewTargetIndex] because of "array index -1 is
    // before the beginning of the array" error.
    internal::Address* values = &values_[0];
    return Local<Value>::FromSlot(values + kNewTargetIndex);
  }
  return Undefined(GetIsolate());
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::Data() const {
  auto target = Local<v8::Data>::FromSlot(&values_[kTargetIndex]);
  return api_internal::GetFunctionTemplateData(GetIsolate(), target);
}

template <typename T>
Isolate* FunctionCallbackInfo<T>::GetIsolate() const {
  return reinterpret_cast<Isolate*>(values_[kIsolateIndex]);
}

template <typename T>
ReturnValue<T> FunctionCallbackInfo<T>::GetReturnValue() const {
  return ReturnValue<T>(&values_[kReturnValueIndex]);
}

template <typename T>
bool FunctionCallbackInfo<T>::IsConstructCall() const {
  return I::SmiValue(values_[kFrameTypeIndex]) == I::kFrameTypeApiConstructExit;
}

template <typename T>
int FunctionCallbackInfo<T>::Length() const {
  return static_cast<int>(values_[kArgcIndex]);
}

template <typename T>
bool PropertyCallbackInfo<T>::IsNamed() const {
  return I::SmiValue(args_[kFrameTypeIndex]) ==
         I::kFrameTypeApiNamedAccessorExit;
}

template <typename T>
Isolate* PropertyCallbackInfo<T>::GetIsolate() const {
  return *reinterpret_cast<Isolate**>(&args_[kIsolateIndex]);
}

template <typename T>
Local<Value> PropertyCallbackInfo<T>::Data() const {
  internal::Address callback_info = args_[kCallbackInfoIndex];
  internal::Address data =
      I::ReadTaggedPointerField(callback_info, I::kCallbackInfoDataOffset);
  return Local<Value>::New(GetIsolate(), data);
}

template <typename T>
Local<Object> PropertyCallbackInfo<T>::This() const {
  return Local<Object>::FromSlot(&args_[kThisIndex]);
}

template <typename T>
Local<Object> PropertyCallbackInfo<T>::HolderV2() const {
  return Local<Object>::FromSlot(&args_[kHolderIndex]);
}

template <typename T>
ReturnValue<T> PropertyCallbackInfo<T>::GetReturnValue() const {
  return ReturnValue<T>(&args_[kReturnValueIndex]);
}

template <typename T>
bool PropertyCallbackInfo<T>::ShouldThrowOnError() const {
  if constexpr (!HasShouldThrowOnError()) return false;
  if (args_[kShouldThrowOnErrorIndex] !=
      I::IntegralToSmi(I::kInferShouldThrowMode)) {
    return args_[kShouldThrowOnErrorIndex] != I::IntegralToSmi(I::kDontThrow);
  }
  return v8::internal::ShouldThrowOnError(
      reinterpret_cast<v8::internal::Isolate*>(GetIsolate()));
}

}  // namespace v8

#endif  // INCLUDE_V8_FUNCTION_CALLBACK_H_
