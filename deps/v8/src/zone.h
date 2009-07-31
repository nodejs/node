// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_ZONE_H_
#define V8_ZONE_H_

namespace v8 {
namespace internal {


// Zone scopes are in one of two modes.  Either they delete the zone
// on exit or they do not.
enum ZoneScopeMode {
  DELETE_ON_EXIT,
  DONT_DELETE_ON_EXIT
};


// The Zone supports very fast allocation of small chunks of
// memory. The chunks cannot be deallocated individually, but instead
// the Zone supports deallocating all chunks in one fast
// operation. The Zone is used to hold temporary data structures like
// the abstract syntax tree, which is deallocated after compilation.

// Note: There is no need to initialize the Zone; the first time an
// allocation is attempted, a segment of memory will be requested
// through a call to malloc().

// Note: The implementation is inherently not thread safe. Do not use
// from multi-threaded code.

class Zone {
 public:
  // Allocate 'size' bytes of memory in the Zone; expands the Zone by
  // allocating new segments of memory on demand using malloc().
  static inline void* New(int size);

  template <typename T>
  static inline T* NewArray(int length);

  // Delete all objects and free all memory allocated in the Zone.
  static void DeleteAll();

  // Returns true if more memory has been allocated in zones than
  // the limit allows.
  static inline bool excess_allocation();

  static inline void adjust_segment_bytes_allocated(int delta);

 private:

  // All pointers returned from New() have this alignment.
  static const int kAlignment = kPointerSize;

  // Never allocate segments smaller than this size in bytes.
  static const int kMinimumSegmentSize = 8 * KB;

  // Never allocate segments larger than this size in bytes.
  static const int kMaximumSegmentSize = 1 * MB;

  // Never keep segments larger than this size in bytes around.
  static const int kMaximumKeptSegmentSize = 64 * KB;

  // Report zone excess when allocation exceeds this limit.
  static int zone_excess_limit_;

  // The number of bytes allocated in segments.  Note that this number
  // includes memory allocated from the OS but not yet allocated from
  // the zone.
  static int segment_bytes_allocated_;

  // The Zone is intentionally a singleton; you should not try to
  // allocate instances of the class.
  Zone() { UNREACHABLE(); }


  // Expand the Zone to hold at least 'size' more bytes and allocate
  // the bytes. Returns the address of the newly allocated chunk of
  // memory in the Zone. Should only be called if there isn't enough
  // room in the Zone already.
  static Address NewExpand(int size);


  // The free region in the current (front) segment is represented as
  // the half-open interval [position, limit). The 'position' variable
  // is guaranteed to be aligned as dictated by kAlignment.
  static Address position_;
  static Address limit_;
};


// ZoneObject is an abstraction that helps define classes of objects
// allocated in the Zone. Use it as a base class; see ast.h.
class ZoneObject {
 public:
  // Allocate a new ZoneObject of 'size' bytes in the Zone.
  void* operator new(size_t size) { return Zone::New(size); }

  // Ideally, the delete operator should be private instead of
  // public, but unfortunately the compiler sometimes synthesizes
  // (unused) destructors for classes derived from ZoneObject, which
  // require the operator to be visible. MSVC requires the delete
  // operator to be public.

  // ZoneObjects should never be deleted individually; use
  // Zone::DeleteAll() to delete all zone objects in one go.
  void operator delete(void*, size_t) { UNREACHABLE(); }
};


class AssertNoZoneAllocation {
 public:
  AssertNoZoneAllocation() : prev_(allow_allocation_) {
    allow_allocation_ = false;
  }
  ~AssertNoZoneAllocation() { allow_allocation_ = prev_; }
  static bool allow_allocation() { return allow_allocation_; }
 private:
  bool prev_;
  static bool allow_allocation_;
};


// The ZoneListAllocationPolicy is used to specialize the GenericList
// implementation to allocate ZoneLists and their elements in the
// Zone.
class ZoneListAllocationPolicy {
 public:
  // Allocate 'size' bytes of memory in the zone.
  static void* New(int size) {  return Zone::New(size); }

  // De-allocation attempts are silently ignored.
  static void Delete(void* p) { }
};


// ZoneLists are growable lists with constant-time access to the
// elements. The list itself and all its elements are allocated in the
// Zone. ZoneLists cannot be deleted individually; you can delete all
// objects in the Zone by calling Zone::DeleteAll().
template<typename T>
class ZoneList: public List<T, ZoneListAllocationPolicy> {
 public:
  // Construct a new ZoneList with the given capacity; the length is
  // always zero. The capacity must be non-negative.
  explicit ZoneList(int capacity)
      : List<T, ZoneListAllocationPolicy>(capacity) { }
};


// ZoneScopes keep track of the current parsing and compilation
// nesting and cleans up generated ASTs in the Zone when exiting the
// outer-most scope.
class ZoneScope BASE_EMBEDDED {
 public:
  explicit ZoneScope(ZoneScopeMode mode) : mode_(mode) {
    nesting_++;
  }

  virtual ~ZoneScope() {
    if (ShouldDeleteOnExit()) Zone::DeleteAll();
    --nesting_;
  }

  bool ShouldDeleteOnExit() {
    return nesting_ == 1 && mode_ == DELETE_ON_EXIT;
  }

  // For ZoneScopes that do not delete on exit by default, call this
  // method to request deletion on exit.
  void DeleteOnExit() {
    mode_ = DELETE_ON_EXIT;
  }

  static int nesting() { return nesting_; }

 private:
  ZoneScopeMode mode_;
  static int nesting_;
};


template <typename Node, class Callback>
static void DoForEach(Node* node, Callback* callback);


// A zone splay tree.  The config type parameter encapsulates the
// different configurations of a concrete splay tree:
//
//   typedef Key: the key type
//   typedef Value: the value type
//   static const kNoKey: the dummy key used when no key is set
//   static const kNoValue: the dummy value used to initialize nodes
//   int (Compare)(Key& a, Key& b) -> {-1, 0, 1}: comparison function
//
template <typename Config>
class ZoneSplayTree : public ZoneObject {
 public:
  typedef typename Config::Key Key;
  typedef typename Config::Value Value;

  class Locator;

  ZoneSplayTree() : root_(NULL) { }

  // Inserts the given key in this tree with the given value.  Returns
  // true if a node was inserted, otherwise false.  If found the locator
  // is enabled and provides access to the mapping for the key.
  bool Insert(const Key& key, Locator* locator);

  // Looks up the key in this tree and returns true if it was found,
  // otherwise false.  If the node is found the locator is enabled and
  // provides access to the mapping for the key.
  bool Find(const Key& key, Locator* locator);

  // Finds the mapping with the greatest key less than or equal to the
  // given key.
  bool FindGreatestLessThan(const Key& key, Locator* locator);

  // Find the mapping with the greatest key in this tree.
  bool FindGreatest(Locator* locator);

  // Finds the mapping with the least key greater than or equal to the
  // given key.
  bool FindLeastGreaterThan(const Key& key, Locator* locator);

  // Find the mapping with the least key in this tree.
  bool FindLeast(Locator* locator);

  // Remove the node with the given key from the tree.
  bool Remove(const Key& key);

  bool is_empty() { return root_ == NULL; }

  // Perform the splay operation for the given key. Moves the node with
  // the given key to the top of the tree.  If no node has the given
  // key, the last node on the search path is moved to the top of the
  // tree.
  void Splay(const Key& key);

  class Node : public ZoneObject {
   public:
    Node(const Key& key, const Value& value)
        : key_(key),
          value_(value),
          left_(NULL),
          right_(NULL) { }
    Key key() { return key_; }
    Value value() { return value_; }
    Node* left() { return left_; }
    Node* right() { return right_; }
   private:
    friend class ZoneSplayTree;
    friend class Locator;
    Key key_;
    Value value_;
    Node* left_;
    Node* right_;
  };

  // A locator provides access to a node in the tree without actually
  // exposing the node.
  class Locator {
   public:
    explicit Locator(Node* node) : node_(node) { }
    Locator() : node_(NULL) { }
    const Key& key() { return node_->key_; }
    Value& value() { return node_->value_; }
    void set_value(const Value& value) { node_->value_ = value; }
    inline void bind(Node* node) { node_ = node; }
   private:
    Node* node_;
  };

  template <class Callback>
  void ForEach(Callback* c) {
    DoForEach<typename ZoneSplayTree<Config>::Node, Callback>(root_, c);
  }

 private:
  Node* root_;
};


} }  // namespace v8::internal

#endif  // V8_ZONE_H_
