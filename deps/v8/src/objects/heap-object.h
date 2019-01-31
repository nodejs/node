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

// HeapObject is the superclass for all classes describing heap allocated
// objects.
class HeapObject : public Object {
 public:
  bool is_null() const { return ptr() == kNullAddress; }

  // [map]: Contains a map which contains the object's reflective
  // information.
  inline Map map() const;
  inline void set_map(Map value);

  inline MapWordSlot map_slot() const;

  // The no-write-barrier version.  This is OK if the object is white and in
  // new space, or if the value is an immortal immutable object, like the maps
  // of primitive (non-JS) objects like strings, heap numbers etc.
  inline void set_map_no_write_barrier(Map value);

  // Get the map using acquire load.
  inline Map synchronized_map() const;
  inline MapWord synchronized_map_word() const;

  // Set the map using release store
  inline void synchronized_set_map(Map value);
  inline void synchronized_set_map_word(MapWord map_word);

  // Initialize the map immediately after the object is allocated.
  // Do not use this outside Heap.
  inline void set_map_after_allocation(
      Map value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // During garbage collection, the map word of a heap object does not
  // necessarily contain a map pointer.
  inline MapWord map_word() const;
  inline void set_map_word(MapWord map_word);

  // TODO(v8:7464): Once RO_SPACE is shared between isolates, this method can be
  // removed as ReadOnlyRoots will be accessible from a global variable. For now
  // this method exists to help remove GetIsolate/GetHeap from HeapObject, in a
  // way that doesn't require passing Isolate/Heap down huge call chains or to
  // places where it might not be safe to access it.
  inline ReadOnlyRoots GetReadOnlyRoots() const;

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsExternal(Isolate* isolate) const;

// Oddball checks are faster when they are raw pointer comparisons, so the
// isolate/read-only roots overloads should be preferred where possible.
#define IS_TYPE_FUNCTION_DECL(Type, Value)            \
  V8_INLINE bool Is##Type(Isolate* isolate) const;    \
  V8_INLINE bool Is##Type(ReadOnlyRoots roots) const; \
  V8_INLINE bool Is##Type() const;
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsNullOrUndefined(Isolate* isolate) const;
  V8_INLINE bool IsNullOrUndefined(ReadOnlyRoots roots) const;
  V8_INLINE bool IsNullOrUndefined() const;

#define DECL_STRUCT_PREDICATE(NAME, Name, name) V8_INLINE bool Is##Name() const;
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // Converts an address to a HeapObject pointer.
  static inline HeapObject FromAddress(Address address);

  // Returns the address of this HeapObject.
  inline Address address() const { return ptr() - kHeapObjectTag; }

  // Iterates over pointers contained in the object (including the Map).
  // If it's not performance critical iteration use the non-templatized
  // version.
  void Iterate(ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateFast(ObjectVisitor* v);

  // Iterates over all pointers contained in the object except the
  // first map pointer.  The object type is given in the first
  // parameter. This function does not access the map pointer in the
  // object, and so is safe to call while the map pointer is modified.
  // If it's not performance critical iteration use the non-templatized
  // version.
  void IterateBody(ObjectVisitor* v);
  void IterateBody(Map map, int object_size, ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateBodyFast(ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateBodyFast(Map map, int object_size, ObjectVisitor* v);

  // Returns true if the object contains a tagged value at given offset.
  // It is used for invalid slots filtering. If the offset points outside
  // of the object or to the map word, the result is UNDEFINED (!!!).
  bool IsValidSlot(Map map, int offset);

  // Returns the heap object's size in bytes
  inline int Size() const;

  // Given a heap object's map pointer, returns the heap size in bytes
  // Useful when the map pointer field is used for other purposes.
  // GC internal.
  inline int SizeFromMap(Map map) const;

  // Returns the field at offset in obj, as a read/write Object reference.
  // Does no checking, and is safe to use during GC, while maps are invalid.
  // Does not invoke write barrier, so should only be assigned to
  // during marking GC.
  inline ObjectSlot RawField(int byte_offset) const;
  static inline ObjectSlot RawField(const HeapObject obj, int offset);
  inline MaybeObjectSlot RawMaybeWeakField(int byte_offset) const;
  static inline MaybeObjectSlot RawMaybeWeakField(HeapObject obj, int offset);

  DECL_CAST(HeapObject)

  // Return the write barrier mode for this. Callers of this function
  // must be able to present a reference to an DisallowHeapAllocation
  // object as a sign that they are not going to use this function
  // from code that allocates and thus invalidates the returned write
  // barrier mode.
  inline WriteBarrierMode GetWriteBarrierMode(
      const DisallowHeapAllocation& promise);

  // Dispatched behavior.
  void HeapObjectShortPrint(std::ostream& os);  // NOLINT
#ifdef OBJECT_PRINT
  void PrintHeader(std::ostream& os, const char* id);  // NOLINT
#endif
  DECL_PRINTER(HeapObject)
  DECL_VERIFIER(HeapObject)
#ifdef VERIFY_HEAP
  inline void VerifyObjectField(Isolate* isolate, int offset);
  inline void VerifySmiField(int offset);
  inline void VerifyMaybeObjectField(Isolate* isolate, int offset);

  // Verify a pointer is a valid HeapObject pointer that points to object
  // areas in the heap.
  static void VerifyHeapPointer(Isolate* isolate, Object p);
#endif

  static inline AllocationAlignment RequiredAlignment(Map map);

  // Whether the object needs rehashing. That is the case if the object's
  // content depends on FLAG_hash_seed. When the object is deserialized into
  // a heap with a different hash seed, these objects need to adapt.
  inline bool NeedsRehashing() const;

  // Rehashing support is not implemented for all objects that need rehashing.
  // With objects that need rehashing but cannot be rehashed, rehashing has to
  // be disabled.
  bool CanBeRehashed() const;

  // Rehash the object based on the layout inferred from its map.
  void RehashBasedOnMap(Isolate* isolate);

  // Layout description.
#define HEAP_OBJECT_FIELDS(V) \
  V(kMapOffset, kTaggedSize)  \
  /* Header size. */          \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Object::kHeaderSize, HEAP_OBJECT_FIELDS)
#undef HEAP_OBJECT_FIELDS

  STATIC_ASSERT(kMapOffset == Internals::kHeapObjectMapOffset);

  inline Address GetFieldAddress(int field_offset) const;

 protected:
  // Special-purpose constructor for subclasses that have fast paths where
  // their ptr() is a Smi.
  enum class AllowInlineSmiStorage { kRequireHeapObjectTag, kAllowBeingASmi };
  inline HeapObject(Address ptr, AllowInlineSmiStorage allow_smi);

  OBJECT_CONSTRUCTORS(HeapObject, Object);
};

// Helper class for objects that can never be in RO space.
class NeverReadOnlySpaceObject {
 public:
  // The Heap the object was allocated in. Used also to access Isolate.
  static inline Heap* GetHeap(const HeapObject object);

  // Convenience method to get current isolate.
  static inline Isolate* GetIsolate(const HeapObject object);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_H_
