// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_OBJECT_H_
#define V8_OBJECTS_HEAP_OBJECT_H_

#include "src/globals.h"

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// This is the new way to represent the Object class. It is temporarily
// separate to allow an incremental transition.
// For a design overview, see https://goo.gl/Ph4CGz.
class ObjectPtr {
 public:
  constexpr ObjectPtr() : ptr_(kNullAddress) {}
  explicit constexpr ObjectPtr(Address ptr) : ptr_(ptr) {}

  // Enable incremental transition.
  operator Object*() const { return reinterpret_cast<Object*>(ptr()); }

  bool operator==(const ObjectPtr other) const {
    return this->ptr() == other.ptr();
  }
  bool operator!=(const ObjectPtr other) const {
    return this->ptr() != other.ptr();
  }
  // Usage in std::set requires operator<.
  bool operator<(const ObjectPtr other) const {
    return this->ptr() < other.ptr();
  }

  // Returns the tagged "(heap) object pointer" representation of this object.
  constexpr Address ptr() const { return ptr_; }

  ObjectPtr* operator->() { return this; }
  const ObjectPtr* operator->() const { return this; }

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL
#define DECL_STRUCT_PREDICATE(NAME, Name, name) V8_INLINE bool Is##Name() const;
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE
#define IS_TYPE_FUNCTION_DECL(Type, Value)            \
  V8_INLINE bool Is##Type(Isolate* isolate) const;    \
  V8_INLINE bool Is##Type(ReadOnlyRoots roots) const; \
  V8_INLINE bool Is##Type() const;
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL
  inline bool IsHashTableBase() const;
  V8_INLINE bool IsSmallOrderedHashTable() const;

  inline bool IsObject() const { return true; }
  inline double Number() const;
  inline bool ToInt32(int32_t* value) const;
  inline bool ToUint32(uint32_t* value) const;

  // ECMA-262 9.2.
  bool BooleanValue(Isolate* isolate);

  inline bool FilterKey(PropertyFilter filter);

  // Returns the permanent hash code associated with this object. May return
  // undefined if not yet created.
  inline Object* GetHash();

  // Returns the permanent hash code associated with this object depending on
  // the actual object type. May create and store a hash code if needed and none
  // exists.
  Smi GetOrCreateHash(Isolate* isolate);

  // Checks whether this object has the same value as the given one.  This
  // function is implemented according to ES5, section 9.12 and can be used
  // to implement the Object.is function.
  V8_EXPORT_PRIVATE bool SameValue(Object* other);

  // Tries to convert an object to an array index. Returns true and sets the
  // output parameter if it succeeds. Equivalent to ToArrayLength, but does not
  // allow kMaxUInt32.
  V8_WARN_UNUSED_RESULT inline bool ToArrayIndex(uint32_t* index) const;

#ifdef VERIFY_HEAP
  void ObjectVerify(Isolate* isolate) {
    reinterpret_cast<Object*>(ptr())->ObjectVerify(isolate);
  }
  // Verify a pointer is a valid object pointer.
  static void VerifyPointer(Isolate* isolate, Object* p);
#endif

  inline void ShortPrint(FILE* out = stdout);
  void ShortPrint(std::ostream& os);  // NOLINT
  inline void Print();
  inline void Print(std::ostream& os);

  // For use with std::unordered_set.
  struct Hasher {
    size_t operator()(const ObjectPtr o) const {
      return std::hash<v8::internal::Address>{}(o.ptr());
    }
  };

 private:
  friend class ObjectSlot;
  Address ptr_;
};

// In heap-objects.h to be usable without heap-objects-inl.h inclusion.
bool ObjectPtr::IsSmi() const { return HAS_SMI_TAG(ptr()); }
bool ObjectPtr::IsHeapObject() const {
  DCHECK_EQ(!IsSmi(), Internals::HasHeapObjectTag(ptr()));
  return !IsSmi();
}

// Replacement for HeapObject; temporarily separate for incremental transition:
class HeapObjectPtr : public ObjectPtr {
 public:
  inline Map map() const;
  inline void set_map(Map value);
  inline void set_map_after_allocation(
      Map value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  // The no-write-barrier version.  This is OK if the object is white and in
  // new space, or if the value is an immortal immutable object, like the maps
  // of primitive (non-JS) objects like strings, heap numbers etc.
  inline void set_map_no_write_barrier(Map value);

  inline ObjectSlot map_slot();
  inline MapWord map_word() const;
  inline void set_map_word(MapWord map_word);

  // Set the map using release store
  inline void synchronized_set_map(Map value);
  inline void synchronized_set_map_word(MapWord map_word);

  inline WriteBarrierMode GetWriteBarrierMode(
      const DisallowHeapAllocation& promise);

  // Enable incremental transition.
  operator HeapObject*() { return reinterpret_cast<HeapObject*>(ptr()); }
  operator const HeapObject*() const {
    return reinterpret_cast<const HeapObject*>(ptr());
  }

  bool is_null() const { return ptr() == kNullAddress; }

  bool IsHeapObjectPtr() const { return IsHeapObject(); }

  inline ReadOnlyRoots GetReadOnlyRoots() const;

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  // Untagged aligned address.
  inline Address address() const { return ptr() - kHeapObjectTag; }

  inline int Size() const;
  inline int SizeFromMap(Map map) const;

  inline ObjectSlot RawField(int byte_offset) const;
  inline MaybeObjectSlot RawMaybeWeakField(int byte_offset) const;

#ifdef OBJECT_PRINT
  void PrintHeader(std::ostream& os, const char* id);  // NOLINT
#endif
  void HeapObjectVerify(Isolate* isolate);
#ifdef VERIFY_HEAP
  static void VerifyHeapPointer(Isolate* isolate, Object* p);
#endif

#ifdef VERIFY_HEAP
  inline void VerifyObjectField(Isolate* isolate, int offset);
  inline void VerifySmiField(int offset);
  inline void VerifyMaybeObjectField(Isolate* isolate, int offset);
#endif

  static const int kMapOffset = HeapObject::kMapOffset;
  static const int kHeaderSize = HeapObject::kHeaderSize;

  inline Address GetFieldAddress(int field_offset) const;

 protected:
  // Special-purpose constructor for subclasses that have fast paths where
  // their ptr() is a Smi.
  enum class AllowInlineSmiStorage { kRequireHeapObjectTag, kAllowBeingASmi };
  inline HeapObjectPtr(Address ptr, AllowInlineSmiStorage allow_smi);

  OBJECT_CONSTRUCTORS(HeapObjectPtr, ObjectPtr)
};

// Replacement for NeverReadOnlySpaceObject, temporarily separate for
// incremental transition.
// Helper class for objects that can never be in RO space.
// TODO(leszeks): Add checks in the factory that we never allocate these objects
// in RO space.
// TODO(3770): Get rid of the duplication.
class NeverReadOnlySpaceObjectPtr {
 public:
  // The Heap the object was allocated in. Used also to access Isolate.
  static inline Heap* GetHeap(const HeapObjectPtr object);

  // Convenience method to get current isolate.
  static inline Isolate* GetIsolate(const HeapObjectPtr object);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_H_
