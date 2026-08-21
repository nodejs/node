// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_H_
#define V8_OBJECTS_TAGGED_H_

#include <type_traits>

#include "include/v8config.h"
#include "src/common/globals.h"
#include "src/objects/tagged-impl.h"
#include "src/objects/union.h"

namespace v8 {
namespace internal {

class BigInt;
class FieldType;
class HeapObject;
class HeapNumber;
class HeapObjectLayout;
class TrustedObject;
class ExposedTrustedObject;
class Object;
class TaggedIndex;
class Smi;

// Tagged<T> represents an uncompressed V8 tagged pointer.
//
// The tagged pointer is a pointer-sized value with a tag in the LSB. The value
// is either:
//
//   * A small integer (Smi), shifted right, with the tag set to 0
//   * A strong pointer to an object on the V8 heap, with the tag set to 01
//   * A weak pointer to an object on the V8 heap, with the tag set to 11
//   * A cleared weak pointer, with the value 11
//
// The exact encoding differs depending on 32- vs 64-bit architectures, and in
// the latter case, whether or not pointer compression is enabled.
//
// On 32-bit architectures, this is:
//             |----- 32 bits -----|
// Pointer:    |______address____w1|
//    Smi:     |____int31_value___0|
//
// On 64-bit architectures with pointer compression:
//             |----- 32 bits -----|----- 32 bits -----|
// Pointer:    |________base_______|______offset_____w1|
//    Smi:     |......garbage......|____int31_value___0|
//
// On 64-bit architectures without pointer compression:
//             |----- 32 bits -----|----- 32 bits -----|
// Pointer:    |________________address______________w1|
//    Smi:     |____int32_value____|00...............00|
//
// where `w` is the "weak" bit.
//
// We specialise Tagged separately for Object, Smi and HeapObject, and then all
// other types T, so that:
//
//                    Tagged<Object> -> StrongTaggedBase
//                       Tagged<Smi> -> StrongTaggedBase
//   Tagged<T> -> Tagged<HeapObject> -> StrongTaggedBase
//
// We also specialize it separately for Weak types, with a parallel
// hierarchy:
//
//                          Tagged<Weak<Object>> -> WeakTaggedBase
//                             Tagged<Weak<Smi>> -> WeakTaggedBase
//   Tagged<Weak<T>> -> Tagged<Weak<HeapObject>> -> WeakTaggedBase
//   Tagged<Weak<T>> -> Tagged<Weak<HeapObject>> -> WeakTaggedBase
template <typename T>
class Tagged;

// Weak<T> represents a reference to T that is weak.
//
// Weak doesn't really exist by itself, but is rather a sentinel type for
// templates on tagged interfaces (like Tagged). For example, where Tagged<T>
// represents a strong reference to T, Tagged<Weak<T>> represents a
// potentially weak reference to T, and it is the responsibility of the Tagged
// interface to provide some mechanism (likely template specialization) to
// distinguish between the two and provide accessors to the T reference itself
// (which will always be strong).
template <typename T>
class Weak {
 public:
  // Smis can't be weak.
  static_assert(!std::is_same_v<T, Smi>);
  // Generic Objects can't be weak, use a Union with a Smi instead.
  static_assert(!std::is_same_v<T, Object>);
  // "Weak" should be inside unions, not outside of them.
  static_assert(!is_union_v<T>);

  using strong_type = T;
};

template <typename T>
struct is_weak : public std::false_type {};
template <typename T>
struct is_weak<Weak<T>> : public std::true_type {};
template <typename... T>
struct is_weak<Union<T...>> : public std::conjunction<is_weak<T>...> {};
template <typename T>
static constexpr bool is_weak_v = is_weak<T>::value;

template <typename T>
struct is_maybe_weak : public std::false_type {};
template <typename T>
struct is_maybe_weak<Weak<T>> : public std::true_type {};
template <typename... T>
struct is_maybe_weak<Union<T...>>
    : public std::disjunction<is_maybe_weak<T>...> {};
template <typename T>
static constexpr bool is_maybe_weak_v = is_maybe_weak<T>::value;

namespace detail {
template <typename T>
struct weak_of_helper {
  using type = Weak<T>;
};
template <>
struct weak_of_helper<Smi> {
  using type = Smi;
};
template <>
struct weak_of_helper<Object> {
  using type = UnionOf<Smi, HeapObject, Weak<HeapObject>>;
};
template <typename T>
struct weak_of_helper<Weak<T>> {
  using type = Weak<T>;
};
template <typename... Ts>
struct weak_of_helper<Union<Ts...>> {
  using type = UnionOf<typename weak_of_helper<Ts>::type...>;
};
}  // namespace detail
template <typename T>
using WeakOf = typename detail::weak_of_helper<T>::type;

namespace detail {
template <typename T>
struct strong_of_helper {
  using type = T;
};
template <>
struct strong_of_helper<Smi> {
  using type = Smi;
};
template <typename T>
struct strong_of_helper<Weak<T>> {
  using type = T;
};
template <>
struct strong_of_helper<MaybeObject> {
  using type = Object;
};
template <typename... Ts>
struct strong_of_helper<Union<Ts...>> {
  using type = UnionOf<typename strong_of_helper<Ts>::type...>;
};
}  // namespace detail
template <typename T>
using StrongOf = typename detail::strong_of_helper<T>::type;

// ClearedWeakValue is a sentinel type for cleared weak values.
class ClearedWeakValue {};

// Convert a reference to T into a definitely weak reference to T.
template <typename T>
inline Tagged<WeakOf<T>> MakeWeak(Tagged<T> value);

// Convert a Smi or reference to T into a Smi or weak reference to T.
template <typename T>
inline Tagged<WeakOf<T>> MakeWeakOrSmi(Tagged<T> value);

// Convert a (maybe) weak reference to T into a strong reference to T.
template <typename T>
inline Tagged<StrongOf<T>> MakeStrong(Tagged<T> value);

// Base class for all Tagged<T> classes.
using StrongTaggedBase = TaggedImpl<HeapObjectReferenceType::STRONG, Address>;
using WeakTaggedBase = TaggedImpl<HeapObjectReferenceType::WEAK, Address>;

// Types which provide both a legacy Foo as well as a new-style FooLayout class.
#define LAYOUT_TYPES(V) V(HeapObject)

// Forward declarations for is_subtype.
class Struct;
class FixedArrayBase;
class FixedArray;
class FixedDoubleArray;
class ByteArray;
class NameDictionary;
class NumberDictionary;
class OrderedHashMap;
class OrderedHashSet;
class OrderedNameDictionary;
class ScriptContextTable;
class ArrayList;
class SloppyArgumentsElements;

namespace detail {
template <typename Derived, typename Base>
consteval bool is_subtype_helper();
}

// `is_subtype<Derived, Base>::value` is true when Derived is a subtype of Base
// according to our object hierarchy. In particular, Smi is considered a
// subtype of Object.
template <typename Derived, typename Base>
concept SubtypeOf = detail::is_subtype_helper<Derived, Base>();
template <typename Derived, typename Base>
static constexpr bool is_subtype_v = detail::is_subtype_helper<Derived, Base>();
template <typename Derived, typename Base>
struct is_subtype : public std::integral_constant<
                        bool, detail::is_subtype_helper<Derived, Base>()> {};

namespace detail {

template <typename T>
struct layout_type {
  using type = T;
};
#define SPECIALIZE_TO_LAYOUT_TYPE(Name) \
  template <>                           \
  struct layout_type<Name> {            \
    using type = Name##Layout;          \
  };
LAYOUT_TYPES(SPECIALIZE_TO_LAYOUT_TYPE)
#undef SPECIALIZE_TO_LAYOUT_TYPE

template <typename T>
struct normalize_type {
  using type = T;
};
template <>
struct normalize_type<Object> {
  using type = Union<Smi, HeapObject>;
};
template <>
struct normalize_type<FieldType> {
  using type = Union<Smi, Map>;
};
#define SPECIALIZE_TO_LAYOUT_TYPE(Name) \
  template <>                           \
  struct normalize_type<Name##Layout> { \
    using type = Name;                  \
  };
LAYOUT_TYPES(SPECIALIZE_TO_LAYOUT_TYPE)
#undef SPECIALIZE_TO_LAYOUT_TYPE

template <typename D, typename B>
consteval bool is_subtype_helper() {
  using std::is_base_of_v;
  using std::is_same_v;

  // Normalize Union-like types and layout types to a canonical representation.
  using Derived = typename normalize_type<D>::type;
  using Base = typename normalize_type<B>::type;

  if constexpr (is_same_v<Derived, Base>) {
    // Exact Identity Match
    return true;
  } else if constexpr (is_same_v<Derived, TaggedIndex> &&
                       (is_same_v<Base,
                                  typename normalize_type<Object>::type> ||
                        is_same_v<Base, typename normalize_type<
                                            MaybeObject>::type>)) {
    // Extra special handling of TaggedIndex as a subclass of (normalized)
    // Object and MaybeObject. Has to be checked before the unions are checked.
    // TODO(leszeks): Make this either part of those unions, or fix the uses
    // which try to cast this.
    return true;
  } else if constexpr (is_union_v<Derived>) {
    // If Derived is a union, ALL of its members must be a subtype of Base.
    // Unpack it by passing it to a templated lambda.
    return []<typename... Ts>(std::type_identity<Union<Ts...>>) consteval {
      return (... && is_subtype_helper<Ts, Base>());
    }(std::type_identity<Derived>{});
  } else if constexpr (is_union_v<Base>) {
    // If Base is a union, Derived must be a subtype of AT LEAST ONE member.
    return []<typename... Ts>(std::type_identity<Union<Ts...>>) consteval {
      return (... || is_subtype_helper<Derived, Ts>());
    }(std::type_identity<Base>{});
  } else if constexpr (is_same_v<Derived, Smi> || is_same_v<Base, Smi> ||
                       is_same_v<Derived, TaggedIndex> ||
                       is_same_v<Base, TaggedIndex>) {
    // Smi and TaggedIndex must match on exact identity or union.
    return false;
  } else if constexpr (is_weak_v<Base>) {
    // Base = Weak<T>

    if constexpr (is_same_v<Derived, ClearedWeakValue>) {
      // ClearedValue is always a subtype of Weak<T>
      return true;
    } else if constexpr (is_weak_v<Derived>) {
      // Weak<T> < Weak<U> <=> T < U
      return is_subtype_helper<typename Derived::strong_type,
                               typename Base::strong_type>();
    } else {
      // non-weak < Weak<U> is always false.
      return false;
    }
  } else if constexpr (is_weak_v<Derived>) {
    // Weak<T> cannot be a subtype of a non-weak Base (unless
    // matched exactly earlier by a union)
    return false;
  } else if constexpr (is_same_v<Base, FixedArrayBase>) {
    // FixedArrayBase Hierarchy Collapse
    return is_same_v<D, FixedArray> || is_same_v<D, FixedDoubleArray> ||
           is_same_v<D, ByteArray> || is_same_v<D, NameDictionary> ||
           is_same_v<D, NumberDictionary> || is_same_v<D, GlobalDictionary> ||
           is_same_v<D, OrderedHashMap> || is_same_v<D, OrderedHashSet> ||
           is_same_v<D, OrderedNameDictionary> ||
           is_same_v<D, ScriptContextTable> || is_same_v<D, ArrayList> ||
           is_same_v<D, SloppyArgumentsElements>;
  } else if constexpr (!is_same_v<Base, typename layout_type<Base>::type>) {
    // Handle layout types.
    return is_base_of_v<Base, Derived> ||
           is_base_of_v<typename layout_type<Base>::type, Derived>;
  } else {
    // Fallback to base_of.
    return is_base_of_v<Base, Derived>;
  }
}
}  // namespace detail

static_assert(is_subtype_v<Smi, Object>);
static_assert(is_subtype_v<HeapObject, Object>);
static_assert(is_subtype_v<HeapObject, HeapObject>);
static_assert(is_subtype_v<Smi, MaybeObject>);
static_assert(
    is_subtype_v<Union<HeapObject, Weak<HeapObject>, Smi>, MaybeObject>);
static_assert(!is_subtype_v<WeakOf<Object>, Object>);
static_assert(is_subtype_v<
              Object, Union<Smi, HeapObject, Weak<HeapObject>, TaggedIndex>>);
static_assert(is_subtype_v<Object, MaybeObject>);
static_assert(is_subtype_v<TaggedIndex, Object>);
static_assert(is_subtype_v<Union<HeapObject, TaggedIndex>, Object>);

// `is_taggable<T>::value` is true when T is a valid type for Tagged. This means
// being a subtype of Smi, TaggedIndex, or a weak/strong HeapObject. Include
// Object in the list because it's special.
template <typename T>
using is_taggable =
    is_subtype<T,
               Union<Smi, TaggedIndex, Object, HeapObject, Weak<HeapObject>>>;
template <typename T>
static constexpr bool is_taggable_v = is_taggable<T>::value;

// `is_castable<From, To>::value` is true when you can use `::cast` to cast from
// From to To. This means an upcast or downcast, which in practice means
// checking `is_subtype` symmetrically.
template <typename From, typename To>
using is_castable =
    std::disjunction<is_subtype<To, From>, is_subtype<From, To>>;
template <typename From, typename To>
static constexpr bool is_castable_v = is_castable<From, To>::value;

// TODO(leszeks): Remove this once there are no more conversions between
// Tagged<Foo> and Foo.
static constexpr bool kTaggedCanConvertToRawObjects = true;

namespace detail {

// {TaggedOperatorArrowRef} is returned by {Tagged::operator->}. It should never
// be stored anywhere or used in any other code; no one should ever have to
// spell out {TaggedOperatorArrowRef} in code. Its only purpose is to be
// dereferenced immediately by "operator-> chaining". Returning the address of
// the field is valid because this objects lifetime only ends at the end of the
// full statement.
template <typename T>
class TaggedOperatorArrowRef {
 public:
  V8_INLINE constexpr T* operator->() V8_LIFETIME_BOUND { return &object_; }

 private:
  friend class Tagged<T>;
  V8_INLINE constexpr explicit TaggedOperatorArrowRef(T object)
      : object_(object) {}
  T object_;
};

template <typename T>
struct BaseForTagged {
  using type = Tagged<HeapObject>;
};

template <typename T>
struct BaseForTagged<Weak<T>> {
  using type = Tagged<Weak<HeapObject>>;
};

template <typename... T>
struct BaseForTagged<Union<T...>> {
  template <typename U>
  using is_non_heap_object =
      std::disjunction<std::is_same<U, Smi>, std::is_same<U, Object>,
                       std::is_same<U, TaggedIndex>,
                       std::is_same<U, FieldType>>;

  using type = std::conditional_t<
      std::disjunction_v<is_maybe_weak<T>...>, WeakTaggedBase,
      std::conditional_t<std::disjunction_v<is_non_heap_object<T>...>,
                         Tagged<Object>, Tagged<HeapObject>>>;
};

// FieldType is special, since it can be Smi or Map. It could probably even be
// its own specialization, to avoid exposing an operator->.
template <>
struct BaseForTagged<FieldType> {
  using type = Tagged<Object>;
};

}  // namespace detail

// Specialization for Object, where it's unknown whether this is a Smi or a
// HeapObject.
template <>
class Tagged<Object> : public StrongTaggedBase {
 public:
  // Allow Tagged<Object> to be created from any address.
  V8_INLINE constexpr explicit Tagged(Address o) : StrongTaggedBase(o) {}

  // Allow explicit uninitialized initialization.
  // TODO(leszeks): Consider zapping this instead, since it's odd that
  // Tagged<Object> implicitly initialises to Smi::zero().
  V8_INLINE constexpr Tagged() : StrongTaggedBase(kNullAddress) {}

  // Allow implicit conversion from const HeapObjectLayout* to Tagged<Object>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const HeapObjectLayout* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {}

  // Implicit conversion for subclasses -- all classes are subclasses of Object,
  // so allow all tagged pointers.
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(StrongTaggedBase other)
      : StrongTaggedBase(other.ptr()) {}
  V8_INLINE constexpr Tagged& operator=(StrongTaggedBase other) {
    return *this = Tagged(other);
  }
};

// Specialization for Smi disallowing any implicit creation or access via ->,
// but offering instead a cast from Object and an int32_t value() method.
template <>
class Tagged<Smi> : public StrongTaggedBase {
 public:
  V8_INLINE constexpr Tagged() = default;
  V8_INLINE constexpr explicit Tagged(Address ptr) : StrongTaggedBase(ptr) {}

  // No implicit conversions from other tagged pointers.

  V8_INLINE constexpr bool IsHeapObject() const { return false; }
  V8_INLINE constexpr bool IsSmi() const { return true; }

  V8_INLINE constexpr int32_t value() const {
    return Internals::SmiValue(ptr());
  }
};

// Specialization for TaggedIndex disallowing any implicit creation or access
// via ->, but offering instead a cast from Object and an intptr_t value()
// method.
template <>
class Tagged<TaggedIndex> : public StrongTaggedBase {
 public:
  V8_INLINE constexpr Tagged() = default;
  V8_INLINE constexpr explicit Tagged(Address ptr) : StrongTaggedBase(ptr) {}

  // No implicit conversions from other tagged pointers.

  V8_INLINE constexpr bool IsHeapObject() const { return false; }
  V8_INLINE constexpr bool IsSmi() const { return true; }

  // Returns the integer value.
  V8_INLINE constexpr intptr_t value() const {
    // Truncate and shift down (requires >> to be sign extending).
    return static_cast<intptr_t>(ptr()) >> kSmiTagSize;
  }

  // Implicit conversions to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(TaggedIndex raw);

 private:
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<TaggedIndex>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<TaggedIndex>;
#endif
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;
};

// Specialization for HeapObject, to group together functions shared between all
// HeapObjects
template <>
class Tagged<HeapObject> : public StrongTaggedBase {
  using Base = StrongTaggedBase;

 public:
  V8_INLINE constexpr Tagged() = default;
  // Allow implicit conversion from const HeapObjectLayout* to
  // Tagged<HeapObject>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const HeapObjectLayout* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {}

  // Implicit conversion for subclasses.
  template <typename U>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other)
    requires(is_subtype_v<U, HeapObject>)
  {
    return *this = Tagged(other);
  }

  // Implicit conversion for subclasses.
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other)
    requires(is_subtype_v<U, HeapObject>)
      : Base(other) {}

  V8_INLINE constexpr HeapObject operator*() const;
  V8_INLINE constexpr detail::TaggedOperatorArrowRef<HeapObject> operator->()
      const;

  V8_INLINE constexpr bool is_null() const {
    return static_cast<Tagged_t>(this->ptr()) ==
           static_cast<Tagged_t>(kNullAddress);
  }

  constexpr V8_INLINE bool IsHeapObject() const {
    DCHECK_IMPLIES(!is_null(), TaggedImpl::IsHeapObject());
    return !is_null();
  }
  constexpr V8_INLINE bool IsSmi() const {
    // Null values overlap with Smi zero, so return "true" here for now.
    // TODO(leszeks): Consider either making callers check for nullness instead
    // of Sminess, or even introducing a "nullable" concept.
    DCHECK_IMPLIES(!is_null(), !TaggedImpl::IsSmi());
    return is_null();
  }

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U>
  // NOLINTNEXTLINE
  constexpr Tagged(U raw)
    requires(std::is_base_of_v<HeapObject, U>)
      : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U>
  static constexpr Tagged<HeapObject> cast(U other) {
    static_assert(kTaggedCanConvertToRawObjects);
    return Cast<HeapObject>(Tagged<U>(other));
  }

  Address address() const { return this->ptr() - kHeapObjectTag; }

 protected:
  V8_INLINE constexpr explicit Tagged(Address ptr) : Base(ptr) {}

 private:
  friend class HeapObject;
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<HeapObject>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<HeapObject>;
#endif
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;
  template <typename TFieldType, typename CompressionScheme>
  friend class TaggedMember;
  template <typename To, typename From>
  friend inline Tagged<To> UncheckedCast(Tagged<From> value);

  template <typename U>
  friend Tagged<StrongOf<U>> MakeStrong(Tagged<U> value);

  V8_INLINE constexpr HeapObject ToRawPtr() const;
};

static_assert(Tagged<HeapObject>().is_null());

// Specialization for Weak<HeapObject>, to group together functions shared
// between all HeapObjects
template <>
class Tagged<Weak<HeapObject>> : public WeakTaggedBase {
  using Base = WeakTaggedBase;

 public:
  V8_INLINE constexpr Tagged() = default;
  // Allow implicit conversion from const HeapObjectLayout* to
  // Tagged<HeapObject>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const HeapObjectLayout* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {}

  // Implicit conversion for subclasses.
  template <typename U>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other)
    requires(is_subtype_v<U, Weak<HeapObject>>)
  {
    return *this = Tagged(other);
  }

  // Implicit conversion for subclasses.
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other)
    requires(is_subtype_v<U, Weak<HeapObject>>)
      : Base(other.ptr()) {}

  V8_INLINE constexpr bool is_null() const {
    return static_cast<Tagged_t>(this->ptr()) ==
           static_cast<Tagged_t>(kNullAddress);
  }

  constexpr V8_INLINE bool IsSmi() const { return false; }

 protected:
  V8_INLINE constexpr explicit Tagged(Address ptr) : Base(ptr) {}

 private:
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<Weak<HeapObject>>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<Weak<HeapObject>>;
#endif
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;
  template <typename TFieldType, typename CompressionScheme>
  friend class TaggedMember;
  template <typename To, typename From>
  friend inline Tagged<To> UncheckedCast(Tagged<From> value);

  template <typename U>
  friend Tagged<WeakOf<U>> MakeWeak(Tagged<U> value);
  template <typename U>
  friend Tagged<WeakOf<U>> MakeWeakOrSmi(Tagged<U> value);
};

// Generic Tagged<T> for Unions. This doesn't allow direct access to the object,
// aside from casting.
template <typename... Ts>
class Tagged<Union<Ts...>> : public detail::BaseForTagged<Union<Ts...>>::type {
  using This = Union<Ts...>;
  using Base = typename detail::BaseForTagged<This>::type;

 public:
  V8_INLINE constexpr Tagged() = default;

  // Implicit conversion for subclasses.
  template <typename U>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other)
    requires(is_subtype_v<U, This>)
  {
    *this = Tagged(other);
    return *this;
  }

  // Implicit conversion for subclasses.
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other)
    requires(is_subtype_v<U, This>)
      : Base(other.ptr()) {}

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(U raw)
    requires(is_subtype_v<U, This> && std::is_base_of_v<HeapObject, U>)
      : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }

  template <typename U>
    requires(is_maybe_weak_v<This> && is_subtype_v<U, This>)
  V8_INLINE explicit constexpr Tagged(Tagged<U> other,
                                      HeapObjectReferenceType type)
      : Base(type == HeapObjectReferenceType::WEAK ? MakeWeak(other).ptr()
                                                   : MakeStrong(other).ptr()) {}

  // For the very specific MaybeObject case, allow the public constructor.
  V8_INLINE constexpr explicit Tagged(Address ptr)
    requires std::is_same_v<This, MaybeObject>
      : Base(ptr) {}

 private:
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<This>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<This>;
#endif
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;
  template <typename TFieldType, typename CompressionScheme>
  friend class TaggedMember;
  friend class CompressedHeapObjectSlot;
  friend class CompressedMaybeObjectSlot;
  friend class FullMaybeObjectSlot;
  friend class FullHeapObjectSlot;
  template <typename THeapObjectSlot>
  friend void UpdateHeapObjectReferenceSlot(THeapObjectSlot slot,
                                            Tagged<HeapObject> value);
  friend V8_EXPORT_PRIVATE Address CheckObjectType(Address raw_value,
                                                   Address raw_type,
                                                   Address raw_location);

  template <typename U>
  friend Tagged<WeakOf<U>> MakeWeak(Tagged<U> value);
  template <typename U>
  friend Tagged<WeakOf<U>> MakeWeakOrSmi(Tagged<U> value);
  template <typename U>
  friend Tagged<StrongOf<U>> MakeStrong(Tagged<U> value);
  template <typename To, typename From>
  friend inline Tagged<To> UncheckedCast(Tagged<From> value);

  V8_INLINE constexpr explicit Tagged(Address ptr) : Base(ptr) {}
};

// Generic Tagged<T> for any T that is a subclass of HeapObject. There are
// separate Tagged<T> specialaizations for T==Smi and T==Object, so we know that
// all other Tagged<T> are definitely pointers and not Smis.
template <typename T>
class Tagged : public detail::BaseForTagged<T>::type {
  using Base = typename detail::BaseForTagged<T>::type;

 public:
  V8_INLINE constexpr Tagged() = default;
  template <typename U = T>
  // Allow implicit conversion from const T* to Tagged<T>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const T* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {
    static_assert(std::is_base_of_v<HeapObjectLayout, U>);
  }

  // Implicit conversion for subclasses.
  template <typename U>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other)
    requires(is_subtype_v<U, T>)
  {
    *this = Tagged(other);
    return *this;
  }

  // Implicit conversion for subclasses.
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other)
    requires(is_subtype_v<U, T>)
      : Base(other) {}

  V8_INLINE constexpr decltype(auto) operator*() const {
    // Indirect operator* through a helper, which has a couple of
    // implementations for old- and new-layout objects, so that gdb only sees
    // this single operator* overload.
    return operator_star_impl();
  }
  V8_INLINE constexpr decltype(auto) operator->() const {
    // Indirect operator-> through a helper, which has a couple of
    // implementations for old- and new-layout objects, so that gdb only sees
    // this single operator-> overload.
    return operator_arrow_impl();
  }

  // Implicit conversions and explicit casts to/from raw pointers.
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  //
  // These come in two flavours keyed on U's base class. The canonical
  // pass-by-value form is used for the legacy HeapObject hierarchy where
  // `U` is a thin wrapper over a tagged address and both the copy
  // constructor and `raw.ptr()` are available. For HeapObjectLayout-derived
  // U the copy constructor is deleted (Tagged<T>::operator* yields U& for
  // layout types, not U-by-value), so pass-by-value wouldn't even compile.
  // The second overload accepts the layout object by const reference and
  // reconstructs the tagged address from its untagged memory location
  // (i.e. `&raw + kHeapObjectTag`).
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(U raw)
    requires(is_subtype_v<U, T> && !std::is_base_of_v<HeapObjectLayout, U>)
      : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const U& raw)
    requires(is_subtype_v<U, T> && std::is_base_of_v<HeapObjectLayout, U>)
      : Base(reinterpret_cast<Address>(&raw) + kHeapObjectTag) {}
  template <typename U>
  static constexpr Tagged<T> cast(U other) {
    static_assert(kTaggedCanConvertToRawObjects);
    return Cast<T>(Tagged<U>(other));
  }

 private:
  friend T;
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<T>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<T>;
#endif
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;
  template <typename TFieldType, typename CompressionScheme>
  friend class TaggedMember;
  template <typename To, typename From>
  friend inline Tagged<To> UncheckedCast(Tagged<From> value);

  template <typename U>
  friend Tagged<StrongOf<U>> MakeStrong(Tagged<U> value);

  V8_INLINE constexpr explicit Tagged(Address ptr) : Base(ptr) {}

  V8_INLINE T& operator_star_impl() const
    requires(std::is_base_of_v<HeapObjectLayout, T>)
  {
    return *ToRawPtr();
  }
  V8_INLINE T* operator_arrow_impl() const
    requires(std::is_base_of_v<HeapObjectLayout, T>)
  {
    return ToRawPtr();
  }

  V8_INLINE constexpr T operator_star_impl() const
    requires(!std::is_base_of_v<HeapObjectLayout, T>)
  {
    return ToRawPtr();
  }
  V8_INLINE constexpr detail::TaggedOperatorArrowRef<T> operator_arrow_impl()
      const
    requires(!std::is_base_of_v<HeapObjectLayout, T>)
  {
    return detail::TaggedOperatorArrowRef<T>{ToRawPtr()};
  }

  template <typename U = T>
  V8_INLINE T* ToRawPtr() const
    requires(std::is_base_of_v<HeapObjectLayout, U>)
  {
    // Check whether T is taggable on raw ptr access rather than top-level, to
    // allow forward declarations.
    static_assert(is_taggable_v<T>);
    return reinterpret_cast<T*>(this->ptr() - kHeapObjectTag);
  }

  template <typename U = T>
  V8_INLINE constexpr T ToRawPtr() const
    requires(!std::is_base_of_v<HeapObjectLayout, U>)
  {
    // Check whether T is taggable on raw ptr access rather than top-level, to
    // allow forward declarations.
    static_assert(is_taggable_v<T>);
    return T(this->ptr(), typename T::SkipTypeCheckTag{});
  }
};

// Specialized Tagged<T> for cleared weak values. This is only used, in
// practice, for conversions from Tagged<ClearedWeakValue> to a
// Tagged<MaybeWeak<T>>, where subtyping rules mean that this works for
// aribitrary T.
template <>
class Tagged<ClearedWeakValue> : public WeakTaggedBase {
 public:
  V8_INLINE constexpr explicit Tagged(Address ptr) : WeakTaggedBase(ptr) {}
};

// Specialized Tagged<T> for weak references to T, which are known to be
// subclasses of HeapObject (Smis can't be weak).
template <typename T>
class Tagged<Weak<T>> : public detail::BaseForTagged<Weak<T>>::type {
  using Base = typename detail::BaseForTagged<Weak<T>>::type;

 public:
  V8_INLINE constexpr Tagged() = default;
  template <typename U = T>
  // Allow implicit conversion from const T* to Tagged<Weak<T>>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const T* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {
    static_assert(std::is_base_of_v<HeapObjectLayout, U>);
  }

  // Implicit conversion for subclasses.
  template <typename U>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other)
    requires(is_subtype_v<U, T>)
  {
    *this = Tagged(other);
    return *this;
  }

  // Implicit conversion for subclasses.
  template <typename U>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other)
    requires(is_subtype_v<U, Weak<T>>)
      : Base(other) {}

 private:
  V8_INLINE constexpr explicit Tagged(Address ptr) : Base(ptr) {}

  friend T;
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<Weak<T>>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<Weak<T>>;
#endif
  template <typename U>
  friend Tagged<WeakOf<U>> MakeWeak(Tagged<U> value);
  template <typename U>
  friend Tagged<WeakOf<U>> MakeWeakOrSmi(Tagged<U> value);
  template <typename To, typename From>
  friend inline Tagged<To> UncheckedCast(Tagged<From> value);
};

template <typename T>
inline Tagged<WeakOf<T>> MakeWeak(Tagged<T> value) {
  static_assert(!is_subtype_v<Smi, T>, "Not allowed to make Smis weak.");
  return Tagged<WeakOf<T>>(value.ptr() | kWeakHeapObjectTag);
}

template <typename T>
inline Tagged<WeakOf<T>> MakeWeakOrSmi(Tagged<T> value) {
  static_assert(is_subtype_v<Smi, T>,
                "Use MakeWeak if this is known to not be a Smi.");
  if (value.IsSmi()) return Tagged<WeakOf<T>>(value.ptr());
  return Tagged<WeakOf<T>>(value.ptr() | kWeakHeapObjectTag);
}

template <typename T>
inline Tagged<StrongOf<T>> MakeStrong(Tagged<T> value) {
  // This works with Smis.
  return Tagged<StrongOf<T>>(value.ptr() &
                             (~kWeakHeapObjectTag | kHeapObjectTag));
}

// Deduction guide to simplify Foo->Tagged<Foo> transition.
// TODO(leszeks): Remove once we're using Tagged everywhere.
static_assert(kTaggedCanConvertToRawObjects);
template <class T>
Tagged(T object) -> Tagged<T>;

Tagged(const HeapObjectLayout* object) -> Tagged<HeapObject>;

template <class T>
Tagged(const T* object) -> Tagged<T>;
template <class T>
Tagged(T* object) -> Tagged<T>;

template <typename T>
struct RemoveTagged {
  using type = T;
};

template <typename T>
struct RemoveTagged<Tagged<T>> {
  using type = T;
};

}  // namespace internal
}  // namespace v8

namespace std {

// Template specialize std::common_type to always return Object when compared
// against a subtype of Object.
//
// This is an incomplete specialization for objects and common_type, but
// sufficient for existing use-cases. A proper specialization would need to be
// conditionally enabled via `requires`, which is C++20, or with `enable_if`,
// which would require a custom common_type implementation.
template <class T>
struct common_type<T, i::Object> {
  static_assert(i::is_subtype_v<T, i::Object>,
                "common_type with Object is only partially specialized.");
  using type = i::Object;
};

}  // namespace std

#endif  // V8_OBJECTS_TAGGED_H_
