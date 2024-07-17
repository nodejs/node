// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CASTING_H_
#define V8_OBJECTS_CASTING_H_

#include "src/objects/tagged.h"

namespace v8::internal {

// CastTraits<T> is a type trait that defines type checking behaviour for
// tagged object casting. The expected specialization is:
//
//     template<>
//     struct CastTraits<SomeObject> {
//       template<typename From>
//       static bool AllowFrom(Tagged<From> value) {
//         return IsSomeObject(value);
//       }
//     };
//
// or, likely, just specializations of AllowFrom for Object and HeapObject,
// under the assumption that the HeapObject implementation is the same for all
// HeapObjects and the Object implementation has additional overhead in Smi
// checks.
//
//     struct CastTraits<Object> {
//       static bool AllowFrom(Tagged<HeapObject> value) {
//         return IsSomeObject(value);
//       }
//       static bool AllowFrom(Tagged<Object> value) {
//         return IsSomeObject(value);
//       }
//     };
//
template <typename To>
struct CastTraits;

// `Is<T>(value)` checks whether `value` is a tagged object of type `T`.
template <typename T, typename U>
inline bool Is(Tagged<U> value) {
  return CastTraits<T>::AllowFrom(value);
}

// `Cast<T>(value)` casts `value` to a tagged object of type `T`, with a debug
// check that `value` is a tagged object of type `T`.
template <typename To, typename From>
inline Tagged<To> Cast(Tagged<From> value) {
  DCHECK(Is<To>(value));
  return UncheckedCast<To>(value);
}
template <typename To, typename From>
inline Handle<To> Cast(Handle<From> value);
template <typename To, typename From>
inline MaybeHandle<To> Cast(MaybeHandle<From> value);
#if V8_ENABLE_DIRECT_HANDLE_BOOL
template <typename To, typename From>
inline DirectHandle<To> Cast(DirectHandle<From> value);
template <typename To, typename From>
inline MaybeDirectHandle<To> Cast(MaybeDirectHandle<From> value);
#endif

// `UncheckedCast<T>(value)` casts `value` to a tagged object of type `T`,
// without checking the type of value.
template <typename To, typename From>
inline Tagged<To> UncheckedCast(Tagged<From> value) {
  return Tagged<To>(value.ptr());
}

// `Is<T>(maybe_weak_value)` specialization for possible weak values and strong
// target `T`, that additionally first checks whether `maybe_weak_value` is
// actually a strong value (or a Smi, which can't be weak).
template <typename T, typename U>
inline bool Is(Tagged<MaybeWeak<U>> value) {
  // TODO(leszeks): Add Is which supports weak conversion targets.
  static_assert(!is_maybe_weak_v<T>);
  // Cast from maybe weak to strong needs to be strong or smi.
  return value.IsStrongOrSmi() && Is<T>(Tagged<U>(value.ptr()));
}

}  // namespace v8::internal

#endif  // V8_OBJECTS_CASTING_H_
