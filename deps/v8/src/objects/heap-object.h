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

  // TODO(3770): Drop these after migrating the respective classes:
  inline bool IsFixedArrayBasePtr() const;
  inline bool IsFixedArrayPtr() const;

  inline bool IsObject() const { return true; }
  inline double Number() const;
  inline bool ToInt32(int32_t* value) const;
  inline bool ToUint32(uint32_t* value) const;

#ifdef VERIFY_HEAP
  void ObjectVerify(Isolate* isolate) {
    reinterpret_cast<Object*>(ptr())->ObjectVerify(isolate);
  }
#endif

  inline void ShortPrint(FILE* out = stdout);
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
  inline void set_map_after_allocation(
      Map value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline ObjectSlot map_slot();

  inline WriteBarrierMode GetWriteBarrierMode(
      const DisallowHeapAllocation& promise);

  // Enable incremental transition.
  operator HeapObject*() { return reinterpret_cast<HeapObject*>(ptr()); }
  operator const HeapObject*() const {
    return reinterpret_cast<const HeapObject*>(ptr());
  }

  bool is_null() const { return ptr() == kNullAddress; }

  bool IsHeapObject() const { return true; }
  bool IsHeapObjectPtr() const { return true; }

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

  static const int kMapOffset = HeapObject::kMapOffset;

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
