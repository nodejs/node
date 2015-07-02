// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/conversions.h"
#include "src/runtime/runtime-utils.h"

// Implement Atomic accesses to SharedArrayBuffers as defined in the
// SharedArrayBuffer draft spec, found here
// https://docs.google.com/document/d/1NDGA_gZJ7M7w1Bh8S0AoDyEqwDdRh4uSoTPSNn77PFk

namespace v8 {
namespace internal {

namespace {

#if V8_CC_GNU

template <typename T>
inline T CompareExchangeSeqCst(T* p, T oldval, T newval) {
  (void)__atomic_compare_exchange_n(p, &oldval, newval, 0, __ATOMIC_SEQ_CST,
                                    __ATOMIC_SEQ_CST);
  return oldval;
}

template <typename T>
inline T LoadSeqCst(T* p) {
  T result;
  __atomic_load(p, &result, __ATOMIC_SEQ_CST);
  return result;
}

template <typename T>
inline void StoreSeqCst(T* p, T value) {
  __atomic_store_n(p, value, __ATOMIC_SEQ_CST);
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

template <typename T>
inline T ExchangeSeqCst(T* p, T value) {
  return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

#if ATOMICS_REQUIRE_LOCK_64_BIT

// We only need to implement the following functions, because the rest of the
// atomic operations only work on integer types, and the only 64-bit type is
// float64. Similarly, because the values are being bit_cast from double ->
// uint64_t, we don't need to implement these functions for int64_t either.

static base::LazyMutex atomic_mutex = LAZY_MUTEX_INITIALIZER;

inline uint64_t CompareExchangeSeqCst(uint64_t* p, uint64_t oldval,
                                      uint64_t newval) {
  base::LockGuard<base::Mutex> lock_guard(atomic_mutex.Pointer());
  uint64_t result = *p;
  if (result == oldval) *p = newval;
  return result;
}


inline uint64_t LoadSeqCst(uint64_t* p) {
  base::LockGuard<base::Mutex> lock_guard(atomic_mutex.Pointer());
  return *p;
}


inline void StoreSeqCst(uint64_t* p, uint64_t value) {
  base::LockGuard<base::Mutex> lock_guard(atomic_mutex.Pointer());
  *p = value;
}

#endif  // ATOMICS_REQUIRE_LOCK_64_BIT

#elif V8_CC_MSVC

#define _InterlockedCompareExchange32 _InterlockedCompareExchange
#define _InterlockedExchange32 _InterlockedExchange
#define _InterlockedExchangeAdd32 _InterlockedExchangeAdd
#define _InterlockedAnd32 _InterlockedAnd
#define _InterlockedOr32 _InterlockedOr
#define _InterlockedXor32 _InterlockedXor

#define INTEGER_TYPES(V)                           \
  V(int8_t, 8, char)                               \
  V(uint8_t, 8, char)                              \
  V(int16_t, 16, short)  /* NOLINT(runtime/int) */ \
  V(uint16_t, 16, short) /* NOLINT(runtime/int) */ \
  V(int32_t, 32, long)   /* NOLINT(runtime/int) */ \
  V(uint32_t, 32, long)  /* NOLINT(runtime/int) */ \
  V(int64_t, 64, LONGLONG)                         \
  V(uint64_t, 64, LONGLONG)

#define ATOMIC_OPS(type, suffix, vctype)                                       \
  inline type CompareExchangeSeqCst(volatile type* p, type oldval,             \
                                    type newval) {                             \
    return _InterlockedCompareExchange##suffix(                                \
        reinterpret_cast<volatile vctype*>(p), bit_cast<vctype>(newval),       \
        bit_cast<vctype>(oldval));                                             \
  }                                                                            \
  inline type LoadSeqCst(volatile type* p) { return *p; }                      \
  inline void StoreSeqCst(volatile type* p, type value) {                      \
    _InterlockedExchange##suffix(reinterpret_cast<volatile vctype*>(p),        \
                                 bit_cast<vctype>(value));                     \
  }                                                                            \
  inline type AddSeqCst(volatile type* p, type value) {                        \
    return _InterlockedExchangeAdd##suffix(                                    \
        reinterpret_cast<volatile vctype*>(p), bit_cast<vctype>(value));       \
  }                                                                            \
  inline type SubSeqCst(volatile type* p, type value) {                        \
    return _InterlockedExchangeAdd##suffix(                                    \
        reinterpret_cast<volatile vctype*>(p), -bit_cast<vctype>(value));      \
  }                                                                            \
  inline type AndSeqCst(volatile type* p, type value) {                        \
    return _InterlockedAnd##suffix(reinterpret_cast<volatile vctype*>(p),      \
                                   bit_cast<vctype>(value));                   \
  }                                                                            \
  inline type OrSeqCst(volatile type* p, type value) {                         \
    return _InterlockedOr##suffix(reinterpret_cast<volatile vctype*>(p),       \
                                  bit_cast<vctype>(value));                    \
  }                                                                            \
  inline type XorSeqCst(volatile type* p, type value) {                        \
    return _InterlockedXor##suffix(reinterpret_cast<volatile vctype*>(p),      \
                                   bit_cast<vctype>(value));                   \
  }                                                                            \
  inline type ExchangeSeqCst(volatile type* p, type value) {                   \
    return _InterlockedExchange##suffix(reinterpret_cast<volatile vctype*>(p), \
                                        bit_cast<vctype>(value));              \
  }
INTEGER_TYPES(ATOMIC_OPS)
#undef ATOMIC_OPS

#undef INTEGER_TYPES
#undef _InterlockedCompareExchange32
#undef _InterlockedExchange32
#undef _InterlockedExchangeAdd32
#undef _InterlockedAnd32
#undef _InterlockedOr32
#undef _InterlockedXor32

#else

#error Unsupported platform!

#endif

template <typename T>
T FromObject(Handle<Object> number);

template <>
inline uint32_t FromObject<uint32_t>(Handle<Object> number) {
  return NumberToUint32(*number);
}

template <>
inline int32_t FromObject<int32_t>(Handle<Object> number) {
  return NumberToInt32(*number);
}

template <>
inline float FromObject<float>(Handle<Object> number) {
  return static_cast<float>(number->Number());
}

template <>
inline double FromObject<double>(Handle<Object> number) {
  return number->Number();
}

template <typename T, typename F>
inline T ToAtomic(F from) {
  return static_cast<T>(from);
}

template <>
inline uint32_t ToAtomic<uint32_t, float>(float from) {
  return bit_cast<uint32_t, float>(from);
}

template <>
inline uint64_t ToAtomic<uint64_t, double>(double from) {
  return bit_cast<uint64_t, double>(from);
}

template <typename T, typename F>
inline T FromAtomic(F from) {
  return static_cast<T>(from);
}

template <>
inline float FromAtomic<float, uint32_t>(uint32_t from) {
  return bit_cast<float, uint32_t>(from);
}

template <>
inline double FromAtomic<double, uint64_t>(uint64_t from) {
  return bit_cast<double, uint64_t>(from);
}

template <typename T>
inline Object* ToObject(Isolate* isolate, T t);

template <>
inline Object* ToObject<int8_t>(Isolate* isolate, int8_t t) {
  return Smi::FromInt(t);
}

template <>
inline Object* ToObject<uint8_t>(Isolate* isolate, uint8_t t) {
  return Smi::FromInt(t);
}

template <>
inline Object* ToObject<int16_t>(Isolate* isolate, int16_t t) {
  return Smi::FromInt(t);
}

template <>
inline Object* ToObject<uint16_t>(Isolate* isolate, uint16_t t) {
  return Smi::FromInt(t);
}

template <>
inline Object* ToObject<int32_t>(Isolate* isolate, int32_t t) {
  return *isolate->factory()->NewNumber(t);
}

template <>
inline Object* ToObject<uint32_t>(Isolate* isolate, uint32_t t) {
  return *isolate->factory()->NewNumber(t);
}

template <>
inline Object* ToObject<float>(Isolate* isolate, float t) {
  return *isolate->factory()->NewNumber(t);
}

template <>
inline Object* ToObject<double>(Isolate* isolate, double t) {
  return *isolate->factory()->NewNumber(t);
}

template <typename T>
struct FromObjectTraits {};

template <>
struct FromObjectTraits<int8_t> {
  typedef int32_t convert_type;
  typedef int8_t atomic_type;
};

template <>
struct FromObjectTraits<uint8_t> {
  typedef uint32_t convert_type;
  typedef uint8_t atomic_type;
};

template <>
struct FromObjectTraits<int16_t> {
  typedef int32_t convert_type;
  typedef int16_t atomic_type;
};

template <>
struct FromObjectTraits<uint16_t> {
  typedef uint32_t convert_type;
  typedef uint16_t atomic_type;
};

template <>
struct FromObjectTraits<int32_t> {
  typedef int32_t convert_type;
  typedef int32_t atomic_type;
};

template <>
struct FromObjectTraits<uint32_t> {
  typedef uint32_t convert_type;
  typedef uint32_t atomic_type;
};

template <>
struct FromObjectTraits<float> {
  typedef float convert_type;
  typedef uint32_t atomic_type;
};

template <>
struct FromObjectTraits<double> {
  typedef double convert_type;
  typedef uint64_t atomic_type;
};


template <typename T>
inline Object* DoCompareExchange(Isolate* isolate, void* buffer, size_t index,
                                 Handle<Object> oldobj, Handle<Object> newobj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type oldval = ToAtomic<atomic_type>(FromObject<convert_type>(oldobj));
  atomic_type newval = ToAtomic<atomic_type>(FromObject<convert_type>(newobj));
  atomic_type result = CompareExchangeSeqCst(
      static_cast<atomic_type*>(buffer) + index, oldval, newval);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoLoad(Isolate* isolate, void* buffer, size_t index) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  atomic_type result = LoadSeqCst(static_cast<atomic_type*>(buffer) + index);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoStore(Isolate* isolate, void* buffer, size_t index,
                       Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  StoreSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return *obj;
}


template <typename T>
inline Object* DoAdd(Isolate* isolate, void* buffer, size_t index,
                     Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  atomic_type result =
      AddSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoSub(Isolate* isolate, void* buffer, size_t index,
                     Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  atomic_type result =
      SubSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoAnd(Isolate* isolate, void* buffer, size_t index,
                     Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  atomic_type result =
      AndSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoOr(Isolate* isolate, void* buffer, size_t index,
                    Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  atomic_type result =
      OrSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoXor(Isolate* isolate, void* buffer, size_t index,
                     Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  atomic_type result =
      XorSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


template <typename T>
inline Object* DoExchange(Isolate* isolate, void* buffer, size_t index,
                          Handle<Object> obj) {
  typedef typename FromObjectTraits<T>::atomic_type atomic_type;
  typedef typename FromObjectTraits<T>::convert_type convert_type;
  atomic_type value = ToAtomic<atomic_type>(FromObject<convert_type>(obj));
  atomic_type result =
      ExchangeSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return ToObject<T>(isolate, FromAtomic<T>(result));
}


// Uint8Clamped functions

uint8_t ClampToUint8(int32_t value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return value;
}


inline Object* DoCompareExchangeUint8Clamped(Isolate* isolate, void* buffer,
                                             size_t index,
                                             Handle<Object> oldobj,
                                             Handle<Object> newobj) {
  typedef int32_t convert_type;
  typedef uint8_t atomic_type;
  atomic_type oldval = ClampToUint8(FromObject<convert_type>(oldobj));
  atomic_type newval = ClampToUint8(FromObject<convert_type>(newobj));
  atomic_type result = CompareExchangeSeqCst(
      static_cast<atomic_type*>(buffer) + index, oldval, newval);
  return ToObject<uint8_t>(isolate, FromAtomic<uint8_t>(result));
}


inline Object* DoStoreUint8Clamped(Isolate* isolate, void* buffer, size_t index,
                                   Handle<Object> obj) {
  typedef int32_t convert_type;
  typedef uint8_t atomic_type;
  atomic_type value = ClampToUint8(FromObject<convert_type>(obj));
  StoreSeqCst(static_cast<atomic_type*>(buffer) + index, value);
  return *obj;
}


#define DO_UINT8_CLAMPED_OP(name, op)                                        \
  inline Object* Do##name##Uint8Clamped(Isolate* isolate, void* buffer,      \
                                        size_t index, Handle<Object> obj) {  \
    typedef int32_t convert_type;                                            \
    typedef uint8_t atomic_type;                                             \
    atomic_type* p = static_cast<atomic_type*>(buffer) + index;              \
    convert_type operand = FromObject<convert_type>(obj);                    \
    atomic_type expected;                                                    \
    atomic_type result;                                                      \
    do {                                                                     \
      expected = *p;                                                         \
      result = ClampToUint8(static_cast<convert_type>(expected) op operand); \
    } while (CompareExchangeSeqCst(p, expected, result) != expected);        \
    return ToObject<uint8_t>(isolate, expected);                             \
  }

DO_UINT8_CLAMPED_OP(Add, +)
DO_UINT8_CLAMPED_OP(Sub, -)
DO_UINT8_CLAMPED_OP(And, &)
DO_UINT8_CLAMPED_OP(Or, | )
DO_UINT8_CLAMPED_OP(Xor, ^)

#undef DO_UINT8_CLAMPED_OP


inline Object* DoExchangeUint8Clamped(Isolate* isolate, void* buffer,
                                      size_t index, Handle<Object> obj) {
  typedef int32_t convert_type;
  typedef uint8_t atomic_type;
  atomic_type* p = static_cast<atomic_type*>(buffer) + index;
  atomic_type result = ClampToUint8(FromObject<convert_type>(obj));
  atomic_type expected;
  do {
    expected = *p;
  } while (CompareExchangeSeqCst(p, expected, result) != expected);
  return ToObject<uint8_t>(isolate, expected);
}


}  // anonymous namespace

// Duplicated from objects.h
// V has parameters (Type, type, TYPE, C type, element_size)
#define INTEGER_TYPED_ARRAYS(V)          \
  V(Uint8, uint8, UINT8, uint8_t, 1)     \
  V(Int8, int8, INT8, int8_t, 1)         \
  V(Uint16, uint16, UINT16, uint16_t, 2) \
  V(Int16, int16, INT16, int16_t, 2)     \
  V(Uint32, uint32, UINT32, uint32_t, 4) \
  V(Int32, int32, INT32, int32_t, 4)


RUNTIME_FUNCTION(Runtime_AtomicsCompareExchange) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(oldobj, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(newobj, 3);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoCompareExchange<ctype>(isolate, buffer, index, oldobj, newobj);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalFloat32Array:
      return DoCompareExchange<float>(isolate, buffer, index, oldobj, newobj);

    case kExternalFloat64Array:
      return DoCompareExchange<double>(isolate, buffer, index, oldobj, newobj);

    case kExternalUint8ClampedArray:
      return DoCompareExchangeUint8Clamped(isolate, buffer, index, oldobj,
                                           newobj);

    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsLoad) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoLoad<ctype>(isolate, buffer, index);

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsStore) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoStore<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalFloat32Array:
      return DoStore<float>(isolate, buffer, index, value);

    case kExternalFloat64Array:
      return DoStore<double>(isolate, buffer, index, value);

    case kExternalUint8ClampedArray:
      return DoStoreUint8Clamped(isolate, buffer, index, value);

    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsAdd) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoAdd<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalUint8ClampedArray:
      return DoAddUint8Clamped(isolate, buffer, index, value);

    case kExternalFloat32Array:
    case kExternalFloat64Array:
    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsSub) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoSub<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalUint8ClampedArray:
      return DoSubUint8Clamped(isolate, buffer, index, value);

    case kExternalFloat32Array:
    case kExternalFloat64Array:
    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsAnd) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoAnd<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalUint8ClampedArray:
      return DoAndUint8Clamped(isolate, buffer, index, value);

    case kExternalFloat32Array:
    case kExternalFloat64Array:
    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsOr) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoOr<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalUint8ClampedArray:
      return DoOrUint8Clamped(isolate, buffer, index, value);

    case kExternalFloat32Array:
    case kExternalFloat64Array:
    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsXor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoXor<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalUint8ClampedArray:
      return DoXorUint8Clamped(isolate, buffer, index, value);

    case kExternalFloat32Array:
    case kExternalFloat64Array:
    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsExchange) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, sta, 0);
  CONVERT_SIZE_ARG_CHECKED(index, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);
  RUNTIME_ASSERT(sta->GetBuffer()->is_shared());
  RUNTIME_ASSERT(index < NumberToSize(isolate, sta->length()));

  void* buffer = sta->GetBuffer()->backing_store();

  switch (sta->type()) {
#define TYPED_ARRAY_CASE(Type, typeName, TYPE, ctype, size) \
  case kExternal##Type##Array:                              \
    return DoExchange<ctype>(isolate, buffer, index, value);

    INTEGER_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case kExternalUint8ClampedArray:
      return DoExchangeUint8Clamped(isolate, buffer, index, value);

    case kExternalFloat32Array:
    case kExternalFloat64Array:
    default:
      break;
  }

  UNREACHABLE();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AtomicsIsLockFree) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(size, 0);
  uint32_t usize = NumberToUint32(*size);

  return Runtime::AtomicIsLockFree(usize) ? isolate->heap()->true_value()
                                          : isolate->heap()->false_value();
}
}
}  // namespace v8::internal
