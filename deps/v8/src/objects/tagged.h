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
  constexpr T* operator->() { return &object_; }

 private:
  friend class Tagged<T>;
  constexpr explicit TaggedOperatorArrowRef(T object) : object_(object) {}
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

// TaggedIndex is special, since it's a more strict Smi. It could probably even
// be its own specialization, to avoid exposing an operator->.
template <>
struct BaseForTagged<TaggedIndex> {
  using type = Tagged<Object>;
};

}  // namespace detail

template <typename T>
class Tagged;

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

  // Tagged<Object> doesn't provide a default constructor on purpose.
  // It depends on the use case if default initializing with Smi::Zero() or a
  // tagged null makes more sense.

  // Allow Tagged<Object> to be created from any address.
  constexpr explicit Tagged(Address o) : TaggedBase(o) {}

  // Allow explicit uninitialized initialization. In debug mode this is zapped.
  // TODO(leszeks): Mark this somehow as uninitialized, so that we get some
  // warning if it is used before initialization.
  constexpr Tagged()
      : TaggedBase(
#ifdef DEBUG
            kZapValue
#endif
        ) {
  }

  // Implicit conversion for subclasses -- all classes are subclasses of Object,
  // so allow all tagged pointers.
  // NOLINTNEXTLINE
  constexpr Tagged(TaggedBase other) : TaggedBase(other.ptr()) {}
  constexpr Tagged& operator=(TaggedBase other) {
    return *this = Tagged(other);
  }

  // Implicit conversions to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  // NOLINTNEXTLINE
  inline constexpr Tagged(Object raw);
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<U, Object> ||
                                        std::is_convertible_v<Object*, U*>>>
  // NOLINTNEXTLINE
  inline constexpr operator U();

 private:
  friend class Object;
  inline constexpr Object ToRawPtr() const;
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
    static_assert(std::is_base_of_v<Smi, U> ||
                  std::is_convertible_v<U*, Smi*> ||
                  std::is_base_of_v<U, Smi> || std::is_convertible_v<Smi*, U*>);
    DCHECK(other.IsSmi());
    return Tagged<Smi>(other.ptr());
  }
  static constexpr Tagged<Smi> unchecked_cast(TaggedBase other) {
    return Tagged<Smi>(other.ptr());
  }

  constexpr Tagged() = default;
  constexpr explicit Tagged(Address ptr) : TaggedBase(ptr) {}

  // No implicit conversions from other tagged pointers.

  constexpr bool IsHeapObject() const { return false; }
  constexpr bool IsSmi() const { return true; }

  constexpr int32_t value() const { return Internals::SmiValue(ptr()); }

  // Implicit conversions to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  // NOLINTNEXTLINE
  inline constexpr Tagged(Smi raw);
  // NOLINTNEXTLINE
  inline constexpr operator Smi();

 private:
  friend class Smi;
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<Smi>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<Smi>;
#endif
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
    static_assert(std::is_base_of_v<TaggedIndex, U> ||
                  std::is_convertible_v<U*, TaggedIndex*> ||
                  std::is_base_of_v<U, TaggedIndex> ||
                  std::is_convertible_v<TaggedIndex*, U*>);
    DCHECK(IsTaggedIndex(other));
    return Tagged<TaggedIndex>(other.ptr());
  }
  static constexpr Tagged<TaggedIndex> unchecked_cast(TaggedBase other) {
    return Tagged<TaggedIndex>(other.ptr());
  }

  constexpr Tagged() = default;
  constexpr explicit Tagged(Address ptr) : TaggedBase(ptr) {}

  // No implicit conversions from other tagged pointers.

  constexpr bool IsHeapObject() const { return false; }
  constexpr bool IsSmi() const { return true; }

  // Returns the integer value.
  constexpr intptr_t value() const {
    // Truncate and shift down (requires >> to be sign extending).
    return static_cast<intptr_t>(ptr()) >> kSmiTagSize;
  }

  // Implicit conversions to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  // NOLINTNEXTLINE
  inline constexpr Tagged(TaggedIndex raw);
  // NOLINTNEXTLINE
  inline constexpr operator TaggedIndex();

 private:
  friend class TaggedIndex;
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<TaggedIndex>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<TaggedIndex>;
#endif
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
    static_assert(std::is_base_of_v<HeapObject, U> ||
                  std::is_convertible_v<U*, HeapObject*> ||
                  std::is_base_of_v<U, HeapObject> ||
                  std::is_convertible_v<HeapObject*, U*>);
    DCHECK(other.IsHeapObject());
    return Tagged<HeapObject>(other.ptr());
  }
  static constexpr Tagged<HeapObject> unchecked_cast(TaggedBase other) {
    // Don't check incoming type for unchecked casts, in case the object
    // definitions are not available.
    return Tagged<HeapObject>(other.ptr());
  }

  constexpr Tagged() = default;

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<HeapObject, U> ||
                                        std::is_convertible_v<U*, HeapObject*>>>
  constexpr Tagged& operator=(Tagged<U> other) {
    return *this = Tagged(other);
  }

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<HeapObject, U> ||
                                        std::is_convertible_v<U*, HeapObject*>>>
  // NOLINTNEXTLINE
  constexpr Tagged(Tagged<U> other) : Base(other) {}

  constexpr HeapObject operator*() const;
  constexpr detail::TaggedOperatorArrowRef<HeapObject> operator->() const;

  constexpr bool is_null() const {
    return static_cast<Tagged_t>(this->ptr()) ==
           static_cast<Tagged_t>(kNullAddress);
  }

  constexpr bool IsHeapObject() const { return true; }
  constexpr bool IsSmi() const { return false; }

  inline bool InAnySharedSpace() const;
  inline bool InWritableSharedSpace() const;
  inline bool InReadOnlySpace() const;

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<HeapObject, U> ||
                                        std::is_convertible_v<U*, HeapObject*>>>
  // NOLINTNEXTLINE
  constexpr Tagged(U raw) : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  // NOLINTNEXTLINE
  constexpr operator HeapObject();
  template <typename U>
  static constexpr Tagged<HeapObject> cast(U other) {
    static_assert(kTaggedCanConvertToRawObjects);
    return Tagged<HeapObject>::cast(Tagged<U>(other));
  }

  Address address() const { return this->ptr() - kHeapObjectTag; }

 protected:
  constexpr explicit Tagged(Address ptr) : Base(ptr) {}

 private:
  friend class HeapObject;
  // Handles of the same type are allowed to access the Address constructor.
  friend class Handle<HeapObject>;
#ifdef V8_ENABLE_DIRECT_HANDLE
  friend class DirectHandle<HeapObject>;
#endif

  constexpr HeapObject ToRawPtr() const;
};

static_assert(Tagged<HeapObject>().is_null());

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
    static_assert(std::is_base_of_v<T, U> || std::is_convertible_v<U*, T*> ||
                  std::is_base_of_v<U, T> || std::is_convertible_v<T*, U*>);
    return T::cast(other);
  }
  static constexpr Tagged<T> unchecked_cast(TaggedBase other) {
    // Don't check incoming type for unchecked casts, in case the object
    // definitions are not available.
    return Tagged<T>(other.ptr());
  }

  constexpr Tagged() = default;

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<T, U> ||
                                        std::is_convertible_v<U*, T*>>>
  constexpr Tagged& operator=(Tagged<U> other) {
    *this = Tagged(other);
    return *this;
  }

  // Implicit conversion for subclasses.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<T, U> ||
                                        std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE
  constexpr Tagged(Tagged<U> other) : Base(other) {}

  constexpr T operator*() const { return ToRawPtr(); }
  constexpr detail::TaggedOperatorArrowRef<T> operator->() const {
    return detail::TaggedOperatorArrowRef<T>{ToRawPtr()};
  }

  // Implicit conversions and explicit casts to/from raw pointers
  // TODO(leszeks): Remove once we're using Tagged everywhere.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<T, U> ||
                                        std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE
  constexpr Tagged(U raw) : Base(raw.ptr()) {
    static_assert(kTaggedCanConvertToRawObjects);
  }
  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<U, T> ||
                                        std::is_convertible_v<T*, U*>>>
  // NOLINTNEXTLINE
  constexpr operator U() const {
    static_assert(kTaggedCanConvertToRawObjects);
    return ToRawPtr();
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

  constexpr explicit Tagged(Address ptr) : Base(ptr) {}
  constexpr T ToRawPtr() const {
    return T(this->ptr(), typename T::SkipTypeCheckTag{});
  }
};

// Deduction guide to simplify Foo->Tagged<Foo> transition.
// TODO(leszeks): Remove once we're using Tagged everywhere.
static_assert(kTaggedCanConvertToRawObjects);
template <class T>
Tagged(T object) -> Tagged<T>;

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
