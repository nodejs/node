// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_H_
#define V8_ZONE_ZONE_H_

#include <limits>

#include "src/base/hashmap.h"
#include "src/base/logging.h"
#include "src/globals.h"
#include "src/splay-tree.h"
#include "src/zone/accounting-allocator.h"

#ifndef ZONE_NAME
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define ZONE_NAME __FILE__ ":" TOSTRING(__LINE__)
#endif

namespace v8 {
namespace internal {

// The Zone supports very fast allocation of small chunks of
// memory. The chunks cannot be deallocated individually, but instead
// the Zone supports deallocating all chunks in one fast
// operation. The Zone is used to hold temporary data structures like
// the abstract syntax tree, which is deallocated after compilation.
//
// Note: There is no need to initialize the Zone; the first time an
// allocation is attempted, a segment of memory will be requested
// through the allocator.
//
// Note: The implementation is inherently not thread safe. Do not use
// from multi-threaded code.

enum class SegmentSize { kLarge, kDefault };

class V8_EXPORT_PRIVATE Zone final {
 public:
  Zone(AccountingAllocator* allocator, const char* name,
       SegmentSize segment_size = SegmentSize::kDefault);
  ~Zone();

  // Allocate 'size' bytes of memory in the Zone; expands the Zone by
  // allocating new segments of memory on demand using malloc().
  void* New(size_t size);

  template <typename T>
  T* NewArray(size_t length) {
    DCHECK_LT(length, std::numeric_limits<size_t>::max() / sizeof(T));
    return static_cast<T*>(New(length * sizeof(T)));
  }

  // Seals the zone to prevent any further allocation.
  void Seal() { sealed_ = true; }

  // Returns true if more memory has been allocated in zones than
  // the limit allows.
  bool excess_allocation() const {
    return segment_bytes_allocated_ > kExcessLimit;
  }

  const char* name() const { return name_; }

  size_t allocation_size() const { return allocation_size_; }

  AccountingAllocator* allocator() const { return allocator_; }

 private:
  // All pointers returned from New() are 8-byte aligned.
  static const size_t kAlignmentInBytes = 8;

  // Never allocate segments smaller than this size in bytes.
  static const size_t kMinimumSegmentSize = 8 * KB;

  // Never allocate segments larger than this size in bytes.
  static const size_t kMaximumSegmentSize = 1 * MB;

  // Report zone excess when allocation exceeds this limit.
  static const size_t kExcessLimit = 256 * MB;

  // Deletes all objects and free all memory allocated in the Zone.
  void DeleteAll();

  // The number of bytes allocated in this zone so far.
  size_t allocation_size_;

  // The number of bytes allocated in segments.  Note that this number
  // includes memory allocated from the OS but not yet allocated from
  // the zone.
  size_t segment_bytes_allocated_;

  // Expand the Zone to hold at least 'size' more bytes and allocate
  // the bytes. Returns the address of the newly allocated chunk of
  // memory in the Zone. Should only be called if there isn't enough
  // room in the Zone already.
  Address NewExpand(size_t size);

  // Creates a new segment, sets it size, and pushes it to the front
  // of the segment chain. Returns the new segment.
  inline Segment* NewSegment(size_t requested_size);

  // The free region in the current (front) segment is represented as
  // the half-open interval [position, limit). The 'position' variable
  // is guaranteed to be aligned as dictated by kAlignment.
  Address position_;
  Address limit_;

  AccountingAllocator* allocator_;

  Segment* segment_head_;
  const char* name_;
  bool sealed_;
  SegmentSize segment_size_;
};

// ZoneObject is an abstraction that helps define classes of objects
// allocated in the Zone. Use it as a base class; see ast.h.
class ZoneObject {
 public:
  // Allocate a new ZoneObject of 'size' bytes in the Zone.
  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  // Ideally, the delete operator should be private instead of
  // public, but unfortunately the compiler sometimes synthesizes
  // (unused) destructors for classes derived from ZoneObject, which
  // require the operator to be visible. MSVC requires the delete
  // operator to be public.

  // ZoneObjects should never be deleted individually; use
  // Zone::DeleteAll() to delete all zone objects in one go.
  void operator delete(void*, size_t) { UNREACHABLE(); }
  void operator delete(void* pointer, Zone* zone) { UNREACHABLE(); }
};

// The ZoneAllocationPolicy is used to specialize generic data
// structures to allocate themselves and their elements in the Zone.
class ZoneAllocationPolicy final {
 public:
  explicit ZoneAllocationPolicy(Zone* zone) : zone_(zone) {}
  void* New(size_t size) { return zone()->New(size); }
  static void Delete(void* pointer) {}
  Zone* zone() const { return zone_; }

 private:
  Zone* zone_;
};

template <typename T>
class Vector;

// ZoneLists are growable lists with constant-time access to the
// elements. The list itself and all its elements are allocated in the
// Zone. ZoneLists cannot be deleted individually; you can delete all
// objects in the Zone by calling Zone::DeleteAll().
template <typename T>
class ZoneList final {
 public:
  // Construct a new ZoneList with the given capacity; the length is
  // always zero. The capacity must be non-negative.
  ZoneList(int capacity, Zone* zone) { Initialize(capacity, zone); }
  // Construct a new ZoneList from a std::initializer_list
  ZoneList(std::initializer_list<T> list, Zone* zone) {
    Initialize(static_cast<int>(list.size()), zone);
    for (auto& i : list) Add(i, zone);
  }
  // Construct a new ZoneList by copying the elements of the given ZoneList.
  ZoneList(const ZoneList<T>& other, Zone* zone) {
    Initialize(other.length(), zone);
    AddAll(other, zone);
  }

  INLINE(~ZoneList()) { DeleteData(data_); }

  // Please the MSVC compiler.  We should never have to execute this.
  INLINE(void operator delete(void* p, ZoneAllocationPolicy allocator)) {
    UNREACHABLE();
  }

  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  // Returns a reference to the element at index i. This reference is not safe
  // to use after operations that can change the list's backing store
  // (e.g. Add).
  inline T& operator[](int i) const {
    DCHECK_LE(0, i);
    DCHECK_GT(static_cast<unsigned>(length_), static_cast<unsigned>(i));
    return data_[i];
  }
  inline T& at(int i) const { return operator[](i); }
  inline T& last() const { return at(length_ - 1); }
  inline T& first() const { return at(0); }

  typedef T* iterator;
  inline iterator begin() const { return &data_[0]; }
  inline iterator end() const { return &data_[length_]; }

  INLINE(bool is_empty() const) { return length_ == 0; }
  INLINE(int length() const) { return length_; }
  INLINE(int capacity() const) { return capacity_; }

  Vector<T> ToVector() const { return Vector<T>(data_, length_); }

  Vector<const T> ToConstVector() const {
    return Vector<const T>(data_, length_);
  }

  INLINE(void Initialize(int capacity, Zone* zone)) {
    DCHECK_GE(capacity, 0);
    data_ = (capacity > 0) ? NewData(capacity, ZoneAllocationPolicy(zone))
                           : nullptr;
    capacity_ = capacity;
    length_ = 0;
  }

  // Adds a copy of the given 'element' to the end of the list,
  // expanding the list if necessary.
  void Add(const T& element, Zone* zone);
  // Add all the elements from the argument list to this list.
  void AddAll(const ZoneList<T>& other, Zone* zone);
  // Add all the elements from the vector to this list.
  void AddAll(const Vector<T>& other, Zone* zone);
  // Inserts the element at the specific index.
  void InsertAt(int index, const T& element, Zone* zone);

  // Added 'count' elements with the value 'value' and returns a
  // vector that allows access to the elements. The vector is valid
  // until the next change is made to this list.
  Vector<T> AddBlock(T value, int count, Zone* zone);

  // Overwrites the element at the specific index.
  void Set(int index, const T& element);

  // Removes the i'th element without deleting it even if T is a
  // pointer type; moves all elements above i "down". Returns the
  // removed element.  This function's complexity is linear in the
  // size of the list.
  T Remove(int i);

  // Removes the last element without deleting it even if T is a
  // pointer type. Returns the removed element.
  INLINE(T RemoveLast()) { return Remove(length_ - 1); }

  // Clears the list by freeing the storage memory. If you want to keep the
  // memory, use Rewind(0) instead. Be aware, that even if T is a
  // pointer type, clearing the list doesn't delete the entries.
  INLINE(void Clear());

  // Drops all but the first 'pos' elements from the list.
  INLINE(void Rewind(int pos));

  inline bool Contains(const T& elm) const;

  // Iterate through all list entries, starting at index 0.
  template <class Visitor>
  void Iterate(Visitor* visitor);

  // Sort all list entries (using QuickSort)
  template <typename CompareFunction>
  void Sort(CompareFunction cmp);
  template <typename CompareFunction>
  void StableSort(CompareFunction cmp, size_t start, size_t length);

  void operator delete(void* pointer) { UNREACHABLE(); }
  void operator delete(void* pointer, Zone* zone) { UNREACHABLE(); }

 private:
  T* data_;
  int capacity_;
  int length_;

  INLINE(T* NewData(int n, ZoneAllocationPolicy allocator)) {
    return static_cast<T*>(allocator.New(n * sizeof(T)));
  }
  INLINE(void DeleteData(T* data)) { ZoneAllocationPolicy::Delete(data); }

  // Increase the capacity of a full list, and add an element.
  // List must be full already.
  void ResizeAdd(const T& element, ZoneAllocationPolicy allocator);

  // Inlined implementation of ResizeAdd, shared by inlined and
  // non-inlined versions of ResizeAdd.
  void ResizeAddInternal(const T& element, ZoneAllocationPolicy allocator);

  // Resize the list.
  void Resize(int new_capacity, ZoneAllocationPolicy allocator);

  DISALLOW_COPY_AND_ASSIGN(ZoneList);
};

// A zone splay tree.  The config type parameter encapsulates the
// different configurations of a concrete splay tree (see splay-tree.h).
// The tree itself and all its elements are allocated in the Zone.
template <typename Config>
class ZoneSplayTree final : public SplayTree<Config, ZoneAllocationPolicy> {
 public:
  explicit ZoneSplayTree(Zone* zone)
      : SplayTree<Config, ZoneAllocationPolicy>(ZoneAllocationPolicy(zone)) {}
  ~ZoneSplayTree() {
    // Reset the root to avoid unneeded iteration over all tree nodes
    // in the destructor.  For a zone-allocated tree, nodes will be
    // freed by the Zone.
    SplayTree<Config, ZoneAllocationPolicy>::ResetRoot();
  }

  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  void operator delete(void* pointer) { UNREACHABLE(); }
  void operator delete(void* pointer, Zone* zone) { UNREACHABLE(); }
};

typedef base::PointerTemplateHashMapImpl<ZoneAllocationPolicy> ZoneHashMap;

typedef base::CustomMatcherTemplateHashMapImpl<ZoneAllocationPolicy>
    CustomMatcherZoneHashMap;

}  // namespace internal
}  // namespace v8

// The accidential pattern
//    new (zone) SomeObject()
// where SomeObject does not inherit from ZoneObject leads to nasty crashes.
// This triggers a compile-time error instead.
template <class T, typename = typename std::enable_if<std::is_convertible<
                       T, const v8::internal::Zone*>::value>::type>
void* operator new(size_t size, T zone) {
  static_assert(false && sizeof(T),
                "Placement new with a zone is only permitted for classes "
                "inheriting from ZoneObject");
  UNREACHABLE();
}

#endif  // V8_ZONE_ZONE_H_
