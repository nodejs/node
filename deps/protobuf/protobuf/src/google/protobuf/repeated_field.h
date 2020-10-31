// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// RepeatedField and RepeatedPtrField are used by generated protocol message
// classes to manipulate repeated fields.  These classes are very similar to
// STL's vector, but include a number of optimizations found to be useful
// specifically in the case of Protocol Buffers.  RepeatedPtrField is
// particularly different from STL vector as it manages ownership of the
// pointers that it contains.
//
// Typically, clients should not need to access RepeatedField objects directly,
// but should instead use the accessor functions generated automatically by the
// protocol compiler.

#ifndef GOOGLE_PROTOBUF_REPEATED_FIELD_H__
#define GOOGLE_PROTOBUF_REPEATED_FIELD_H__

#include <utility>
#ifdef _MSC_VER
// This is required for min/max on VS2013 only.
#include <algorithm>
#endif

#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/implicit_weak_message.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/port.h>
#include <google/protobuf/stubs/casts.h>
#include <type_traits>


#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

// Forward-declare these so that we can make them friends.
namespace upb {
namespace google_opensource {
class GMR_Handlers;
}  // namespace google_opensource
}  // namespace upb

namespace google {
namespace protobuf {

class Message;
class Reflection;

namespace internal {

class MergePartialFromCodedStreamHelper;

static const int kMinRepeatedFieldAllocationSize = 4;

// A utility function for logging that doesn't need any template types.
void LogIndexOutOfBounds(int index, int size);

template <typename Iter>
inline int CalculateReserve(Iter begin, Iter end, std::forward_iterator_tag) {
  return static_cast<int>(std::distance(begin, end));
}

template <typename Iter>
inline int CalculateReserve(Iter /*begin*/, Iter /*end*/,
                            std::input_iterator_tag /*unused*/) {
  return -1;
}

template <typename Iter>
inline int CalculateReserve(Iter begin, Iter end) {
  typedef typename std::iterator_traits<Iter>::iterator_category Category;
  return CalculateReserve(begin, end, Category());
}
}  // namespace internal

// RepeatedField is used to represent repeated fields of a primitive type (in
// other words, everything except strings and nested Messages).  Most users will
// not ever use a RepeatedField directly; they will use the get-by-index,
// set-by-index, and add accessors that are generated for all repeated fields.
template <typename Element>
class RepeatedField final {
  static_assert(
      alignof(Arena) >= alignof(Element),
      "We only support types that have an alignment smaller than Arena");

 public:
  RepeatedField();
  explicit RepeatedField(Arena* arena);
  RepeatedField(const RepeatedField& other);
  template <typename Iter>
  RepeatedField(Iter begin, const Iter& end);
  ~RepeatedField();

  RepeatedField& operator=(const RepeatedField& other);

  RepeatedField(RepeatedField&& other) noexcept;
  RepeatedField& operator=(RepeatedField&& other) noexcept;

  bool empty() const;
  int size() const;

  const Element& Get(int index) const;
  Element* Mutable(int index);

  const Element& operator[](int index) const { return Get(index); }
  Element& operator[](int index) { return *Mutable(index); }

  const Element& at(int index) const;
  Element& at(int index);

  void Set(int index, const Element& value);
  void Add(const Element& value);
  // Appends a new element and return a pointer to it.
  // The new element is uninitialized if |Element| is a POD type.
  Element* Add();
  // Append elements in the range [begin, end) after reserving
  // the appropriate number of elements.
  template <typename Iter>
  void Add(Iter begin, Iter end);

  // Remove the last element in the array.
  void RemoveLast();

  // Extract elements with indices in "[start .. start+num-1]".
  // Copy them into "elements[0 .. num-1]" if "elements" is not NULL.
  // Caution: implementation also moves elements with indices [start+num ..].
  // Calling this routine inside a loop can cause quadratic behavior.
  void ExtractSubrange(int start, int num, Element* elements);

  void Clear();
  void MergeFrom(const RepeatedField& other);
  void CopyFrom(const RepeatedField& other);

  // Reserve space to expand the field to at least the given size.  If the
  // array is grown, it will always be at least doubled in size.
  void Reserve(int new_size);

  // Resize the RepeatedField to a new, smaller size.  This is O(1).
  void Truncate(int new_size);

  void AddAlreadyReserved(const Element& value);
  // Appends a new element and return a pointer to it.
  // The new element is uninitialized if |Element| is a POD type.
  // Should be called only if Capacity() > Size().
  Element* AddAlreadyReserved();
  Element* AddNAlreadyReserved(int elements);
  int Capacity() const;

  // Like STL resize.  Uses value to fill appended elements.
  // Like Truncate() if new_size <= size(), otherwise this is
  // O(new_size - size()).
  void Resize(int new_size, const Element& value);

  // Gets the underlying array.  This pointer is possibly invalidated by
  // any add or remove operation.
  Element* mutable_data();
  const Element* data() const;

  // Swap entire contents with "other". If they are separate arenas then, copies
  // data between each other.
  void Swap(RepeatedField* other);

  // Swap entire contents with "other". Should be called only if the caller can
  // guarantee that both repeated fields are on the same arena or are on the
  // heap. Swapping between different arenas is disallowed and caught by a
  // GOOGLE_DCHECK (see API docs for details).
  void UnsafeArenaSwap(RepeatedField* other);

  // Swap two elements.
  void SwapElements(int index1, int index2);

  // STL-like iterator support
  typedef Element* iterator;
  typedef const Element* const_iterator;
  typedef Element value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef int size_type;
  typedef ptrdiff_t difference_type;

  iterator begin();
  const_iterator begin() const;
  const_iterator cbegin() const;
  iterator end();
  const_iterator end() const;
  const_iterator cend() const;

  // Reverse iterator support
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // Returns the number of bytes used by the repeated field, excluding
  // sizeof(*this)
  size_t SpaceUsedExcludingSelfLong() const;

  int SpaceUsedExcludingSelf() const {
    return internal::ToIntSize(SpaceUsedExcludingSelfLong());
  }

  // Removes the element referenced by position.
  //
  // Returns an iterator to the element immediately following the removed
  // element.
  //
  // Invalidates all iterators at or after the removed element, including end().
  iterator erase(const_iterator position);

  // Removes the elements in the range [first, last).
  //
  // Returns an iterator to the element immediately following the removed range.
  //
  // Invalidates all iterators at or after the removed range, including end().
  iterator erase(const_iterator first, const_iterator last);

  // Get the Arena on which this RepeatedField stores its elements.
  Arena* GetArena() const { return GetArenaNoVirtual(); }

  // For internal use only.
  //
  // This is public due to it being called by generated code.
  inline void InternalSwap(RepeatedField* other);

 private:
  static const int kInitialSize = 0;
  // A note on the representation here (see also comment below for
  // RepeatedPtrFieldBase's struct Rep):
  //
  // We maintain the same sizeof(RepeatedField) as before we added arena support
  // so that we do not degrade performance by bloating memory usage. Directly
  // adding an arena_ element to RepeatedField is quite costly. By using
  // indirection in this way, we keep the same size when the RepeatedField is
  // empty (common case), and add only an 8-byte header to the elements array
  // when non-empty. We make sure to place the size fields directly in the
  // RepeatedField class to avoid costly cache misses due to the indirection.
  int current_size_;
  int total_size_;
  struct Rep {
    Arena* arena;
    Element elements[1];
  };
  // We can not use sizeof(Rep) - sizeof(Element) due to the trailing padding on
  // the struct. We can not use sizeof(Arena*) as well because there might be
  // a "gap" after the field arena and before the field elements (e.g., when
  // Element is double and pointer is 32bit).
  static const size_t kRepHeaderSize;

  // If total_size_ == 0 this points to an Arena otherwise it points to the
  // elements member of a Rep struct. Using this invariant allows the storage of
  // the arena pointer without an extra allocation in the constructor.
  void* arena_or_elements_;

  // Return pointer to elements array.
  // pre-condition: the array must have been allocated.
  Element* elements() const {
    GOOGLE_DCHECK_GT(total_size_, 0);
    // Because of above pre-condition this cast is safe.
    return unsafe_elements();
  }

  // Return pointer to elements array if it exists otherwise either null or
  // a invalid pointer is returned. This only happens for empty repeated fields,
  // where you can't dereference this pointer anyway (it's empty).
  Element* unsafe_elements() const {
    return static_cast<Element*>(arena_or_elements_);
  }

  // Return pointer to the Rep struct.
  // pre-condition: the Rep must have been allocated, ie elements() is safe.
  Rep* rep() const {
    char* addr = reinterpret_cast<char*>(elements()) - offsetof(Rep, elements);
    return reinterpret_cast<Rep*>(addr);
  }

  friend class Arena;
  typedef void InternalArenaConstructable_;


  // Move the contents of |from| into |to|, possibly clobbering |from| in the
  // process.  For primitive types this is just a memcpy(), but it could be
  // specialized for non-primitive types to, say, swap each element instead.
  void MoveArray(Element* to, Element* from, int size);

  // Copy the elements of |from| into |to|.
  void CopyArray(Element* to, const Element* from, int size);

  // Internal helper expected by Arena methods.
  inline Arena* GetArenaNoVirtual() const {
    return (total_size_ == 0) ? static_cast<Arena*>(arena_or_elements_)
                              : rep()->arena;
  }

  // Internal helper to delete all elements and deallocate the storage.
  // If Element has a trivial destructor (for example, if it's a fundamental
  // type, like int32), the loop will be removed by the optimizer.
  void InternalDeallocate(Rep* rep, int size) {
    if (rep != NULL) {
      Element* e = &rep->elements[0];
      Element* limit = &rep->elements[size];
      for (; e < limit; e++) {
        e->~Element();
      }
      if (rep->arena == NULL) {
#if defined(__GXX_DELETE_WITH_SIZE__) || defined(__cpp_sized_deallocation)
        const size_t bytes = size * sizeof(*e) + kRepHeaderSize;
        ::operator delete(static_cast<void*>(rep), bytes);
#else
        ::operator delete(static_cast<void*>(rep));
#endif
      }
    }
  }
};

template <typename Element>
const size_t RepeatedField<Element>::kRepHeaderSize =
    reinterpret_cast<size_t>(&reinterpret_cast<Rep*>(16)->elements[0]) - 16;

namespace internal {
template <typename It>
class RepeatedPtrIterator;
template <typename It, typename VoidPtr>
class RepeatedPtrOverPtrsIterator;
}  // namespace internal

namespace internal {

// This is a helper template to copy an array of elements efficiently when they
// have a trivial copy constructor, and correctly otherwise. This really
// shouldn't be necessary, but our compiler doesn't optimize std::copy very
// effectively.
template <typename Element,
          bool HasTrivialCopy =
              std::is_pod<Element>::value>
struct ElementCopier {
  void operator()(Element* to, const Element* from, int array_size);
};

}  // namespace internal

namespace internal {

// type-traits helper for RepeatedPtrFieldBase: we only want to invoke
// arena-related "copy if on different arena" behavior if the necessary methods
// exist on the contained type. In particular, we rely on MergeFrom() existing
// as a general proxy for the fact that a copy will work, and we also provide a
// specific override for std::string*.
template <typename T>
struct TypeImplementsMergeBehaviorProbeForMergeFrom {
  typedef char HasMerge;
  typedef long HasNoMerge;

  // We accept either of:
  // - void MergeFrom(const T& other)
  // - bool MergeFrom(const T& other)
  //
  // We mangle these names a bit to avoid compatibility issues in 'unclean'
  // include environments that may have, e.g., "#define test ..." (yes, this
  // exists).
  template <typename U, typename RetType, RetType (U::*)(const U& arg)>
  struct CheckType;
  template <typename U>
  static HasMerge Check(CheckType<U, void, &U::MergeFrom>*);
  template <typename U>
  static HasMerge Check(CheckType<U, bool, &U::MergeFrom>*);
  template <typename U>
  static HasNoMerge Check(...);

  // Resolves to either std::true_type or std::false_type.
  typedef std::integral_constant<bool,
                                 (sizeof(Check<T>(0)) == sizeof(HasMerge))>
      type;
};

template <typename T, typename = void>
struct TypeImplementsMergeBehavior
    : TypeImplementsMergeBehaviorProbeForMergeFrom<T> {};


template <>
struct TypeImplementsMergeBehavior<std::string> {
  typedef std::true_type type;
};

template <typename T>
struct IsMovable
    : std::integral_constant<bool, std::is_move_constructible<T>::value &&
                                       std::is_move_assignable<T>::value> {};

// This is the common base class for RepeatedPtrFields.  It deals only in void*
// pointers.  Users should not use this interface directly.
//
// The methods of this interface correspond to the methods of RepeatedPtrField,
// but may have a template argument called TypeHandler.  Its signature is:
//   class TypeHandler {
//    public:
//     typedef MyType Type;
//     // WeakType is almost always the same as MyType, but we use it in
//     // ImplicitWeakTypeHandler.
//     typedef MyType WeakType;
//     static Type* New();
//     static WeakType* NewFromPrototype(const WeakType* prototype,
//                                       Arena* arena);
//     static void Delete(Type*);
//     static void Clear(Type*);
//     static void Merge(const Type& from, Type* to);
//
//     // Only needs to be implemented if SpaceUsedExcludingSelf() is called.
//     static int SpaceUsedLong(const Type&);
//   };
class PROTOBUF_EXPORT RepeatedPtrFieldBase {
 protected:
  RepeatedPtrFieldBase();
  explicit RepeatedPtrFieldBase(Arena* arena);
  ~RepeatedPtrFieldBase() {}

  // Must be called from destructor.
  template <typename TypeHandler>
  void Destroy();

  bool empty() const;
  int size() const;

  template <typename TypeHandler>
  const typename TypeHandler::Type& at(int index) const;
  template <typename TypeHandler>
  typename TypeHandler::Type& at(int index);

  template <typename TypeHandler>
  typename TypeHandler::Type* Mutable(int index);
  template <typename TypeHandler>
  void Delete(int index);
  template <typename TypeHandler>
  typename TypeHandler::Type* Add(typename TypeHandler::Type* prototype = NULL);

 public:
  // The next few methods are public so that they can be called from generated
  // code when implicit weak fields are used, but they should never be called by
  // application code.

  template <typename TypeHandler>
  const typename TypeHandler::WeakType& Get(int index) const;

  // Creates and adds an element using the given prototype, without introducing
  // a link-time dependency on the concrete message type. This method is used to
  // implement implicit weak fields. The prototype may be NULL, in which case an
  // ImplicitWeakMessage will be used as a placeholder.
  MessageLite* AddWeak(const MessageLite* prototype);

  template <typename TypeHandler>
  void Clear();

  template <typename TypeHandler>
  void MergeFrom(const RepeatedPtrFieldBase& other);

  inline void InternalSwap(RepeatedPtrFieldBase* other);

 protected:
  template <
      typename TypeHandler,
      typename std::enable_if<TypeHandler::Movable::value>::type* = nullptr>
  void Add(typename TypeHandler::Type&& value);

  template <typename TypeHandler>
  void RemoveLast();
  template <typename TypeHandler>
  void CopyFrom(const RepeatedPtrFieldBase& other);

  void CloseGap(int start, int num);

  void Reserve(int new_size);

  int Capacity() const;

  // Used for constructing iterators.
  void* const* raw_data() const;
  void** raw_mutable_data() const;

  template <typename TypeHandler>
  typename TypeHandler::Type** mutable_data();
  template <typename TypeHandler>
  const typename TypeHandler::Type* const* data() const;

  template <typename TypeHandler>
  PROTOBUF_ALWAYS_INLINE void Swap(RepeatedPtrFieldBase* other);

  void SwapElements(int index1, int index2);

  template <typename TypeHandler>
  size_t SpaceUsedExcludingSelfLong() const;

  // Advanced memory management --------------------------------------

  // Like Add(), but if there are no cleared objects to use, returns NULL.
  template <typename TypeHandler>
  typename TypeHandler::Type* AddFromCleared();

  template <typename TypeHandler>
  void AddAllocated(typename TypeHandler::Type* value) {
    typename TypeImplementsMergeBehavior<typename TypeHandler::Type>::type t;
    AddAllocatedInternal<TypeHandler>(value, t);
  }

  template <typename TypeHandler>
  void UnsafeArenaAddAllocated(typename TypeHandler::Type* value);

  template <typename TypeHandler>
  typename TypeHandler::Type* ReleaseLast() {
    typename TypeImplementsMergeBehavior<typename TypeHandler::Type>::type t;
    return ReleaseLastInternal<TypeHandler>(t);
  }

  // Releases last element and returns it, but does not do out-of-arena copy.
  // And just returns the raw pointer to the contained element in the arena.
  template <typename TypeHandler>
  typename TypeHandler::Type* UnsafeArenaReleaseLast();

  int ClearedCount() const;
  template <typename TypeHandler>
  void AddCleared(typename TypeHandler::Type* value);
  template <typename TypeHandler>
  typename TypeHandler::Type* ReleaseCleared();

  template <typename TypeHandler>
  void AddAllocatedInternal(typename TypeHandler::Type* value, std::true_type);
  template <typename TypeHandler>
  void AddAllocatedInternal(typename TypeHandler::Type* value, std::false_type);

  template <typename TypeHandler>
  PROTOBUF_NOINLINE void AddAllocatedSlowWithCopy(
      typename TypeHandler::Type* value, Arena* value_arena, Arena* my_arena);
  template <typename TypeHandler>
  PROTOBUF_NOINLINE void AddAllocatedSlowWithoutCopy(
      typename TypeHandler::Type* value);

  template <typename TypeHandler>
  typename TypeHandler::Type* ReleaseLastInternal(std::true_type);
  template <typename TypeHandler>
  typename TypeHandler::Type* ReleaseLastInternal(std::false_type);

  template <typename TypeHandler>
  PROTOBUF_NOINLINE void SwapFallback(RepeatedPtrFieldBase* other);

  inline Arena* GetArenaNoVirtual() const { return arena_; }

 private:
  static const int kInitialSize = 0;
  // A few notes on internal representation:
  //
  // We use an indirected approach, with struct Rep, to keep
  // sizeof(RepeatedPtrFieldBase) equivalent to what it was before arena support
  // was added, namely, 3 8-byte machine words on x86-64. An instance of Rep is
  // allocated only when the repeated field is non-empty, and it is a
  // dynamically-sized struct (the header is directly followed by elements[]).
  // We place arena_ and current_size_ directly in the object to avoid cache
  // misses due to the indirection, because these fields are checked frequently.
  // Placing all fields directly in the RepeatedPtrFieldBase instance costs
  // significant performance for memory-sensitive workloads.
  Arena* arena_;
  int current_size_;
  int total_size_;
  struct Rep {
    int allocated_size;
    void* elements[1];
  };
  static const size_t kRepHeaderSize = sizeof(Rep) - sizeof(void*);
  // Contains arena ptr and the elements array. We also keep the invariant that
  // if rep_ is NULL, then arena is NULL.
  Rep* rep_;

  template <typename TypeHandler>
  static inline typename TypeHandler::Type* cast(void* element) {
    return reinterpret_cast<typename TypeHandler::Type*>(element);
  }
  template <typename TypeHandler>
  static inline const typename TypeHandler::Type* cast(const void* element) {
    return reinterpret_cast<const typename TypeHandler::Type*>(element);
  }

  // Non-templated inner function to avoid code duplication. Takes a function
  // pointer to the type-specific (templated) inner allocate/merge loop.
  void MergeFromInternal(const RepeatedPtrFieldBase& other,
                         void (RepeatedPtrFieldBase::*inner_loop)(void**,
                                                                  void**, int,
                                                                  int));

  template <typename TypeHandler>
  void MergeFromInnerLoop(void** our_elems, void** other_elems, int length,
                          int already_allocated);

  // Internal helper: extend array space if necessary to contain |extend_amount|
  // more elements, and return a pointer to the element immediately following
  // the old list of elements.  This interface factors out common behavior from
  // Reserve() and MergeFrom() to reduce code size. |extend_amount| must be > 0.
  void** InternalExtend(int extend_amount);

  // The reflection implementation needs to call protected methods directly,
  // reinterpreting pointers as being to Message instead of a specific Message
  // subclass.
  friend class ::PROTOBUF_NAMESPACE_ID::Reflection;

  // ExtensionSet stores repeated message extensions as
  // RepeatedPtrField<MessageLite>, but non-lite ExtensionSets need to implement
  // SpaceUsedLong(), and thus need to call SpaceUsedExcludingSelfLong()
  // reinterpreting MessageLite as Message.  ExtensionSet also needs to make use
  // of AddFromCleared(), which is not part of the public interface.
  friend class ExtensionSet;

  // The MapFieldBase implementation needs to call protected methods directly,
  // reinterpreting pointers as being to Message instead of a specific Message
  // subclass.
  friend class MapFieldBase;

  // The table-driven MergePartialFromCodedStream implementation needs to
  // operate on RepeatedPtrField<MessageLite>.
  friend class MergePartialFromCodedStreamHelper;

  // To parse directly into a proto2 generated class, the upb class GMR_Handlers
  // needs to be able to modify a RepeatedPtrFieldBase directly.
  friend class upb::google_opensource::GMR_Handlers;

  friend class AccessorHelper;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RepeatedPtrFieldBase);
};

template <typename GenericType>
class GenericTypeHandler {
 public:
  typedef GenericType Type;
  typedef GenericType WeakType;
  using Movable = IsMovable<GenericType>;

  static inline GenericType* New(Arena* arena) {
    return Arena::CreateMaybeMessage<Type>(arena);
  }
  static inline GenericType* New(Arena* arena, GenericType&& value) {
    return Arena::Create<GenericType>(arena, std::move(value));
  }
  static inline GenericType* NewFromPrototype(const GenericType* prototype,
                                              Arena* arena = NULL);
  static inline void Delete(GenericType* value, Arena* arena) {
    if (arena == NULL) {
      delete value;
    }
  }
  static inline Arena* GetArena(GenericType* value) {
    return Arena::GetArena<Type>(value);
  }
  static inline void* GetMaybeArenaPointer(GenericType* value) {
    return Arena::GetArena<Type>(value);
  }

  static inline void Clear(GenericType* value) { value->Clear(); }
  PROTOBUF_NOINLINE
  static void Merge(const GenericType& from, GenericType* to);
  static inline size_t SpaceUsedLong(const GenericType& value) {
    return value.SpaceUsedLong();
  }
};

template <typename GenericType>
GenericType* GenericTypeHandler<GenericType>::NewFromPrototype(
    const GenericType* /* prototype */, Arena* arena) {
  return New(arena);
}
template <typename GenericType>
void GenericTypeHandler<GenericType>::Merge(const GenericType& from,
                                            GenericType* to) {
  to->MergeFrom(from);
}

// NewFromPrototype() and Merge() are not defined inline here, as we will need
// to do a virtual function dispatch anyways to go from Message* to call
// New/Merge.
template <>
MessageLite* GenericTypeHandler<MessageLite>::NewFromPrototype(
    const MessageLite* prototype, Arena* arena);
template <>
inline Arena* GenericTypeHandler<MessageLite>::GetArena(MessageLite* value) {
  return value->GetArena();
}
template <>
inline void* GenericTypeHandler<MessageLite>::GetMaybeArenaPointer(
    MessageLite* value) {
  return value->GetMaybeArenaPointer();
}
template <>
void GenericTypeHandler<MessageLite>::Merge(const MessageLite& from,
                                            MessageLite* to);
template <>
inline void GenericTypeHandler<std::string>::Clear(std::string* value) {
  value->clear();
}
template <>
void GenericTypeHandler<std::string>::Merge(const std::string& from,
                                            std::string* to);

// Declarations of the specialization as we cannot define them here, as the
// header that defines ProtocolMessage depends on types defined in this header.
#define DECLARE_SPECIALIZATIONS_FOR_BASE_PROTO_TYPES(TypeName)              \
  template <>                                                               \
  PROTOBUF_EXPORT TypeName* GenericTypeHandler<TypeName>::NewFromPrototype( \
      const TypeName* prototype, Arena* arena);                             \
  template <>                                                               \
  PROTOBUF_EXPORT Arena* GenericTypeHandler<TypeName>::GetArena(            \
      TypeName* value);                                                     \
  template <>                                                               \
  PROTOBUF_EXPORT void* GenericTypeHandler<TypeName>::GetMaybeArenaPointer( \
      TypeName* value);

// Message specialization bodies defined in message.cc. This split is necessary
// to allow proto2-lite (which includes this header) to be independent of
// Message.
DECLARE_SPECIALIZATIONS_FOR_BASE_PROTO_TYPES(Message)


#undef DECLARE_SPECIALIZATIONS_FOR_BASE_PROTO_TYPES

class StringTypeHandler {
 public:
  typedef std::string Type;
  typedef std::string WeakType;
  using Movable = IsMovable<Type>;

  static inline std::string* New(Arena* arena) {
    return Arena::Create<std::string>(arena);
  }
  static inline std::string* New(Arena* arena, std::string&& value) {
    return Arena::Create<std::string>(arena, std::move(value));
  }
  static inline std::string* NewFromPrototype(const std::string*,
                                              Arena* arena) {
    return New(arena);
  }
  static inline Arena* GetArena(std::string*) { return NULL; }
  static inline void* GetMaybeArenaPointer(std::string* /* value */) {
    return NULL;
  }
  static inline void Delete(std::string* value, Arena* arena) {
    if (arena == NULL) {
      delete value;
    }
  }
  static inline void Clear(std::string* value) { value->clear(); }
  static inline void Merge(const std::string& from, std::string* to) {
    *to = from;
  }
  static size_t SpaceUsedLong(const std::string& value) {
    return sizeof(value) + StringSpaceUsedExcludingSelfLong(value);
  }
};

}  // namespace internal

// RepeatedPtrField is like RepeatedField, but used for repeated strings or
// Messages.
template <typename Element>
class RepeatedPtrField final : private internal::RepeatedPtrFieldBase {
 public:
  RepeatedPtrField();
  explicit RepeatedPtrField(Arena* arena);

  RepeatedPtrField(const RepeatedPtrField& other);
  template <typename Iter>
  RepeatedPtrField(Iter begin, const Iter& end);
  ~RepeatedPtrField();

  RepeatedPtrField& operator=(const RepeatedPtrField& other);

  RepeatedPtrField(RepeatedPtrField&& other) noexcept;
  RepeatedPtrField& operator=(RepeatedPtrField&& other) noexcept;

  bool empty() const;
  int size() const;

  const Element& Get(int index) const;
  Element* Mutable(int index);
  Element* Add();
  void Add(Element&& value);

  const Element& operator[](int index) const { return Get(index); }
  Element& operator[](int index) { return *Mutable(index); }

  const Element& at(int index) const;
  Element& at(int index);

  // Remove the last element in the array.
  // Ownership of the element is retained by the array.
  void RemoveLast();

  // Delete elements with indices in the range [start .. start+num-1].
  // Caution: implementation moves all elements with indices [start+num .. ].
  // Calling this routine inside a loop can cause quadratic behavior.
  void DeleteSubrange(int start, int num);

  void Clear();
  void MergeFrom(const RepeatedPtrField& other);
  void CopyFrom(const RepeatedPtrField& other);

  // Reserve space to expand the field to at least the given size.  This only
  // resizes the pointer array; it doesn't allocate any objects.  If the
  // array is grown, it will always be at least doubled in size.
  void Reserve(int new_size);

  int Capacity() const;

  // Gets the underlying array.  This pointer is possibly invalidated by
  // any add or remove operation.
  Element** mutable_data();
  const Element* const* data() const;

  // Swap entire contents with "other". If they are on separate arenas, then
  // copies data.
  void Swap(RepeatedPtrField* other);

  // Swap entire contents with "other". Caller should guarantee that either both
  // fields are on the same arena or both are on the heap. Swapping between
  // different arenas with this function is disallowed and is caught via
  // GOOGLE_DCHECK.
  void UnsafeArenaSwap(RepeatedPtrField* other);

  // Swap two elements.
  void SwapElements(int index1, int index2);

  // STL-like iterator support
  typedef internal::RepeatedPtrIterator<Element> iterator;
  typedef internal::RepeatedPtrIterator<const Element> const_iterator;
  typedef Element value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef int size_type;
  typedef ptrdiff_t difference_type;

  iterator begin();
  const_iterator begin() const;
  const_iterator cbegin() const;
  iterator end();
  const_iterator end() const;
  const_iterator cend() const;

  // Reverse iterator support
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // Custom STL-like iterator that iterates over and returns the underlying
  // pointers to Element rather than Element itself.
  typedef internal::RepeatedPtrOverPtrsIterator<Element*, void*>
      pointer_iterator;
  typedef internal::RepeatedPtrOverPtrsIterator<const Element* const,
                                                const void* const>
      const_pointer_iterator;
  pointer_iterator pointer_begin();
  const_pointer_iterator pointer_begin() const;
  pointer_iterator pointer_end();
  const_pointer_iterator pointer_end() const;

  // Returns (an estimate of) the number of bytes used by the repeated field,
  // excluding sizeof(*this).
  size_t SpaceUsedExcludingSelfLong() const;

  int SpaceUsedExcludingSelf() const {
    return internal::ToIntSize(SpaceUsedExcludingSelfLong());
  }

  // Advanced memory management --------------------------------------
  // When hardcore memory management becomes necessary -- as it sometimes
  // does here at Google -- the following methods may be useful.

  // Add an already-allocated object, passing ownership to the
  // RepeatedPtrField.
  //
  // Note that some special behavior occurs with respect to arenas:
  //
  //   (i) if this field holds submessages, the new submessage will be copied if
  //   the original is in an arena and this RepeatedPtrField is either in a
  //   different arena, or on the heap.
  //   (ii) if this field holds strings, the passed-in string *must* be
  //   heap-allocated, not arena-allocated. There is no way to dynamically check
  //   this at runtime, so User Beware.
  void AddAllocated(Element* value);

  // Remove the last element and return it, passing ownership to the caller.
  // Requires:  size() > 0
  //
  // If this RepeatedPtrField is on an arena, an object copy is required to pass
  // ownership back to the user (for compatible semantics). Use
  // UnsafeArenaReleaseLast() if this behavior is undesired.
  Element* ReleaseLast();

  // Add an already-allocated object, skipping arena-ownership checks. The user
  // must guarantee that the given object is in the same arena as this
  // RepeatedPtrField.
  // It is also useful in legacy code that uses temporary ownership to avoid
  // copies. Example:
  //   RepeatedPtrField<T> temp_field;
  //   temp_field.AddAllocated(new T);
  //   ... // Do something with temp_field
  //   temp_field.ExtractSubrange(0, temp_field.size(), nullptr);
  // If you put temp_field on the arena this fails, because the ownership
  // transfers to the arena at the "AddAllocated" call and is not released
  // anymore causing a double delete. UnsafeArenaAddAllocated prevents this.
  void UnsafeArenaAddAllocated(Element* value);

  // Remove the last element and return it.  Works only when operating on an
  // arena. The returned pointer is to the original object in the arena, hence
  // has the arena's lifetime.
  // Requires:  current_size_ > 0
  Element* UnsafeArenaReleaseLast();

  // Extract elements with indices in the range "[start .. start+num-1]".
  // The caller assumes ownership of the extracted elements and is responsible
  // for deleting them when they are no longer needed.
  // If "elements" is non-NULL, then pointers to the extracted elements
  // are stored in "elements[0 .. num-1]" for the convenience of the caller.
  // If "elements" is NULL, then the caller must use some other mechanism
  // to perform any further operations (like deletion) on these elements.
  // Caution: implementation also moves elements with indices [start+num ..].
  // Calling this routine inside a loop can cause quadratic behavior.
  //
  // Memory copying behavior is identical to ReleaseLast(), described above: if
  // this RepeatedPtrField is on an arena, an object copy is performed for each
  // returned element, so that all returned element pointers are to
  // heap-allocated copies. If this copy is not desired, the user should call
  // UnsafeArenaExtractSubrange().
  void ExtractSubrange(int start, int num, Element** elements);

  // Identical to ExtractSubrange() described above, except that when this
  // repeated field is on an arena, no object copies are performed. Instead, the
  // raw object pointers are returned. Thus, if on an arena, the returned
  // objects must not be freed, because they will not be heap-allocated objects.
  void UnsafeArenaExtractSubrange(int start, int num, Element** elements);

  // When elements are removed by calls to RemoveLast() or Clear(), they
  // are not actually freed.  Instead, they are cleared and kept so that
  // they can be reused later.  This can save lots of CPU time when
  // repeatedly reusing a protocol message for similar purposes.
  //
  // Hardcore programs may choose to manipulate these cleared objects
  // to better optimize memory management using the following routines.

  // Get the number of cleared objects that are currently being kept
  // around for reuse.
  int ClearedCount() const;
  // Add an element to the pool of cleared objects, passing ownership to
  // the RepeatedPtrField.  The element must be cleared prior to calling
  // this method.
  //
  // This method cannot be called when the repeated field is on an arena or when
  // |value| is; both cases will trigger a GOOGLE_DCHECK-failure.
  void AddCleared(Element* value);
  // Remove a single element from the cleared pool and return it, passing
  // ownership to the caller.  The element is guaranteed to be cleared.
  // Requires:  ClearedCount() > 0
  //
  //
  // This method cannot be called when the repeated field is on an arena; doing
  // so will trigger a GOOGLE_DCHECK-failure.
  Element* ReleaseCleared();

  // Removes the element referenced by position.
  //
  // Returns an iterator to the element immediately following the removed
  // element.
  //
  // Invalidates all iterators at or after the removed element, including end().
  iterator erase(const_iterator position);

  // Removes the elements in the range [first, last).
  //
  // Returns an iterator to the element immediately following the removed range.
  //
  // Invalidates all iterators at or after the removed range, including end().
  iterator erase(const_iterator first, const_iterator last);

  // Gets the arena on which this RepeatedPtrField stores its elements.
  Arena* GetArena() const { return GetArenaNoVirtual(); }

  // For internal use only.
  //
  // This is public due to it being called by generated code.
  using RepeatedPtrFieldBase::InternalSwap;

 private:
  // Note:  RepeatedPtrField SHOULD NOT be subclassed by users.
  class TypeHandler;

  // Internal arena accessor expected by helpers in Arena.
  inline Arena* GetArenaNoVirtual() const;

  // Implementations for ExtractSubrange(). The copying behavior must be
  // included only if the type supports the necessary operations (e.g.,
  // MergeFrom()), so we must resolve this at compile time. ExtractSubrange()
  // uses SFINAE to choose one of the below implementations.
  void ExtractSubrangeInternal(int start, int num, Element** elements,
                               std::true_type);
  void ExtractSubrangeInternal(int start, int num, Element** elements,
                               std::false_type);

  friend class Arena;
  friend class MessageLite;

  typedef void InternalArenaConstructable_;

};

// implementation ====================================================

template <typename Element>
inline RepeatedField<Element>::RepeatedField()
    : current_size_(0), total_size_(0), arena_or_elements_(nullptr) {}

template <typename Element>
inline RepeatedField<Element>::RepeatedField(Arena* arena)
    : current_size_(0), total_size_(0), arena_or_elements_(arena) {}

template <typename Element>
inline RepeatedField<Element>::RepeatedField(const RepeatedField& other)
    : current_size_(0), total_size_(0), arena_or_elements_(nullptr) {
  if (other.current_size_ != 0) {
    Reserve(other.size());
    AddNAlreadyReserved(other.size());
    CopyArray(Mutable(0), &other.Get(0), other.size());
  }
}

template <typename Element>
template <typename Iter>
RepeatedField<Element>::RepeatedField(Iter begin, const Iter& end)
    : current_size_(0), total_size_(0), arena_or_elements_(nullptr) {
  Add(begin, end);
}

template <typename Element>
RepeatedField<Element>::~RepeatedField() {
  if (total_size_ > 0) {
    InternalDeallocate(rep(), total_size_);
  }
}

template <typename Element>
inline RepeatedField<Element>& RepeatedField<Element>::operator=(
    const RepeatedField& other) {
  if (this != &other) CopyFrom(other);
  return *this;
}

template <typename Element>
inline RepeatedField<Element>::RepeatedField(RepeatedField&& other) noexcept
    : RepeatedField() {
  // We don't just call Swap(&other) here because it would perform 3 copies if
  // other is on an arena. This field can't be on an arena because arena
  // construction always uses the Arena* accepting constructor.
  if (other.GetArenaNoVirtual()) {
    CopyFrom(other);
  } else {
    InternalSwap(&other);
  }
}

template <typename Element>
inline RepeatedField<Element>& RepeatedField<Element>::operator=(
    RepeatedField&& other) noexcept {
  // We don't just call Swap(&other) here because it would perform 3 copies if
  // the two fields are on different arenas.
  if (this != &other) {
    if (this->GetArenaNoVirtual() != other.GetArenaNoVirtual()) {
      CopyFrom(other);
    } else {
      InternalSwap(&other);
    }
  }
  return *this;
}

template <typename Element>
inline bool RepeatedField<Element>::empty() const {
  return current_size_ == 0;
}

template <typename Element>
inline int RepeatedField<Element>::size() const {
  return current_size_;
}

template <typename Element>
inline int RepeatedField<Element>::Capacity() const {
  return total_size_;
}

template <typename Element>
inline void RepeatedField<Element>::AddAlreadyReserved(const Element& value) {
  GOOGLE_DCHECK_LT(current_size_, total_size_);
  elements()[current_size_++] = value;
}

template <typename Element>
inline Element* RepeatedField<Element>::AddAlreadyReserved() {
  GOOGLE_DCHECK_LT(current_size_, total_size_);
  return &elements()[current_size_++];
}

template <typename Element>
inline Element* RepeatedField<Element>::AddNAlreadyReserved(int n) {
  GOOGLE_DCHECK_GE(total_size_ - current_size_, n)
      << total_size_ << ", " << current_size_;
  // Warning: sometimes people call this when n == 0 and total_size_ == 0. In
  // this case the return pointer points to a zero size array (n == 0). Hence
  // we can just use unsafe_elements(), because the user cannot dereference the
  // pointer anyway.
  Element* ret = unsafe_elements() + current_size_;
  current_size_ += n;
  return ret;
}

template <typename Element>
inline void RepeatedField<Element>::Resize(int new_size, const Element& value) {
  GOOGLE_DCHECK_GE(new_size, 0);
  if (new_size > current_size_) {
    Reserve(new_size);
    std::fill(&elements()[current_size_], &elements()[new_size], value);
  }
  current_size_ = new_size;
}

template <typename Element>
inline const Element& RepeatedField<Element>::Get(int index) const {
  GOOGLE_DCHECK_GE(index, 0);
  GOOGLE_DCHECK_LT(index, current_size_);
  return elements()[index];
}

template <typename Element>
inline const Element& RepeatedField<Element>::at(int index) const {
  GOOGLE_CHECK_GE(index, 0);
  GOOGLE_CHECK_LT(index, current_size_);
  return elements()[index];
}

template <typename Element>
inline Element& RepeatedField<Element>::at(int index) {
  GOOGLE_CHECK_GE(index, 0);
  GOOGLE_CHECK_LT(index, current_size_);
  return elements()[index];
}

template <typename Element>
inline Element* RepeatedField<Element>::Mutable(int index) {
  GOOGLE_DCHECK_GE(index, 0);
  GOOGLE_DCHECK_LT(index, current_size_);
  return &elements()[index];
}

template <typename Element>
inline void RepeatedField<Element>::Set(int index, const Element& value) {
  GOOGLE_DCHECK_GE(index, 0);
  GOOGLE_DCHECK_LT(index, current_size_);
  elements()[index] = value;
}

template <typename Element>
inline void RepeatedField<Element>::Add(const Element& value) {
  if (current_size_ == total_size_) Reserve(total_size_ + 1);
  elements()[current_size_++] = value;
}

template <typename Element>
inline Element* RepeatedField<Element>::Add() {
  if (current_size_ == total_size_) Reserve(total_size_ + 1);
  return &elements()[current_size_++];
}

template <typename Element>
template <typename Iter>
inline void RepeatedField<Element>::Add(Iter begin, Iter end) {
  int reserve = internal::CalculateReserve(begin, end);
  if (reserve != -1) {
    if (reserve == 0) {
      return;
    }

    Reserve(reserve + size());
    // TODO(ckennelly):  The compiler loses track of the buffer freshly
    // allocated by Reserve() by the time we call elements, so it cannot
    // guarantee that elements does not alias [begin(), end()).
    //
    // If restrict is available, annotating the pointer obtained from elements()
    // causes this to lower to memcpy instead of memmove.
    std::copy(begin, end, elements() + size());
    current_size_ = reserve + size();
  } else {
    for (; begin != end; ++begin) {
      Add(*begin);
    }
  }
}

template <typename Element>
inline void RepeatedField<Element>::RemoveLast() {
  GOOGLE_DCHECK_GT(current_size_, 0);
  current_size_--;
}

template <typename Element>
void RepeatedField<Element>::ExtractSubrange(int start, int num,
                                             Element* elements) {
  GOOGLE_DCHECK_GE(start, 0);
  GOOGLE_DCHECK_GE(num, 0);
  GOOGLE_DCHECK_LE(start + num, this->current_size_);

  // Save the values of the removed elements if requested.
  if (elements != NULL) {
    for (int i = 0; i < num; ++i) elements[i] = this->Get(i + start);
  }

  // Slide remaining elements down to fill the gap.
  if (num > 0) {
    for (int i = start + num; i < this->current_size_; ++i)
      this->Set(i - num, this->Get(i));
    this->Truncate(this->current_size_ - num);
  }
}

template <typename Element>
inline void RepeatedField<Element>::Clear() {
  current_size_ = 0;
}

template <typename Element>
inline void RepeatedField<Element>::MergeFrom(const RepeatedField& other) {
  GOOGLE_DCHECK_NE(&other, this);
  if (other.current_size_ != 0) {
    int existing_size = size();
    Reserve(existing_size + other.size());
    AddNAlreadyReserved(other.size());
    CopyArray(Mutable(existing_size), &other.Get(0), other.size());
  }
}

template <typename Element>
inline void RepeatedField<Element>::CopyFrom(const RepeatedField& other) {
  if (&other == this) return;
  Clear();
  MergeFrom(other);
}

template <typename Element>
inline typename RepeatedField<Element>::iterator RepeatedField<Element>::erase(
    const_iterator position) {
  return erase(position, position + 1);
}

template <typename Element>
inline typename RepeatedField<Element>::iterator RepeatedField<Element>::erase(
    const_iterator first, const_iterator last) {
  size_type first_offset = first - cbegin();
  if (first != last) {
    Truncate(std::copy(last, cend(), begin() + first_offset) - cbegin());
  }
  return begin() + first_offset;
}

template <typename Element>
inline Element* RepeatedField<Element>::mutable_data() {
  return unsafe_elements();
}

template <typename Element>
inline const Element* RepeatedField<Element>::data() const {
  return unsafe_elements();
}

template <typename Element>
inline void RepeatedField<Element>::InternalSwap(RepeatedField* other) {
  GOOGLE_DCHECK(this != other);
  GOOGLE_DCHECK(GetArenaNoVirtual() == other->GetArenaNoVirtual());

  std::swap(arena_or_elements_, other->arena_or_elements_);
  std::swap(current_size_, other->current_size_);
  std::swap(total_size_, other->total_size_);
}

template <typename Element>
void RepeatedField<Element>::Swap(RepeatedField* other) {
  if (this == other) return;
  if (GetArenaNoVirtual() == other->GetArenaNoVirtual()) {
    InternalSwap(other);
  } else {
    RepeatedField<Element> temp(other->GetArenaNoVirtual());
    temp.MergeFrom(*this);
    CopyFrom(*other);
    other->UnsafeArenaSwap(&temp);
  }
}

template <typename Element>
void RepeatedField<Element>::UnsafeArenaSwap(RepeatedField* other) {
  if (this == other) return;
  InternalSwap(other);
}

template <typename Element>
void RepeatedField<Element>::SwapElements(int index1, int index2) {
  using std::swap;  // enable ADL with fallback
  swap(elements()[index1], elements()[index2]);
}

template <typename Element>
inline typename RepeatedField<Element>::iterator
RepeatedField<Element>::begin() {
  return unsafe_elements();
}
template <typename Element>
inline typename RepeatedField<Element>::const_iterator
RepeatedField<Element>::begin() const {
  return unsafe_elements();
}
template <typename Element>
inline typename RepeatedField<Element>::const_iterator
RepeatedField<Element>::cbegin() const {
  return unsafe_elements();
}
template <typename Element>
inline typename RepeatedField<Element>::iterator RepeatedField<Element>::end() {
  return unsafe_elements() + current_size_;
}
template <typename Element>
inline typename RepeatedField<Element>::const_iterator
RepeatedField<Element>::end() const {
  return unsafe_elements() + current_size_;
}
template <typename Element>
inline typename RepeatedField<Element>::const_iterator
RepeatedField<Element>::cend() const {
  return unsafe_elements() + current_size_;
}

template <typename Element>
inline size_t RepeatedField<Element>::SpaceUsedExcludingSelfLong() const {
  return total_size_ > 0 ? (total_size_ * sizeof(Element) + kRepHeaderSize) : 0;
}

// Avoid inlining of Reserve(): new, copy, and delete[] lead to a significant
// amount of code bloat.
template <typename Element>
void RepeatedField<Element>::Reserve(int new_size) {
  if (total_size_ >= new_size) return;
  Rep* old_rep = total_size_ > 0 ? rep() : NULL;
  Rep* new_rep;
  Arena* arena = GetArenaNoVirtual();
  new_size = std::max(internal::kMinRepeatedFieldAllocationSize,
                      std::max(total_size_ * 2, new_size));
  GOOGLE_DCHECK_LE(
      static_cast<size_t>(new_size),
      (std::numeric_limits<size_t>::max() - kRepHeaderSize) / sizeof(Element))
      << "Requested size is too large to fit into size_t.";
  size_t bytes =
      kRepHeaderSize + sizeof(Element) * static_cast<size_t>(new_size);
  if (arena == NULL) {
    new_rep = static_cast<Rep*>(::operator new(bytes));
  } else {
    new_rep = reinterpret_cast<Rep*>(Arena::CreateArray<char>(arena, bytes));
  }
  new_rep->arena = arena;
  int old_total_size = total_size_;
  total_size_ = new_size;
  arena_or_elements_ = new_rep->elements;
  // Invoke placement-new on newly allocated elements. We shouldn't have to do
  // this, since Element is supposed to be POD, but a previous version of this
  // code allocated storage with "new Element[size]" and some code uses
  // RepeatedField with non-POD types, relying on constructor invocation. If
  // Element has a trivial constructor (e.g., int32), gcc (tested with -O2)
  // completely removes this loop because the loop body is empty, so this has no
  // effect unless its side-effects are required for correctness.
  // Note that we do this before MoveArray() below because Element's copy
  // assignment implementation will want an initialized instance first.
  Element* e = &elements()[0];
  Element* limit = e + total_size_;
  for (; e < limit; e++) {
    new (e) Element;
  }
  if (current_size_ > 0) {
    MoveArray(&elements()[0], old_rep->elements, current_size_);
  }

  // Likewise, we need to invoke destructors on the old array.
  InternalDeallocate(old_rep, old_total_size);

}

template <typename Element>
inline void RepeatedField<Element>::Truncate(int new_size) {
  GOOGLE_DCHECK_LE(new_size, current_size_);
  if (current_size_ > 0) {
    current_size_ = new_size;
  }
}

template <typename Element>
inline void RepeatedField<Element>::MoveArray(Element* to, Element* from,
                                              int array_size) {
  CopyArray(to, from, array_size);
}

template <typename Element>
inline void RepeatedField<Element>::CopyArray(Element* to, const Element* from,
                                              int array_size) {
  internal::ElementCopier<Element>()(to, from, array_size);
}

namespace internal {

template <typename Element, bool HasTrivialCopy>
void ElementCopier<Element, HasTrivialCopy>::operator()(Element* to,
                                                        const Element* from,
                                                        int array_size) {
  std::copy(from, from + array_size, to);
}

template <typename Element>
struct ElementCopier<Element, true> {
  void operator()(Element* to, const Element* from, int array_size) {
    memcpy(to, from, static_cast<size_t>(array_size) * sizeof(Element));
  }
};

}  // namespace internal


// -------------------------------------------------------------------

namespace internal {

inline RepeatedPtrFieldBase::RepeatedPtrFieldBase()
    : arena_(NULL), current_size_(0), total_size_(0), rep_(NULL) {}

inline RepeatedPtrFieldBase::RepeatedPtrFieldBase(Arena* arena)
    : arena_(arena), current_size_(0), total_size_(0), rep_(NULL) {}

template <typename TypeHandler>
void RepeatedPtrFieldBase::Destroy() {
  if (rep_ != NULL && arena_ == NULL) {
    int n = rep_->allocated_size;
    void* const* elements = rep_->elements;
    for (int i = 0; i < n; i++) {
      TypeHandler::Delete(cast<TypeHandler>(elements[i]), NULL);
    }
#if defined(__GXX_DELETE_WITH_SIZE__) || defined(__cpp_sized_deallocation)
    const size_t size = total_size_ * sizeof(elements[0]) + kRepHeaderSize;
    ::operator delete(static_cast<void*>(rep_), size);
#else
    ::operator delete(static_cast<void*>(rep_));
#endif
  }
  rep_ = NULL;
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::Swap(RepeatedPtrFieldBase* other) {
  if (other->GetArenaNoVirtual() == GetArenaNoVirtual()) {
    InternalSwap(other);
  } else {
    SwapFallback<TypeHandler>(other);
  }
}

template <typename TypeHandler>
void RepeatedPtrFieldBase::SwapFallback(RepeatedPtrFieldBase* other) {
  GOOGLE_DCHECK(other->GetArenaNoVirtual() != GetArenaNoVirtual());

  // Copy semantics in this case. We try to improve efficiency by placing the
  // temporary on |other|'s arena so that messages are copied cross-arena only
  // once, not twice.
  RepeatedPtrFieldBase temp(other->GetArenaNoVirtual());
  temp.MergeFrom<TypeHandler>(*this);
  this->Clear<TypeHandler>();
  this->MergeFrom<TypeHandler>(*other);
  other->Clear<TypeHandler>();
  other->InternalSwap(&temp);
  temp.Destroy<TypeHandler>();  // Frees rep_ if `other` had no arena.
}

inline bool RepeatedPtrFieldBase::empty() const { return current_size_ == 0; }

inline int RepeatedPtrFieldBase::size() const { return current_size_; }

template <typename TypeHandler>
inline const typename TypeHandler::WeakType& RepeatedPtrFieldBase::Get(
    int index) const {
  GOOGLE_DCHECK_GE(index, 0);
  GOOGLE_DCHECK_LT(index, current_size_);
  return *cast<TypeHandler>(rep_->elements[index]);
}

template <typename TypeHandler>
inline const typename TypeHandler::Type& RepeatedPtrFieldBase::at(
    int index) const {
  GOOGLE_CHECK_GE(index, 0);
  GOOGLE_CHECK_LT(index, current_size_);
  return *cast<TypeHandler>(rep_->elements[index]);
}

template <typename TypeHandler>
inline typename TypeHandler::Type& RepeatedPtrFieldBase::at(int index) {
  GOOGLE_CHECK_GE(index, 0);
  GOOGLE_CHECK_LT(index, current_size_);
  return *cast<TypeHandler>(rep_->elements[index]);
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::Mutable(int index) {
  GOOGLE_DCHECK_GE(index, 0);
  GOOGLE_DCHECK_LT(index, current_size_);
  return cast<TypeHandler>(rep_->elements[index]);
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::Delete(int index) {
  GOOGLE_DCHECK_GE(index, 0);
  GOOGLE_DCHECK_LT(index, current_size_);
  TypeHandler::Delete(cast<TypeHandler>(rep_->elements[index]), arena_);
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::Add(
    typename TypeHandler::Type* prototype) {
  if (rep_ != NULL && current_size_ < rep_->allocated_size) {
    return cast<TypeHandler>(rep_->elements[current_size_++]);
  }
  if (!rep_ || rep_->allocated_size == total_size_) {
    Reserve(total_size_ + 1);
  }
  ++rep_->allocated_size;
  typename TypeHandler::Type* result =
      TypeHandler::NewFromPrototype(prototype, arena_);
  rep_->elements[current_size_++] = result;
  return result;
}

template <typename TypeHandler,
          typename std::enable_if<TypeHandler::Movable::value>::type*>
inline void RepeatedPtrFieldBase::Add(typename TypeHandler::Type&& value) {
  if (rep_ != NULL && current_size_ < rep_->allocated_size) {
    *cast<TypeHandler>(rep_->elements[current_size_++]) = std::move(value);
    return;
  }
  if (!rep_ || rep_->allocated_size == total_size_) {
    Reserve(total_size_ + 1);
  }
  ++rep_->allocated_size;
  typename TypeHandler::Type* result =
      TypeHandler::New(arena_, std::move(value));
  rep_->elements[current_size_++] = result;
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::RemoveLast() {
  GOOGLE_DCHECK_GT(current_size_, 0);
  TypeHandler::Clear(cast<TypeHandler>(rep_->elements[--current_size_]));
}

template <typename TypeHandler>
void RepeatedPtrFieldBase::Clear() {
  const int n = current_size_;
  GOOGLE_DCHECK_GE(n, 0);
  if (n > 0) {
    void* const* elements = rep_->elements;
    int i = 0;
    do {
      TypeHandler::Clear(cast<TypeHandler>(elements[i++]));
    } while (i < n);
    current_size_ = 0;
  }
}

// To avoid unnecessary code duplication and reduce binary size, we use a
// layered approach to implementing MergeFrom(). The toplevel method is
// templated, so we get a small thunk per concrete message type in the binary.
// This calls a shared implementation with most of the logic, passing a function
// pointer to another type-specific piece of code that calls the object-allocate
// and merge handlers.
template <typename TypeHandler>
inline void RepeatedPtrFieldBase::MergeFrom(const RepeatedPtrFieldBase& other) {
  GOOGLE_DCHECK_NE(&other, this);
  if (other.current_size_ == 0) return;
  MergeFromInternal(other,
                    &RepeatedPtrFieldBase::MergeFromInnerLoop<TypeHandler>);
}

inline void RepeatedPtrFieldBase::MergeFromInternal(
    const RepeatedPtrFieldBase& other,
    void (RepeatedPtrFieldBase::*inner_loop)(void**, void**, int, int)) {
  // Note: wrapper has already guaranteed that other.rep_ != NULL here.
  int other_size = other.current_size_;
  void** other_elements = other.rep_->elements;
  void** new_elements = InternalExtend(other_size);
  int allocated_elems = rep_->allocated_size - current_size_;
  (this->*inner_loop)(new_elements, other_elements, other_size,
                      allocated_elems);
  current_size_ += other_size;
  if (rep_->allocated_size < current_size_) {
    rep_->allocated_size = current_size_;
  }
}

// Merges other_elems to our_elems.
template <typename TypeHandler>
void RepeatedPtrFieldBase::MergeFromInnerLoop(void** our_elems,
                                              void** other_elems, int length,
                                              int already_allocated) {
  // Split into two loops, over ranges [0, allocated) and [allocated, length),
  // to avoid a branch within the loop.
  for (int i = 0; i < already_allocated && i < length; i++) {
    // Already allocated: use existing element.
    typename TypeHandler::WeakType* other_elem =
        reinterpret_cast<typename TypeHandler::WeakType*>(other_elems[i]);
    typename TypeHandler::WeakType* new_elem =
        reinterpret_cast<typename TypeHandler::WeakType*>(our_elems[i]);
    TypeHandler::Merge(*other_elem, new_elem);
  }
  Arena* arena = GetArenaNoVirtual();
  for (int i = already_allocated; i < length; i++) {
    // Not allocated: alloc a new element first, then merge it.
    typename TypeHandler::WeakType* other_elem =
        reinterpret_cast<typename TypeHandler::WeakType*>(other_elems[i]);
    typename TypeHandler::WeakType* new_elem =
        TypeHandler::NewFromPrototype(other_elem, arena);
    TypeHandler::Merge(*other_elem, new_elem);
    our_elems[i] = new_elem;
  }
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::CopyFrom(const RepeatedPtrFieldBase& other) {
  if (&other == this) return;
  RepeatedPtrFieldBase::Clear<TypeHandler>();
  RepeatedPtrFieldBase::MergeFrom<TypeHandler>(other);
}

inline int RepeatedPtrFieldBase::Capacity() const { return total_size_; }

inline void* const* RepeatedPtrFieldBase::raw_data() const {
  return rep_ ? rep_->elements : NULL;
}

inline void** RepeatedPtrFieldBase::raw_mutable_data() const {
  return rep_ ? const_cast<void**>(rep_->elements) : NULL;
}

template <typename TypeHandler>
inline typename TypeHandler::Type** RepeatedPtrFieldBase::mutable_data() {
  // TODO(kenton):  Breaks C++ aliasing rules.  We should probably remove this
  //   method entirely.
  return reinterpret_cast<typename TypeHandler::Type**>(raw_mutable_data());
}

template <typename TypeHandler>
inline const typename TypeHandler::Type* const* RepeatedPtrFieldBase::data()
    const {
  // TODO(kenton):  Breaks C++ aliasing rules.  We should probably remove this
  //   method entirely.
  return reinterpret_cast<const typename TypeHandler::Type* const*>(raw_data());
}

inline void RepeatedPtrFieldBase::SwapElements(int index1, int index2) {
  using std::swap;  // enable ADL with fallback
  swap(rep_->elements[index1], rep_->elements[index2]);
}

template <typename TypeHandler>
inline size_t RepeatedPtrFieldBase::SpaceUsedExcludingSelfLong() const {
  size_t allocated_bytes = static_cast<size_t>(total_size_) * sizeof(void*);
  if (rep_ != NULL) {
    for (int i = 0; i < rep_->allocated_size; ++i) {
      allocated_bytes +=
          TypeHandler::SpaceUsedLong(*cast<TypeHandler>(rep_->elements[i]));
    }
    allocated_bytes += kRepHeaderSize;
  }
  return allocated_bytes;
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::AddFromCleared() {
  if (rep_ != NULL && current_size_ < rep_->allocated_size) {
    return cast<TypeHandler>(rep_->elements[current_size_++]);
  } else {
    return NULL;
  }
}

// AddAllocated version that implements arena-safe copying behavior.
template <typename TypeHandler>
void RepeatedPtrFieldBase::AddAllocatedInternal(
    typename TypeHandler::Type* value, std::true_type) {
  Arena* element_arena =
      reinterpret_cast<Arena*>(TypeHandler::GetMaybeArenaPointer(value));
  Arena* arena = GetArenaNoVirtual();
  if (arena == element_arena && rep_ && rep_->allocated_size < total_size_) {
    // Fast path: underlying arena representation (tagged pointer) is equal to
    // our arena pointer, and we can add to array without resizing it (at least
    // one slot that is not allocated).
    void** elems = rep_->elements;
    if (current_size_ < rep_->allocated_size) {
      // Make space at [current] by moving first allocated element to end of
      // allocated list.
      elems[rep_->allocated_size] = elems[current_size_];
    }
    elems[current_size_] = value;
    current_size_ = current_size_ + 1;
    rep_->allocated_size = rep_->allocated_size + 1;
  } else {
    AddAllocatedSlowWithCopy<TypeHandler>(value, TypeHandler::GetArena(value),
                                          arena);
  }
}

// Slowpath handles all cases, copying if necessary.
template <typename TypeHandler>
void RepeatedPtrFieldBase::AddAllocatedSlowWithCopy(
    // Pass value_arena and my_arena to avoid duplicate virtual call (value) or
    // load (mine).
    typename TypeHandler::Type* value, Arena* value_arena, Arena* my_arena) {
  // Ensure that either the value is in the same arena, or if not, we do the
  // appropriate thing: Own() it (if it's on heap and we're in an arena) or copy
  // it to our arena/heap (otherwise).
  if (my_arena != NULL && value_arena == NULL) {
    my_arena->Own(value);
  } else if (my_arena != value_arena) {
    typename TypeHandler::Type* new_value =
        TypeHandler::NewFromPrototype(value, my_arena);
    TypeHandler::Merge(*value, new_value);
    TypeHandler::Delete(value, value_arena);
    value = new_value;
  }

  UnsafeArenaAddAllocated<TypeHandler>(value);
}

// AddAllocated version that does not implement arena-safe copying behavior.
template <typename TypeHandler>
void RepeatedPtrFieldBase::AddAllocatedInternal(
    typename TypeHandler::Type* value, std::false_type) {
  if (rep_ && rep_->allocated_size < total_size_) {
    // Fast path: underlying arena representation (tagged pointer) is equal to
    // our arena pointer, and we can add to array without resizing it (at least
    // one slot that is not allocated).
    void** elems = rep_->elements;
    if (current_size_ < rep_->allocated_size) {
      // Make space at [current] by moving first allocated element to end of
      // allocated list.
      elems[rep_->allocated_size] = elems[current_size_];
    }
    elems[current_size_] = value;
    current_size_ = current_size_ + 1;
    ++rep_->allocated_size;
  } else {
    UnsafeArenaAddAllocated<TypeHandler>(value);
  }
}

template <typename TypeHandler>
void RepeatedPtrFieldBase::UnsafeArenaAddAllocated(
    typename TypeHandler::Type* value) {
  // Make room for the new pointer.
  if (!rep_ || current_size_ == total_size_) {
    // The array is completely full with no cleared objects, so grow it.
    Reserve(total_size_ + 1);
    ++rep_->allocated_size;
  } else if (rep_->allocated_size == total_size_) {
    // There is no more space in the pointer array because it contains some
    // cleared objects awaiting reuse.  We don't want to grow the array in this
    // case because otherwise a loop calling AddAllocated() followed by Clear()
    // would leak memory.
    TypeHandler::Delete(cast<TypeHandler>(rep_->elements[current_size_]),
                        arena_);
  } else if (current_size_ < rep_->allocated_size) {
    // We have some cleared objects.  We don't care about their order, so we
    // can just move the first one to the end to make space.
    rep_->elements[rep_->allocated_size] = rep_->elements[current_size_];
    ++rep_->allocated_size;
  } else {
    // There are no cleared objects.
    ++rep_->allocated_size;
  }

  rep_->elements[current_size_++] = value;
}

// ReleaseLast() for types that implement merge/copy behavior.
template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::ReleaseLastInternal(
    std::true_type) {
  // First, release an element.
  typename TypeHandler::Type* result = UnsafeArenaReleaseLast<TypeHandler>();
  // Now perform a copy if we're on an arena.
  Arena* arena = GetArenaNoVirtual();
  if (arena == NULL) {
    return result;
  } else {
    typename TypeHandler::Type* new_result =
        TypeHandler::NewFromPrototype(result, NULL);
    TypeHandler::Merge(*result, new_result);
    return new_result;
  }
}

// ReleaseLast() for types that *do not* implement merge/copy behavior -- this
// is the same as UnsafeArenaReleaseLast(). Note that we GOOGLE_DCHECK-fail if we're on
// an arena, since the user really should implement the copy operation in this
// case.
template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::ReleaseLastInternal(
    std::false_type) {
  GOOGLE_DCHECK(GetArenaNoVirtual() == NULL)
      << "ReleaseLast() called on a RepeatedPtrField that is on an arena, "
      << "with a type that does not implement MergeFrom. This is unsafe; "
      << "please implement MergeFrom for your type.";
  return UnsafeArenaReleaseLast<TypeHandler>();
}

template <typename TypeHandler>
inline typename TypeHandler::Type*
RepeatedPtrFieldBase::UnsafeArenaReleaseLast() {
  GOOGLE_DCHECK_GT(current_size_, 0);
  typename TypeHandler::Type* result =
      cast<TypeHandler>(rep_->elements[--current_size_]);
  --rep_->allocated_size;
  if (current_size_ < rep_->allocated_size) {
    // There are cleared elements on the end; replace the removed element
    // with the last allocated element.
    rep_->elements[current_size_] = rep_->elements[rep_->allocated_size];
  }
  return result;
}

inline int RepeatedPtrFieldBase::ClearedCount() const {
  return rep_ ? (rep_->allocated_size - current_size_) : 0;
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::AddCleared(
    typename TypeHandler::Type* value) {
  GOOGLE_DCHECK(GetArenaNoVirtual() == NULL)
      << "AddCleared() can only be used on a RepeatedPtrField not on an arena.";
  GOOGLE_DCHECK(TypeHandler::GetArena(value) == NULL)
      << "AddCleared() can only accept values not on an arena.";
  if (!rep_ || rep_->allocated_size == total_size_) {
    Reserve(total_size_ + 1);
  }
  rep_->elements[rep_->allocated_size++] = value;
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::ReleaseCleared() {
  GOOGLE_DCHECK(GetArenaNoVirtual() == NULL)
      << "ReleaseCleared() can only be used on a RepeatedPtrField not on "
      << "an arena.";
  GOOGLE_DCHECK(GetArenaNoVirtual() == NULL);
  GOOGLE_DCHECK(rep_ != NULL);
  GOOGLE_DCHECK_GT(rep_->allocated_size, current_size_);
  return cast<TypeHandler>(rep_->elements[--rep_->allocated_size]);
}

}  // namespace internal

// -------------------------------------------------------------------

template <typename Element>
class RepeatedPtrField<Element>::TypeHandler
    : public internal::GenericTypeHandler<Element> {};

template <>
class RepeatedPtrField<std::string>::TypeHandler
    : public internal::StringTypeHandler {};

template <typename Element>
inline RepeatedPtrField<Element>::RepeatedPtrField() : RepeatedPtrFieldBase() {}

template <typename Element>
inline RepeatedPtrField<Element>::RepeatedPtrField(Arena* arena)
    : RepeatedPtrFieldBase(arena) {}

template <typename Element>
inline RepeatedPtrField<Element>::RepeatedPtrField(
    const RepeatedPtrField& other)
    : RepeatedPtrFieldBase() {
  MergeFrom(other);
}

template <typename Element>
template <typename Iter>
inline RepeatedPtrField<Element>::RepeatedPtrField(Iter begin,
                                                   const Iter& end) {
  int reserve = internal::CalculateReserve(begin, end);
  if (reserve != -1) {
    Reserve(reserve);
  }
  for (; begin != end; ++begin) {
    *Add() = *begin;
  }
}

template <typename Element>
RepeatedPtrField<Element>::~RepeatedPtrField() {
  Destroy<TypeHandler>();
}

template <typename Element>
inline RepeatedPtrField<Element>& RepeatedPtrField<Element>::operator=(
    const RepeatedPtrField& other) {
  if (this != &other) CopyFrom(other);
  return *this;
}

template <typename Element>
inline RepeatedPtrField<Element>::RepeatedPtrField(
    RepeatedPtrField&& other) noexcept
    : RepeatedPtrField() {
  // We don't just call Swap(&other) here because it would perform 3 copies if
  // other is on an arena. This field can't be on an arena because arena
  // construction always uses the Arena* accepting constructor.
  if (other.GetArenaNoVirtual()) {
    CopyFrom(other);
  } else {
    InternalSwap(&other);
  }
}

template <typename Element>
inline RepeatedPtrField<Element>& RepeatedPtrField<Element>::operator=(
    RepeatedPtrField&& other) noexcept {
  // We don't just call Swap(&other) here because it would perform 3 copies if
  // the two fields are on different arenas.
  if (this != &other) {
    if (this->GetArenaNoVirtual() != other.GetArenaNoVirtual()) {
      CopyFrom(other);
    } else {
      InternalSwap(&other);
    }
  }
  return *this;
}

template <typename Element>
inline bool RepeatedPtrField<Element>::empty() const {
  return RepeatedPtrFieldBase::empty();
}

template <typename Element>
inline int RepeatedPtrField<Element>::size() const {
  return RepeatedPtrFieldBase::size();
}

template <typename Element>
inline const Element& RepeatedPtrField<Element>::Get(int index) const {
  return RepeatedPtrFieldBase::Get<TypeHandler>(index);
}

template <typename Element>
inline const Element& RepeatedPtrField<Element>::at(int index) const {
  return RepeatedPtrFieldBase::at<TypeHandler>(index);
}

template <typename Element>
inline Element& RepeatedPtrField<Element>::at(int index) {
  return RepeatedPtrFieldBase::at<TypeHandler>(index);
}


template <typename Element>
inline Element* RepeatedPtrField<Element>::Mutable(int index) {
  return RepeatedPtrFieldBase::Mutable<TypeHandler>(index);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::Add() {
  return RepeatedPtrFieldBase::Add<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::Add(Element&& value) {
  RepeatedPtrFieldBase::Add<TypeHandler>(std::move(value));
}

template <typename Element>
inline void RepeatedPtrField<Element>::RemoveLast() {
  RepeatedPtrFieldBase::RemoveLast<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::DeleteSubrange(int start, int num) {
  GOOGLE_DCHECK_GE(start, 0);
  GOOGLE_DCHECK_GE(num, 0);
  GOOGLE_DCHECK_LE(start + num, size());
  for (int i = 0; i < num; ++i) {
    RepeatedPtrFieldBase::Delete<TypeHandler>(start + i);
  }
  ExtractSubrange(start, num, NULL);
}

template <typename Element>
inline void RepeatedPtrField<Element>::ExtractSubrange(int start, int num,
                                                       Element** elements) {
  typename internal::TypeImplementsMergeBehavior<
      typename TypeHandler::Type>::type t;
  ExtractSubrangeInternal(start, num, elements, t);
}

// ExtractSubrange() implementation for types that implement merge/copy
// behavior.
template <typename Element>
inline void RepeatedPtrField<Element>::ExtractSubrangeInternal(
    int start, int num, Element** elements, std::true_type) {
  GOOGLE_DCHECK_GE(start, 0);
  GOOGLE_DCHECK_GE(num, 0);
  GOOGLE_DCHECK_LE(start + num, size());

  if (num > 0) {
    // Save the values of the removed elements if requested.
    if (elements != NULL) {
      if (GetArenaNoVirtual() != NULL) {
        // If we're on an arena, we perform a copy for each element so that the
        // returned elements are heap-allocated.
        for (int i = 0; i < num; ++i) {
          Element* element =
              RepeatedPtrFieldBase::Mutable<TypeHandler>(i + start);
          typename TypeHandler::Type* new_value =
              TypeHandler::NewFromPrototype(element, NULL);
          TypeHandler::Merge(*element, new_value);
          elements[i] = new_value;
        }
      } else {
        for (int i = 0; i < num; ++i) {
          elements[i] = RepeatedPtrFieldBase::Mutable<TypeHandler>(i + start);
        }
      }
    }
    CloseGap(start, num);
  }
}

// ExtractSubrange() implementation for types that do not implement merge/copy
// behavior.
template <typename Element>
inline void RepeatedPtrField<Element>::ExtractSubrangeInternal(
    int start, int num, Element** elements, std::false_type) {
  // This case is identical to UnsafeArenaExtractSubrange(). However, since
  // ExtractSubrange() must return heap-allocated objects by contract, and we
  // cannot fulfill this contract if we are an on arena, we must GOOGLE_DCHECK() that
  // we are not on an arena.
  GOOGLE_DCHECK(GetArenaNoVirtual() == NULL)
      << "ExtractSubrange() when arena is non-NULL is only supported when "
      << "the Element type supplies a MergeFrom() operation to make copies.";
  UnsafeArenaExtractSubrange(start, num, elements);
}

template <typename Element>
inline void RepeatedPtrField<Element>::UnsafeArenaExtractSubrange(
    int start, int num, Element** elements) {
  GOOGLE_DCHECK_GE(start, 0);
  GOOGLE_DCHECK_GE(num, 0);
  GOOGLE_DCHECK_LE(start + num, size());

  if (num > 0) {
    // Save the values of the removed elements if requested.
    if (elements != NULL) {
      for (int i = 0; i < num; ++i) {
        elements[i] = RepeatedPtrFieldBase::Mutable<TypeHandler>(i + start);
      }
    }
    CloseGap(start, num);
  }
}

template <typename Element>
inline void RepeatedPtrField<Element>::Clear() {
  RepeatedPtrFieldBase::Clear<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::MergeFrom(
    const RepeatedPtrField& other) {
  RepeatedPtrFieldBase::MergeFrom<TypeHandler>(other);
}

template <typename Element>
inline void RepeatedPtrField<Element>::CopyFrom(const RepeatedPtrField& other) {
  RepeatedPtrFieldBase::CopyFrom<TypeHandler>(other);
}

template <typename Element>
inline typename RepeatedPtrField<Element>::iterator
RepeatedPtrField<Element>::erase(const_iterator position) {
  return erase(position, position + 1);
}

template <typename Element>
inline typename RepeatedPtrField<Element>::iterator
RepeatedPtrField<Element>::erase(const_iterator first, const_iterator last) {
  size_type pos_offset = std::distance(cbegin(), first);
  size_type last_offset = std::distance(cbegin(), last);
  DeleteSubrange(pos_offset, last_offset - pos_offset);
  return begin() + pos_offset;
}

template <typename Element>
inline Element** RepeatedPtrField<Element>::mutable_data() {
  return RepeatedPtrFieldBase::mutable_data<TypeHandler>();
}

template <typename Element>
inline const Element* const* RepeatedPtrField<Element>::data() const {
  return RepeatedPtrFieldBase::data<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::Swap(RepeatedPtrField* other) {
  if (this == other) return;
  RepeatedPtrFieldBase::Swap<TypeHandler>(other);
}

template <typename Element>
inline void RepeatedPtrField<Element>::UnsafeArenaSwap(
    RepeatedPtrField* other) {
  if (this == other) return;
  RepeatedPtrFieldBase::InternalSwap(other);
}

template <typename Element>
inline void RepeatedPtrField<Element>::SwapElements(int index1, int index2) {
  RepeatedPtrFieldBase::SwapElements(index1, index2);
}

template <typename Element>
inline Arena* RepeatedPtrField<Element>::GetArenaNoVirtual() const {
  return RepeatedPtrFieldBase::GetArenaNoVirtual();
}

template <typename Element>
inline size_t RepeatedPtrField<Element>::SpaceUsedExcludingSelfLong() const {
  return RepeatedPtrFieldBase::SpaceUsedExcludingSelfLong<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::AddAllocated(Element* value) {
  RepeatedPtrFieldBase::AddAllocated<TypeHandler>(value);
}

template <typename Element>
inline void RepeatedPtrField<Element>::UnsafeArenaAddAllocated(Element* value) {
  RepeatedPtrFieldBase::UnsafeArenaAddAllocated<TypeHandler>(value);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::ReleaseLast() {
  return RepeatedPtrFieldBase::ReleaseLast<TypeHandler>();
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::UnsafeArenaReleaseLast() {
  return RepeatedPtrFieldBase::UnsafeArenaReleaseLast<TypeHandler>();
}

template <typename Element>
inline int RepeatedPtrField<Element>::ClearedCount() const {
  return RepeatedPtrFieldBase::ClearedCount();
}

template <typename Element>
inline void RepeatedPtrField<Element>::AddCleared(Element* value) {
  return RepeatedPtrFieldBase::AddCleared<TypeHandler>(value);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::ReleaseCleared() {
  return RepeatedPtrFieldBase::ReleaseCleared<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::Reserve(int new_size) {
  return RepeatedPtrFieldBase::Reserve(new_size);
}

template <typename Element>
inline int RepeatedPtrField<Element>::Capacity() const {
  return RepeatedPtrFieldBase::Capacity();
}

// -------------------------------------------------------------------

namespace internal {

// STL-like iterator implementation for RepeatedPtrField.  You should not
// refer to this class directly; use RepeatedPtrField<T>::iterator instead.
//
// The iterator for RepeatedPtrField<T>, RepeatedPtrIterator<T>, is
// very similar to iterator_ptr<T**> in util/gtl/iterator_adaptors.h,
// but adds random-access operators and is modified to wrap a void** base
// iterator (since RepeatedPtrField stores its array as a void* array and
// casting void** to T** would violate C++ aliasing rules).
//
// This code based on net/proto/proto-array-internal.h by Jeffrey Yasskin
// (jyasskin@google.com).
template <typename Element>
class RepeatedPtrIterator {
 public:
  using iterator = RepeatedPtrIterator<Element>;
  using iterator_category = std::random_access_iterator_tag;
  using value_type = typename std::remove_const<Element>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = Element*;
  using reference = Element&;

  RepeatedPtrIterator() : it_(NULL) {}
  explicit RepeatedPtrIterator(void* const* it) : it_(it) {}

  // Allow "upcasting" from RepeatedPtrIterator<T**> to
  // RepeatedPtrIterator<const T*const*>.
  template <typename OtherElement>
  RepeatedPtrIterator(const RepeatedPtrIterator<OtherElement>& other)
      : it_(other.it_) {
    // Force a compiler error if the other type is not convertible to ours.
    if (false) {
      implicit_cast<Element*>(static_cast<OtherElement*>(nullptr));
    }
  }

  // dereferenceable
  reference operator*() const { return *reinterpret_cast<Element*>(*it_); }
  pointer operator->() const { return &(operator*()); }

  // {inc,dec}rementable
  iterator& operator++() {
    ++it_;
    return *this;
  }
  iterator operator++(int) { return iterator(it_++); }
  iterator& operator--() {
    --it_;
    return *this;
  }
  iterator operator--(int) { return iterator(it_--); }

  // equality_comparable
  bool operator==(const iterator& x) const { return it_ == x.it_; }
  bool operator!=(const iterator& x) const { return it_ != x.it_; }

  // less_than_comparable
  bool operator<(const iterator& x) const { return it_ < x.it_; }
  bool operator<=(const iterator& x) const { return it_ <= x.it_; }
  bool operator>(const iterator& x) const { return it_ > x.it_; }
  bool operator>=(const iterator& x) const { return it_ >= x.it_; }

  // addable, subtractable
  iterator& operator+=(difference_type d) {
    it_ += d;
    return *this;
  }
  friend iterator operator+(iterator it, const difference_type d) {
    it += d;
    return it;
  }
  friend iterator operator+(const difference_type d, iterator it) {
    it += d;
    return it;
  }
  iterator& operator-=(difference_type d) {
    it_ -= d;
    return *this;
  }
  friend iterator operator-(iterator it, difference_type d) {
    it -= d;
    return it;
  }

  // indexable
  reference operator[](difference_type d) const { return *(*this + d); }

  // random access iterator
  difference_type operator-(const iterator& x) const { return it_ - x.it_; }

 private:
  template <typename OtherElement>
  friend class RepeatedPtrIterator;

  // The internal iterator.
  void* const* it_;
};

// Provide an iterator that operates on pointers to the underlying objects
// rather than the objects themselves as RepeatedPtrIterator does.
// Consider using this when working with stl algorithms that change
// the array.
// The VoidPtr template parameter holds the type-agnostic pointer value
// referenced by the iterator.  It should either be "void *" for a mutable
// iterator, or "const void* const" for a constant iterator.
template <typename Element, typename VoidPtr>
class RepeatedPtrOverPtrsIterator {
 public:
  using iterator = RepeatedPtrOverPtrsIterator<Element, VoidPtr>;
  using iterator_category = std::random_access_iterator_tag;
  using value_type = typename std::remove_const<Element>::type;
  using difference_type = std::ptrdiff_t;
  using pointer = Element*;
  using reference = Element&;

  RepeatedPtrOverPtrsIterator() : it_(NULL) {}
  explicit RepeatedPtrOverPtrsIterator(VoidPtr* it) : it_(it) {}

  // dereferenceable
  reference operator*() const { return *reinterpret_cast<Element*>(it_); }
  pointer operator->() const { return &(operator*()); }

  // {inc,dec}rementable
  iterator& operator++() {
    ++it_;
    return *this;
  }
  iterator operator++(int) { return iterator(it_++); }
  iterator& operator--() {
    --it_;
    return *this;
  }
  iterator operator--(int) { return iterator(it_--); }

  // equality_comparable
  bool operator==(const iterator& x) const { return it_ == x.it_; }
  bool operator!=(const iterator& x) const { return it_ != x.it_; }

  // less_than_comparable
  bool operator<(const iterator& x) const { return it_ < x.it_; }
  bool operator<=(const iterator& x) const { return it_ <= x.it_; }
  bool operator>(const iterator& x) const { return it_ > x.it_; }
  bool operator>=(const iterator& x) const { return it_ >= x.it_; }

  // addable, subtractable
  iterator& operator+=(difference_type d) {
    it_ += d;
    return *this;
  }
  friend iterator operator+(iterator it, difference_type d) {
    it += d;
    return it;
  }
  friend iterator operator+(difference_type d, iterator it) {
    it += d;
    return it;
  }
  iterator& operator-=(difference_type d) {
    it_ -= d;
    return *this;
  }
  friend iterator operator-(iterator it, difference_type d) {
    it -= d;
    return it;
  }

  // indexable
  reference operator[](difference_type d) const { return *(*this + d); }

  // random access iterator
  difference_type operator-(const iterator& x) const { return it_ - x.it_; }

 private:
  template <typename OtherElement>
  friend class RepeatedPtrIterator;

  // The internal iterator.
  VoidPtr* it_;
};

void RepeatedPtrFieldBase::InternalSwap(RepeatedPtrFieldBase* other) {
  GOOGLE_DCHECK(this != other);
  GOOGLE_DCHECK(GetArenaNoVirtual() == other->GetArenaNoVirtual());

  std::swap(rep_, other->rep_);
  std::swap(current_size_, other->current_size_);
  std::swap(total_size_, other->total_size_);
}

}  // namespace internal

template <typename Element>
inline typename RepeatedPtrField<Element>::iterator
RepeatedPtrField<Element>::begin() {
  return iterator(raw_data());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_iterator
RepeatedPtrField<Element>::begin() const {
  return iterator(raw_data());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_iterator
RepeatedPtrField<Element>::cbegin() const {
  return begin();
}
template <typename Element>
inline typename RepeatedPtrField<Element>::iterator
RepeatedPtrField<Element>::end() {
  return iterator(raw_data() + size());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_iterator
RepeatedPtrField<Element>::end() const {
  return iterator(raw_data() + size());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_iterator
RepeatedPtrField<Element>::cend() const {
  return end();
}

template <typename Element>
inline typename RepeatedPtrField<Element>::pointer_iterator
RepeatedPtrField<Element>::pointer_begin() {
  return pointer_iterator(raw_mutable_data());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_pointer_iterator
RepeatedPtrField<Element>::pointer_begin() const {
  return const_pointer_iterator(const_cast<const void* const*>(raw_data()));
}
template <typename Element>
inline typename RepeatedPtrField<Element>::pointer_iterator
RepeatedPtrField<Element>::pointer_end() {
  return pointer_iterator(raw_mutable_data() + size());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_pointer_iterator
RepeatedPtrField<Element>::pointer_end() const {
  return const_pointer_iterator(
      const_cast<const void* const*>(raw_data() + size()));
}

// Iterators and helper functions that follow the spirit of the STL
// std::back_insert_iterator and std::back_inserter but are tailor-made
// for RepeatedField and RepeatedPtrField. Typical usage would be:
//
//   std::copy(some_sequence.begin(), some_sequence.end(),
//             RepeatedFieldBackInserter(proto.mutable_sequence()));
//
// Ported by johannes from util/gtl/proto-array-iterators.h

namespace internal {
// A back inserter for RepeatedField objects.
template <typename T>
class RepeatedFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  explicit RepeatedFieldBackInsertIterator(
      RepeatedField<T>* const mutable_field)
      : field_(mutable_field) {}
  RepeatedFieldBackInsertIterator<T>& operator=(const T& value) {
    field_->Add(value);
    return *this;
  }
  RepeatedFieldBackInsertIterator<T>& operator*() { return *this; }
  RepeatedFieldBackInsertIterator<T>& operator++() { return *this; }
  RepeatedFieldBackInsertIterator<T>& operator++(int /* unused */) {
    return *this;
  }

 private:
  RepeatedField<T>* field_;
};

// A back inserter for RepeatedPtrField objects.
template <typename T>
class RepeatedPtrFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  RepeatedPtrFieldBackInsertIterator(RepeatedPtrField<T>* const mutable_field)
      : field_(mutable_field) {}
  RepeatedPtrFieldBackInsertIterator<T>& operator=(const T& value) {
    *field_->Add() = value;
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator=(
      const T* const ptr_to_value) {
    *field_->Add() = *ptr_to_value;
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator=(T&& value) {
    *field_->Add() = std::move(value);
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator*() { return *this; }
  RepeatedPtrFieldBackInsertIterator<T>& operator++() { return *this; }
  RepeatedPtrFieldBackInsertIterator<T>& operator++(int /* unused */) {
    return *this;
  }

 private:
  RepeatedPtrField<T>* field_;
};

// A back inserter for RepeatedPtrFields that inserts by transferring ownership
// of a pointer.
template <typename T>
class AllocatedRepeatedPtrFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  explicit AllocatedRepeatedPtrFieldBackInsertIterator(
      RepeatedPtrField<T>* const mutable_field)
      : field_(mutable_field) {}
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator=(
      T* const ptr_to_value) {
    field_->AddAllocated(ptr_to_value);
    return *this;
  }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator*() { return *this; }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator++() { return *this; }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator++(int /* unused */) {
    return *this;
  }

 private:
  RepeatedPtrField<T>* field_;
};

// Almost identical to AllocatedRepeatedPtrFieldBackInsertIterator. This one
// uses the UnsafeArenaAddAllocated instead.
template <typename T>
class UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  explicit UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator(
      RepeatedPtrField<T>* const mutable_field)
      : field_(mutable_field) {}
  UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator<T>& operator=(
      T const* const ptr_to_value) {
    field_->UnsafeArenaAddAllocated(const_cast<T*>(ptr_to_value));
    return *this;
  }
  UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator<T>& operator*() {
    return *this;
  }
  UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator<T>& operator++() {
    return *this;
  }
  UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator<T>& operator++(
      int /* unused */) {
    return *this;
  }

 private:
  RepeatedPtrField<T>* field_;
};

}  // namespace internal

// Provides a back insert iterator for RepeatedField instances,
// similar to std::back_inserter().
template <typename T>
internal::RepeatedFieldBackInsertIterator<T> RepeatedFieldBackInserter(
    RepeatedField<T>* const mutable_field) {
  return internal::RepeatedFieldBackInsertIterator<T>(mutable_field);
}

// Provides a back insert iterator for RepeatedPtrField instances,
// similar to std::back_inserter().
template <typename T>
internal::RepeatedPtrFieldBackInsertIterator<T> RepeatedPtrFieldBackInserter(
    RepeatedPtrField<T>* const mutable_field) {
  return internal::RepeatedPtrFieldBackInsertIterator<T>(mutable_field);
}

// Special back insert iterator for RepeatedPtrField instances, just in
// case someone wants to write generic template code that can access both
// RepeatedFields and RepeatedPtrFields using a common name.
template <typename T>
internal::RepeatedPtrFieldBackInsertIterator<T> RepeatedFieldBackInserter(
    RepeatedPtrField<T>* const mutable_field) {
  return internal::RepeatedPtrFieldBackInsertIterator<T>(mutable_field);
}

// Provides a back insert iterator for RepeatedPtrField instances
// similar to std::back_inserter() which transfers the ownership while
// copying elements.
template <typename T>
internal::AllocatedRepeatedPtrFieldBackInsertIterator<T>
AllocatedRepeatedPtrFieldBackInserter(
    RepeatedPtrField<T>* const mutable_field) {
  return internal::AllocatedRepeatedPtrFieldBackInsertIterator<T>(
      mutable_field);
}

// Similar to AllocatedRepeatedPtrFieldBackInserter, using
// UnsafeArenaAddAllocated instead of AddAllocated.
// This is slightly faster if that matters. It is also useful in legacy code
// that uses temporary ownership to avoid copies. Example:
//   RepeatedPtrField<T> temp_field;
//   temp_field.AddAllocated(new T);
//   ... // Do something with temp_field
//   temp_field.ExtractSubrange(0, temp_field.size(), nullptr);
// If you put temp_field on the arena this fails, because the ownership
// transfers to the arena at the "AddAllocated" call and is not released anymore
// causing a double delete. Using UnsafeArenaAddAllocated prevents this.
template <typename T>
internal::UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator<T>
UnsafeArenaAllocatedRepeatedPtrFieldBackInserter(
    RepeatedPtrField<T>* const mutable_field) {
  return internal::UnsafeArenaAllocatedRepeatedPtrFieldBackInsertIterator<T>(
      mutable_field);
}

// Extern declarations of common instantiations to reduce libray bloat.
extern template class PROTOBUF_EXPORT RepeatedField<bool>;
extern template class PROTOBUF_EXPORT RepeatedField<int32>;
extern template class PROTOBUF_EXPORT RepeatedField<uint32>;
extern template class PROTOBUF_EXPORT RepeatedField<int64>;
extern template class PROTOBUF_EXPORT RepeatedField<uint64>;
extern template class PROTOBUF_EXPORT RepeatedField<float>;
extern template class PROTOBUF_EXPORT RepeatedField<double>;
extern template class PROTOBUF_EXPORT RepeatedPtrField<std::string>;

}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_REPEATED_FIELD_H__
