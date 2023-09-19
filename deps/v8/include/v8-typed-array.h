// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_TYPED_ARRAY_H_
#define INCLUDE_V8_TYPED_ARRAY_H_

#include "v8-array-buffer.h"  // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class SharedArrayBuffer;

/**
 * A base class for an instance of TypedArray series of constructors
 * (ES6 draft 15.13.6).
 */
class V8_EXPORT TypedArray : public ArrayBufferView {
 public:
  /*
   * The largest typed array size that can be constructed using New.
   */
  static constexpr size_t kMaxLength =
      internal::kApiSystemPointerSize == 4
          ? internal::kSmiMaxValue
          : static_cast<size_t>(uint64_t{1} << 32);

  /**
   * Number of elements in this typed array
   * (e.g. for Int16Array, |ByteLength|/2).
   */
  size_t Length();

  V8_INLINE static TypedArray* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<TypedArray*>(value);
  }

 private:
  TypedArray();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Uint8Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint8Array : public TypedArray {
 public:
  static Local<Uint8Array> New(Local<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Local<Uint8Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                               size_t byte_offset, size_t length);
  V8_INLINE static Uint8Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Uint8Array*>(value);
  }

 private:
  Uint8Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Uint8ClampedArray constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint8ClampedArray : public TypedArray {
 public:
  static Local<Uint8ClampedArray> New(Local<ArrayBuffer> array_buffer,
                                      size_t byte_offset, size_t length);
  static Local<Uint8ClampedArray> New(
      Local<SharedArrayBuffer> shared_array_buffer, size_t byte_offset,
      size_t length);
  V8_INLINE static Uint8ClampedArray* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Uint8ClampedArray*>(value);
  }

 private:
  Uint8ClampedArray();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Int8Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Int8Array : public TypedArray {
 public:
  static Local<Int8Array> New(Local<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t length);
  static Local<Int8Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                              size_t byte_offset, size_t length);
  V8_INLINE static Int8Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Int8Array*>(value);
  }

 private:
  Int8Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Uint16Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint16Array : public TypedArray {
 public:
  static Local<Uint16Array> New(Local<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Local<Uint16Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                size_t byte_offset, size_t length);
  V8_INLINE static Uint16Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Uint16Array*>(value);
  }

 private:
  Uint16Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Int16Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Int16Array : public TypedArray {
 public:
  static Local<Int16Array> New(Local<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Local<Int16Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                               size_t byte_offset, size_t length);
  V8_INLINE static Int16Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Int16Array*>(value);
  }

 private:
  Int16Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Uint32Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint32Array : public TypedArray {
 public:
  static Local<Uint32Array> New(Local<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Local<Uint32Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                size_t byte_offset, size_t length);
  V8_INLINE static Uint32Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Uint32Array*>(value);
  }

 private:
  Uint32Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Int32Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Int32Array : public TypedArray {
 public:
  static Local<Int32Array> New(Local<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Local<Int32Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                               size_t byte_offset, size_t length);
  V8_INLINE static Int32Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Int32Array*>(value);
  }

 private:
  Int32Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Float32Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Float32Array : public TypedArray {
 public:
  static Local<Float32Array> New(Local<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Local<Float32Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                 size_t byte_offset, size_t length);
  V8_INLINE static Float32Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Float32Array*>(value);
  }

 private:
  Float32Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Float64Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Float64Array : public TypedArray {
 public:
  static Local<Float64Array> New(Local<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Local<Float64Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                 size_t byte_offset, size_t length);
  V8_INLINE static Float64Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<Float64Array*>(value);
  }

 private:
  Float64Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of BigInt64Array constructor.
 */
class V8_EXPORT BigInt64Array : public TypedArray {
 public:
  static Local<BigInt64Array> New(Local<ArrayBuffer> array_buffer,
                                  size_t byte_offset, size_t length);
  static Local<BigInt64Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                  size_t byte_offset, size_t length);
  V8_INLINE static BigInt64Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<BigInt64Array*>(value);
  }

 private:
  BigInt64Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of BigUint64Array constructor.
 */
class V8_EXPORT BigUint64Array : public TypedArray {
 public:
  static Local<BigUint64Array> New(Local<ArrayBuffer> array_buffer,
                                   size_t byte_offset, size_t length);
  static Local<BigUint64Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                   size_t byte_offset, size_t length);
  V8_INLINE static BigUint64Array* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<BigUint64Array*>(value);
  }

 private:
  BigUint64Array();
  static void CheckCast(Value* obj);
};

}  // namespace v8

#endif  // INCLUDE_V8_TYPED_ARRAY_H_
