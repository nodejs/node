// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_H_
#define V8_OBJECTS_TAGGED_H_

#include <type_traits>

#include "src/common/checks.h"
#include "src/common/globals.h"
#include "src/objects/tagged-impl.h"

namespace v8 {
namespace internal {

class Object;
class Smi;
class HeapObject;
class HeapObjectLayout;
class TaggedIndex;
class FieldType;

// Tagged<T> represents an uncompressed V8 tagged pointer.
//
// The tagged pointer is a pointer-sized value with a tag in the LSB. The value
// is either:
//
//   * A pointer to an object on the V8 heap, with the tag set to 1
//   * A small integer (Smi), shifted right, with the tag set to 0
//
// The exact encoding differs depending on 32- vs 64-bit architectures, and in
// the latter case, whether or not pointer compression is enabled.
//
// On 32-bit architectures, this is:
//             |----- 32 bits -----|
// Pointer:    |______address____01|
//    Smi:     |____int31_value___0|
//
// On 64-bit architectures with pointer compression:
//             |----- 32 bits -----|----- 32 bits -----|
// Pointer:    |________base_______|______offset_____01|
//    Smi:     |......garbage......|____int31_value___0|
//
// On 64-bit architectures without pointer compression:
//             |----- 32 bits -----|----- 32 bits -----|
// Pointer:    |________________address______________01|
//    Smi:     |____int32_value____|00...............00|
//
// We specialise Tagged separately for Object, Smi and HeapObject, and then all
// other types T, so that:
//
//                    Tagged<Object> -> TaggedBase
//                       Tagged<Smi> -> TaggedBase
//   Tagged<T> -> Tagged<HeapObject> -> TaggedBase
template <typename T>
class Tagged;

// `is_subtype<Derived, Base>::value` is true when Derived is a subtype of Base
// according to our object hierarchy. In particular, Smi is considered a subtype
// of Object.
template <typename Derived, typename Base, typename Enabled = void>
struct is_subtype : public std::is_base_of<Base, Derived> {};
template <typename Derived, typename Base>
static constexpr bool is_subtype_v = is_subtype<Derived, Base>::value;

template <>
struct is_subtype<Object, Object> : public std::true_type {};
template <>
struct is_subtype<Smi, Object> : public std::true_type {};
template <>
struct is_subtype<TaggedIndex, Object> : public std::true_type {};
template <>
struct is_subtype<FieldType, Object> : public std::true_type {};
template <typename Base>
struct is_subtype<Base, Object,
                  std::enable_if_t<std::disjunction_v<
                      std::is_base_of<HeapObject, Base>,
                      std::is_base_of<HeapObjectLayout, Base>>>>
    : public std::true_type {};
template <typename Base>
struct is_subtype<Base, HeapObject,
                  std::enable_if_t<std::disjunction_v<
                      std::is_base_of<HeapObject, Base>,
                      std::is_base_of<HeapObjectLayout, Base>>>>
    : public std::true_type {};

// For reasons (the tnode.h type hierarchy), the Object hierarchy is considered
// to be part of the MaybeObject hierarchy wrt is_subtype.
// But `Tagged<MaybeObject>` is invalid. Currently, just `MaybeObject` should
// be used instead. This specialization ensures that no such instances are
// constructed.
//
// The UnionT and is_union_t definitions have to be pulled in as well,
// unfortunately.
//
// TODO(leszeks): Clean this up once MaybeObject is supported in Tagged land,
// and move UnionT and is_union_t back to tnode.h
template <class T1, class T2>
struct UnionT;
template <typename T>
struct is_union_t : public std::false_type {};
template <typename T1, typename T2>
struct is_union_t<UnionT<T1, T2>> : public std::true_type {};
template <typename Base>
struct is_subtype<Base, MaybeObject, std::enable_if_t<!is_union_t<Base>::value>>
    : public std::disjunction<std::is_base_of<MaybeObject, Base>,
                              is_subtype<Base, Object>> {};

// TODO(jgruber): Clean up this artificial FixedArrayBase hierarchy. Only types
// that can be used as elements should be in it.
// TODO(jgruber): Replace FixedArrayBase with a union type, once they exist.
class FixedArrayBase;
#define DEF_FIXED_ARRAY_SUBTYPE(Subtype) \
  class Subtype;                         \
  template <>                            \
  struct is_subtype<Subtype, FixedArrayBase> : public std::true_type {};
DEF_FIXED_ARRAY_SUBTYPE(FixedArray)
DEF_FIXED_ARRAY_SUBTYPE(FixedDoubleArray)
DEF_FIXED_ARRAY_SUBTYPE(ByteArray)
DEF_FIXED_ARRAY_SUBTYPE(NameDictionary)
DEF_FIXED_ARRAY_SUBTYPE(NumberDictionary)
DEF_FIXED_ARRAY_SUBTYPE(OrderedHashMap)
DEF_FIXED_ARRAY_SUBTYPE(OrderedHashSet)
DEF_FIXED_ARRAY_SUBTYPE(OrderedNameDictionary)
DEF_FIXED_ARRAY_SUBTYPE(ScriptContextTable)
DEF_FIXED_ARRAY_SUBTYPE(ArrayList)
#undef DEF_FIXED_ARRAY_SUBTYPE

static_assert(is_subtype_v<Smi, Object>);
static_assert(is_subtype_v<HeapObject, Object>);
static_assert(is_subtype_v<HeapObject, HeapObject>);

// `is_taggable<T>::value` is true when T is a valid type for Tagged. This means
// de-facto being a subtype of Object.
template <typename T>
using is_taggable = is_subtype<T, Object>;
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

// Base class for all Tagged<T> classes.
// TODO(leszeks): Merge with TaggedImpl.
using TaggedBase = TaggedImpl<HeapObjectReferenceType::STRONG, Address>;

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
  V8_INLINE constexpr T* operator->() { return &object_; }

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
class Tagged<Object> : public TaggedBase {
 public:
  // Allow all casts -- all classes are subclasses of Object, nothing to check
  // here.
  static V8_INLINE constexpr Tagged<Object> cast(TaggedBase other) {
    return Tagged<Object>(other);
  }
  static V8_INLINE constexpr Tagged<Object> unchecked_cast(TaggedBase other) {
    return Tagged<Object>(other);
  }

  // Tagged<Object> doesn't provide a default constructor on purpose.
  // It depends on the use case if default initializing with Smi::Zero() or a
  // tagged null makes more sense.

  // Allow Tagged<Object> to be created from any address.
  V8_INLINE constexpr explicit Tagged(Address o) : TaggedBase(o) {}

  // Allow explicit uninitialized initialization.
  // TODO(leszeks): Consider zapping this instead, since it's odd that
  // Tagged<Object> implicitly initialises to Smi::zero().
  V8_INLINE constexpr Tagged() : TaggedBase(kNullAddress) {}

  // Allow implicit conversion from const HeapObjectLayout* to Tagged<Object>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const HeapObjectLayout* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {}

  // Implicit conversion for subclasses -- all classes are subclasses of Object,
  // so allow all tagged pointers.
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(TaggedBase other) : TaggedBase(other.ptr()) {}
  V8_INLINE constexpr Tagged& operator=(TaggedBase other) {
    return *this = Tagged(other);
  }
};

// Specialization for Smi disallowing any implicit creation or access via ->,
// but offering instead a cast from Object and an int32_t value() method.
template <>
class Tagged<Smi> : public TaggedBase {
 public:
  // Explicit cast for sub- and superclasses (in practice, only Object will pass
  // this static assert).
  template <typename U>
  static constexpr Tagged<Smi> cast(Tagged<U> other) {
    static_assert(is_castable_v<U, Smi>);
    DCHECK(other.IsSmi());
    return Tagged<Smi>(other.ptr());
  }
  V8_INLINE static constexpr Tagged<Smi> unchecked_cast(TaggedBase other) {
    return Tagged<Smi>(other.ptr());
  }

  V8_INLINE constexpr Tagged() = default;
  V8_INLINE constexpr explicit Tagged(Address ptr) : TaggedBase(ptr) {}

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
class Tagged<TaggedIndex> : public TaggedBase {
 public:
  // Explicit cast for sub- and superclasses (in practice, only Object will pass
  // this static assert).
  template <typename U>
  static constexpr Tagged<TaggedIndex> cast(Tagged<U> other) {
    static_assert(is_castable_v<U, TaggedIndex>);
    DCHECK(IsTaggedIndex(other));
    return Tagged<TaggedIndex>(other.ptr());
  }
  static V8_INLINE constexpr Tagged<TaggedIndex> unchecked_cast(
      TaggedBase other) {
    return Tagged<TaggedIndex>(other.ptr());
  }

  V8_INLINE constexpr Tagged() = default;
  V8_INLINE constexpr explicit Tagged(Address ptr) : TaggedBase(ptr) {}

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
class Tagged<HeapObject> : public TaggedBase {
  using Base = TaggedBase;

 public:
  // Explicit cast for sub- and superclasses.
  template <typename U>
  static constexpr Tagged<HeapObject> cast(Tagged<U> other) {
    static_assert(is_castable_v<U, HeapObject>);
    DCHECK(other.IsHeapObject());
    return Tagged<HeapObject>(other.ptr());
  }
  static V8_INLINE constexpr Tagged<HeapObject> unchecked_cast(
      TaggedBase other) {
    // Don't check incoming type for unchecked casts, in case the object
    // definitions are not available.
    return Tagged<HeapObject>(other.ptr());
  }

  V8_INLINE constexpr Tagged() = default;
  // Allow implicit conversion from const HeapObjectLayout* to
  // Tagged<HeapObject>.
  // TODO(leszeks): Make this more const-correct.
  // TODO(leszeks): Consider making this an explicit conversion.
  // NOLINTNEXTLINE
  V8_INLINE Tagged(const HeapObjectLayout* ptr)
      : Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag) {}

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<is_subtype_v<U, HeapObject>>>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other) {
    return *this = Tagged(other);
  }

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<is_subtype_v<U, HeapObject>>>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other) : Base(other) {}

  V8_INLINE constexpr HeapObject operator*() const;
  V8_INLINE constexpr detail::TaggedOperatorArrowRef<HeapObject> operator->()
      const;

  V8_INLINE constexpr bool is_null() const {
    return static_cast<Tagged_t>(this->ptr()) ==
           static_cast<Tagged_t>(kNullAddress);
  }

  constexpr V8_INLINE bool IsHeapObject() const { return true; }
  constexpr V8_INLINE bool IsSmi() const { return false; }

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U,
            typename = std::enable_if_t<is_subtype_v<U, HeapObject>>>
  // NOLINTNEXTLINE
  constexpr Tagged(U raw) : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U>
  static constexpr Tagged<HeapObject> cast(U other) {
    static_assert(kTaggedCanConvertToRawObjects);
    return Tagged<HeapObject>::cast(Tagged<U>(other));
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

  V8_INLINE constexpr HeapObject ToRawPtr() const;
};

static_assert(Tagged<HeapObject>().is_null());

// For reasons (the tnode.h type hierarchy), the Object hierarchy is considered
// to be part of the MaybeObject hierarchy wrt is_subtype.
// But `Tagged<MaybeObject>` is invalid. Currently, just `MaybeObject` should
// be used instead. This specialization ensures that no such instances are
// constructed.
// TODO(leszeks): Clean this up once MaybeObject is supported in Tagged land.
template <>
class Tagged<MaybeObject> : public TaggedBase {
  constexpr explicit Tagged(Address ptr) = delete;
};

// Generic Tagged<T> for any T that is a subclass of HeapObject. There are
// separate Tagged<T> specializations for T==Smi and T==Object, so we know that
// all other Tagged<T> are definitely pointers and not Smis.
template <typename T>
class Tagged : public detail::BaseForTagged<T>::type {
  using Base = typename detail::BaseForTagged<T>::type;

 public:
  // Explicit cast for sub- and superclasses.
  template <typename U>
  static constexpr Tagged<T> cast(Tagged<U> other) {
    static_assert(is_castable_v<T, U>);
    return T::cast(other);
  }
  static V8_INLINE constexpr Tagged<T> unchecked_cast(TaggedBase other) {
    // Don't check incoming type for unchecked casts, in case the object
    // definitions are not available.
    return Tagged<T>(other.ptr());
  }

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
  template <typename U, typename = std::enable_if_t<is_subtype_v<U, T>>>
  V8_INLINE constexpr Tagged& operator=(Tagged<U> other) {
    *this = Tagged(other);
    return *this;
  }

  // Implicit conversion for subclasses.
  template <typename U, typename = std::enable_if_t<is_subtype_v<U, T>>>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(Tagged<U> other) : Base(other) {}

  template <typename U = T,
            typename = std::enable_if_t<std::is_base_of_v<HeapObjectLayout, U>>>
  V8_INLINE T& operator*() const {
    return *ToRawPtr();
  }
  template <typename U = T,
            typename = std::enable_if_t<std::is_base_of_v<HeapObjectLayout, U>>>
  V8_INLINE T* operator->() const {
    return ToRawPtr();
  }

  template <typename U = T, typename = std::enable_if_t<
                                !std::is_base_of_v<HeapObjectLayout, U>>>
  V8_INLINE constexpr T operator*() const {
    return ToRawPtr();
  }
  template <typename U = T, typename = std::enable_if_t<
                                !std::is_base_of_v<HeapObjectLayout, U>>>
  V8_INLINE constexpr detail::TaggedOperatorArrowRef<T> operator->() const {
    return detail::TaggedOperatorArrowRef<T>{ToRawPtr()};
  }

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U, typename = std::enable_if_t<is_subtype_v<U, T>>>
  // NOLINTNEXTLINE
  V8_INLINE constexpr Tagged(U raw) : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U>
  static constexpr Tagged<T> cast(U other) {
    static_assert(kTaggedCanConvertToRawObjects);
    return Tagged<T>::cast(Tagged<U>(other));
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

  V8_INLINE constexpr explicit Tagged(Address ptr) : Base(ptr) {}

  template <typename U = T,
            typename = std::enable_if_t<std::is_base_of_v<HeapObjectLayout, U>>>
  V8_INLINE T* ToRawPtr() const {
    // Check whether T is taggable on raw ptr access rather than top-level, to
    // allow forward declarations.
    static_assert(is_taggable_v<T>);
    return reinterpret_cast<T*>(this->ptr() - kHeapObjectTag);
  }

  template <typename U = T, typename = std::enable_if_t<
                                !std::is_base_of_v<HeapObjectLayout, U>>>
  V8_INLINE constexpr T ToRawPtr() const {
    // Check whether T is taggable on raw ptr access rather than top-level, to
    // allow forward declarations.
    static_assert(is_taggable_v<T>);
    return T(this->ptr(), typename T::SkipTypeCheckTag{});
  }
};  // namespace internal

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
