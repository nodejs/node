// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_FUNCTION_CALLBACK_H_
#define INCLUDE_V8_FUNCTION_CALLBACK_H_

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

template <typename T>
class ReturnValue {
 public:
  template <class S>
  V8_INLINE ReturnValue(const ReturnValue<S>& that) : value_(that.value_) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }
  // Local setters
  template <typename S>
  V8_INLINE void Set(const Global<S>& handle);
  template <typename S>
  V8_INLINE void Set(const BasicTracedReference<S>& handle);
  template <typename S>
  V8_INLINE void Set(const Local<S> handle);
  // Fast primitive setters
  V8_INLINE void Set(bool value);
  V8_INLINE void Set(double i);
  V8_INLINE void Set(int32_t i);
  V8_INLINE void Set(uint32_t i);
  // Fast JS primitive setters
  V8_INLINE void SetNull();
  V8_INLINE void SetUndefined();
  V8_INLINE void SetEmptyString();
  // Convenience getter for Isolate
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
  V8_INLINE void SetInternal(internal::Address value) { *value_ = value; }
  V8_INLINE internal::Address GetDefaultValue();
  V8_INLINE explicit ReturnValue(internal::Address* slot);

  // See FunctionCallbackInfo.
  static constexpr int kIsolateValueIndex = -2;
  static constexpr int kDefaultValueValueIndex = -1;

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
  /**
   * If the callback was created without a Signature, this is the same
   * value as This(). If there is a signature, and the signature didn't match
   * This() but one of its hidden prototypes, this will be the respective
   * hidden prototype.
   *
   * Note that this is not the prototype of This() on which the accessor
   * referencing this callback was found (which in V8 internally is often
   * referred to as holder [sic]).
   */
  V8_INLINE Local<Object> Holder() const;
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
  friend class internal::Builtins;
  static constexpr int kHolderIndex = 0;
  static constexpr int kIsolateIndex = 1;
  static constexpr int kReturnValueDefaultValueIndex = 2;
  static constexpr int kReturnValueIndex = 3;
  static constexpr int kDataIndex = 4;
  static constexpr int kNewTargetIndex = 5;

  static constexpr int kArgsLength = 6;
  static constexpr int kArgsLengthWithReceiver = 7;

  // Codegen constants:
  static constexpr int kSize = 3 * internal::kApiSystemPointerSize;
  static constexpr int kImplicitArgsOffset = 0;
  static constexpr int kValuesOffset =
      kImplicitArgsOffset + internal::kApiSystemPointerSize;
  static constexpr int kLengthOffset =
      kValuesOffset + internal::kApiSystemPointerSize;

  static constexpr int kThisValuesIndex = -1;
  static_assert(ReturnValue<Value>::kDefaultValueValueIndex ==
                kReturnValueDefaultValueIndex - kReturnValueIndex);
  static_assert(ReturnValue<Value>::kIsolateValueIndex ==
                kIsolateIndex - kReturnValueIndex);

  V8_INLINE FunctionCallbackInfo(internal::Address* implicit_args,
                                 internal::Address* values, int length);
  internal::Address* implicit_args_;
  internal::Address* values_;
  int length_;
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
  V8_INLINE Local<Object> This() const;

  /**
   * \return The object in the prototype chain of the receiver that has the
   * interceptor. Suppose you have `x` and its prototype is `y`, and `y`
   * has an interceptor. Then `info.This()` is `x` and `info.Holder()` is `y`.
   * The Holder() could be a hidden object (the global object, rather
   * than the global proxy).
   *
   * \note For security reasons, do not pass the object back into the runtime.
   */
  V8_INLINE Local<Object> Holder() const;

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
   * \return True if the intercepted function should throw if an error occurs.
   * Usually, `true` corresponds to `'use strict'`.
   *
   * \note Always `false` when intercepting `Reflect.set()`
   * independent of the language mode.
   */
  V8_INLINE bool ShouldThrowOnError() const;

 private:
  friend class MacroAssembler;
  friend class internal::PropertyCallbackArguments;
  friend class internal::CustomArguments<PropertyCallbackInfo>;
  static constexpr int kShouldThrowOnErrorIndex = 0;
  static constexpr int kHolderIndex = 1;
  static constexpr int kIsolateIndex = 2;
  static constexpr int kReturnValueDefaultValueIndex = 3;
  static constexpr int kReturnValueIndex = 4;
  static constexpr int kDataIndex = 5;
  static constexpr int kThisIndex = 6;

  static constexpr int kArgsLength = 7;

  static constexpr int kSize = 1 * internal::kApiSystemPointerSize;

  V8_INLINE explicit PropertyCallbackInfo(internal::Address* args)
      : args_(args) {}
  internal::Address* args_;
};

using FunctionCallback = void (*)(const FunctionCallbackInfo<Value>& info);

// --- Implementation ---

template <typename T>
ReturnValue<T>::ReturnValue(internal::Address* slot) : value_(slot) {}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Global<S>& handle) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Address*>(*handle);
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const BasicTracedReference<S>& handle) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Address*>(handle.val_);
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Local<S> handle) {
  static_assert(std::is_void<T>::value || std::is_base_of<T, S>::value,
                "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = internal::ValueHelper::ValueAsAddress(*handle);
  }
}

template <typename T>
void ReturnValue<T>::Set(double i) {
  static_assert(std::is_base_of<T, Number>::value, "type check");
  Set(Number::New(GetIsolate(), i));
}

template <typename T>
void ReturnValue<T>::Set(int32_t i) {
  static_assert(std::is_base_of<T, Integer>::value, "type check");
  using I = internal::Internals;
  if (V8_LIKELY(I::IsValidSmi(i))) {
    *value_ = I::IntToSmi(i);
    return;
  }
  Set(Integer::New(GetIsolate(), i));
}

template <typename T>
void ReturnValue<T>::Set(uint32_t i) {
  static_assert(std::is_base_of<T, Integer>::value, "type check");
  // Can't simply use INT32_MAX here for whatever reason.
  bool fits_into_int32_t = (i & (1U << 31)) == 0;
  if (V8_LIKELY(fits_into_int32_t)) {
    Set(static_cast<int32_t>(i));
    return;
  }
  Set(Integer::NewFromUnsigned(GetIsolate(), i));
}

template <typename T>
void ReturnValue<T>::Set(bool value) {
  static_assert(std::is_base_of<T, Boolean>::value, "type check");
  using I = internal::Internals;
  int root_index;
  if (value) {
    root_index = I::kTrueValueRootIndex;
  } else {
    root_index = I::kFalseValueRootIndex;
  }
  *value_ = I::GetRoot(GetIsolate(), root_index);
}

template <typename T>
void ReturnValue<T>::SetNull() {
  static_assert(std::is_base_of<T, Primitive>::value, "type check");
  using I = internal::Internals;
  *value_ = I::GetRoot(GetIsolate(), I::kNullValueRootIndex);
}

template <typename T>
void ReturnValue<T>::SetUndefined() {
  static_assert(std::is_base_of<T, Primitive>::value, "type check");
  using I = internal::Internals;
  *value_ = I::GetRoot(GetIsolate(), I::kUndefinedValueRootIndex);
}

template <typename T>
void ReturnValue<T>::SetEmptyString() {
  static_assert(std::is_base_of<T, String>::value, "type check");
  using I = internal::Internals;
  *value_ = I::GetRoot(GetIsolate(), I::kEmptyStringRootIndex);
}

template <typename T>
Isolate* ReturnValue<T>::GetIsolate() const {
  // Isolate is always the pointer below the default value on the stack.
  return *reinterpret_cast<Isolate**>(&value_[kIsolateValueIndex]);
}

template <typename T>
Local<Value> ReturnValue<T>::Get() const {
  using I = internal::Internals;
#if V8_STATIC_ROOTS_BOOL
  if (I::is_identical(*value_, I::StaticReadOnlyRoot::kTheHoleValue)) {
#else
  if (*value_ == I::GetRoot(GetIsolate(), I::kTheHoleValueRootIndex)) {
#endif
    return Undefined(GetIsolate());
  }
  return Local<Value>::New(GetIsolate(), reinterpret_cast<Value*>(value_));
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(S* whatever) {
  static_assert(sizeof(S) < 0, "incompilable to prevent inadvertent misuse");
}

template <typename T>
internal::Address ReturnValue<T>::GetDefaultValue() {
  // Default value is always the pointer below value_ on the stack.
  return value_[kDefaultValueValueIndex];
}

template <typename T>
FunctionCallbackInfo<T>::FunctionCallbackInfo(internal::Address* implicit_args,
                                              internal::Address* values,
                                              int length)
    : implicit_args_(implicit_args), values_(values), length_(length) {}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::operator[](int i) const {
  // values_ points to the first argument (not the receiver).
  if (i < 0 || length_ <= i) return Undefined(GetIsolate());
  return Local<Value>::FromSlot(values_ + i);
}

template <typename T>
Local<Object> FunctionCallbackInfo<T>::This() const {
  // values_ points to the first argument (not the receiver).
  return Local<Object>::FromSlot(values_ + kThisValuesIndex);
}

template <typename T>
Local<Object> FunctionCallbackInfo<T>::Holder() const {
  return Local<Object>::FromSlot(&implicit_args_[kHolderIndex]);
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::NewTarget() const {
  return Local<Value>::FromSlot(&implicit_args_[kNewTargetIndex]);
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::Data() const {
  return Local<Value>::FromSlot(&implicit_args_[kDataIndex]);
}

template <typename T>
Isolate* FunctionCallbackInfo<T>::GetIsolate() const {
  return *reinterpret_cast<Isolate**>(&implicit_args_[kIsolateIndex]);
}

template <typename T>
ReturnValue<T> FunctionCallbackInfo<T>::GetReturnValue() const {
  return ReturnValue<T>(&implicit_args_[kReturnValueIndex]);
}

template <typename T>
bool FunctionCallbackInfo<T>::IsConstructCall() const {
  return !NewTarget()->IsUndefined();
}

template <typename T>
int FunctionCallbackInfo<T>::Length() const {
  return length_;
}

template <typename T>
Isolate* PropertyCallbackInfo<T>::GetIsolate() const {
  return *reinterpret_cast<Isolate**>(&args_[kIsolateIndex]);
}

template <typename T>
Local<Value> PropertyCallbackInfo<T>::Data() const {
  return Local<Value>::FromSlot(&args_[kDataIndex]);
}

template <typename T>
Local<Object> PropertyCallbackInfo<T>::This() const {
  return Local<Object>::FromSlot(&args_[kThisIndex]);
}

template <typename T>
Local<Object> PropertyCallbackInfo<T>::Holder() const {
  return Local<Object>::FromSlot(&args_[kHolderIndex]);
}

template <typename T>
ReturnValue<T> PropertyCallbackInfo<T>::GetReturnValue() const {
  return ReturnValue<T>(&args_[kReturnValueIndex]);
}

template <typename T>
bool PropertyCallbackInfo<T>::ShouldThrowOnError() const {
  using I = internal::Internals;
  if (args_[kShouldThrowOnErrorIndex] !=
      I::IntToSmi(I::kInferShouldThrowMode)) {
    return args_[kShouldThrowOnErrorIndex] != I::IntToSmi(I::kDontThrow);
  }
  return v8::internal::ShouldThrowOnError(
      reinterpret_cast<v8::internal::Isolate*>(GetIsolate()));
}

}  // namespace v8

#endif  // INCLUDE_V8_FUNCTION_CALLBACK_H_
