// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The reason we write our own hash map instead of using unordered_map in STL,
// is that STL containers use a mutex pool on debug build, which will lead to
// deadlock when we are using async signal handler.

#ifndef V8_BASE_HASHMAP_H_
#define V8_BASE_HASHMAP_H_

#include <stdlib.h>

#include "src/base/bits.h"
#include "src/base/hashmap-entry.h"
#include "src/base/logging.h"

namespace v8 {
namespace base {

class DefaultAllocationPolicy {
 public:
  V8_INLINE void* New(size_t size) { return malloc(size); }
  V8_INLINE static void Delete(void* p) { free(p); }
};

template <typename Key, typename Value, class MatchFun, class AllocationPolicy>
class TemplateHashMapImpl {
 public:
  using Entry = TemplateHashMapEntry<Key, Value>;

  // The default capacity.  This is used by the call sites which want
  // to pass in a non-default AllocationPolicy but want to use the
  // default value of capacity specified by the implementation.
  static const uint32_t kDefaultHashMapCapacity = 8;

  // initial_capacity is the size of the initial hash map;
  // it must be a power of 2 (and thus must not be 0).
  TemplateHashMapImpl(uint32_t capacity = kDefaultHashMapCapacity,
                      MatchFun match = MatchFun(),
                      AllocationPolicy allocator = AllocationPolicy());

  // Clones the given hashmap and creates a copy with the same entries.
  TemplateHashMapImpl(const TemplateHashMapImpl<Key, Value, MatchFun,
                                                AllocationPolicy>* original,
                      AllocationPolicy allocator = AllocationPolicy());

  ~TemplateHashMapImpl();

  // If an entry with matching key is found, returns that entry.
  // Otherwise, nullptr is returned.
  Entry* Lookup(const Key& key, uint32_t hash) const;

  // If an entry with matching key is found, returns that entry.
  // If no matching entry is found, a new entry is inserted with
  // corresponding key, key hash, and default initialized value.
  Entry* LookupOrInsert(const Key& key, uint32_t hash,
                        AllocationPolicy allocator = AllocationPolicy());

  // If an entry with matching key is found, returns that entry.
  // If no matching entry is found, a new entry is inserted with
  // corresponding key, key hash, and value created by func.
  template <typename Func>
  Entry* LookupOrInsert(const Key& key, uint32_t hash, const Func& value_func,
                        AllocationPolicy allocator = AllocationPolicy());

  Entry* InsertNew(const Key& key, uint32_t hash,
                   AllocationPolicy allocator = AllocationPolicy());

  // Removes the entry with matching key.
  // It returns the value of the deleted entry
  // or null if there is no value for such key.
  Value Remove(const Key& key, uint32_t hash);

  // Empties the hash map (occupancy() == 0).
  void Clear();

  // Empties the map and makes it unusable for allocation.
  void Invalidate() {
    AllocationPolicy::Delete(map_);
    map_ = nullptr;
    occupancy_ = 0;
    capacity_ = 0;
  }

  // The number of (non-empty) entries in the table.
  uint32_t occupancy() const { return occupancy_; }

  // The capacity of the table. The implementation
  // makes sure that occupancy is at most 80% of
  // the table capacity.
  uint32_t capacity() const { return capacity_; }

  // Iteration
  //
  // for (Entry* p = map.Start(); p != nullptr; p = map.Next(p)) {
  //   ...
  // }
  //
  // If entries are inserted during iteration, the effect of
  // calling Next() is undefined.
  Entry* Start() const;
  Entry* Next(Entry* entry) const;

  void Reset(AllocationPolicy allocator) {
    Initialize(capacity_, allocator);
    occupancy_ = 0;
  }

 protected:
  void Initialize(uint32_t capacity, AllocationPolicy allocator);

 private:
  Entry* map_;
  uint32_t capacity_;
  uint32_t occupancy_;
  // TODO(leszeks): This takes up space even if it has no state, maybe replace
  // with something that does the empty base optimisation e.g. std::tuple
  MatchFun match_;

  Entry* map_end() const { return map_ + capacity_; }
  Entry* Probe(const Key& key, uint32_t hash) const;
  Entry* FillEmptyEntry(Entry* entry, const Key& key, const Value& value,
                        uint32_t hash,
                        AllocationPolicy allocator = AllocationPolicy());
  void Resize(AllocationPolicy allocator);

  DISALLOW_COPY_AND_ASSIGN(TemplateHashMapImpl);
};
template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::
    TemplateHashMapImpl(uint32_t initial_capacity, MatchFun match,
                        AllocationPolicy allocator)
    : match_(match) {
  Initialize(initial_capacity, allocator);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::
    TemplateHashMapImpl(const TemplateHashMapImpl<Key, Value, MatchFun,
                                                  AllocationPolicy>* original,
                        AllocationPolicy allocator)
    : capacity_(original->capacity_),
      occupancy_(original->occupancy_),
      match_(original->match_) {
  map_ = reinterpret_cast<Entry*>(allocator.New(capacity_ * sizeof(Entry)));
  memcpy(map_, original->map_, capacity_ * sizeof(Entry));
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
TemplateHashMapImpl<Key, Value, MatchFun,
                    AllocationPolicy>::~TemplateHashMapImpl() {
  AllocationPolicy::Delete(map_);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Lookup(
    const Key& key, uint32_t hash) const {
  Entry* entry = Probe(key, hash);
  return entry->exists() ? entry : nullptr;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::LookupOrInsert(
    const Key& key, uint32_t hash, AllocationPolicy allocator) {
  return LookupOrInsert(key, hash, []() { return Value(); }, allocator);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
template <typename Func>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::LookupOrInsert(
    const Key& key, uint32_t hash, const Func& value_func,
    AllocationPolicy allocator) {
  // Find a matching entry.
  Entry* entry = Probe(key, hash);
  if (entry->exists()) {
    return entry;
  }

  return FillEmptyEntry(entry, key, value_func(), hash, allocator);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::InsertNew(
    const Key& key, uint32_t hash, AllocationPolicy allocator) {
  Entry* entry = Probe(key, hash);
  return FillEmptyEntry(entry, key, Value(), hash, allocator);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
Value TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Remove(
    const Key& key, uint32_t hash) {
  // Lookup the entry for the key to remove.
  Entry* p = Probe(key, hash);
  if (!p->exists()) {
    // Key not found nothing to remove.
    return nullptr;
  }

  Value value = p->value;
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
    if (!q->exists()) {
      break;
    }

    // Find the initial position for the entry at position q.
    Entry* r = map_ + (q->hash & (capacity_ - 1));

    // If the entry at position q has its initial position outside the range
    // between p and q it can be moved forward to position p and will still be
    // found. There is now a new candidate entry for clearing.
    if ((q > p && (r <= p || r > q)) || (q < p && (r <= p && r > q))) {
      *p = *q;
      p = q;
    }
  }

  // Clear the entry which is allowed to en emptied.
  p->clear();
  occupancy_--;
  return value;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
void TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Clear() {
  // Mark all entries as empty.
  for (size_t i = 0; i < capacity_; ++i) {
    map_[i].clear();
  }
  occupancy_ = 0;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Start() const {
  return Next(map_ - 1);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Next(
    Entry* entry) const {
  const Entry* end = map_end();
  DCHECK(map_ - 1 <= entry && entry < end);
  for (entry++; entry < end; entry++) {
    if (entry->exists()) {
      return entry;
    }
  }
  return nullptr;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Probe(
    const Key& key, uint32_t hash) const {
  DCHECK(base::bits::IsPowerOfTwo(capacity_));
  size_t i = hash & (capacity_ - 1);
  DCHECK(i < capacity_);

  DCHECK(occupancy_ < capacity_);  // Guarantees loop termination.
  while (map_[i].exists() && !match_(hash, map_[i].hash, key, map_[i].key)) {
    i = (i + 1) & (capacity_ - 1);
  }

  return &map_[i];
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::FillEmptyEntry(
    Entry* entry, const Key& key, const Value& value, uint32_t hash,
    AllocationPolicy allocator) {
  DCHECK(!entry->exists());

  new (entry) Entry(key, value, hash);
  occupancy_++;

  // Grow the map if we reached >= 80% occupancy.
  if (occupancy_ + occupancy_ / 4 >= capacity_) {
    Resize(allocator);
    entry = Probe(key, hash);
  }

  return entry;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
void TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Initialize(
    uint32_t capacity, AllocationPolicy allocator) {
  DCHECK(base::bits::IsPowerOfTwo(capacity));
  map_ = reinterpret_cast<Entry*>(allocator.New(capacity * sizeof(Entry)));
  if (map_ == nullptr) {
    FATAL("Out of memory: HashMap::Initialize");
    return;
  }
  capacity_ = capacity;
  Clear();
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
void TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Resize(
    AllocationPolicy allocator) {
  Entry* map = map_;
  uint32_t n = occupancy_;

  // Allocate larger map.
  Initialize(capacity_ * 2, allocator);

  // Rehash all current entries.
  for (Entry* entry = map; n > 0; entry++) {
    if (entry->exists()) {
      Entry* new_entry = Probe(entry->key, entry->hash);
      new_entry = FillEmptyEntry(new_entry, entry->key, entry->value,
                                 entry->hash, allocator);
      n--;
    }
  }

  // Delete old map.
  AllocationPolicy::Delete(map);
}

// Match function which compares hashes before executing a (potentially
// expensive) key comparison.
template <typename Key, typename MatchFun>
struct HashEqualityThenKeyMatcher {
  explicit HashEqualityThenKeyMatcher(MatchFun match) : match_(match) {}

  bool operator()(uint32_t hash1, uint32_t hash2, const Key& key1,
                  const Key& key2) const {
    return hash1 == hash2 && match_(key1, key2);
  }

 private:
  MatchFun match_;
};

// Hashmap<void*, void*> which takes a custom key comparison function pointer.
template <typename AllocationPolicy>
class CustomMatcherTemplateHashMapImpl
    : public TemplateHashMapImpl<
          void*, void*,
          HashEqualityThenKeyMatcher<void*, bool (*)(void*, void*)>,
          AllocationPolicy> {
  using Base = TemplateHashMapImpl<
      void*, void*, HashEqualityThenKeyMatcher<void*, bool (*)(void*, void*)>,
      AllocationPolicy>;

 public:
  using MatchFun = bool (*)(void*, void*);

  CustomMatcherTemplateHashMapImpl(
      MatchFun match, uint32_t capacity = Base::kDefaultHashMapCapacity,
      AllocationPolicy allocator = AllocationPolicy())
      : Base(capacity, HashEqualityThenKeyMatcher<void*, MatchFun>(match),
             allocator) {}

  CustomMatcherTemplateHashMapImpl(
      const CustomMatcherTemplateHashMapImpl<AllocationPolicy>* original,
      AllocationPolicy allocator = AllocationPolicy())
      : Base(original, allocator) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomMatcherTemplateHashMapImpl);
};

using CustomMatcherHashMap =
    CustomMatcherTemplateHashMapImpl<DefaultAllocationPolicy>;

// Match function which compares keys directly by equality.
template <typename Key>
struct KeyEqualityMatcher {
  bool operator()(uint32_t hash1, uint32_t hash2, const Key& key1,
                  const Key& key2) const {
    return key1 == key2;
  }
};

// Hashmap<void*, void*> which compares the key pointers directly.
template <typename AllocationPolicy>
class PointerTemplateHashMapImpl
    : public TemplateHashMapImpl<void*, void*, KeyEqualityMatcher<void*>,
                                 AllocationPolicy> {
  using Base = TemplateHashMapImpl<void*, void*, KeyEqualityMatcher<void*>,
                                   AllocationPolicy>;

 public:
  PointerTemplateHashMapImpl(uint32_t capacity = Base::kDefaultHashMapCapacity,
                             AllocationPolicy allocator = AllocationPolicy())
      : Base(capacity, KeyEqualityMatcher<void*>(), allocator) {}
};

using HashMap = PointerTemplateHashMapImpl<DefaultAllocationPolicy>;

// A hash map for pointer keys and values with an STL-like interface.
template <class Key, class Value, class MatchFun, class AllocationPolicy>
class TemplateHashMap
    : private TemplateHashMapImpl<void*, void*,
                                  HashEqualityThenKeyMatcher<void*, MatchFun>,
                                  AllocationPolicy> {
  using Base = TemplateHashMapImpl<void*, void*,
                                   HashEqualityThenKeyMatcher<void*, MatchFun>,
                                   AllocationPolicy>;

 public:
  STATIC_ASSERT(sizeof(Key*) == sizeof(void*));    // NOLINT
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
    bool operator!=(const Iterator& other) { return entry_ != other.entry_; }

   private:
    Iterator(const Base* map, typename Base::Entry* entry)
        : map_(map), entry_(entry) {}

    const Base* map_;
    typename Base::Entry* entry_;

    friend class TemplateHashMap;
  };

  TemplateHashMap(MatchFun match,
                  AllocationPolicy allocator = AllocationPolicy())
      : Base(Base::kDefaultHashMapCapacity,
             HashEqualityThenKeyMatcher<void*, MatchFun>(match), allocator) {}

  Iterator begin() const { return Iterator(this, this->Start()); }
  Iterator end() const { return Iterator(this, nullptr); }
  Iterator find(Key* key, bool insert = false,
                AllocationPolicy allocator = AllocationPolicy()) {
    if (insert) {
      return Iterator(this, this->LookupOrInsert(key, key->Hash(), allocator));
    }
    return Iterator(this, this->Lookup(key, key->Hash()));
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_HASHMAP_H_
