// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_H_
#define V8_OBJECTS_FIXED_ARRAY_H_

#include "src/handles/maybe-handles.h"
#include "src/objects/instance-type.h"
#include "src/objects/smi.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {
using WeakArrayBodyDescriptor =
    FlexibleWeakBodyDescriptor<HeapObject::kHeaderSize>;

#define FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(V)    \
  V(BYTECODE_ARRAY_CONSTANT_POOL_SUB_TYPE)       \
  V(BYTECODE_ARRAY_HANDLER_TABLE_SUB_TYPE)       \
  V(CODE_STUBS_TABLE_SUB_TYPE)                   \
  V(COMPILATION_CACHE_TABLE_SUB_TYPE)            \
  V(CONTEXT_SUB_TYPE)                            \
  V(COPY_ON_WRITE_SUB_TYPE)                      \
  V(DEOPTIMIZATION_DATA_SUB_TYPE)                \
  V(DESCRIPTOR_ARRAY_SUB_TYPE)                   \
  V(EMBEDDED_OBJECT_SUB_TYPE)                    \
  V(ENUM_CACHE_SUB_TYPE)                         \
  V(ENUM_INDICES_CACHE_SUB_TYPE)                 \
  V(DEPENDENT_CODE_SUB_TYPE)                     \
  V(DICTIONARY_ELEMENTS_SUB_TYPE)                \
  V(DICTIONARY_PROPERTIES_SUB_TYPE)              \
  V(EMPTY_PROPERTIES_DICTIONARY_SUB_TYPE)        \
  V(PACKED_ELEMENTS_SUB_TYPE)                    \
  V(FAST_PROPERTIES_SUB_TYPE)                    \
  V(FAST_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE) \
  V(HANDLER_TABLE_SUB_TYPE)                      \
  V(JS_COLLECTION_SUB_TYPE)                      \
  V(JS_WEAK_COLLECTION_SUB_TYPE)                 \
  V(NOSCRIPT_SHARED_FUNCTION_INFOS_SUB_TYPE)     \
  V(NUMBER_STRING_CACHE_SUB_TYPE)                \
  V(OBJECT_TO_CODE_SUB_TYPE)                     \
  V(OPTIMIZED_CODE_LITERALS_SUB_TYPE)            \
  V(OPTIMIZED_CODE_MAP_SUB_TYPE)                 \
  V(PROTOTYPE_USERS_SUB_TYPE)                    \
  V(REGEXP_MULTIPLE_CACHE_SUB_TYPE)              \
  V(RETAINED_MAPS_SUB_TYPE)                      \
  V(SCOPE_INFO_SUB_TYPE)                         \
  V(SCRIPT_LIST_SUB_TYPE)                        \
  V(SERIALIZED_OBJECTS_SUB_TYPE)                 \
  V(SHARED_FUNCTION_INFOS_SUB_TYPE)              \
  V(SINGLE_CHARACTER_STRING_CACHE_SUB_TYPE)      \
  V(SLOW_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE) \
  V(STRING_SPLIT_CACHE_SUB_TYPE)                 \
  V(STRING_TABLE_SUB_TYPE)                       \
  V(TEMPLATE_INFO_SUB_TYPE)                      \
  V(FEEDBACK_METADATA_SUB_TYPE)                  \
  V(WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE)

enum FixedArraySubInstanceType {
#define DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE(name) name,
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE)
#undef DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE
      LAST_FIXED_ARRAY_SUB_TYPE = WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE
};

// Common superclass for FixedArrays that allow implementations to share
// common accessors and some code paths.
class FixedArrayBase : public HeapObject {
 public:
  // [length]: length of the array.
  DECL_INT_ACCESSORS(length)

  // Get and set the length using acquire loads and release stores.
  DECL_SYNCHRONIZED_INT_ACCESSORS(length)

  inline Object unchecked_synchronized_length() const;

  DECL_CAST(FixedArrayBase)
  DECL_VERIFIER(FixedArrayBase)

  static int GetMaxLengthForNewSpaceAllocation(ElementsKind kind);

  V8_EXPORT_PRIVATE bool IsCowArray() const;

// Maximal allowed size, in bytes, of a single FixedArrayBase.
// Prevents overflowing size computations, as well as extreme memory
// consumption.
#ifdef V8_HOST_ARCH_32_BIT
  static const int kMaxSize = 512 * MB;
#else
  static const int kMaxSize = 1024 * MB;
#endif  // V8_HOST_ARCH_32_BIT

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_FIXED_ARRAY_BASE_FIELDS)

 protected:
  // Special-purpose constructor for subclasses that have fast paths where
  // their ptr() is a Smi.
  inline FixedArrayBase(Address ptr, AllowInlineSmiStorage allow_smi);

  OBJECT_CONSTRUCTORS(FixedArrayBase, HeapObject);
};

// FixedArray describes fixed-sized arrays with element type Object.
class FixedArray : public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline Object get(int index) const;
  inline Object get(Isolate* isolate, int index) const;

  static inline Handle<Object> get(FixedArray array, int index,
                                   Isolate* isolate);

  // Return a grown copy if the index is bigger than the array's length.
  V8_EXPORT_PRIVATE static Handle<FixedArray> SetAndGrow(
      Isolate* isolate, Handle<FixedArray> array, int index,
      Handle<Object> value, AllocationType allocation = AllocationType::kYoung);

  // Setter that uses write barrier.
  inline void set(int index, Object value);
  inline bool is_the_hole(Isolate* isolate, int index);

  // Setter that doesn't need write barrier.
  inline void set(int index, Smi value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object value, WriteBarrierMode mode);

  // Setters for frequently used oddballs located in old space.
  inline void set_undefined(int index);
  inline void set_undefined(Isolate* isolate, int index);
  inline void set_null(int index);
  inline void set_null(Isolate* isolate, int index);
  inline void set_the_hole(int index);
  inline void set_the_hole(Isolate* isolate, int index);

  inline ObjectSlot GetFirstElementAddress();
  inline bool ContainsOnlySmisOrHoles();

  // Gives access to raw memory which stores the array's data.
  inline ObjectSlot data_start();

  inline void MoveElements(Isolate* isolate, int dst_index, int src_index,
                           int len, WriteBarrierMode mode);

  inline void CopyElements(Isolate* isolate, int dst_index, FixedArray src,
                           int src_index, int len, WriteBarrierMode mode);

  inline void FillWithHoles(int from, int to);

  // Shrink the array and insert filler objects. {new_length} must be > 0.
  V8_EXPORT_PRIVATE void Shrink(Isolate* isolate, int new_length);
  // If {new_length} is 0, return the canonical empty FixedArray. Otherwise
  // like above.
  static Handle<FixedArray> ShrinkOrEmpty(Isolate* isolate,
                                          Handle<FixedArray> array,
                                          int new_length);

  // Copy a sub array from the receiver to dest.
  V8_EXPORT_PRIVATE void CopyTo(int pos, FixedArray dest, int dest_pos,
                                int len) const;

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kTaggedSize;
  }

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Garbage collection support.
  inline ObjectSlot RawFieldOfElementAt(int index);

  DECL_CAST(FixedArray)
  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kTaggedSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "FixedArray maxLength not a Smi");

  // Maximally allowed length for regular (non large object space) object.
  STATIC_ASSERT(kMaxRegularHeapObjectSize < kMaxSize);
  static const int kMaxRegularLength =
      (kMaxRegularHeapObjectSize - kHeaderSize) / kTaggedSize;

  // Dispatched behavior.
  DECL_PRINTER(FixedArray)
  DECL_VERIFIER(FixedArray)

  using BodyDescriptor = FlexibleBodyDescriptor<kHeaderSize>;

  static constexpr int kObjectsOffset = kHeaderSize;

 protected:
  // Set operation on FixedArray without using write barriers. Can
  // only be used for storing old space objects or smis.
  static inline void NoWriteBarrierSet(FixedArray array, int index,
                                       Object value);

 private:
  STATIC_ASSERT(kHeaderSize == Internals::kFixedArrayHeaderSize);

  inline void set_undefined(ReadOnlyRoots ro_roots, int index);
  inline void set_null(ReadOnlyRoots ro_roots, int index);
  inline void set_the_hole(ReadOnlyRoots ro_roots, int index);

  OBJECT_CONSTRUCTORS(FixedArray, FixedArrayBase);
};

// FixedArray alias added only because of IsFixedArrayExact() predicate, which
// checks for the exact instance type FIXED_ARRAY_TYPE instead of a range
// check: [FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE].
class FixedArrayExact final : public FixedArray {};

// FixedDoubleArray describes fixed-sized arrays with element type double.
class FixedDoubleArray : public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline double get_scalar(int index);
  inline uint64_t get_representation(int index);
  static inline Handle<Object> get(FixedDoubleArray array, int index,
                                   Isolate* isolate);
  inline void set(int index, double value);
  inline void set_the_hole(Isolate* isolate, int index);
  inline void set_the_hole(int index);

  // Checking for the hole.
  inline bool is_the_hole(Isolate* isolate, int index);
  inline bool is_the_hole(int index);

  // Garbage collection support.
  inline static int SizeFor(int length) {
    return kHeaderSize + length * kDoubleSize;
  }

  inline void MoveElements(Isolate* isolate, int dst_index, int src_index,
                           int len, WriteBarrierMode mode);

  inline void FillWithHoles(int from, int to);

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  DECL_CAST(FixedDoubleArray)

  // Start offset of elements.
  static constexpr int kFloatsOffset = kHeaderSize;

  // Maximally allowed length of a FixedDoubleArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kDoubleSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "FixedDoubleArray maxLength not a Smi");

  // Dispatched behavior.
  DECL_PRINTER(FixedDoubleArray)
  DECL_VERIFIER(FixedDoubleArray)

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(FixedDoubleArray, FixedArrayBase);
};

// WeakFixedArray describes fixed-sized arrays with element type
// MaybeObject.
class WeakFixedArray : public HeapObject {
 public:
  DECL_CAST(WeakFixedArray)

  inline MaybeObject Get(int index) const;
  inline MaybeObject Get(Isolate* isolate, int index) const;

  // Setter that uses write barrier.
  inline void Set(int index, MaybeObject value);

  // Setter with explicit barrier mode.
  inline void Set(int index, MaybeObject value, WriteBarrierMode mode);

  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kTaggedSize;
  }

  DECL_INT_ACCESSORS(length)

  // Get and set the length using acquire loads and release stores.
  DECL_SYNCHRONIZED_INT_ACCESSORS(length)

  // Gives access to raw memory which stores the array's data.
  inline MaybeObjectSlot data_start();

  inline MaybeObjectSlot RawFieldOfElementAt(int index);

  inline void CopyElements(Isolate* isolate, int dst_index, WeakFixedArray src,
                           int src_index, int len, WriteBarrierMode mode);

  DECL_PRINTER(WeakFixedArray)
  DECL_VERIFIER(WeakFixedArray)

  using BodyDescriptor = WeakArrayBodyDescriptor;

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_WEAK_FIXED_ARRAY_FIELDS)
  static constexpr int kHeaderSize = kSize;

  static const int kMaxLength =
      (FixedArray::kMaxSize - kHeaderSize) / kTaggedSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "WeakFixedArray maxLength not a Smi");

 protected:
  static int OffsetOfElementAt(int index) {
    return kHeaderSize + index * kTaggedSize;
  }

 private:
  friend class Heap;

  static const int kFirstIndex = 1;

  OBJECT_CONSTRUCTORS(WeakFixedArray, HeapObject);
};

// WeakArrayList is like a WeakFixedArray with static convenience methods for
// adding more elements. length() returns the number of elements in the list and
// capacity() returns the allocated size. The number of elements is stored at
// kLengthOffset and is updated with every insertion. The array grows
// dynamically with O(1) amortized insertion.
class WeakArrayList : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_CAST(WeakArrayList)
  DECL_VERIFIER(WeakArrayList)
  DECL_PRINTER(WeakArrayList)

  V8_EXPORT_PRIVATE static Handle<WeakArrayList> AddToEnd(
      Isolate* isolate, Handle<WeakArrayList> array,
      const MaybeObjectHandle& value);

  inline MaybeObject Get(int index) const;
  inline MaybeObject Get(Isolate* isolate, int index) const;

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the WeakArrayList, use the static AddToEnd() method
  // instead.
  inline void Set(int index, MaybeObject value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static constexpr int SizeForCapacity(int capacity) {
    return kHeaderSize + capacity * kTaggedSize;
  }

  // Gives access to raw memory which stores the array's data.
  inline MaybeObjectSlot data_start();

  inline void CopyElements(Isolate* isolate, int dst_index, WeakArrayList src,
                           int src_index, int len, WriteBarrierMode mode);

  V8_EXPORT_PRIVATE bool IsFull();

  DECL_INT_ACCESSORS(capacity)
  DECL_INT_ACCESSORS(length)

  // Get and set the capacity using acquire loads and release stores.
  DECL_SYNCHRONIZED_INT_ACCESSORS(capacity)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_WEAK_ARRAY_LIST_FIELDS)
  static constexpr int kHeaderSize = kSize;

  using BodyDescriptor = WeakArrayBodyDescriptor;

  static const int kMaxCapacity =
      (FixedArray::kMaxSize - kHeaderSize) / kTaggedSize;

  static Handle<WeakArrayList> EnsureSpace(
      Isolate* isolate, Handle<WeakArrayList> array, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Returns the number of non-cleaned weak references in the array.
  int CountLiveWeakReferences() const;

  // Returns whether an entry was found and removed. Will move the elements
  // around in the array - this method can only be used in cases where the user
  // doesn't care about the indices! Users should make sure there are no
  // duplicates.
  V8_EXPORT_PRIVATE bool RemoveOne(const MaybeObjectHandle& value);

  class Iterator;

 private:
  static int OffsetOfElementAt(int index) {
    return kHeaderSize + index * kTaggedSize;
  }

  OBJECT_CONSTRUCTORS(WeakArrayList, HeapObject);
};

class WeakArrayList::Iterator {
 public:
  explicit Iterator(WeakArrayList array) : index_(0), array_(array) {}

  inline HeapObject Next();

 private:
  int index_;
  WeakArrayList array_;
#ifdef DEBUG
  DisallowHeapAllocation no_gc_;
#endif  // DEBUG
  DISALLOW_COPY_AND_ASSIGN(Iterator);
};

// Generic array grows dynamically with O(1) amortized insertion.
//
// ArrayList is a FixedArray with static convenience methods for adding more
// elements. The Length() method returns the number of elements in the list, not
// the allocated size. The number of elements is stored at kLengthIndex and is
// updated with every insertion. The elements of the ArrayList are stored in the
// underlying FixedArray starting at kFirstIndex.
class ArrayList : public FixedArray {
 public:
  V8_EXPORT_PRIVATE static Handle<ArrayList> Add(Isolate* isolate,
                                                 Handle<ArrayList> array,
                                                 Handle<Object> obj);
  V8_EXPORT_PRIVATE static Handle<ArrayList> Add(Isolate* isolate,
                                                 Handle<ArrayList> array,
                                                 Handle<Object> obj1,
                                                 Handle<Object> obj2);
  static Handle<ArrayList> New(Isolate* isolate, int size);

  // Returns the number of elements in the list, not the allocated size, which
  // is length(). Lower and upper case length() return different results!
  inline int Length() const;

  // Sets the Length() as used by Elements(). Does not change the underlying
  // storage capacity, i.e., length().
  inline void SetLength(int length);
  inline Object Get(int index) const;
  inline Object Get(Isolate* isolate, int index) const;
  inline ObjectSlot Slot(int index);

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the ArrayList, use the static Add() methods instead.
  inline void Set(int index, Object obj,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Set the element at index to undefined. This does not change the Length().
  inline void Clear(int index, Object undefined);

  // Return a copy of the list of size Length() without the first entry. The
  // number returned by Length() is stored in the first entry.
  static Handle<FixedArray> Elements(Isolate* isolate, Handle<ArrayList> array);
  DECL_CAST(ArrayList)

 private:
  static Handle<ArrayList> EnsureSpace(Isolate* isolate,
                                       Handle<ArrayList> array, int length);
  static const int kLengthIndex = 0;
  static const int kFirstIndex = 1;
  OBJECT_CONSTRUCTORS(ArrayList, FixedArray);
};

enum SearchMode { ALL_ENTRIES, VALID_ENTRIES };

template <SearchMode search_mode, typename T>
inline int Search(T* array, Name name, int valid_entries = 0,
                  int* out_insertion_index = nullptr);

// ByteArray represents fixed sized byte arrays.  Used for the relocation info
// that is attached to code objects.
class ByteArray : public FixedArrayBase {
 public:
  inline int Size();

  // Setter and getter.
  inline byte get(int index) const;
  inline void set(int index, byte value);

  // Copy in / copy out whole byte slices.
  inline void copy_out(int index, byte* buffer, int length);
  inline void copy_in(int index, const byte* buffer, int length);

  // Treat contents as an int array.
  inline int get_int(int index) const;
  inline void set_int(int index, int value);

  inline uint32_t get_uint32(int index) const;
  inline void set_uint32(int index, uint32_t value);

  inline uint32_t get_uint32_relaxed(int index) const;
  inline void set_uint32_relaxed(int index, uint32_t value);

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }
  // We use byte arrays for free blocks in the heap.  Given a desired size in
  // bytes that is a multiple of the word size and big enough to hold a byte
  // array, this function returns the number of elements a byte array should
  // have.
  static int LengthFor(int size_in_bytes) {
    DCHECK(IsAligned(size_in_bytes, kTaggedSize));
    DCHECK_GE(size_in_bytes, kHeaderSize);
    return size_in_bytes - kHeaderSize;
  }

  // Returns data start address.
  inline byte* GetDataStartAddress();
  // Returns address of the past-the-end element.
  inline byte* GetDataEndAddress();

  inline int DataSize() const;

  // Returns a pointer to the ByteArray object for a given data start address.
  static inline ByteArray FromDataStartAddress(Address address);

  DECL_CAST(ByteArray)

  // Dispatched behavior.
  inline int ByteArraySize();
  DECL_PRINTER(ByteArray)
  DECL_VERIFIER(ByteArray)

  // Layout description.
  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kHeaderSize);

  // Maximal length of a single ByteArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "ByteArray maxLength not a Smi");

  class BodyDescriptor;

 protected:
  // Special-purpose constructor for subclasses that have fast paths where
  // their ptr() is a Smi.
  inline ByteArray(Address ptr, AllowInlineSmiStorage allow_smi);

  OBJECT_CONSTRUCTORS(ByteArray, FixedArrayBase);
};

// Wrapper class for ByteArray which can store arbitrary C++ classes, as long
// as they can be copied with memcpy.
template <class T>
class PodArray : public ByteArray {
 public:
  static Handle<PodArray<T>> New(
      Isolate* isolate, int length,
      AllocationType allocation = AllocationType::kYoung);
  void copy_out(int index, T* result, int length) {
    ByteArray::copy_out(index * sizeof(T), reinterpret_cast<byte*>(result),
                        length * sizeof(T));
  }

  void copy_in(int index, const T* buffer, int length) {
    ByteArray::copy_in(index * sizeof(T), reinterpret_cast<const byte*>(buffer),
                       length * sizeof(T));
  }

  bool matches(const T* buffer, int length) {
    DCHECK_LE(length, this->length());
    return memcmp(GetDataStartAddress(), buffer, length * sizeof(T)) == 0;
  }

  T get(int index) {
    T result;
    copy_out(index, &result, 1);
    return result;
  }

  void set(int index, const T& value) { copy_in(index, &value, 1); }

  inline int length() const;
  DECL_CAST(PodArray<T>)

  OBJECT_CONSTRUCTORS(PodArray<T>, ByteArray);
};

class TemplateList : public FixedArray {
 public:
  static Handle<TemplateList> New(Isolate* isolate, int size);
  inline int length() const;
  inline Object get(int index) const;
  inline Object get(Isolate* isolate, int index) const;
  inline void set(int index, Object value);
  static Handle<TemplateList> Add(Isolate* isolate, Handle<TemplateList> list,
                                  Handle<Object> value);
  DECL_CAST(TemplateList)
 private:
  static const int kLengthIndex = 0;
  static const int kFirstElementIndex = kLengthIndex + 1;

  OBJECT_CONSTRUCTORS(TemplateList, FixedArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_H_
