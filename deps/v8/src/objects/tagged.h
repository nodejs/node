// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_H_
#define V8_OBJECTS_TAGGED_H_

#include <type_traits>

#include "src/common/globals.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

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

// TODO(leszeks): Remove this once there are no more conversions between
// Tagged<Foo> and Foo.
static constexpr bool kTaggedCanConvertToRawObjects = true;

// Base class for all Tagged<T> classes.
class TaggedBase {
 public:
  constexpr TaggedBase() = default;

  constexpr Address ptr() const { return ptr_; }

  constexpr bool operator==(TaggedBase other) const {
    return static_cast<Tagged_t>(ptr()) == static_cast<Tagged_t>(other.ptr());
  }
  constexpr bool operator!=(TaggedBase other) const {
    return static_cast<Tagged_t>(ptr()) != static_cast<Tagged_t>(other.ptr());
  }

 protected:
  constexpr explicit TaggedBase(Address ptr) : ptr_(ptr) {}
  // TODO(leszeks): Consider a different default value, e.g. a tagged null.
  Address ptr_ = kNullAddress;
};

// Implicit comparisons with raw pointers
// TODO(leszeks): Remove once we're using Tagged everywhere.
inline constexpr bool operator==(TaggedBase tagged_ptr, Object obj) {
  static_assert(kTaggedCanConvertToRawObjects);
  return static_cast<Tagged_t>(tagged_ptr.ptr()) ==
         static_cast<Tagged_t>(obj.ptr());
}
inline constexpr bool operator==(Object obj, TaggedBase tagged_ptr) {
  static_assert(kTaggedCanConvertToRawObjects);
  return static_cast<Tagged_t>(obj.ptr()) ==
         static_cast<Tagged_t>(tagged_ptr.ptr());
}
inline constexpr bool operator!=(TaggedBase tagged_ptr, Object obj) {
  static_assert(kTaggedCanConvertToRawObjects);
  return static_cast<Tagged_t>(tagged_ptr.ptr()) !=
         static_cast<Tagged_t>(obj.ptr());
}
inline constexpr bool operator!=(Object obj, TaggedBase tagged_ptr) {
  static_assert(kTaggedCanConvertToRawObjects);
  return static_cast<Tagged_t>(obj.ptr()) !=
         static_cast<Tagged_t>(tagged_ptr.ptr());
}

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
  constexpr T* operator->() { return &object_; }

 private:
  friend class Tagged<T>;
  constexpr explicit TaggedOperatorArrowRef(T object) : object_(object) {}
  T object_;
};

// Special hack to make all Tagged<T> (except Tagged<Object> and Tagged<Smi>) a
// subclass of Tagged<HeapObject>, so that Tagged<HeapObject> function overloads
// accept them without an ambiguous user-defined conversion.
template <typename T>
using GetBaseForTagged = std::conditional_t<std::is_same_v<T, HeapObject>,
                                            TaggedBase, Tagged<HeapObject>>;

}  // namespace detail

// Generic Tagged<T> for any T that is a subclass of HeapObject. There are
// separate Tagged<T> specializations for T==Smi and T==Object, so we know that
// all other Tagged<T> are definitely pointers and not Smis.
template <typename T>
class Tagged : public detail::GetBaseForTagged<T> {
  using Base = detail::GetBaseForTagged<T>;

 public:
  // Explicit cast for sub- and superclasses.
  template <typename U>
  static constexpr Tagged<T> cast(Tagged<U> other) {
    static_assert(std::is_convertible_v<U*, T*> ||
                  std::is_convertible_v<T*, U*>);
    return Tagged<T>(T::cast(*other).ptr());
  }
  static constexpr Tagged<T> unchecked_cast(TaggedBase other) {
    // Don't check incoming type for unchecked casts, in case the object
    // definitions are not available.
    return Tagged<T>(other.ptr());
  }

  constexpr Tagged() = default;

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  constexpr Tagged& operator=(Tagged<U> other) {
    this->ptr_ = other.ptr();
    return *this;
  }

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE
  constexpr Tagged(Tagged<U> other) : Base(other) {}

  constexpr T operator*() const { return ToRawPtr(); }
  constexpr detail::TaggedOperatorArrowRef<T> operator->() const {
    return detail::TaggedOperatorArrowRef<T>{ToRawPtr()};
  }

  constexpr bool is_null() const {
    return static_cast<Tagged_t>(this->ptr()) ==
           static_cast<Tagged_t>(kNullAddress);
  }

  constexpr bool IsHeapObject() const { return true; }
  constexpr bool IsSmi() const { return false; }

#define IS_TYPE_FUNCTION_DEF(type_)                         \
  bool Is##type_() const { return ToRawPtr().Is##type_(); } \
  bool Is##type_(PtrComprCageBase cage_base) const {        \
    return ToRawPtr().Is##type_(cage_base);                 \
  }
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DEF)
  IS_TYPE_FUNCTION_DEF(HashTableBase)
  IS_TYPE_FUNCTION_DEF(SmallOrderedHashTable)
#undef IS_TYPE_FUNCTION_DEF

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE
  constexpr Tagged(U raw) : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<T*, U*>>>
  // NOLINTNEXTLINE
  constexpr operator U() {
    static_assert(kTaggedCanConvertToRawObjects);
    return ToRawPtr();
  }
  template <typename U>
  static constexpr Tagged<T> cast(U other) {
    static_assert(kTaggedCanConvertToRawObjects);
    return Tagged<T>::cast(Tagged<U>(other));
  }

 private:
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<T>;

  using Base::Base;
  constexpr T ToRawPtr() const {
    return T::unchecked_cast(Object(this->ptr()));
  }
};

// Specialization for Object, where it's unknown whether this is a Smi or a
// HeapObject.
template <>
class Tagged<Object> : public TaggedBase {
 public:
  // Allow all casts -- all classes are subclasses of Object, nothing to check
  // here.
  static constexpr Tagged<Object> cast(TaggedBase other) {
    return Tagged<Object>(other);
  }
  static constexpr Tagged<Object> unchecked_cast(TaggedBase other) {
    return Tagged<Object>(other);
  }

  // Allow Tagged<Object> to be created from any address.
  constexpr explicit Tagged(Address o) : TaggedBase(o) {}

  // Implicit conversion for subclasses -- all classes are subclasses of Object,
  // so allow all tagged pointers.
  // NOLINTNEXTLINE
  constexpr Tagged(TaggedBase other) : TaggedBase(other.ptr()) {}
  constexpr Tagged& operator=(TaggedBase other) {
    ptr_ = other.ptr();
    return *this;
  }

  // TODO(leszeks): Tagged<Object> is not known to be a pointer, so it shouldn't
  // have an operator* or operator->. Remove once all Object member functions
  // are free/static functions.
  constexpr Object operator*() { return ToRawPtr(); }
  constexpr detail::TaggedOperatorArrowRef<Object> operator->() {
    return detail::TaggedOperatorArrowRef<Object>{ToRawPtr()};
  }

  bool IsHeapObject() const { return ToRawPtr().IsHeapObject(); }
  bool IsSmi() const { return ToRawPtr().IsSmi(); }

#define IS_TYPE_FUNCTION_DEF(type_)                               \
  /* Hack in a default templated type to delay name resolution */ \
  template <typename U = void>                                    \
  bool Is##type_() const {                                        \
    return ToRawPtr().Is##type_();                                \
  }                                                               \
  /* Hack in a default templated type to delay name resolution */ \
  template <typename U = void>                                    \
  bool Is##type_(PtrComprCageBase cage_base) const {              \
    return ToRawPtr().Is##type_(cage_base);                       \
  }
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DEF)
  IS_TYPE_FUNCTION_DEF(HashTableBase)
  IS_TYPE_FUNCTION_DEF(SmallOrderedHashTable)
#undef IS_TYPE_FUNCTION_DEF

  // Implicit conversions to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  // NOLINTNEXTLINE
  constexpr Tagged(Object raw) : TaggedBase(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<Object*, U*>>>
  // NOLINTNEXTLINE
  constexpr operator U() {
    static_assert(kTaggedCanConvertToRawObjects);
    return ToRawPtr();
  }

 private:
  constexpr Object ToRawPtr() const { return Object(ptr()); }
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
    static_assert(std::is_convertible_v<U*, Smi*>);
    return Tagged<Smi>(Smi::cast(*other).ptr());
  }
  static constexpr Tagged<Smi> unchecked_cast(TaggedBase other) {
    return Tagged<Smi>(other.ptr());
  }

  // No implicit conversions from other tagged pointers.

  constexpr bool IsHeapObject() const { return false; }
  constexpr bool IsSmi() const { return true; }

  constexpr int32_t value() const { return Smi(ptr_).value(); }

  // Implicit conversions to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  // NOLINTNEXTLINE
  constexpr Tagged(Smi raw) : TaggedBase(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<Smi*, U*>>>
  // NOLINTNEXTLINE
  constexpr operator U() {
    static_assert(kTaggedCanConvertToRawObjects);
    return Smi(ptr_);
  }

  // Access via ->, remove once Smi doesn't have its own address.
  constexpr Smi operator*() { return Smi(ptr_); }
  constexpr detail::TaggedOperatorArrowRef<Smi> operator->() {
    return detail::TaggedOperatorArrowRef<Smi>(Smi(ptr_));
  }

 private:
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<Smi>;

  using TaggedBase::TaggedBase;
};

static_assert(Tagged<HeapObject>().is_null());

// Deduction guide to simplify Foo->Tagged<Foo> transition.
// TODO(leszeks): Remove once we're using Tagged everywhere.
static_assert(kTaggedCanConvertToRawObjects);
template <class T>
Tagged(T object) -> Tagged<T>;

template <typename T>
inline std::ostream& operator<<(std::ostream& os, Tagged<T> o) {
  return os << *o;
}

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

#endif  // V8_OBJECTS_TAGGED_H_
