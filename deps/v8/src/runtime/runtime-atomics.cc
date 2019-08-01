// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/execution/arguments-inl.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/runtime/runtime-utils.h"

// Implement Atomic accesses to SharedArrayBuffers as defined in the
// SharedArrayBuffer draft spec, found here
// https://github.com/tc39/ecmascript_sharedmem

namespace v8 {
namespace internal {

// Other platforms have CSA support, see builtins-sharedarraybuffer-gen.h.
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64 || \
    V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_S390X

namespace {

#if V8_CC_GNU

// GCC/Clang helpfully warn us that using 64-bit atomics on 32-bit platforms
// can be slow. Good to know, but we don't have a choice.
#ifdef V8_TARGET_ARCH_32_BIT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Watomic-alignment"
#endif  // V8_TARGET_ARCH_32_BIT

template <typename T>
inline T LoadSeqCst(T* p) {
  return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

template <typename T>
inline void StoreSeqCst(T* p, T value) {
  __atomic_store_n(p, value, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T ExchangeSeqCst(T* p, T value) {
  return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T CompareExchangeSeqCst(T* p, T oldval, T newval) {
  (void)__atomic_compare_exchange_n(p, &oldval, newval, 0, __ATOMIC_SEQ_CST,
                                    __ATOMIC_SEQ_CST);
  return oldval;
}

template <typename T>
inline T AddSeqCst(T* p, T value) {
  return __atomic_fetch_add(p, value, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T SubSeqCst(T* p, T value) {
  return __atomic_fetch_sub(p, value, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T AndSeqCst(T* p, T value) {
  return __atomic_fetch_and(p, value, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T OrSeqCst(T* p, T value) {
  return __atomic_fetch_or(p, value, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T XorSeqCst(T* p, T value) {
  return __atomic_fetch_xor(p, value, __ATOMIC_SEQ_CST);
}

#ifdef V8_TARGET_ARCH_32_BIT
#pragma GCC diagnostic pop
#endif  // V8_TARGET_ARCH_32_BIT

#elif V8_CC_MSVC

#define InterlockedExchange32 _InterlockedExchange
#define InterlockedCompareExchange32 _InterlockedCompareExchange
#define InterlockedCompareExchange8 _InterlockedCompareExchange8
#define InterlockedExchangeAdd32 _InterlockedExchangeAdd
#define InterlockedExchangeAdd16 _InterlockedExchangeAdd16
#define InterlockedExchangeAdd8 _InterlockedExchangeAdd8
#define InterlockedAnd32 _InterlockedAnd
#define InterlockedOr64 _InterlockedOr64
#define InterlockedOr32 _InterlockedOr
#define InterlockedXor32 _InterlockedXor

#if defined(V8_HOST_ARCH_ARM64)
#define InterlockedExchange8 _InterlockedExchange8
#endif

#define ATOMIC_OPS(type, suffix, vctype)                                    \
  inline type ExchangeSeqCst(type* p, type value) {                         \
    return InterlockedExchange##suffix(reinterpret_cast<vctype*>(p),        \
                                       bit_cast<vctype>(value));            \
  }                                                                         \
  inline type CompareExchangeSeqCst(type* p, type oldval, type newval) {    \
    return InterlockedCompareExchange##suffix(reinterpret_cast<vctype*>(p), \
                                              bit_cast<vctype>(newval),     \
                                              bit_cast<vctype>(oldval));    \
  }                                                                         \
  inline type AddSeqCst(type* p, type value) {                              \
    return InterlockedExchangeAdd##suffix(reinterpret_cast<vctype*>(p),     \
                                          bit_cast<vctype>(value));         \
  }                                                                         \
  inline type SubSeqCst(type* p, type value) {                              \
    return InterlockedExchangeAdd##suffix(reinterpret_cast<vctype*>(p),     \
                                          -bit_cast<vctype>(value));        \
  }                                                                         \
  inline type AndSeqCst(type* p, type value) {                              \
    return InterlockedAnd##suffix(reinterpret_cast<vctype*>(p),             \
                                  bit_cast<vctype>(value));                 \
  }                                                                         \
  inline type OrSeqCst(type* p, type value) {                               \
    return InterlockedOr##suffix(reinterpret_cast<vctype*>(p),              \
                                 bit_cast<vctype>(value));                  \
  }                                                                         \
  inline type XorSeqCst(type* p, type value) {                              \
    return InterlockedXor##suffix(reinterpret_cast<vctype*>(p),             \
                                  bit_cast<vctype>(value));                 \
  }

ATOMIC_OPS(int8_t, 8, char)
ATOMIC_OPS(uint8_t, 8, char)
ATOMIC_OPS(int16_t, 16, short)  /* NOLINT(runtime/int) */
ATOMIC_OPS(uint16_t, 16, short) /* NOLINT(runtime/int) */
ATOMIC_OPS(int32_t, 32, long)   /* NOLINT(runtime/int) */
ATOMIC_OPS(uint32_t, 32, long)  /* NOLINT(runtime/int) */
ATOMIC_OPS(int64_t, 64, __int64)
ATOMIC_OPS(uint64_t, 64, __int64)

template <typename T>
inline T LoadSeqCst(T* p) {
  UNREACHABLE();
}

template <typename T>
inline void StoreSeqCst(T* p, T value) {
  UNREACHABLE();
}

#undef ATOMIC_OPS

#undef InterlockedExchange32
#undef InterlockedCompareExchange32
#undef InterlockedCompareExchange8
#undef InterlockedExchangeAdd32
#undef InterlockedExchangeAdd16
#undef InterlockedExchangeAdd8
#undef InterlockedAnd32
#undef InterlockedOr64
#undef InterlockedOr32
#undef InterlockedXor32

#if defined(V8_HOST_ARCH_ARM64)
#undef InterlockedExchange8
#endif

#else

#error Unsupported platform!

#endif

template <typename T>
T FromObject(Handle<Object> number);

template <>
inline uint8_t FromObject<uint8_t>(Handle<Object> number) {
  return NumberToUint32(*number);
}

template <>
inline int8_t FromObject<int8_t>(Handle<Object> number) {
  return NumberToInt32(*number);
}

template <>
inline uint16_t FromObject<uint16_t>(Handle<Object> number) {
  return NumberToUint32(*number);
}

template <>
inline int16_t FromObject<int16_t>(Handle<Object> number) {
  return NumberToInt32(*number);
}

template <>
inline uint32_t FromObject<uint32_t>(Handle<Object> number) {
  return NumberToUint32(*number);
}

template <>
inline int32_t FromObject<int32_t>(Handle<Object> number) {
  return NumberToInt32(*number);
}

template <>
inline uint64_t FromObject<uint64_t>(Handle<Object> bigint) {
  return Handle<BigInt>::cast(bigint)->AsUint64();
}

template <>
inline int64_t FromObject<int64_t>(Handle<Object> bigint) {
  return Handle<BigInt>::cast(bigint)->AsInt64();
}

inline Object ToObject(Isolate* isolate, int8_t t) { return Smi::FromInt(t); }

inline Object ToObject(Isolate* isolate, uint8_t t) { return Smi::FromInt(t); }

inline Object ToObject(Isolate* isolate, int16_t t) { return Smi::FromInt(t); }

inline Object ToObject(Isolate* isolate, uint16_t t) { return Smi::FromInt(t); }

inline Object ToObject(Isolate* isolate, int32_t t) {
  return *isolate->factory()->NewNumber(t);
}

inline Object ToObject(Isolate* isolate, uint32_t t) {
  return *isolate->factory()->NewNumber(t);
}

inline Object ToObject(Isolate* isolate, int64_t t) {
  return *BigInt::FromInt64(isolate, t);
}

inline Object ToObject(Isolate* isolate, uint64_t t) {
  return *BigInt::FromUint64(isolate, t);
}

template <typename T>
struct Load {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index) {
    T result = LoadSeqCst(static_cast<T*>(buffer) + index);
    return ToObject(isolate, result);
  }
};

template <typename T>
struct Store {
  static inline void Do(Isolate* isolate, void* buffer, size_t index,
                        Handle<Object> obj) {
    T value = FromObject<T>(obj);
    StoreSeqCst(static_cast<T*>(buffer) + index, value);
  }
};

template <typename T>
struct Exchange {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
    T value = FromObject<T>(obj);
    T result = ExchangeSeqCst(static_cast<T*>(buffer) + index, value);
    return ToObject(isolate, result);
  }
};

template <typename T>
inline Object DoCompareExchange(Isolate* isolate, void* buffer, size_t index,
                                Handle<Object> oldobj, Handle<Object> newobj) {
  T oldval = FromObject<T>(oldobj);
  T newval = FromObject<T>(newobj);
  T result =
      CompareExchangeSeqCst(static_cast<T*>(buffer) + index, oldval, newval);
  return ToObject(isolate, result);
}

template <typename T>
struct Add {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
    T value = FromObject<T>(obj);
    T result = AddSeqCst(static_cast<T*>(buffer) + index, value);
    return ToObject(isolate, result);
  }
};

template <typename T>
struct Sub {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
    T value = FromObject<T>(obj);
    T result = SubSeqCst(static_cast<T*>(buffer) + index, value);
    return ToObject(isolate, result);
  }
};

template <typename T>
struct And {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
    T value = FromObject<T>(obj);
    T result = AndSeqCst(static_cast<T*>(buffer) + index, value);
    return ToObject(isolate, result);
  }
};

template <typename T>
struct Or {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
    T value = FromObject<T>(obj);
    T result = OrSeqCst(static_cast<T*>(buffer) + index, value);
    return ToObject(isolate, result);
  }
};

template <typename T>
struct Xor {
  static inline Object Do(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
    T value = FromObject<T>(obj);
    T result = XorSeqCst(static_cast<T*>(buffer) + index, value);
    return ToObject(isolate, result);
  }
};

}  // anonymous namespace

// Duplicated from objects.h
// V has parameters (Type, type, TYPE, C type)
#define INTEGER_TYPED_ARRAYS(V)       \
  V(Uint8, uint8, UINT8, uint8_t)     \
  V(Int8, int8, INT8, int8_t)         \
  V(Uint16, uint16, UINT16, uint16_t) \
  V(Int16, int16, INT16, int16_t)     \
  V(Uint32, uint32, UINT32, uint32_t) \
  V(Int32, int32, INT32, int32_t)

// This is https://tc39.github.io/ecma262/#sec-getmodifysetvalueinbuffer
// but also includes the ToInteger/ToBigInt conversion that's part of
// https://tc39.github.io/ecma262/#sec-atomicreadmodifywrite
template <template <typename> class Op>
Object GetModifySetValueInBuffer(Arguments args, Isolate* isolate) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value_obj, 2);
  CHECK(sta->GetBuffer()->is_shared());

  uint8_t* source = static_cast<uint8_t*>(sta->GetBuffer()->backing_store()) +
                    sta->byte_offset();

  if (sta->type() >= kExternalBigInt64Array) {
    Handle<BigInt> bigint;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, bigint,
                                       BigInt::FromObject(isolate, value_obj));
    // SharedArrayBuffers are not detachable.
    CHECK_LT(index, sta->length());
    if (sta->type() == kExternalBigInt64Array) {
      return Op<int64_t>::Do(isolate, source, index, bigint);
    }
    DCHECK(sta->type() == kExternalBigUint64Array);
    return Op<uint64_t>::Do(isolate, source, index, bigint);
  }

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     Object::ToInteger(isolate, value_obj));
  // SharedArrayBuffers are not detachable.
  CHECK_LT(index, sta->length());

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype) \
  case kExternal##Type##Array:                        \
    return Op<ctype>::Do(isolate, source, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      break;
  }

  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_AtomicsLoad64) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CHECK(sta->GetBuffer()->is_shared());

  uint8_t* source = static_cast<uint8_t*>(sta->GetBuffer()->backing_store()) +
                    sta->byte_offset();

  DCHECK(sta->type() == kExternalBigInt64Array ||
         sta->type() == kExternalBigUint64Array);
  // SharedArrayBuffers are not detachable.
  CHECK_LT(index, sta->length());
  if (sta->type() == kExternalBigInt64Array) {
    return Load<int64_t>::Do(isolate, source, index);
  }
  DCHECK(sta->type() == kExternalBigUint64Array);
  return Load<uint64_t>::Do(isolate, source, index);
}

RUNTIME_FUNCTION(Runtime_AtomicsStore64) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value_obj, 2);
  CHECK(sta->GetBuffer()->is_shared());

  uint8_t* source = static_cast<uint8_t*>(sta->GetBuffer()->backing_store()) +
                    sta->byte_offset();

  Handle<BigInt> bigint;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, bigint,
                                     BigInt::FromObject(isolate, value_obj));

  DCHECK(sta->type() == kExternalBigInt64Array ||
         sta->type() == kExternalBigUint64Array);
  // SharedArrayBuffers are not detachable.
  CHECK_LT(index, sta->length());
  if (sta->type() == kExternalBigInt64Array) {
    Store<int64_t>::Do(isolate, source, index, bigint);
    return *bigint;
  }
  DCHECK(sta->type() == kExternalBigUint64Array);
  Store<uint64_t>::Do(isolate, source, index, bigint);
  return *bigint;
}

RUNTIME_FUNCTION(Runtime_AtomicsExchange) {
  return GetModifySetValueInBuffer<Exchange>(args, isolate);
}

RUNTIME_FUNCTION(Runtime_AtomicsCompareExchange) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, old_value_obj, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_value_obj, 3);
  CHECK(sta->GetBuffer()->is_shared());
  CHECK_LT(index, sta->length());

  uint8_t* source = static_cast<uint8_t*>(sta->GetBuffer()->backing_store()) +
                    sta->byte_offset();

  if (sta->type() >= kExternalBigInt64Array) {
    Handle<BigInt> old_bigint;
    Handle<BigInt> new_bigint;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, old_bigint, BigInt::FromObject(isolate, old_value_obj));
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, new_bigint, BigInt::FromObject(isolate, new_value_obj));
    // SharedArrayBuffers are not detachable.
    CHECK_LT(index, sta->length());
    if (sta->type() == kExternalBigInt64Array) {
      return DoCompareExchange<int64_t>(isolate, source, index, old_bigint,
                                        new_bigint);
    }
    DCHECK(sta->type() == kExternalBigUint64Array);
    return DoCompareExchange<uint64_t>(isolate, source, index, old_bigint,
                                       new_bigint);
  }

  Handle<Object> old_value;
  Handle<Object> new_value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, old_value,
                                     Object::ToInteger(isolate, old_value_obj));
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, new_value,
                                     Object::ToInteger(isolate, new_value_obj));
  // SharedArrayBuffers are not detachable.
  CHECK_LT(index, sta->length());

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype)                  \
  case kExternal##Type##Array:                                         \
    return DoCompareExchange<ctype>(isolate, source, index, old_value, \
                                    new_value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      break;
  }

  UNREACHABLE();
}

// ES #sec-atomics.add
// Atomics.add( typedArray, index, value )
RUNTIME_FUNCTION(Runtime_AtomicsAdd) {
  return GetModifySetValueInBuffer<Add>(args, isolate);
}

// ES #sec-atomics.sub
// Atomics.sub( typedArray, index, value )
RUNTIME_FUNCTION(Runtime_AtomicsSub) {
  return GetModifySetValueInBuffer<Sub>(args, isolate);
}

// ES #sec-atomics.and
// Atomics.and( typedArray, index, value )
RUNTIME_FUNCTION(Runtime_AtomicsAnd) {
  return GetModifySetValueInBuffer<And>(args, isolate);
}

// ES #sec-atomics.or
// Atomics.or( typedArray, index, value )
RUNTIME_FUNCTION(Runtime_AtomicsOr) {
  return GetModifySetValueInBuffer<Or>(args, isolate);
}

// ES #sec-atomics.xor
// Atomics.xor( typedArray, index, value )
RUNTIME_FUNCTION(Runtime_AtomicsXor) {
  return GetModifySetValueInBuffer<Xor>(args, isolate);
}

#undef INTEGER_TYPED_ARRAYS

#else

RUNTIME_FUNCTION(Runtime_AtomicsLoad64) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsStore64) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsExchange) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsCompareExchange) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsAdd) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsSub) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsAnd) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsOr) { UNREACHABLE(); }

RUNTIME_FUNCTION(Runtime_AtomicsXor) { UNREACHABLE(); }

#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64
        // || V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_S390X

}  // namespace internal
}  // namespace v8
