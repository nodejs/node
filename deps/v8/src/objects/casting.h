// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CASTING_H_
#define V8_OBJECTS_CASTING_H_

/**
 * V8 Casting Guide
 * ================
 *
 * This file defines the casting infrastructure for V8's tagged objects.
 *
 * Untrusted objects (e.g. JSObject) live in the sandbox and their Map can be
 * tampered with. Trusted objects (e.g. BytecodeArray) live outside the
 * sandbox in trusted space.
 *
 *
 * 1. Release-Time Security Checks (Always-on CHECK or SBXCHECK)
 * -------------------------------------------------------------
 * These are required when type safety must be guaranteed at runtime under
 * the V8 Sandbox attacker model.
 *
 *         To      |  Untrusted        Trusted
 * From            |
 * --------------------------------------------------
 * Object          |  CheckedCast      <Impossible>*
 * TrustedObject   |  -                SbxCast
 *
 * * Note: A checked cast from a generic untrusted Object to a TrustedObject is
 *   impossible at runtime because untrusted pointers can point to arbitrary,
 *   unaligned memory, making a sound runtime type check impossible.
 *
 *
 * 2. Debug-Only Validation Checks (DCHECK-only or no-op)
 * ------------------------------------------------------
 * These are used when type safety is statically guaranteed, validated via
 * non-typesystem invariants, or when performance is critical.
 *
 *         To      |  Untrusted        Trusted
 * From            |
 * --------------------------------------------------
 * Object          |  Cast             TrustedCast
 * TrustedObject   |  -                TrustedCast
 * Uninitialized   |  UncheckedCast    -
 *
 * * Note: "Uninitialized" refers to partially-initialized objects (e.g., during
 *   deserialization or construction) where CastTraits checks would fail.
 *
 *
 * Guide to Cast Functions
 * =======================
 *
 * Cast<T>
 * -------
 * The default cast for untrusted objects. Only performs a debug-only DCHECK.
 * Since untrusted objects can be tampered with anyway, release-time checks
 * provide no additional security.
 *
 * TrustedCast<T>
 * --------------
 * Use this for trusted objects when the type is known through external means
 * but cannot be proven locally to the type system (e.g., trusted stack slots).
 * Like Cast<T>, it only performs a debug-only DCHECK.
 *
 * WARNING: Never use TrustedCast<T> on pointers loaded from untrusted/sandbox-
 * accessible fields, as an attacker can tamper with them and cause release-time
 * type confusion. Use SbxCast<T> or TryCast<T> instead.
 *
 * SbxCast<T>
 * ----------
 * Use this when a trusted pointer is obtained from an untrusted source AND the
 * TPT (Trusted Pointer Table) tag is shared among multiple types (1:N mapping).
 * It performs an always-on SBXCHECK to prevent type-confusion attacks.
 *
 * CheckedCast<T>
 * --------------
 * Performs an always-on CHECK at release time. Use this when casting untrusted
 * objects where correctness is absolutely critical.
 *
 * TryCast<T>
 * ----------
 * Performs a dynamic type check and returns a boolean. For trusted objects,
 * this is specifically used for 1:N mappings where multiple instance types
 * share the same TPT tag and you need to safely distinguish them at runtime.
 *
 * Note on 1:1 Mappings: For trusted objects with a 1:1 mapping between their
 * InstanceType and their TPT tag (like BytecodeArray), no explicit cast is
 * usually needed when reading from a field, as ReadTrustedPointerField<T>
 * already ensures the correct type.
 *
 * UncheckedCast<T>
 * ----------------
 * Performs a raw cast with zero compile-time or runtime checks. Used when the
 * object is partially initialized (so Is<T> checks would fail), or when the
 * type is already proven.
 *
 * GCSafeCast<T>
 * -------------
 * Performs a debug-only GC-aware type check. Used only within garbage
 * collection code paths.
 */

#include <type_traits>

#include "include/v8-source-location.h"
#include "src/base/logging.h"
#include "src/objects/tagged.h"
#include "src/sandbox/check.h"

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

class Oddball;

template <typename T>
concept NotGCedType = !std::is_base_of_v<HeapObject, T>;

// `Is<T>(value)` checks whether `value` is a tagged object of type `T`.
template <typename T, typename U>
inline bool Is(Tagged<U> value) {
  return CastTraits<T>::AllowFrom(value);
}
template <typename T>
inline bool Is(const HeapObject* obj) {
  return Is<T>(Tagged<HeapObject>(obj));
}
template <typename T, typename U>
inline bool Is(IndirectHandle<U> value);
template <typename T, typename U>
inline bool Is(DirectHandle<U> value);
// Is<T> checks for MaybeHandles are forbidden -- you need to first convert to
// a Handle with ToHandle or ToHandleChecked.
template <typename T, typename U>
bool Is(MaybeIndirectHandle<U> value) = delete;
template <typename T, typename U>
bool Is(MaybeDirectHandle<U> value) = delete;
// C++ versions for `Is<T>` that accept pointers and references.
template <typename To, typename From>
  requires NotGCedType<To>
bool Is(const From& value) {
  return CastTraits<std::remove_const_t<To>>::AllowFrom(value);
}
template <typename To, typename From>
  requires NotGCedType<To>
bool Is(From& value) {
  return Is<To>(const_cast<const From&>(value));
}
template <typename To, typename From>
  requires NotGCedType<To>
bool Is(const From* value) {
  return value && Is<To>(*value);
}
template <typename To, typename From>
  requires NotGCedType<To>
bool Is(From* value) {
  return value && Is<To>(*static_cast<const From*>(value));
}

// `UncheckedCast<T>(value)` casts `value` to a tagged object of type `T`,
// without checking the type of value.
template <typename To, typename From>
inline Tagged<To> UncheckedCast(Tagged<From> value) {
  return Tagged<To>(value.ptr());
}
template <typename To, typename From>
inline IndirectHandle<To> UncheckedCast(IndirectHandle<From> value);
template <typename To, typename From>
inline MaybeIndirectHandle<To> UncheckedCast(MaybeIndirectHandle<From> value);
template <typename To, typename From>
inline DirectHandle<To> UncheckedCast(DirectHandle<From> value);
template <typename To, typename From>
inline MaybeDirectHandle<To> UncheckedCast(MaybeDirectHandle<From> value);

// HasCastImplementation is a concept that checks for the existence of
// Is<To>(Holder<From>) and UncheckedCast<To>(Holder<From>).
template <template <typename> class Holder, typename To, typename From>
concept HasTryCastImplementation = requires(Holder<From> value) {
  { Is<To>(value) } -> std::same_as<bool>;
  { UncheckedCast<To>(value) } -> std::same_as<Holder<To>>;
};

template <typename Derived, typename Base>
concept HasCppRefTryCastImplementation =
    NotGCedType<Derived> && requires(Base& value) {
      { Is<Derived>(value) } -> std::same_as<bool>;
    };

template <typename Derived, typename Base>
concept HasCppPointerTryCastImplementation =
    NotGCedType<Derived> && requires(Base* value) {
      { Is<Derived>(value) } -> std::same_as<bool>;
    };

// `TryCast<T>(value, &out)` casts `value` to a tagged object of type `T` and
// writes the value to `out`, returning true if the cast succeeded and false if
// it failed.
template <typename To, typename From, template <typename> class Holder>
  requires HasTryCastImplementation<Holder, To, From>
inline bool TryCast(Holder<From> value, Holder<To>* out) {
  static_assert(!is_subtype_v<To, TrustedObject> ||
                    is_subtype_v<From, Union<Smi, Oddball, TrustedObject>>,
                "Type of untrusted objects cannot be checked reliably");
  if (!Is<To>(value)) return false;
  *out = UncheckedCast<To>(value);
  return true;
}

template <typename Derived, typename Base>
  requires HasCppRefTryCastImplementation<Derived, Base>
inline bool TryCast(Base& value, Derived** out) {
  if (!Is<Derived>(value)) return false;
  *out = static_cast<Derived*>(&value);
  return true;
}

template <typename Derived, typename Base>
  requires HasCppPointerTryCastImplementation<Derived, Base>
inline bool TryCast(Base* value, Derived** out) {
  if (!Is<Derived>(value)) return false;
  *out = static_cast<Derived*>(value);
  return true;
}

#ifdef DEBUG
template <typename T>
bool GCAwareObjectTypeCheck(Tagged<Object> object, const Heap* heap);
#endif  // DEBUG

// `GCSafeCast<T>(value)` casts `object` to a tagged object of type `T` and
// should be used when the cast can be called from within a GC. The case all
// includes a debug check that `object` is either a tagged object of type `T`,
// or one of few special cases possible during GC (see GCAwareObjectTypeCheck):
// 1) `object` was already evacuated and the forwarding address refers to a
//     tagged object of type `T`.
// 2) During Scavenger, `object` is a large object.
// 3) During a conservative Scavenger, `object` is a pinned object.
template <typename T>
Tagged<T> GCSafeCast(Tagged<Object> object, const Heap* heap) {
  DCHECK(GCAwareObjectTypeCheck<T>(object, heap));
  return UncheckedCast<T>(object);
}

// Check if a value holder is null -- used for Cast DCHECKs, so that null
// handles pass the check.
template <typename T, typename U>
inline bool NullOrIs(Tagged<U> value) {
  // Raw tagged values are not allowed to be null, just check Is<T>.
  return Is<T>(value);
}
template <typename T, typename U>
inline bool NullOrIs(IndirectHandle<U> value) {
  return value.is_null() || Is<T>(value);
}
template <typename T, typename U>
inline bool NullOrIs(DirectHandle<U> value) {
  return value.is_null() || Is<T>(value);
}
template <typename T, typename U>
inline bool NullOrIs(MaybeIndirectHandle<U> value) {
  IndirectHandle<U> handle;
  return !value.ToHandle(&handle) || Is<T>(handle);
}
template <typename T, typename U>
inline bool NullOrIs(MaybeDirectHandle<U> value) {
  DirectHandle<U> handle;
  return !value.ToHandle(&handle) || Is<T>(handle);
}
template <typename Derived, typename Base>
bool NullOrIs(Base& value) {
  return Is<Derived>(value);
}
template <typename Derived, typename Base>
bool NullOrIs(Base* value) {
  return !value || Is<Derived>(*value);
}

// HasCastImplementation is a concept that checks for the existence of
// NullOrIs<To>(Holder<From>) and UncheckedCast<To>(Holder<From>).
template <template <typename> class Holder, typename To, typename From>
concept HasCastImplementation = requires(Holder<From> value) {
  { NullOrIs<To>(value) } -> std::same_as<bool>;
  { UncheckedCast<To>(value) } -> std::same_as<Holder<To>>;
};

template <typename Derived, typename Base>
concept HasCppRefCastImplementation =
    NotGCedType<Derived> && requires(Base& value) {
      { Is<Derived>(value) } -> std::same_as<bool>;
    };

template <typename Derived, typename Base>
concept HasCppPointerCastImplementation =
    NotGCedType<Derived> && requires(Base* value) {
      { Is<Derived>(value) } -> std::same_as<bool>;
    };

// `TrustedCast<T>(value)` casts `value` to a tagged object of type `T`, only
// doing a debug check that `value` is a tagged object of type `T`. Down-casts
// to trusted objects are allowed for callers which already ensured their
// correctness. Null-valued holders are allowed.
//
// WARNING: This must NEVER be used on pointers loaded from untrusted/sandbox-
// accessible fields, as an attacker can tamper with them and cause release-time
// type confusion. Use SbxCast<T> or TryCast<T> instead for those cases.
template <typename To, typename From, template <typename> class Holder>
  requires HasCastImplementation<Holder, To, From>
inline Holder<To> TrustedCast(
    Holder<From> value, SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  // Casting pointers to in-sandbox objects is allowed even with an active
  // DisallowSandboxAccess scope (as casting itself doesn't access any memory).
  // However, in debug builds the DCHECK below will need to access the Map of
  // the object, so we need to temporarily enable sandbox access. If no
  // DisallowSandboxAccess scope is active, this is a no-op.
  DCHECK_WITH_SANDBOX_ACCESS_AND_MSG_AND_LOC(
      NullOrIs<To>(value), V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  return UncheckedCast<To>(value);
}

// `SbxCast<T>(value)` casts `value` to a tagged object of trusted type `T`,
// with a sandbox-only dynamic type check ensuring that `value` is a tagged
// object of type `T`. This is required for security when the object was
// obtained from an untrusted source AND the TPT tag is shared among multiple
// types (1:N mapping). Null-valued holders are allowed.
template <typename To, typename From, template <typename> class Holder>
  requires HasCastImplementation<Holder, To, From>
inline Holder<To> SbxCast(
    Holder<From> value, SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  // Under the attacker model of the sandbox we cannot trust the type of
  // in-sandbox objects for sandbox checks as these objects can be arbitrarily
  // tampered with.
  static_assert(is_subtype_v<To, TrustedObject>);
  static_assert(is_subtype_v<From, Union<Smi, Oddball, TrustedObject>>,
                "Type of untrusted objects cannot be checked reliably");
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<To>(value),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  SBXCHECK(NullOrIs<To>(value));
  return UncheckedCast<To>(value);
}

template <typename Derived, typename Base>
  requires HasCppRefCastImplementation<Derived, Base>
inline Derived& SbxCast(Base& from,
                        SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<Derived>(from),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  SBXCHECK(NullOrIs<Derived>(from));
  return static_cast<Derived&>(from);
}

template <typename Derived, typename Base>
  requires HasCppPointerCastImplementation<Derived, Base>
inline Derived* SbxCast(Base* from,
                        SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<Derived>(from),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  SBXCHECK(NullOrIs<Derived>(from));
  return static_cast<Derived*>(from);
}

// `CheckedCast<T>(value)` casts `value` to a tagged object of type `T`,
// with a always-on dynamic type check ensuring that `value` is a tagged object
// of type `T`. Null-valued holders are allowed.
template <typename To, typename From, template <typename> class Holder>
  requires HasCastImplementation<Holder, To, From>
inline Holder<To> CheckedCast(
    Holder<From> value, SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<To>(value),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  CHECK(NullOrIs<To>(value));
  return UncheckedCast<To>(value);
}

template <typename Derived, typename Base>
  requires HasCppRefCastImplementation<Derived, Base>
inline Derived& CheckedCast(
    Base& from, SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<Derived>(from),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  CHECK(NullOrIs<Derived>(from));
  return static_cast<Derived&>(from);
}

template <typename Derived, typename Base>
  requires HasCppPointerCastImplementation<Derived, Base>
inline Derived* CheckedCast(
    Base* from, SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<Derived>(from),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  CHECK(NullOrIs<Derived>(from));
  return static_cast<Derived*>(from);
}

// `Cast<T>(value)` casts `value` to a tagged object of type `T`, with a debug
// check that `value` is a tagged object of type `T`. Null-valued holders are
// allowed. Down-casts to trusted objects are not allowed to prevent mistakes.
template <typename To, typename From, template <typename> class Holder>
  requires HasCastImplementation<Holder, To, From>
inline Holder<To> Cast(Holder<From> value,
                       SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  static_assert(
      !is_subtype_v<To, TrustedObject> || is_subtype_v<From, To>,
      "Down-casting to trusted objects is verboten. Pick one of:\n"
      "* TryCast if the use is guarded by a test\n"
      "      if (Trusted t; TryCast(obj, &t)) { t->foo(); }\n"
      "* (Checked|Sbx)Cast if the object is expected to be of a certain type\n"
      "      Tagged<Trusted> t = SbxCast<Trusted>(obj);\n"
      "* TrustedCast if correct by construction (or other deprecated usage)\n"
      "      Tagged<Trusted> t = "
      "TrustedCast<Trusted>(TrustedPointerField::ReadTrustedPointerField<"
      "kTrustedTag>(obj, ...));\n");
  return TrustedCast<To>(value, loc);
}

// TODO(leszeks): Figure out a way to make these cast to actual pointers rather
// than Tagged.
template <typename To, typename From>
  requires std::is_base_of_v<HeapObject, From>
inline Tagged<To> UncheckedCast(const From* value) {
  return UncheckedCast<To>(Tagged(value));
}
template <typename To, typename From>
  requires std::is_base_of_v<HeapObject, From>
inline Tagged<To> TrustedCast(const From* value) {
  return TrustedCast<To>(Tagged(value));
}
template <typename To, typename From>
  requires std::is_base_of_v<HeapObject, From>
inline Tagged<To> CheckedCast(const From* value) {
  return CheckedCast<To>(Tagged(value));
}
template <typename To, typename From>
  requires std::is_base_of_v<HeapObject, From>
inline Tagged<To> SbxCast(const From* value) {
  return SbxCast<To>(Tagged(value));
}
template <typename To, typename From>
  requires std::is_base_of_v<HeapObject, From>
inline Tagged<To> Cast(const From* value,
                       SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  return Cast<To>(Tagged(value), loc);
}

template <typename Derived, typename Base>
  requires HasCppRefCastImplementation<Derived, Base>
inline Derived& Cast(Base& from,
                     SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<Derived>(from),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  return static_cast<Derived&>(from);
}

template <typename Derived, typename Base>
  requires HasCppPointerCastImplementation<Derived, Base>
inline Derived* Cast(Base* from,
                     SourceLocation loc = SourceLocation::CurrentIfDebug()) {
  DCHECK_WITH_MSG_AND_LOC(NullOrIs<Derived>(from),
                          V8_PRETTY_FUNCTION_VALUE_OR("Cast type check"), loc);
  return static_cast<Derived*>(from);
}

// `Is<T>(maybe_weak_value)` specialization for possible weak values and strong
// target `T`, that additionally first checks whether `maybe_weak_value` is
// actually a strong value (or a Smi, which can't be weak).
template <typename T, typename U>
inline bool Is(Tagged<MaybeWeak<U>> value) {
  // Cast from maybe weak to strong needs to be strong or smi.
  if constexpr (!is_maybe_weak_v<T>) {
    if (!value.IsStrongOrSmi()) return false;
    return CastTraits<T>::AllowFrom(Tagged<U>(value.ptr()));
  } else {
    // Dispatches to CastTraits<MaybeWeak<T>> below.
    return CastTraits<T>::AllowFrom(value);
  }
}
template <typename T, typename... U>
constexpr inline bool Is(Tagged<Union<U...>> value) {
  using UnionU = Union<U...>;
  if constexpr (is_weak_v<UnionU>) {
    return Is<T>(Tagged<Weak<HeapObject>>(value));
  } else if constexpr (is_maybe_weak_v<UnionU>) {
    // Cleared values are always ok.
    if (value.IsCleared()) return true;
    // TODO(leszeks): Skip Smi check for values that are known to not be Smi.
    if (value.IsSmi()) {
      return Is<T>(Tagged<Smi>(value.ptr()));
    }
    return Is<T>(MakeStrong(value));
  } else if constexpr (is_subtype_v<UnionU, HeapObject>) {
    return Is<T>(Tagged<HeapObject>(value));
  } else {
    static_assert(is_subtype_v<UnionU, Object>);
    return Is<T>(Tagged<Object>(value));
  }
}

// Specialization for weak cast targets, which first converts the incoming
// value to a strong reference and then checks if the cast to the strong T
// is allowed. Cleared weak references always return true.
template <typename T>
struct CastTraits<Weak<T>> {
  template <typename U>
  static bool AllowFrom(Tagged<U> value) {
    if constexpr (is_weak_v<U>) {
      return CastTraits<T>::AllowFrom(MakeStrong(value));
    }
    if constexpr (is_maybe_weak_v<U>) {
      // Cleared values are always ok.
      if (value.IsCleared()) return true;
      // TODO(leszeks): Skip Smi check for values that are known to not be Smi.
      if (value.IsSmi()) return false;
      return CastTraits<T>::AllowFrom(MakeStrong(value));
    } else {
      // Can't cast weak to non-weak.
      return false;
    }
  }
};

template <typename... T>
struct CastTraits<Union<T...>> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return (Is<T>(value) || ...);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return (Is<T>(value) || ...);
  }
};

template <>
struct CastTraits<Object> {
  static inline bool AllowFrom(Tagged<Object> value) { return true; }
};
template <>
struct CastTraits<Smi> {
  static inline bool AllowFrom(Tagged<Object> value) { return value.IsSmi(); }
  static inline bool AllowFrom(Tagged<HeapObject> value) { return false; }
};
template <>
struct CastTraits<HeapObject> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return value.IsHeapObject();
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) { return true; }
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_CASTING_H_
