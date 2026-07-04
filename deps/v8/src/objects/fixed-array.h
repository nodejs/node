// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_H_
#define V8_OBJECTS_FIXED_ARRAY_H_

#include <optional>

#include "src/base/strong-alias.h"
#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/fixed-array-base.h"
#include "src/objects/heap-object.h"
#include "src/objects/instance-type.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"
#include "src/objects/tagged.h"
#include "src/objects/trusted-object.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

template <typename T>
concept HasCapacity = requires(T a) { a.capacity_; };
template <typename T>
concept HasLength = requires(T a) { a.length_; };

template <typename T>
constexpr bool CheckFirstField() {
  static_assert(sizeof(TrustedObject) == sizeof(HeapObject));
  if constexpr (HasCapacity<T>) {
    return offsetof(T, capacity_) == sizeof(HeapObject) &&
           std::is_same_v<decltype(std::declval<T>().capacity_), uint32_t>;
  } else {
    static_assert(HasLength<T>);
    return offsetof(T, length_) == sizeof(HeapObject) &&
           std::is_same_v<decltype(std::declval<T>().length_), uint32_t>;
  }
}

template <typename T>
concept TaggedArrayBaseCompatible = requires(T a) {
  requires CheckFirstField<T>();
  a.objects();
  requires std::is_base_of_v<TaggedMemberBase,
                             std::remove_reference_t<decltype(a.objects()[0])>>;
};

// The Super template parameter names the concrete base class (e.g.
// (Trusted)FixedArrayBase) from which subclasses inherit the length_ field.
// Classes with a capacity_ + length_ layout pass HeapObject and declare both
// fields themselves.
V8_OBJECT template <class Derived, typename ElementT_,
                    class Super = FixedArrayBase>
class TaggedArrayBase : public Super {
 private:
  V8_INLINE Derived* derived() { return static_cast<Derived*>(this); }
  V8_INLINE const Derived* derived() const {
    return static_cast<const Derived*>(this);
  }

  uint32_t& capacity_field() {
    if constexpr (HasCapacity<Derived>) {
      return derived()->capacity_;
    } else {
      return derived()->length_;
    }
  }
  const uint32_t& capacity_field() const {
    if constexpr (HasCapacity<Derived>) {
      return derived()->capacity_;
    } else {
      return derived()->length_;
    }
  }

  static_assert(std::is_base_of_v<HeapObject, Super>);

  static_assert(sizeof(TaggedMember<ElementT_>) == kTaggedSize);
  static_assert(is_subtype_v<ElementT_, MaybeObject>);

  template <typename T>
  static constexpr bool kSupportsSmiElements = std::is_convertible_v<Smi, T>;

  static constexpr WriteBarrierMode kDefaultMode =
      std::is_same_v<ElementT_, Smi> ? SKIP_WRITE_BARRIER
                                     : UPDATE_WRITE_BARRIER;

 public:
  using ElementT = ElementT_;
  using ElementMemberT = TaggedMember<ElementT>;
  static constexpr bool kElementsAreMaybeObject = is_maybe_weak_v<ElementT>;
  static constexpr int kElementSize = kTaggedSize;

 private:
  using SlotType =
      std::conditional_t<kElementsAreMaybeObject, MaybeObjectSlot, ObjectSlot>;

 public:
  inline SafeHeapObjectSize capacity() const {
    return SafeHeapObjectSize(capacity_field());
  }
  inline SafeHeapObjectSize capacity(RelaxedLoadTag) const {
    return SafeHeapObjectSize(
        base::AsAtomic32::Relaxed_Load(&capacity_field()));
  }
  inline SafeHeapObjectSize capacity(AcquireLoadTag) const {
    return SafeHeapObjectSize(
        base::AsAtomic32::Acquire_Load(&capacity_field()));
  }
  inline void set_capacity(uint32_t value) { capacity_field() = value; }
  inline void set_capacity(uint32_t value, ReleaseStoreTag) {
    base::AsAtomic32::Release_Store(&capacity_field(), value);
  }

  // length() / set_length() are inherited from (Trusted)FixedArrayBase.

  inline void clear_optional_padding() {
    if constexpr (requires { derived()->optional_padding_; }) {
      derived()->optional_padding_ = 0;
    }
  }

  // Index is never supposed to be negative.
  // See https://crbug.com/441221573.

  inline Tagged<ElementT> get(uint32_t index) const;
  inline Tagged<ElementT> get(uint32_t index, RelaxedLoadTag) const;
  inline Tagged<ElementT> get(uint32_t index, AcquireLoadTag) const;
  inline Tagged<ElementT> get(uint32_t index, SeqCstAccessTag) const;

  inline void set(uint32_t index, Tagged<ElementT> value,
                  WriteBarrierMode mode = kDefaultMode);
  template <typename T = ElementT,
            typename = std::enable_if<kSupportsSmiElements<T>>>
  inline void set(uint32_t index, Tagged<Smi> value);
  inline void set(uint32_t index, Tagged<ElementT> value, RelaxedStoreTag,
                  WriteBarrierMode mode = kDefaultMode);
  template <typename T = ElementT,
            typename = std::enable_if<kSupportsSmiElements<T>>>
  inline void set(uint32_t index, Tagged<Smi> value, RelaxedStoreTag);
  inline void set(uint32_t index, Tagged<ElementT> value, ReleaseStoreTag,
                  WriteBarrierMode mode = kDefaultMode);
  template <typename T = ElementT,
            typename = std::enable_if<kSupportsSmiElements<T>>>
  inline void set(uint32_t index, Tagged<Smi> value, ReleaseStoreTag);
  inline void set(uint32_t index, Tagged<ElementT> value, SeqCstAccessTag,
                  WriteBarrierMode mode = kDefaultMode);
  template <typename T = ElementT,
            typename = std::enable_if<kSupportsSmiElements<T>>>
  inline void set(uint32_t index, Tagged<Smi> value, SeqCstAccessTag);

  inline Tagged<ElementT> swap(uint32_t index, Tagged<ElementT> value,
                               SeqCstAccessTag,
                               WriteBarrierMode mode = kDefaultMode);
  inline Tagged<ElementT> compare_and_swap(
      uint32_t index, Tagged<ElementT> expected, Tagged<ElementT> value,
      SeqCstAccessTag, WriteBarrierMode mode = kDefaultMode);

  // Move vs. Copy behaves like memmove vs. memcpy: for Move, the memory
  // regions may overlap, for Copy they must not overlap.
  inline static void MoveElements(Isolate* isolate, Tagged<Derived> dst,
                                  uint32_t dst_index, Tagged<Derived> src,
                                  uint32_t src_index, uint32_t len,
                                  WriteBarrierMode mode = kDefaultMode);
  inline static void CopyElements(Isolate* isolate, Tagged<Derived> dst,
                                  uint32_t dst_index, Tagged<Derived> src,
                                  uint32_t src_index, uint32_t len,
                                  WriteBarrierMode mode = kDefaultMode);

  // Right-trim the array.
  // Invariant: 0 < new_length <= length()
  inline void RightTrim(Isolate* isolate, uint32_t new_capacity);

  inline int AllocatedSize() const;
  static constexpr int SizeFor(int capacity);
  static constexpr int OffsetOfElementAt(int index);

  // Gives access to raw memory which stores the array's data.
  inline SlotType RawFieldOfFirstElement() const;
  inline SlotType RawFieldOfElementAt(uint32_t index) const;

  // Maximal allowed capacity, in number of elements. Chosen s.t. the byte size
  // fits into a Smi which is necessary for being able to create a free space
  // filler.
  static constexpr uint32_t kMaxCapacity = kMaxFixedArrayCapacity;

  static constexpr int MaxRegularCapacity();

 protected:
  template <class IsolateT>
  static Handle<Derived> Allocate(
      IsolateT* isolate, uint32_t capacity,
      std::optional<DisallowGarbageCollection>* no_gc_out,
      AllocationType allocation = AllocationType::kYoung,
      AllocationHint hint = AllocationHint());

  static constexpr uint32_t NewCapacityForIndex(uint32_t index,
                                                uint32_t old_capacity);

  inline bool IsInBounds(int index) const;
  inline bool IsCowArray() const;
} V8_OBJECT_END;

template <class Derived, typename ElementT, class Super>
constexpr int TaggedArrayBase<Derived, ElementT, Super>::SizeFor(int capacity) {
  static_assert(TaggedArrayBaseCompatible<Derived>);
  return OFFSET_OF_DATA_START(Derived) + capacity * kElementSize;
}

template <class Derived, typename ElementT, class Super>
constexpr int TaggedArrayBase<Derived, ElementT, Super>::OffsetOfElementAt(
    int index) {
  return SizeFor(index);
}

template <class Derived, typename ElementT, class Super>
constexpr int TaggedArrayBase<Derived, ElementT, Super>::MaxRegularCapacity() {
  return (kMaxRegularHeapObjectSize - OFFSET_OF_DATA_START(Derived)) /
         kElementSize;
}

// FixedArray describes fixed-sized arrays with element type Object.
V8_OBJECT class FixedArray : public TaggedArrayBase<FixedArray, Object> {
  using Super = TaggedArrayBase<FixedArray, Object>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kFixedArrayMap;
  using ElementMemberT = TaggedMember<Object>;

 public:
  template <class IsolateT>
  static inline Handle<FixedArray> New(
      IsolateT* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kYoung,
      AllocationHint hint = AllocationHint());
  template <class IsolateT, typename ElementsCallback>
  static inline Handle<FixedArray> New(
      IsolateT* isolate, uint32_t length, ElementsCallback elements_callback,
      AllocationType allocation = AllocationType::kYoung,
      AllocationHint hint = AllocationHint());

  using Super::CopyElements;
  using Super::MoveElements;

  // TODO(jgruber): Only needed for FixedArrays used as JSObject elements.
  inline void MoveElements(Isolate* isolate, uint32_t dst_index,
                           uint32_t src_index, uint32_t len,
                           WriteBarrierMode mode);
  inline void CopyElements(Isolate* isolate, uint32_t dst_index,
                           Tagged<FixedArray> src, uint32_t src_index,
                           uint32_t len, WriteBarrierMode mode);

  // Return a grown copy if the index is bigger than the array's length.
  template <template <typename> typename HandleType>
    requires(
        std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
  V8_EXPORT_PRIVATE static HandleType<FixedArray> SetAndGrow(
      Isolate* isolate, HandleType<FixedArray> array, uint32_t index,
      DirectHandle<Object> value);
  template <template <typename> typename HandleType>
    requires(
        std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
  V8_EXPORT_PRIVATE static HandleType<FixedArray> SetAndGrow(
      Isolate* isolate, HandleType<FixedArray> array, uint32_t index,
      Tagged<Smi> value);

  // Right-trim the array.
  // Invariant: 0 < new_length <= length()
  V8_EXPORT_PRIVATE void RightTrim(Isolate* isolate, uint32_t new_capacity);
  // Right-trims the array, and canonicalizes length 0 to empty_fixed_array.
  template <template <typename> typename HandleType>
    requires(
        std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
  static HandleType<FixedArray> RightTrimOrEmpty(Isolate* isolate,
                                                 HandleType<FixedArray> array,
                                                 uint32_t new_length);

  // TODO(jgruber): Only needed for FixedArrays used as JSObject elements.
  inline void FillWithHoles(uint32_t from, uint32_t to);

  // For compatibility with FixedDoubleArray:
  // TODO(jgruber): Only needed for FixedArrays used as JSObject elements.
  inline bool is_the_hole(Isolate* isolate, uint32_t index);
  inline void set_the_hole(Isolate* isolate, uint32_t index);
  inline void set_the_hole(ReadOnlyRoots ro_roots, uint32_t index);

  DECL_PRINTER(FixedArray)
  DECL_VERIFIER(FixedArray)

  class BodyDescriptor;

  static constexpr uint32_t kMaxLength = Super::kMaxCapacity;
  static constexpr int kMaxRegularLength =
      (kMaxRegularHeapObjectSize -
       (sizeof(HeapObject) +
        (TAGGED_SIZE_8_BYTES ? kTaggedSize : kUInt32Size))) /
      kTaggedSize;

 private:
  template <template <typename> typename HandleType>
    requires(
        std::is_convertible_v<HandleType<FixedArray>, DirectHandle<FixedArray>>)
  static HandleType<FixedArray> Grow(Isolate* isolate,
                                     HandleType<FixedArray> array,
                                     uint32_t index);

  inline static Handle<FixedArray> Resize(
      Isolate* isolate, DirectHandle<FixedArray> xs, uint32_t new_capacity,
      AllocationType allocation = AllocationType::kYoung,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

 public:
  // length_ / optional_padding_ live in FixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<Object>, objects);
} V8_OBJECT_END;

static_assert(sizeof(FixedArray) == Internals::kFixedArrayHeaderSize);

// A FixedArray in trusted space and with a unique instance type.
//
// Note: while the array itself is trusted, it contains tagged pointers into
// the main pointer compression heap and therefore to _untrusted_ objects.
// If you are storing references to other trusted object (i.e. protected
// pointers), use ProtectedFixedArray.
V8_OBJECT class TrustedFixedArray
    : public TaggedArrayBase<TrustedFixedArray, Object, TrustedFixedArrayBase> {
  using Super =
      TaggedArrayBase<TrustedFixedArray, Object, TrustedFixedArrayBase>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kTrustedFixedArrayMap;
  using ElementMemberT = TaggedMember<Object>;

 public:
  template <class IsolateT>
  static inline Handle<TrustedFixedArray> New(
      IsolateT* isolate, uint32_t capacity,
      AllocationType allocation = AllocationType::kTrusted);

  DECL_PRINTER(TrustedFixedArray)
  DECL_VERIFIER(TrustedFixedArray)

  class BodyDescriptor;

  static constexpr uint32_t kMaxLength = Super::kMaxCapacity;
  static constexpr int kMaxRegularLength =
      (kMaxRegularHeapObjectSize -
       (sizeof(TrustedObject) +
        (TAGGED_SIZE_8_BYTES ? kTaggedSize : kUInt32Size))) /
      kTaggedSize;

 public:
  // length_ / optional_padding_ live in TrustedFixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<Object>, objects);
} V8_OBJECT_END;

// A FixedArray in trusted space, holding protected pointers (to other trusted
// objects). If you want to store JS-heap references, use TrustedFixedArray.
// ProtectedFixedArray has a unique instance type.
V8_OBJECT class ProtectedFixedArray
    : public TaggedArrayBase<ProtectedFixedArray, UnionOf<TrustedObject, Smi>,
                             TrustedFixedArrayBase> {
  using Super =
      TaggedArrayBase<ProtectedFixedArray, UnionOf<TrustedObject, Smi>,
                      TrustedFixedArrayBase>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kProtectedFixedArrayMap;
  using ElementMemberT = ProtectedTaggedMember<UnionOf<TrustedObject, Smi>>;

 public:
  // Allocate a new ProtectedFixedArray of the given capacity, initialized with
  // Smi::zero().
  template <class IsolateT>
  static inline Handle<ProtectedFixedArray> New(IsolateT* isolate,
                                                uint32_t capacity,
                                                SharedFlag shared = SharedFlag{
                                                    false});

  DECL_PRINTER(ProtectedFixedArray)
  DECL_VERIFIER(ProtectedFixedArray)

  class BodyDescriptor;

  static constexpr uint32_t kMaxLength = Super::kMaxCapacity;
  static constexpr int kMaxRegularLength =
      (kMaxRegularHeapObjectSize -
       (sizeof(TrustedObject) +
        (TAGGED_SIZE_8_BYTES ? kTaggedSize : kUInt32Size))) /
      kTaggedSize;

 public:
  // length_ / optional_padding_ live in TrustedFixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(ElementMemberT, objects);
} V8_OBJECT_END;

// FixedArray alias added only because of IsFixedArrayExact() predicate, which
// checks for the exact instance type FIXED_ARRAY_TYPE instead of a range
// check: [FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE].
V8_OBJECT
class FixedArrayExact final : public FixedArray {
} V8_OBJECT_END;

// WeakFixedArray describes fixed-sized arrays with element type
// Tagged<MaybeObject>.
V8_OBJECT class WeakFixedArray
    : public TaggedArrayBase<WeakFixedArray, MaybeObject> {
  using Super = TaggedArrayBase<WeakFixedArray, MaybeObject>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kWeakFixedArrayMap;
  using ElementMemberT = TaggedMember<MaybeObject>;

 public:
  template <class IsolateT>
  static inline Handle<WeakFixedArray> New(
      IsolateT* isolate, uint32_t capacity,
      AllocationType allocation = AllocationType::kYoung,
      MaybeDirectHandle<Object> initial_value = {});

  DECL_PRINTER(WeakFixedArray)
  DECL_VERIFIER(WeakFixedArray)

  class BodyDescriptor;

 public:
  // length_ / optional_padding_ live in FixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<MaybeObject>, objects);
} V8_OBJECT_END;

// WeakHomomorphicFixedArray describes fixed-sized arrays with element type
// Tagged<MaybeObject>, used for homomorphic feedback.
// It acts as a fixed-size lossy hashmap-based cache, where collisions
// overwrite existing entries.
V8_OBJECT class WeakHomomorphicFixedArray
    : public TaggedArrayBase<WeakHomomorphicFixedArray, MaybeObject> {
  using Super = TaggedArrayBase<WeakHomomorphicFixedArray, MaybeObject>;

 public:
  static constexpr RootIndex kMapRootIndex =
      RootIndex::kWeakHomomorphicFixedArrayMap;
  using ElementMemberT = TaggedMember<MaybeObject>;

 public:
  template <class IsolateT>
  static inline Handle<WeakHomomorphicFixedArray> New(
      IsolateT* isolate, uint32_t capacity,
      AllocationType allocation = AllocationType::kYoung,
      MaybeDirectHandle<Object> initial_value = {});

  DECL_PRINTER(WeakHomomorphicFixedArray)
  DECL_VERIFIER(WeakHomomorphicFixedArray)

  class BodyDescriptor;

 public:
  // length_ / optional_padding_ live in FixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<MaybeObject>, objects);
} V8_OBJECT_END;

// A WeakFixedArray in trusted space holding pointers into the main cage.
V8_OBJECT class TrustedWeakFixedArray
    : public TaggedArrayBase<TrustedWeakFixedArray, MaybeObject,
                             TrustedFixedArrayBase> {
  using Super = TaggedArrayBase<TrustedWeakFixedArray, MaybeObject,
                                TrustedFixedArrayBase>;

 public:
  static constexpr RootIndex kMapRootIndex =
      RootIndex::kTrustedWeakFixedArrayMap;
  using ElementMemberT = TaggedMember<MaybeObject>;

 public:
  template <class IsolateT>
  static inline Handle<TrustedWeakFixedArray> New(IsolateT* isolate,
                                                  uint32_t capacity);

  DECL_PRINTER(TrustedWeakFixedArray)
  DECL_VERIFIER(TrustedWeakFixedArray)

  class BodyDescriptor;

 public:
  // length_ / optional_padding_ live in TrustedFixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<MaybeObject>, objects);
} V8_OBJECT_END;

// A WeakFixedArray in trusted space, containing weak pointers to other
// trusted objects (or smis).
V8_OBJECT class ProtectedWeakFixedArray
    : public TaggedArrayBase<ProtectedWeakFixedArray,
                             UnionOf<MaybeWeak<TrustedObject>, Smi>,
                             TrustedFixedArrayBase> {
  using Super = TaggedArrayBase<ProtectedWeakFixedArray,
                                UnionOf<MaybeWeak<TrustedObject>, Smi>,
                                TrustedFixedArrayBase>;

 public:
  static constexpr RootIndex kMapRootIndex =
      RootIndex::kProtectedWeakFixedArrayMap;
  using ElementMemberT =
      ProtectedTaggedMember<UnionOf<MaybeWeak<TrustedObject>, Smi>>;

 public:
  template <class IsolateT>
  static inline Handle<ProtectedWeakFixedArray> New(IsolateT* isolate,
                                                    uint32_t capacity);
  DECL_PRINTER(ProtectedWeakFixedArray)
  DECL_VERIFIER(ProtectedWeakFixedArray)

  class BodyDescriptor;
 public:
  // length_ / optional_padding_ live in TrustedFixedArrayBase.
  FLEXIBLE_ARRAY_MEMBER(ElementMemberT, objects);
} V8_OBJECT_END;

// WeakArrayList is like a WeakFixedArray with static convenience methods for
// adding more elements. length() returns the number of elements in the list and
// capacity() returns the allocated size. The number of elements is stored at
// kLengthOffset and is updated with every insertion. The array grows
// dynamically with O(1) amortized insertion.
V8_OBJECT class WeakArrayList
    : public TaggedArrayBase<WeakArrayList, MaybeObject, HeapObject> {
  using Super = TaggedArrayBase<WeakArrayList, MaybeObject, HeapObject>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kWeakArrayListMap;
  using ElementMemberT = TaggedMember<MaybeObject>;

 public:
  DECL_PRINTER(WeakArrayList)
  DECL_VERIFIER(WeakArrayList)

  V8_EXPORT_PRIVATE static Handle<WeakArrayList> AddToEnd(
      Isolate* isolate, Handle<WeakArrayList> array,
      MaybeObjectDirectHandle value);

  // A version that adds two elements. This ensures that the elements are
  // inserted atomically w.r.t GC.
  V8_EXPORT_PRIVATE static Handle<WeakArrayList> AddToEnd(
      Isolate* isolate, Handle<WeakArrayList> array,
      MaybeObjectDirectHandle value1, Tagged<Smi> value2);

  // Appends an element to the array and possibly compacts and shrinks live weak
  // references to the start of the collection. Only use this method when
  // indices to elements can change.
  static V8_WARN_UNUSED_RESULT DirectHandle<WeakArrayList> Append(
      Isolate* isolate, DirectHandle<WeakArrayList> array,
      MaybeObjectDirectHandle value,
      AllocationType allocation = AllocationType::kYoung);

  // Compact weak references to the beginning of the array.
  V8_EXPORT_PRIVATE void Compact(Isolate* isolate);

  inline Tagged<MaybeObject> Get(uint32_t index) const;

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the WeakArrayList, use the static AddToEnd() method
  // instead.
  inline void Set(uint32_t index, Tagged<MaybeObject> value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void Set(uint32_t index, Tagged<Smi> value);

  inline SafeHeapObjectSize capacity() const;
  inline SafeHeapObjectSize capacity(RelaxedLoadTag) const;

  // The function returns an alias instead of uint32_t to incrementally convert
  // callsites without missing any implicit casts.
  inline SafeHeapObjectSize length() const;
  inline SafeHeapObjectSize ulength() const;
  inline void set_length(uint32_t value);

  static constexpr int SizeForCapacity(uint32_t capacity);

  static constexpr uint32_t CapacityForLength(uint32_t length) {
    return length + std::max(length / 2, 2u);
  }

  // Gives access to raw memory which stores the array's data.
  inline MaybeObjectSlot data_start();

  inline void CopyElements(Isolate* isolate, uint32_t dst_index,
                           Tagged<WeakArrayList> src, uint32_t src_index,
                           uint32_t len, WriteBarrierMode mode);

  V8_EXPORT_PRIVATE bool IsFull() const;

  inline int AllocatedSize() const;

  class BodyDescriptor;

  // Maximal allowed length, in number of elements. Chosen s.t. the byte size
  // fits into a Smi which is necessary for being able to create a free space
  // filler.
  // TODO(jgruber): The kMaxLength could be larger (`(Smi::kMaxValue -
  // sizeof(Header)) / kElementSize`), but our tests rely on a
  // smaller maximum to avoid timeouts.
  static constexpr uint32_t kMaxCapacity = kMaxFixedArrayCapacity;

  static Handle<WeakArrayList> EnsureSpace(
      Isolate* isolate, Handle<WeakArrayList> array, uint32_t length,
      AllocationType allocation = AllocationType::kYoung);

  // Returns the number of non-cleaned weak references in the array.
  uint32_t CountLiveWeakReferences() const;

  // Returns the number of non-cleaned elements in the array.
  uint32_t CountLiveElements() const;

  // Returns whether an entry was found and removed. Will move the elements
  // around in the array - this method can only be used in cases where the user
  // doesn't care about the indices! Users should make sure there are no
  // duplicates.
  V8_EXPORT_PRIVATE bool RemoveOne(MaybeObjectDirectHandle value);

  // Searches the array (linear time) and returns whether it contains the value.
  V8_EXPORT_PRIVATE bool Contains(Tagged<MaybeObject> value);

  class Iterator;

  static constexpr uint32_t kCapacityOffset = sizeof(HeapObject);
  static constexpr uint32_t kLengthOffset = kCapacityOffset + kUInt32Size;
  static constexpr uint32_t kHeaderSize = kLengthOffset + kUInt32Size;

 private:
  static constexpr int OffsetOfElementAt(int index) {
    return kHeaderSize + index * kTaggedSize;
  }

 public:
  uint32_t capacity_;
  uint32_t length_;
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<MaybeObject>, objects);
} V8_OBJECT_END;

constexpr int WeakArrayList::SizeForCapacity(uint32_t capacity) {
  return SizeFor(capacity);
}

class WeakArrayList::Iterator {
 public:
  explicit Iterator(Tagged<WeakArrayList> array) : index_(0), array_(array) {}
  Iterator(const Iterator&) = delete;
  Iterator& operator=(const Iterator&) = delete;

  inline Tagged<HeapObject> Next();

 private:
  uint32_t index_;
  Tagged<WeakArrayList> array_;
  DISALLOW_GARBAGE_COLLECTION(no_gc_)
};


// A generic array that grows dynamically with O(1) amortized insertion.
V8_OBJECT class ArrayList
    : public TaggedArrayBase<ArrayList, Object, HeapObject> {
  using Super = TaggedArrayBase<ArrayList, Object, HeapObject>;

 public:
  static constexpr RootIndex kMapRootIndex = RootIndex::kArrayListMap;
  using ElementMemberT = TaggedMember<Object>;

  template <class IsolateT>
  static inline DirectHandle<ArrayList> New(
      IsolateT* isolate, uint32_t capacity,
      AllocationType allocation = AllocationType::kYoung);

  inline int length() const;
  // The function returns an alias instead of uint32_t to force conversion at
  // the callsites without missing any implicit casts.
  inline SafeHeapObjectSize ulength() const;
  inline void set_length(uint32_t value);

  V8_EXPORT_PRIVATE static DirectHandle<ArrayList> Add(
      Isolate* isolate, DirectHandle<ArrayList> array, Tagged<Smi> obj,
      AllocationType allocation = AllocationType::kYoung);
  V8_EXPORT_PRIVATE static DirectHandle<ArrayList> Add(
      Isolate* isolate, DirectHandle<ArrayList> array, DirectHandle<Object> obj,
      AllocationType allocation = AllocationType::kYoung);
  V8_EXPORT_PRIVATE static DirectHandle<ArrayList> Add(
      Isolate* isolate, DirectHandle<ArrayList> array,
      DirectHandle<Object> obj0, DirectHandle<Object> obj1,
      AllocationType allocation = AllocationType::kYoung);

  V8_EXPORT_PRIVATE static DirectHandle<FixedArray> ToFixedArray(
      Isolate* isolate, DirectHandle<ArrayList> array,
      AllocationType allocation = AllocationType::kYoung);

  // Right-trim the array.
  // Invariant: 0 < new_length <= length()
  void RightTrim(Isolate* isolate, uint32_t new_capacity);

  DECL_PRINTER(ArrayList)
  DECL_VERIFIER(ArrayList)

  class BodyDescriptor;

  static constexpr uint32_t kCapacityOffset = sizeof(HeapObject);
  static constexpr uint32_t kLengthOffset = kCapacityOffset + kUInt32Size;
  static constexpr uint32_t kHeaderSize = kLengthOffset + kUInt32Size;

 private:
  static DirectHandle<ArrayList> EnsureSpace(
      Isolate* isolate, DirectHandle<ArrayList> array, uint32_t length,
      AllocationType allocation = AllocationType::kYoung);

 public:
  uint32_t capacity_;
  uint32_t length_;
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<Object>, objects);
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_H_
