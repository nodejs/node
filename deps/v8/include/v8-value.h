// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VALUE_H_
#define INCLUDE_V8_VALUE_H_

#include "v8-data.h"          // NOLINT(build/include_directory)
#include "v8-internal.h"      // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-maybe.h"         // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

/**
 * The v8 JavaScript engine.
 */
namespace v8 {

class BigInt;
class Int32;
class Integer;
class Number;
class Object;
class String;
class Uint32;

/**
 * The superclass of all JavaScript values and objects.
 */
class V8_EXPORT Value : public Data {
 public:
  /**
   * Returns true if this value is the undefined value.  See ECMA-262
   * 4.3.10.
   *
   * This is equivalent to `value === undefined` in JS.
   */
  V8_INLINE bool IsUndefined() const;

  /**
   * Returns true if this value is the null value.  See ECMA-262
   * 4.3.11.
   *
   * This is equivalent to `value === null` in JS.
   */
  V8_INLINE bool IsNull() const;

  /**
   * Returns true if this value is either the null or the undefined value.
   * See ECMA-262
   * 4.3.11. and 4.3.12
   *
   * This is equivalent to `value == null` in JS.
   */
  V8_INLINE bool IsNullOrUndefined() const;

  /**
   * Returns true if this value is true.
   *
   * This is not the same as `BooleanValue()`. The latter performs a
   * conversion to boolean, i.e. the result of `Boolean(value)` in JS, whereas
   * this checks `value === true`.
   */
  bool IsTrue() const;

  /**
   * Returns true if this value is false.
   *
   * This is not the same as `!BooleanValue()`. The latter performs a
   * conversion to boolean, i.e. the result of `!Boolean(value)` in JS, whereas
   * this checks `value === false`.
   */
  bool IsFalse() const;

  /**
   * Returns true if this value is a symbol or a string.
   *
   * This is equivalent to
   * `typeof value === 'string' || typeof value === 'symbol'` in JS.
   */
  bool IsName() const;

  /**
   * Returns true if this value is an instance of the String type.
   * See ECMA-262 8.4.
   *
   * This is equivalent to `typeof value === 'string'` in JS.
   */
  V8_INLINE bool IsString() const;

  /**
   * Returns true if this value is a symbol.
   *
   * This is equivalent to `typeof value === 'symbol'` in JS.
   */
  bool IsSymbol() const;

  /**
   * Returns true if this value is a function.
   *
   * This is equivalent to `typeof value === 'function'` in JS.
   */
  bool IsFunction() const;

  /**
   * Returns true if this value is an array. Note that it will return false for
   * an Proxy for an array.
   */
  bool IsArray() const;

  /**
   * Returns true if this value is an object.
   */
  bool IsObject() const;

  /**
   * Returns true if this value is a bigint.
   *
   * This is equivalent to `typeof value === 'bigint'` in JS.
   */
  bool IsBigInt() const;

  /**
   * Returns true if this value is boolean.
   *
   * This is equivalent to `typeof value === 'boolean'` in JS.
   */
  bool IsBoolean() const;

  /**
   * Returns true if this value is a number.
   *
   * This is equivalent to `typeof value === 'number'` in JS.
   */
  bool IsNumber() const;

  /**
   * Returns true if this value is an `External` object.
   */
  bool IsExternal() const;

  /**
   * Returns true if this value is a 32-bit signed integer.
   */
  bool IsInt32() const;

  /**
   * Returns true if this value is a 32-bit unsigned integer.
   */
  bool IsUint32() const;

  /**
   * Returns true if this value is a Date.
   */
  bool IsDate() const;

  /**
   * Returns true if this value is an Arguments object.
   */
  bool IsArgumentsObject() const;

  /**
   * Returns true if this value is a BigInt object.
   */
  bool IsBigIntObject() const;

  /**
   * Returns true if this value is a Boolean object.
   */
  bool IsBooleanObject() const;

  /**
   * Returns true if this value is a Number object.
   */
  bool IsNumberObject() const;

  /**
   * Returns true if this value is a String object.
   */
  bool IsStringObject() const;

  /**
   * Returns true if this value is a Symbol object.
   */
  bool IsSymbolObject() const;

  /**
   * Returns true if this value is a NativeError.
   */
  bool IsNativeError() const;

  /**
   * Returns true if this value is a RegExp.
   */
  bool IsRegExp() const;

  /**
   * Returns true if this value is an async function.
   */
  bool IsAsyncFunction() const;

  /**
   * Returns true if this value is a Generator function.
   */
  bool IsGeneratorFunction() const;

  /**
   * Returns true if this value is a Generator object (iterator).
   */
  bool IsGeneratorObject() const;

  /**
   * Returns true if this value is a Promise.
   */
  bool IsPromise() const;

  /**
   * Returns true if this value is a Map.
   */
  bool IsMap() const;

  /**
   * Returns true if this value is a Set.
   */
  bool IsSet() const;

  /**
   * Returns true if this value is a Map Iterator.
   */
  bool IsMapIterator() const;

  /**
   * Returns true if this value is a Set Iterator.
   */
  bool IsSetIterator() const;

  /**
   * Returns true if this value is a WeakMap.
   */
  bool IsWeakMap() const;

  /**
   * Returns true if this value is a WeakSet.
   */
  bool IsWeakSet() const;

  /**
   * Returns true if this value is an ArrayBuffer.
   */
  bool IsArrayBuffer() const;

  /**
   * Returns true if this value is an ArrayBufferView.
   */
  bool IsArrayBufferView() const;

  /**
   * Returns true if this value is one of TypedArrays.
   */
  bool IsTypedArray() const;

  /**
   * Returns true if this value is an Uint8Array.
   */
  bool IsUint8Array() const;

  /**
   * Returns true if this value is an Uint8ClampedArray.
   */
  bool IsUint8ClampedArray() const;

  /**
   * Returns true if this value is an Int8Array.
   */
  bool IsInt8Array() const;

  /**
   * Returns true if this value is an Uint16Array.
   */
  bool IsUint16Array() const;

  /**
   * Returns true if this value is an Int16Array.
   */
  bool IsInt16Array() const;

  /**
   * Returns true if this value is an Uint32Array.
   */
  bool IsUint32Array() const;

  /**
   * Returns true if this value is an Int32Array.
   */
  bool IsInt32Array() const;

  /**
   * Returns true if this value is a Float32Array.
   */
  bool IsFloat32Array() const;

  /**
   * Returns true if this value is a Float64Array.
   */
  bool IsFloat64Array() const;

  /**
   * Returns true if this value is a BigInt64Array.
   */
  bool IsBigInt64Array() const;

  /**
   * Returns true if this value is a BigUint64Array.
   */
  bool IsBigUint64Array() const;

  /**
   * Returns true if this value is a DataView.
   */
  bool IsDataView() const;

  /**
   * Returns true if this value is a SharedArrayBuffer.
   */
  bool IsSharedArrayBuffer() const;

  /**
   * Returns true if this value is a JavaScript Proxy.
   */
  bool IsProxy() const;

  /**
   * Returns true if this value is a WasmMemoryObject.
   */
  bool IsWasmMemoryObject() const;

  /**
   * Returns true if this value is a WasmModuleObject.
   */
  bool IsWasmModuleObject() const;

  /**
   * Returns true if the value is a Module Namespace Object.
   */
  bool IsModuleNamespaceObject() const;

  /**
   * Perform the equivalent of `BigInt(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<BigInt> ToBigInt(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Number> ToNumber(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `String(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToString(
      Local<Context> context) const;
  /**
   * Provide a string representation of this value usable for debugging.
   * This operation has no observable side effects and will succeed
   * unless e.g. execution is being terminated.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToDetailString(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Object(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> ToObject(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS and convert the result
   * to an integer. Negative values are rounded up, positive values are rounded
   * down. NaN is converted to 0. Infinite values yield undefined results.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Integer> ToInteger(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS and convert the result
   * to an unsigned 32-bit integer by performing the steps in
   * https://tc39.es/ecma262/#sec-touint32.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToUint32(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS and convert the result
   * to a signed 32-bit integer by performing the steps in
   * https://tc39.es/ecma262/#sec-toint32.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Int32> ToInt32(Local<Context> context) const;

  /**
   * Perform the equivalent of `Boolean(value)` in JS. This can never fail.
   */
  Local<Boolean> ToBoolean(Isolate* isolate) const;

  /**
   * Attempts to convert a string to an array index.
   * Returns an empty handle if the conversion fails.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToArrayIndex(
      Local<Context> context) const;

  /** Returns the equivalent of `ToBoolean()->Value()`. */
  bool BooleanValue(Isolate* isolate) const;

  /** Returns the equivalent of `ToNumber()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<double> NumberValue(Local<Context> context) const;
  /** Returns the equivalent of `ToInteger()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<int64_t> IntegerValue(
      Local<Context> context) const;
  /** Returns the equivalent of `ToUint32()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<uint32_t> Uint32Value(
      Local<Context> context) const;
  /** Returns the equivalent of `ToInt32()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<int32_t> Int32Value(Local<Context> context) const;

  /** JS == */
  V8_WARN_UNUSED_RESULT Maybe<bool> Equals(Local<Context> context,
                                           Local<Value> that) const;
  bool StrictEquals(Local<Value> that) const;
  bool SameValue(Local<Value> that) const;

  template <class T>
  V8_INLINE static Value* Cast(T* value) {
    return static_cast<Value*>(value);
  }

  Local<String> TypeOf(Isolate*);

  Maybe<bool> InstanceOf(Local<Context> context, Local<Object> object);

 private:
  V8_INLINE bool QuickIsUndefined() const;
  V8_INLINE bool QuickIsNull() const;
  V8_INLINE bool QuickIsNullOrUndefined() const;
  V8_INLINE bool QuickIsString() const;
  bool FullIsUndefined() const;
  bool FullIsNull() const;
  bool FullIsString() const;

  static void CheckCast(Data* that);
};

template <>
V8_INLINE Value* Value::Cast(Data* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Value*>(value);
}

bool Value::IsUndefined() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsUndefined();
#else
  return QuickIsUndefined();
#endif
}

bool Value::QuickIsUndefined() const {
  using A = internal::Address;
  using I = internal::Internals;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  if (I::GetInstanceType(obj) != I::kOddballType) return false;
  return (I::GetOddballKind(obj) == I::kUndefinedOddballKind);
}

bool Value::IsNull() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsNull();
#else
  return QuickIsNull();
#endif
}

bool Value::QuickIsNull() const {
  using A = internal::Address;
  using I = internal::Internals;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  if (I::GetInstanceType(obj) != I::kOddballType) return false;
  return (I::GetOddballKind(obj) == I::kNullOddballKind);
}

bool Value::IsNullOrUndefined() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsNull() || FullIsUndefined();
#else
  return QuickIsNullOrUndefined();
#endif
}

bool Value::QuickIsNullOrUndefined() const {
  using A = internal::Address;
  using I = internal::Internals;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  if (I::GetInstanceType(obj) != I::kOddballType) return false;
  int kind = I::GetOddballKind(obj);
  return kind == I::kNullOddballKind || kind == I::kUndefinedOddballKind;
}

bool Value::IsString() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsString();
#else
  return QuickIsString();
#endif
}

bool Value::QuickIsString() const {
  using A = internal::Address;
  using I = internal::Internals;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  return (I::GetInstanceType(obj) < I::kFirstNonstringType);
}

}  // namespace v8

#endif  // INCLUDE_V8_VALUE_H_
