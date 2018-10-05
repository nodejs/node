// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_H_
#define V8_OBJECTS_FIXED_ARRAY_H_

#include "src/maybe-handles.h"
#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class WeakArrayBodyDescriptor;

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
  inline int length() const;
  inline void set_length(int value);

  // Get and set the length using acquire loads and release stores.
  inline int synchronized_length() const;
  inline void synchronized_set_length(int value);

  inline Object* unchecked_synchronized_length() const;

  DECL_CAST(FixedArrayBase)

  static int GetMaxLengthForNewSpaceAllocation(ElementsKind kind);

  bool IsCowArray() const;

// Maximal allowed size, in bytes, of a single FixedArrayBase.
// Prevents overflowing size computations, as well as extreme memory
// consumption.
#ifdef V8_HOST_ARCH_32_BIT
  static const int kMaxSize = 512 * MB;
#else
  static const int kMaxSize = 1024 * MB;
#endif  // V8_HOST_ARCH_32_BIT

  // Layout description.
  // Length is smi tagged when it is stored.
  static const int kLengthOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kLengthOffset + kPointerSize;
};

// FixedArray describes fixed-sized arrays with element type Object*.
class FixedArray : public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline Object* get(int index) const;
  static inline Handle<Object> get(FixedArray* array, int index,
                                   Isolate* isolate);
  template <class T>
  MaybeHandle<T> GetValue(Isolate* isolate, int index) const;

  template <class T>
  Handle<T> GetValueChecked(Isolate* isolate, int index) const;

  // Return a grown copy if the index is bigger than the array's length.
  static Handle<FixedArray> SetAndGrow(Isolate* isolate,
                                       Handle<FixedArray> array, int index,
                                       Handle<Object> value,
                                       PretenureFlag pretenure = NOT_TENURED);

  // Setter that uses write barrier.
  inline void set(int index, Object* value);
  inline bool is_the_hole(Isolate* isolate, int index);

  // Setter that doesn't need write barrier.
  inline void set(int index, Smi* value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object* value, WriteBarrierMode mode);

  // Setters for frequently used oddballs located in old space.
  inline void set_undefined(int index);
  inline void set_undefined(Isolate* isolate, int index);
  inline void set_null(int index);
  inline void set_null(Isolate* isolate, int index);
  inline void set_the_hole(int index);
  inline void set_the_hole(Isolate* isolate, int index);

  inline Object** GetFirstElementAddress();
  inline bool ContainsOnlySmisOrHoles();
  // Returns true iff the elements are Numbers and sorted ascending.
  bool ContainsSortedNumbers();

  // Gives access to raw memory which stores the array's data.
  inline Object** data_start();

  inline void FillWithHoles(int from, int to);

  // Shrink the array and insert filler objects. {new_length} must be > 0.
  void Shrink(Isolate* isolate, int new_length);
  // If {new_length} is 0, return the canonical empty FixedArray. Otherwise
  // like above.
  static Handle<FixedArray> ShrinkOrEmpty(Isolate* isolate,
                                          Handle<FixedArray> array,
                                          int new_length);

  // Copy a sub array from the receiver to dest.
  void CopyTo(int pos, FixedArray* dest, int dest_pos, int len) const;

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kPointerSize;
  }

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Garbage collection support.
  inline Object** RawFieldOfElementAt(int index);

  DECL_CAST(FixedArray)
  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kPointerSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "FixedArray maxLength not a Smi");

  // Maximally allowed length for regular (non large object space) object.
  STATIC_ASSERT(kMaxRegularHeapObjectSize < kMaxSize);
  static const int kMaxRegularLength =
      (kMaxRegularHeapObjectSize - kHeaderSize) / kPointerSize;

  // Dispatched behavior.
  DECL_PRINTER(FixedArray)
  DECL_VERIFIER(FixedArray)
#ifdef DEBUG
  // Checks if two FixedArrays have identical contents.
  bool IsEqualTo(FixedArray* other);
#endif

  typedef FlexibleBodyDescriptor<kHeaderSize> BodyDescriptor;

 protected:
  // Set operation on FixedArray without using write barriers. Can
  // only be used for storing old space objects or smis.
  static inline void NoWriteBarrierSet(FixedArray* array, int index,
                                       Object* value);

 private:
  STATIC_ASSERT(kHeaderSize == Internals::kFixedArrayHeaderSize);

  inline void set_undefined(ReadOnlyRoots ro_roots, int index);
  inline void set_null(ReadOnlyRoots ro_roots, int index);
  inline void set_the_hole(ReadOnlyRoots ro_roots, int index);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedArray);
};

// FixedArray alias added only because of IsFixedArrayExact() predicate, which
// checks for the exact instance type FIXED_ARRAY_TYPE instead of a range
// check: [FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE].
class FixedArrayExact final : public FixedArray {
 public:
  DECL_CAST(FixedArrayExact)
};

// FixedDoubleArray describes fixed-sized arrays with element type double.
class FixedDoubleArray : public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline double get_scalar(int index);
  inline uint64_t get_representation(int index);
  static inline Handle<Object> get(FixedDoubleArray* array, int index,
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

  // Gives access to raw memory which stores the array's data.
  inline double* data_start();

  inline void FillWithHoles(int from, int to);

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  DECL_CAST(FixedDoubleArray)

  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kDoubleSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "FixedDoubleArray maxLength not a Smi");

  // Dispatched behavior.
  DECL_PRINTER(FixedDoubleArray)
  DECL_VERIFIER(FixedDoubleArray)

  class BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedDoubleArray);
};

// WeakFixedArray describes fixed-sized arrays with element type
// MaybeObject*.
class WeakFixedArray : public HeapObject {
 public:
  DECL_CAST(WeakFixedArray)

  inline MaybeObject* Get(int index) const;

  // Setter that uses write barrier.
  inline void Set(int index, MaybeObject* value);

  // Setter with explicit barrier mode.
  inline void Set(int index, MaybeObject* value, WriteBarrierMode mode);

  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kPointerSize;
  }

  DECL_INT_ACCESSORS(length)

  // Get and set the length using acquire loads and release stores.
  inline int synchronized_length() const;
  inline void synchronized_set_length(int value);

  // Gives access to raw memory which stores the array's data.
  inline MaybeObject** data_start();

  inline MaybeObject** RawFieldOfElementAt(int index);

  inline MaybeObject** GetFirstElementAddress();

  DECL_PRINTER(WeakFixedArray)
  DECL_VERIFIER(WeakFixedArray)

  typedef WeakArrayBodyDescriptor BodyDescriptor;

  static const int kLengthOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kLengthOffset + kPointerSize;

  static const int kMaxLength =
      (FixedArray::kMaxSize - kHeaderSize) / kPointerSize;
  static_assert(Internals::IsValidSmi(kMaxLength),
                "WeakFixedArray maxLength not a Smi");

 protected:
  static int OffsetOfElementAt(int index) {
    return kHeaderSize + index * kPointerSize;
  }

 private:
  friend class Heap;

  static const int kFirstIndex = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakFixedArray);
};

// WeakArrayList is like a WeakFixedArray with static convenience methods for
// adding more elements. length() returns the number of elements in the list and
// capacity() returns the allocated size. The number of elements is stored at
// kLengthOffset and is updated with every insertion. The array grows
// dynamically with O(1) amortized insertion.
class WeakArrayList : public HeapObject {
 public:
  DECL_CAST(WeakArrayList)
  DECL_VERIFIER(WeakArrayList)
  DECL_PRINTER(WeakArrayList)

  static Handle<WeakArrayList> AddToEnd(Isolate* isolate,
                                        Handle<WeakArrayList> array,
                                        const MaybeObjectHandle& value);

  inline MaybeObject* Get(int index) const;

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the WeakArrayList, use the static AddToEnd() method
  // instead.
  inline void Set(int index, MaybeObject* value,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static constexpr int SizeForCapacity(int capacity) {
    return kHeaderSize + capacity * kPointerSize;
  }

  // Gives access to raw memory which stores the array's data.
  inline MaybeObject** data_start();

  bool IsFull();

  DECL_INT_ACCESSORS(capacity)
  DECL_INT_ACCESSORS(length)

  // Get and set the capacity using acquire loads and release stores.
  inline int synchronized_capacity() const;
  inline void synchronized_set_capacity(int value);

  typedef WeakArrayBodyDescriptor BodyDescriptor;

  static const int kCapacityOffset = HeapObject::kHeaderSize;
  static const int kLengthOffset = kCapacityOffset + kPointerSize;
  static const int kHeaderSize = kLengthOffset + kPointerSize;

  static const int kMaxCapacity =
      (FixedArray::kMaxSize - kHeaderSize) / kPointerSize;

  static Handle<WeakArrayList> EnsureSpace(
      Isolate* isolate, Handle<WeakArrayList> array, int length,
      PretenureFlag pretenure = NOT_TENURED);

  // Returns the number of non-cleaned weak references in the array.
  int CountLiveWeakReferences() const;

  // Returns whether an entry was found and removed. Will move the elements
  // around in the array - this method can only be used in cases where the user
  // doesn't care about the indices! Users should make sure there are no
  // duplicates.
  bool RemoveOne(const MaybeObjectHandle& value);

  class Iterator {
   public:
    explicit Iterator(WeakArrayList* array) : index_(0), array_(array) {}

    inline HeapObject* Next();

   private:
    int index_;
    WeakArrayList* array_;
#ifdef DEBUG
    DisallowHeapAllocation no_gc_;
#endif  // DEBUG
    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

 private:
  static int OffsetOfElementAt(int index) {
    return kHeaderSize + index * kPointerSize;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakArrayList);
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
  static Handle<ArrayList> Add(Isolate* isolate, Handle<ArrayList> array,
                               Handle<Object> obj);
  static Handle<ArrayList> Add(Isolate* isolate, Handle<ArrayList> array,
                               Handle<Object> obj1, Handle<Object> obj2);
  static Handle<ArrayList> New(Isolate* isolate, int size);

  // Returns the number of elements in the list, not the allocated size, which
  // is length(). Lower and upper case length() return different results!
  inline int Length() const;

  // Sets the Length() as used by Elements(). Does not change the underlying
  // storage capacity, i.e., length().
  inline void SetLength(int length);
  inline Object* Get(int index) const;
  inline Object** Slot(int index);

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the ArrayList, use the static Add() methods instead.
  inline void Set(int index, Object* obj,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Set the element at index to undefined. This does not change the Length().
  inline void Clear(int index, Object* undefined);

  // Return a copy of the list of size Length() without the first entry. The
  // number returned by Length() is stored in the first entry.
  static Handle<FixedArray> Elements(Isolate* isolate, Handle<ArrayList> array);
  DECL_CAST(ArrayList)

 private:
  static Handle<ArrayList> EnsureSpace(Isolate* isolate,
                                       Handle<ArrayList> array, int length);
  static const int kLengthIndex = 0;
  static const int kFirstIndex = 1;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArrayList);
};

enum SearchMode { ALL_ENTRIES, VALID_ENTRIES };

template <SearchMode search_mode, typename T>
inline int Search(T* array, Name* name, int valid_entries = 0,
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
    DCHECK(IsAligned(size_in_bytes, kPointerSize));
    DCHECK_GE(size_in_bytes, kHeaderSize);
    return size_in_bytes - kHeaderSize;
  }

  // Returns data start address.
  inline byte* GetDataStartAddress();

  inline int DataSize() const;

  // Returns a pointer to the ByteArray object for a given data start address.
  static inline ByteArray* FromDataStartAddress(Address address);

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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ByteArray);
};

// Wrapper class for ByteArray which can store arbitrary C++ classes, as long
// as they can be copied with memcpy.
template <class T>
class PodArray : public ByteArray {
 public:
  static Handle<PodArray<T>> New(Isolate* isolate, int length,
                                 PretenureFlag pretenure = NOT_TENURED);
  void copy_out(int index, T* result) {
    ByteArray::copy_out(index * sizeof(T), reinterpret_cast<byte*>(result),
                        sizeof(T));
  }
  T get(int index) {
    T result;
    copy_out(index, &result);
    return result;
  }
  void set(int index, const T& value) {
    copy_in(index * sizeof(T), reinterpret_cast<const byte*>(&value),
            sizeof(T));
  }
  inline int length();
  DECL_CAST(PodArray<T>)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PodArray<T>);
};

class FixedTypedArrayBase : public FixedArrayBase {
 public:
  // [base_pointer]: Either points to the FixedTypedArrayBase itself or nullptr.
  DECL_ACCESSORS(base_pointer, Object)

  // [external_pointer]: Contains the offset between base_pointer and the start
  // of the data. If the base_pointer is a nullptr, the external_pointer
  // therefore points to the actual backing store.
  DECL_ACCESSORS(external_pointer, void)

  // Dispatched behavior.
  DECL_CAST(FixedTypedArrayBase)

  static const int kBasePointerOffset = FixedArrayBase::kHeaderSize;
  static const int kExternalPointerOffset = kBasePointerOffset + kPointerSize;
  static const int kHeaderSize =
      DOUBLE_POINTER_ALIGN(kExternalPointerOffset + kPointerSize);

  static const int kDataOffset = kHeaderSize;

  static const int kMaxElementSize = 8;

#ifdef V8_HOST_ARCH_32_BIT
  static const size_t kMaxByteLength = std::numeric_limits<size_t>::max();
#else
  static const size_t kMaxByteLength =
      static_cast<size_t>(Smi::kMaxValue) * kMaxElementSize;
#endif  // V8_HOST_ARCH_32_BIT

  static const size_t kMaxLength = Smi::kMaxValue;

  class BodyDescriptor;

  inline int size() const;

  static inline int TypedArraySize(InstanceType type, int length);
  inline int TypedArraySize(InstanceType type) const;

  // Use with care: returns raw pointer into heap.
  inline void* DataPtr();

  inline int DataSize() const;

  inline size_t ByteLength() const;

 private:
  static inline int ElementSize(InstanceType type);

  inline int DataSize(InstanceType type) const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArrayBase);
};

template <class Traits>
class FixedTypedArray : public FixedTypedArrayBase {
 public:
  typedef typename Traits::ElementType ElementType;
  static const InstanceType kInstanceType = Traits::kInstanceType;

  DECL_CAST(FixedTypedArray<Traits>)

  static inline ElementType get_scalar_from_data_ptr(void* data_ptr, int index);
  inline ElementType get_scalar(int index);
  static inline Handle<Object> get(Isolate* isolate, FixedTypedArray* array,
                                   int index);
  inline void set(int index, ElementType value);

  static inline ElementType from(int value);
  static inline ElementType from(uint32_t value);
  static inline ElementType from(double value);
  static inline ElementType from(int64_t value);
  static inline ElementType from(uint64_t value);

  static inline ElementType FromHandle(Handle<Object> value,
                                       bool* lossless = nullptr);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  inline void SetValue(uint32_t index, Object* value);

  DECL_PRINTER(FixedTypedArray)
  DECL_VERIFIER(FixedTypedArray)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArray);
};

#define FIXED_TYPED_ARRAY_TRAITS(Type, type, TYPE, elementType)               \
  STATIC_ASSERT(sizeof(elementType) <= FixedTypedArrayBase::kMaxElementSize); \
  class Type##ArrayTraits {                                                   \
   public: /* NOLINT */                                                       \
    typedef elementType ElementType;                                          \
    static const InstanceType kInstanceType = FIXED_##TYPE##_ARRAY_TYPE;      \
    static const char* Designator() { return #type " array"; }                \
    static inline Handle<Object> ToHandle(Isolate* isolate,                   \
                                          elementType scalar);                \
    static inline elementType defaultValue();                                 \
  };                                                                          \
                                                                              \
  typedef FixedTypedArray<Type##ArrayTraits> Fixed##Type##Array;

TYPED_ARRAYS(FIXED_TYPED_ARRAY_TRAITS)

#undef FIXED_TYPED_ARRAY_TRAITS

class TemplateList : public FixedArray {
 public:
  static Handle<TemplateList> New(Isolate* isolate, int size);
  inline int length() const;
  inline Object* get(int index) const;
  inline void set(int index, Object* value);
  static Handle<TemplateList> Add(Isolate* isolate, Handle<TemplateList> list,
                                  Handle<Object> value);
  DECL_CAST(TemplateList)
 private:
  static const int kLengthIndex = 0;
  static const int kFirstElementIndex = kLengthIndex + 1;
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateList);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_H_
