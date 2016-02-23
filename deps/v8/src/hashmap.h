// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HASHMAP_H_
#define V8_HASHMAP_H_

#include "src/allocation.h"
#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

template<class AllocationPolicy>
class TemplateHashMapImpl {
 public:
  typedef bool (*MatchFun) (void* key1, void* key2);

  // The default capacity.  This is used by the call sites which want
  // to pass in a non-default AllocationPolicy but want to use the
  // default value of capacity specified by the implementation.
  static const uint32_t kDefaultHashMapCapacity = 8;

  // initial_capacity is the size of the initial hash map;
  // it must be a power of 2 (and thus must not be 0).
  TemplateHashMapImpl(MatchFun match,
                      uint32_t capacity = kDefaultHashMapCapacity,
                      AllocationPolicy allocator = AllocationPolicy());

  ~TemplateHashMapImpl();

  // HashMap entries are (key, value, hash) triplets.
  // Some clients may not need to use the value slot
  // (e.g. implementers of sets, where the key is the value).
  struct Entry {
    void* key;
    void* value;
    uint32_t hash;  // The full hash value for key
    int order;  // If you never remove entries this is the insertion order.
  };

  // If an entry with matching key is found, returns that entry.
  // Otherwise, NULL is returned.
  Entry* Lookup(void* key, uint32_t hash) const;

  // If an entry with matching key is found, returns that entry.
  // If no matching entry is found, a new entry is inserted with
  // corresponding key, key hash, and NULL value.
  Entry* LookupOrInsert(void* key, uint32_t hash,
                        AllocationPolicy allocator = AllocationPolicy());

  // Removes the entry with matching key.
  // It returns the value of the deleted entry
  // or null if there is no value for such key.
  void* Remove(void* key, uint32_t hash);

  // Empties the hash map (occupancy() == 0).
  void Clear();

  // The number of (non-empty) entries in the table.
  uint32_t occupancy() const { return occupancy_; }

  // The capacity of the table. The implementation
  // makes sure that occupancy is at most 80% of
  // the table capacity.
  uint32_t capacity() const { return capacity_; }

  // Iteration
  //
  // for (Entry* p = map.Start(); p != NULL; p = map.Next(p)) {
  //   ...
  // }
  //
  // If entries are inserted during iteration, the effect of
  // calling Next() is undefined.
  Entry* Start() const;
  Entry* Next(Entry* p) const;

  // Some match functions defined for convenience.
  static bool PointersMatch(void* key1, void* key2) {
    return key1 == key2;
  }

 private:
  MatchFun match_;
  Entry* map_;
  uint32_t capacity_;
  uint32_t occupancy_;

  Entry* map_end() const { return map_ + capacity_; }
  Entry* Probe(void* key, uint32_t hash) const;
  void Initialize(uint32_t capacity, AllocationPolicy allocator);
  void Resize(AllocationPolicy allocator);
};

typedef TemplateHashMapImpl<FreeStoreAllocationPolicy> HashMap;

template<class AllocationPolicy>
TemplateHashMapImpl<AllocationPolicy>::TemplateHashMapImpl(
    MatchFun match, uint32_t initial_capacity, AllocationPolicy allocator) {
  match_ = match;
  Initialize(initial_capacity, allocator);
}


template<class AllocationPolicy>
TemplateHashMapImpl<AllocationPolicy>::~TemplateHashMapImpl() {
  AllocationPolicy::Delete(map_);
}


template <class AllocationPolicy>
typename TemplateHashMapImpl<AllocationPolicy>::Entry*
TemplateHashMapImpl<AllocationPolicy>::Lookup(void* key, uint32_t hash) const {
  Entry* p = Probe(key, hash);
  return p->key != NULL ? p : NULL;
}


template <class AllocationPolicy>
typename TemplateHashMapImpl<AllocationPolicy>::Entry*
TemplateHashMapImpl<AllocationPolicy>::LookupOrInsert(
    void* key, uint32_t hash, AllocationPolicy allocator) {
  // Find a matching entry.
  Entry* p = Probe(key, hash);
  if (p->key != NULL) {
    return p;
  }

  // No entry found; insert one.
  p->key = key;
  p->value = NULL;
  p->hash = hash;
  p->order = occupancy_;
  occupancy_++;

  // Grow the map if we reached >= 80% occupancy.
  if (occupancy_ + occupancy_ / 4 >= capacity_) {
    Resize(allocator);
    p = Probe(key, hash);
  }

  return p;
}


template<class AllocationPolicy>
void* TemplateHashMapImpl<AllocationPolicy>::Remove(void* key, uint32_t hash) {
  // Lookup the entry for the key to remove.
  Entry* p = Probe(key, hash);
  if (p->key == NULL) {
    // Key not found nothing to remove.
    return NULL;
  }

  void* value = p->value;
  // To remove an entry we need to ensure that it does not create an empty
  // entry that will cause the search for another entry to stop too soon. If all
  // the entries between the entry to remove and the next empty slot have their
  // initial position inside this interval, clearing the entry to remove will
  // not break the search. If, while searching for the next empty entry, an
  // entry is encountered which does not have its initial position between the
  // entry to remove and the position looked at, then this entry can be moved to
  // the place of the entry to remove without breaking the search for it. The
  // entry made vacant by this move is now the entry to remove and the process
  // starts over.
  // Algorithm from http://en.wikipedia.org/wiki/Open_addressing.

  // This guarantees loop termination as there is at least one empty entry so
  // eventually the removed entry will have an empty entry after it.
  DCHECK(occupancy_ < capacity_);

  // p is the candidate entry to clear. q is used to scan forwards.
  Entry* q = p;  // Start at the entry to remove.
  while (true) {
    // Move q to the next entry.
    q = q + 1;
    if (q == map_end()) {
      q = map_;
    }

    // All entries between p and q have their initial position between p and q
    // and the entry p can be cleared without breaking the search for these
    // entries.
    if (q->key == NULL) {
      break;
    }

    // Find the initial position for the entry at position q.
    Entry* r = map_ + (q->hash & (capacity_ - 1));

    // If the entry at position q has its initial position outside the range
    // between p and q it can be moved forward to position p and will still be
    // found. There is now a new candidate entry for clearing.
    if ((q > p && (r <= p || r > q)) ||
        (q < p && (r <= p && r > q))) {
      *p = *q;
      p = q;
    }
  }

  // Clear the entry which is allowed to en emptied.
  p->key = NULL;
  occupancy_--;
  return value;
}


template<class AllocationPolicy>
void TemplateHashMapImpl<AllocationPolicy>::Clear() {
  // Mark all entries as empty.
  const Entry* end = map_end();
  for (Entry* p = map_; p < end; p++) {
    p->key = NULL;
  }
  occupancy_ = 0;
}


template<class AllocationPolicy>
typename TemplateHashMapImpl<AllocationPolicy>::Entry*
    TemplateHashMapImpl<AllocationPolicy>::Start() const {
  return Next(map_ - 1);
}


template<class AllocationPolicy>
typename TemplateHashMapImpl<AllocationPolicy>::Entry*
    TemplateHashMapImpl<AllocationPolicy>::Next(Entry* p) const {
  const Entry* end = map_end();
  DCHECK(map_ - 1 <= p && p < end);
  for (p++; p < end; p++) {
    if (p->key != NULL) {
      return p;
    }
  }
  return NULL;
}


template <class AllocationPolicy>
typename TemplateHashMapImpl<AllocationPolicy>::Entry*
TemplateHashMapImpl<AllocationPolicy>::Probe(void* key, uint32_t hash) const {
  DCHECK(key != NULL);

  DCHECK(base::bits::IsPowerOfTwo32(capacity_));
  Entry* p = map_ + (hash & (capacity_ - 1));
  const Entry* end = map_end();
  DCHECK(map_ <= p && p < end);

  DCHECK(occupancy_ < capacity_);  // Guarantees loop termination.
  while (p->key != NULL && (hash != p->hash || !match_(key, p->key))) {
    p++;
    if (p >= end) {
      p = map_;
    }
  }

  return p;
}


template<class AllocationPolicy>
void TemplateHashMapImpl<AllocationPolicy>::Initialize(
    uint32_t capacity, AllocationPolicy allocator) {
  DCHECK(base::bits::IsPowerOfTwo32(capacity));
  map_ = reinterpret_cast<Entry*>(allocator.New(capacity * sizeof(Entry)));
  if (map_ == NULL) {
    v8::internal::FatalProcessOutOfMemory("HashMap::Initialize");
    return;
  }
  capacity_ = capacity;
  Clear();
}


template<class AllocationPolicy>
void TemplateHashMapImpl<AllocationPolicy>::Resize(AllocationPolicy allocator) {
  Entry* map = map_;
  uint32_t n = occupancy_;

  // Allocate larger map.
  Initialize(capacity_ * 2, allocator);

  // Rehash all current entries.
  for (Entry* p = map; n > 0; p++) {
    if (p->key != NULL) {
      Entry* entry = LookupOrInsert(p->key, p->hash, allocator);
      entry->value = p->value;
      entry->order = p->order;
      n--;
    }
  }

  // Delete old map.
  AllocationPolicy::Delete(map);
}


// A hash map for pointer keys and values with an STL-like interface.
template<class Key, class Value, class AllocationPolicy>
class TemplateHashMap: private TemplateHashMapImpl<AllocationPolicy> {
 public:
  STATIC_ASSERT(sizeof(Key*) == sizeof(void*));  // NOLINT
  STATIC_ASSERT(sizeof(Value*) == sizeof(void*));  // NOLINT
  struct value_type {
    Key* first;
    Value* second;
  };

  class Iterator {
   public:
    Iterator& operator++() {
      entry_ = map_->Next(entry_);
      return *this;
    }

    value_type* operator->() { return reinterpret_cast<value_type*>(entry_); }
    bool operator!=(const Iterator& other) { return  entry_ != other.entry_; }

   private:
    Iterator(const TemplateHashMapImpl<AllocationPolicy>* map,
             typename TemplateHashMapImpl<AllocationPolicy>::Entry* entry) :
        map_(map), entry_(entry) { }

    const TemplateHashMapImpl<AllocationPolicy>* map_;
    typename TemplateHashMapImpl<AllocationPolicy>::Entry* entry_;

    friend class TemplateHashMap;
  };

  TemplateHashMap(
      typename TemplateHashMapImpl<AllocationPolicy>::MatchFun match,
      AllocationPolicy allocator = AllocationPolicy())
        : TemplateHashMapImpl<AllocationPolicy>(
            match,
            TemplateHashMapImpl<AllocationPolicy>::kDefaultHashMapCapacity,
            allocator) { }

  Iterator begin() const { return Iterator(this, this->Start()); }
  Iterator end() const { return Iterator(this, NULL); }
  Iterator find(Key* key, bool insert = false,
                AllocationPolicy allocator = AllocationPolicy()) {
    if (insert) {
      return Iterator(this, this->LookupOrInsert(key, key->Hash(), allocator));
    }
    return Iterator(this, this->Lookup(key, key->Hash()));
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HASHMAP_H_
