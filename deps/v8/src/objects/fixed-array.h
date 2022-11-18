// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_H_
#define V8_OBJECTS_FIXED_ARRAY_H_

#include "src/handles/maybe-handles.h"
#include "src/objects/instance-type.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

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
  V(SINGLE_CHARACTER_STRING_TABLE_SUB_TYPE)      \
  V(SLOW_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE) \
  V(STRING_SPLIT_CACHE_SUB_TYPE)                 \
  V(TEMPLATE_INFO_SUB_TYPE)                      \
  V(FEEDBACK_METADATA_SUB_TYPE)                  \
  V(WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE)

enum FixedArraySubInstanceType {
#define DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE(name) name,
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE)
#undef DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE
      LAST_FIXED_ARRAY_SUB_TYPE = WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE
};

#include "torque-generated/src/objects/fixed-array-tq.inc"

// Common superclass for FixedArrays that allow implementations to share
// common accessors and some code paths.
class FixedArrayBase
    : public TorqueGeneratedFixedArrayBase<FixedArrayBase, HeapObject> {
 public:
  // Forward declare the non-atomic (set_)length defined in torque.
  using TorqueGeneratedFixedArrayBase::length;
  using TorqueGeneratedFixedArrayBase::set_length;
  DECL_RELEASE_ACQUIRE_INT_ACCESSORS(length)

  inline Object unchecked_length(AcquireLoadTag) const;

  static int GetMaxLengthForNewSpaceAllocation(ElementsKind kind);

  V8_EXPORT_PRIVATE bool IsCowArray() const;

  // Maximal allowed size, in bytes, of a single FixedArrayBase.
  // Prevents overflowing size computations, as well as extreme memory
  // consumption. It's either (512Mb - kTaggedSize) or (1024Mb - kTaggedSize).
  // -kTaggedSize is here to ensure that this max size always fits into Smi
  // which is necessary for being able to create a free space filler for the
  // whole array of kMaxSize.
  static const int kMaxSize = 128 * kTaggedSize * MB - kTaggedSize;
  static_assert(Smi::IsValid(kMaxSize));

 protected:
  TQ_OBJECT_CONSTRUCTORS(FixedArrayBase)
  inline FixedArrayBase(Address ptr,
                        HeapObject::AllowInlineSmiStorage allow_smi);
};

// FixedArray describes fixed-sized arrays with element type Object.
class FixedArray
    : public TorqueGeneratedFixedArray<FixedArray, FixedArrayBase> {
 public:
  // Setter and getter for elements.
  inline Object get(int index) const;
  inline Object get(PtrComprCageBase cage_base, int index) const;

  static inline Handle<Object> get(FixedArray array, int index,
                                   Isolate* isolate);

  // Return a grown copy if the index is bigger than the array's length.
  V8_EXPORT_PRIVATE static Handle<FixedArray> SetAndGrow(
      Isolate* isolate, Handle<FixedArray> array, int index,
      Handle<Object> value);

  // Relaxed accessors.
  inline Object get(int index, RelaxedLoadTag) const;
  inline Object get(PtrComprCageBase cage_base, int index,
                    RelaxedLoadTag) const;
  inline void set(int index, Object value, RelaxedStoreTag,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set(int index, Smi value, RelaxedStoreTag);

  // SeqCst accessors.
  inline Object get(int index, SeqCstAccessTag) const;
  inline Object get(PtrComprCageBase cage_base, int index,
                    SeqCstAccessTag) const;
  inline void set(int index, Object value, SeqCstAccessTag,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set(int index, Smi value, SeqCstAccessTag);

  // Acquire/release accessors.
  inline Object get(int index, AcquireLoadTag) const;
  inline Object get(PtrComprCageBase cage_base, int index,
                    AcquireLoadTag) const;
  inline void set(int index, Object value, ReleaseStoreTag,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set(int index, Smi value, ReleaseStoreTag);

  // Setter that uses write barrier.
  inline void set(int index, Object value);
  inline bool is_the_hole(Isolate* isolate, int index);

  // Setter that doesn't need write barrier.
  inline void set(int index, Smi value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object value, WriteBarrierMode mode);

  // Atomic swap that doesn't need write barrier.
  inline Object swap(int index, Smi value, SeqCstAccessTag);
  // Atomic swap with explicit barrier mode.
  inline Object swap(int index, Object value, SeqCstAccessTag,
                     WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Setters for frequently used oddballs located in old space.
  inline void set_undefined(int index);
  inline void set_undefined(Isolate* isolate, int index);
  inline void set_undefined(ReadOnlyRoots ro_roots, int index);
  inline void set_null(int index);
  inline void set_null(Isolate* isolate, int index);
  inline void set_null(ReadOnlyRoots ro_roots, int index);
  inline void set_the_hole(int index);
  inline void set_the_hole(Isolate* isolate, int index);
  inline void set_the_hole(ReadOnlyRoots ro_roots, int index);

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
  static constexpr int OffsetOfElementAt(int index) {
    static_assert(kObjectsOffset == SizeFor(0));
    return SizeFor(index);
  }

  // Garbage collection support.
  inline ObjectSlot RawFieldOfElementAt(int index);

  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kTaggedSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "FixedArray maxLength not a Smi");

  // Maximally allowed length for regular (non large object space) object.
  static_assert(kMaxRegularHeapObjectSize < kMaxSize);
  static const int kMaxRegularLength =
      (kMaxRegularHeapObjectSize - kHeaderSize) / kTaggedSize;

  // Dispatched behavior.
  DECL_PRINTER(FixedArray)
  DECL_VERIFIER(FixedArray)

  int AllocatedSize();

  class BodyDescriptor;

  static constexpr int kObjectsOffset = kHeaderSize;

 protected:
  // Set operation on FixedArray without using write barriers. Can
  // only be used for storing old space objects or smis.
  static inline void NoWriteBarrierSet(FixedArray array, int index,
                                       Object value);

 private:
  static_assert(kHeaderSize == Internals::kFixedArrayHeaderSize);

  TQ_OBJECT_CONSTRUCTORS(FixedArray)
};

// FixedArray alias added only because of IsFixedArrayExact() predicate, which
// checks for the exact instance type FIXED_ARRAY_TYPE instead of a range
// check: [FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE].
class FixedArrayExact final : public FixedArray {};

// FixedDoubleArray describes fixed-sized arrays with element type double.
class FixedDoubleArray
    : public TorqueGeneratedFixedDoubleArray<FixedDoubleArray, FixedArrayBase> {
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

  TQ_OBJECT_CONSTRUCTORS(FixedDoubleArray)
};

// WeakFixedArray describes fixed-sized arrays with element type
// MaybeObject.
class WeakFixedArray
    : public TorqueGeneratedWeakFixedArray<WeakFixedArray, HeapObject> {
 public:
  inline MaybeObject Get(int index) const;
  inline MaybeObject Get(PtrComprCageBase cage_base, int index) const;

  inline void Set(
      int index, MaybeObject value,
      WriteBarrierMode mode = WriteBarrierMode::UPDATE_WRITE_BARRIER);

  static inline Handle<WeakFixedArray> EnsureSpace(Isolate* isolate,
                                                   Handle<WeakFixedArray> array,
                                                   int length);

  // Forward declare the non-atomic (set_)length defined in torque.
  using TorqueGeneratedWeakFixedArray::length;
  using TorqueGeneratedWeakFixedArray::set_length;
  DECL_RELEASE_ACQUIRE_INT_ACCESSORS(length)

  // Gives access to raw memory which stores the array's data.
  inline MaybeObjectSlot data_start();

  inline MaybeObjectSlot RawFieldOfElementAt(int index);

  inline void CopyElements(Isolate* isolate, int dst_index, WeakFixedArray src,
                           int src_index, int len, WriteBarrierMode mode);

  DECL_PRINTER(WeakFixedArray)
  DECL_VERIFIER(WeakFixedArray)

  class BodyDescriptor;

  static const int kMaxLength =
      (FixedArray::kMaxSize - kHeaderSize) / kTaggedSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "WeakFixedArray maxLength not a Smi");

  int AllocatedSize();

  static int OffsetOfElementAt(int index) {
    static_assert(kHeaderSize == SizeFor(0));
    return SizeFor(index);
  }

 private:
  friend class Heap;

  static const int kFirstIndex = 1;

  TQ_OBJECT_CONSTRUCTORS(WeakFixedArray)
};

// WeakArrayList is like a WeakFixedArray with static convenience methods for
// adding more elements. length() returns the number of elements in the list and
// capacity() returns the allocated size. The number of elements is stored at
// kLengthOffset and is updated with every insertion. The array grows
// dynamically with O(1) amortized insertion.
class WeakArrayList
    : public TorqueGeneratedWeakArrayList<WeakArrayList, HeapObject> {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_PRINTER(WeakArrayList)

  V8_EXPORT_PRIVATE static Handle<WeakArrayList> AddToEnd(
      Isolate* isolate, Handle<WeakArrayList> array,
      const MaybeObjectHandle& value);

  // A version that adds to elements. This ensures that the elements are
  // inserted atomically w.r.t GC.
  V8_EXPORT_PRIVATE static Handle<WeakArrayList> AddToEnd(
      Isolate* isolate, Handle<WeakArrayList> array,
      const MaybeObjectHandle& value1, const MaybeObjectHandle& value2);

  // Appends an element to the array and possibly compacts and shrinks live weak
  // references to the start of the collection. Only use this method when
  // indices to elements can change.
  static Handle<WeakArrayList> Append(
      Isolate* isolate, Handle<WeakArrayList> array,
      const MaybeObjectHandle& value,
      AllocationType allocation = AllocationType::kYoung);

  // Compact weak references to the beginning of the array.
  V8_EXPORT_PRIVATE void Compact(Isolate* isolate);

  inline MaybeObject Get(int index) const;
  inline MaybeObject Get(PtrComprCageBase cage_base, int index) const;

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the WeakArrayList, use the static AddToEnd() method
  // instead.
  inline void Set(int index, MaybeObject value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void Set(int index, Smi value);

  static constexpr int SizeForCapacity(int capacity) {
    return SizeFor(capacity);
  }

  static constexpr int CapacityForLength(int length) {
    return length + std::max(length / 2, 2);
  }

  // Gives access to raw memory which stores the array's data.
  inline MaybeObjectSlot data_start();

  inline void CopyElements(Isolate* isolate, int dst_index, WeakArrayList src,
                           int src_index, int len, WriteBarrierMode mode);

  V8_EXPORT_PRIVATE bool IsFull() const;

  int AllocatedSize();

  class BodyDescriptor;

  static const int kMaxCapacity =
      (FixedArray::kMaxSize - kHeaderSize) / kTaggedSize;

  static Handle<WeakArrayList> EnsureSpace(
      Isolate* isolate, Handle<WeakArrayList> array, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Returns the number of non-cleaned weak references in the array.
  int CountLiveWeakReferences() const;

  // Returns the number of non-cleaned elements in the array.
  int CountLiveElements() const;

  // Returns whether an entry was found and removed. Will move the elements
  // around in the array - this method can only be used in cases where the user
  // doesn't care about the indices! Users should make sure there are no
  // duplicates.
  V8_EXPORT_PRIVATE bool RemoveOne(const MaybeObjectHandle& value);

  // Searches the array (linear time) and returns whether it contains the value.
  V8_EXPORT_PRIVATE bool Contains(MaybeObject value);

  class Iterator;

 private:
  static int OffsetOfElementAt(int index) {
    return kHeaderSize + index * kTaggedSize;
  }

  TQ_OBJECT_CONSTRUCTORS(WeakArrayList)
};

class WeakArrayList::Iterator {
 public:
  explicit Iterator(WeakArrayList array) : index_(0), array_(array) {}
  Iterator(const Iterator&) = delete;
  Iterator& operator=(const Iterator&) = delete;

  inline HeapObject Next();

 private:
  int index_;
  WeakArrayList array_;
  DISALLOW_GARBAGE_COLLECTION(no_gc_)
};

// Generic array grows dynamically with O(1) amortized insertion.
//
// ArrayList is a FixedArray with static convenience methods for adding more
// elements. The Length() method returns the number of elements in the list, not
// the allocated size. The number of elements is stored at kLengthIndex and is
// updated with every insertion. The elements of the ArrayList are stored in the
// underlying FixedArray starting at kFirstIndex.
class ArrayList : public TorqueGeneratedArrayList<ArrayList, FixedArray> {
 public:
  V8_EXPORT_PRIVATE static Handle<ArrayList> Add(
      Isolate* isolate, Handle<ArrayList> array, Handle<Object> obj,
      AllocationType allocation = AllocationType::kYoung);
  V8_EXPORT_PRIVATE static Handle<ArrayList> Add(Isolate* isolate,
                                                 Handle<ArrayList> array,
                                                 Handle<Object> obj1,
                                                 Handle<Object> obj2);
  V8_EXPORT_PRIVATE static Handle<ArrayList> Add(Isolate* isolate,
                                                 Handle<ArrayList> array,
                                                 Handle<Object> obj1, Smi obj2,
                                                 Smi obj3, Smi obj4);
  static Handle<ArrayList> New(Isolate* isolate, int size);

  // Returns the number of elements in the list, not the allocated size, which
  // is length(). Lower and upper case length() return different results!
  inline int Length() const;

  // Sets the Length() as used by Elements(). Does not change the underlying
  // storage capacity, i.e., length().
  inline void SetLength(int length);
  inline Object Get(int index) const;
  inline Object Get(PtrComprCageBase cage_base, int index) const;
  inline ObjectSlot Slot(int index);

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the ArrayList, use the static Add() methods instead.
  inline void Set(int index, Object obj,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline void Set(int index, Smi obj);

  // Set the element at index to undefined. This does not change the Length().
  inline void Clear(int index, Object undefined);

  // Return a copy of the list of size Length() without the first entry. The
  // number returned by Length() is stored in the first entry.
  static Handle<FixedArray> Elements(Isolate* isolate, Handle<ArrayList> array);

  static const int kHeaderFields = 1;

  static const int kLengthIndex = 0;
  static const int kFirstIndex = 1;
  static_assert(kHeaderFields == kFirstIndex);

  DECL_VERIFIER(ArrayList)

 private:
  static Handle<ArrayList> EnsureSpace(
      Isolate* isolate, Handle<ArrayList> array, int length,
      AllocationType allocation = AllocationType::kYoung);
  TQ_OBJECT_CONSTRUCTORS(ArrayList)
};

enum SearchMode { ALL_ENTRIES, VALID_ENTRIES };

template <SearchMode search_mode, typename T>
inline int Search(T* array, Name name, int valid_entries = 0,
                  int* out_insertion_index = nullptr,
                  bool concurrent_search = false);

// ByteArray represents fixed sized arrays containing raw bytes that will not
// be scanned by the garbage collector.
class ByteArray : public TorqueGeneratedByteArray<ByteArray, FixedArrayBase> {
 public:
  inline int Size();

  // Get/set the contents of this array.
  inline byte get(int offset) const;
  inline void set(int offset, byte value);

  inline int get_int(int offset) const;
  inline void set_int(int offset, int value);

  inline Address get_sandboxed_pointer(int offset) const;
  inline void set_sandboxed_pointer(int offset, Address value);

  // Copy in / copy out whole byte slices.
  inline void copy_out(int index, byte* buffer, int slice_length);
  inline void copy_in(int index, const byte* buffer, int slice_length);

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

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return kHeaderSize + index; }

  // Dispatched behavior.
  DECL_PRINTER(ByteArray)

  // Layout description.
  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kHeaderSize);

  // Maximal length of a single ByteArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "ByteArray maxLength not a Smi");

  class BodyDescriptor;

 protected:
  TQ_OBJECT_CONSTRUCTORS(ByteArray)
  inline ByteArray(Address ptr, HeapObject::AllowInlineSmiStorage allow_smi);
};

// Convenience class for treating a ByteArray as array of fixed-size integers.
template <typename T>
class FixedIntegerArray : public ByteArray {
  static_assert(std::is_integral<T>::value);

 public:
  static Handle<FixedIntegerArray<T>> New(
      Isolate* isolate, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Get/set the contents of this array.
  T get(int index) const;
  void set(int index, T value);

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index) {
    return kHeaderSize + index * sizeof(T);
  }

  inline int length() const;

  DECL_CAST(FixedIntegerArray<T>)

  OBJECT_CONSTRUCTORS(FixedIntegerArray<T>, ByteArray);
};

using FixedInt8Array = FixedIntegerArray<int8_t>;
using FixedUInt8Array = FixedIntegerArray<uint8_t>;
using FixedInt16Array = FixedIntegerArray<int16_t>;
using FixedUInt16Array = FixedIntegerArray<uint16_t>;
using FixedInt32Array = FixedIntegerArray<int32_t>;
using FixedUInt32Array = FixedIntegerArray<uint32_t>;
using FixedInt64Array = FixedIntegerArray<int64_t>;
using FixedUInt64Array = FixedIntegerArray<uint64_t>;
// Use with care! Raw addresses on the heap are not safe in combination with
// the sandbox. However, this can for example be used to store sandboxed
// pointers, which is safe.
using FixedAddressArray = FixedIntegerArray<Address>;

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

  bool matches(int offset, const T* buffer, int length) {
    DCHECK_LE(offset, this->length());
    DCHECK_LE(offset + length, this->length());
    return memcmp(GetDataStartAddress() + sizeof(T) * offset, buffer,
                  length * sizeof(T)) == 0;
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

class TemplateList
    : public TorqueGeneratedTemplateList<TemplateList, FixedArray> {
 public:
  static Handle<TemplateList> New(Isolate* isolate, int size);
  inline int length() const;
  inline Object get(int index) const;
  inline Object get(PtrComprCageBase cage_base, int index) const;
  inline void set(int index, Object value);
  static Handle<TemplateList> Add(Isolate* isolate, Handle<TemplateList> list,
                                  Handle<Object> value);
 private:
  static const int kLengthIndex = 0;
  static const int kFirstElementIndex = kLengthIndex + 1;

  TQ_OBJECT_CONSTRUCTORS(TemplateList)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_H_
