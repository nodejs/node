// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_HASHMAP_H_
#define V8_BASE_HASHMAP_H_

// The reason we write our own hash map instead of using unordered_map in STL,
// is that STL containers use a mutex pool on debug build, which will lead to
// deadlock when we are using async signal handler.

#include <stdlib.h>

#include "src/base/bits.h"
#include "src/base/hashmap-entry.h"
#include "src/base/logging.h"
#include "src/base/platform/memory.h"

namespace v8 {
namespace base {

class DefaultAllocationPolicy {
 public:
  template <typename T, typename TypeTag = T[]>
  V8_INLINE T* AllocateArray(size_t length) {
    return static_cast<T*>(base::Malloc(length * sizeof(T)));
  }
  template <typename T, typename TypeTag = T[]>
  V8_INLINE void DeleteArray(T* p, size_t length) {
    base::Free(p);
  }
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
  explicit TemplateHashMapImpl(uint32_t capacity = kDefaultHashMapCapacity,
                               MatchFun match = MatchFun(),
                               AllocationPolicy allocator = AllocationPolicy());

  TemplateHashMapImpl(const TemplateHashMapImpl&) = delete;
  TemplateHashMapImpl& operator=(const TemplateHashMapImpl&) = delete;

  // Clones the given hashmap and creates a copy with the same entries.
  explicit TemplateHashMapImpl(const TemplateHashMapImpl* original,
                               AllocationPolicy allocator = AllocationPolicy());

  TemplateHashMapImpl(TemplateHashMapImpl&& other) V8_NOEXCEPT = default;

  ~TemplateHashMapImpl();

  TemplateHashMapImpl& operator=(TemplateHashMapImpl&& other)
      V8_NOEXCEPT = default;

  // If an entry with matching key is found, returns that entry.
  // Otherwise, nullptr is returned.
  Entry* Lookup(const Key& key, uint32_t hash) const;

  // If an entry with matching key is found, returns that entry.
  // If no matching entry is found, a new entry is inserted with
  // corresponding key, key hash, and default initialized value.
  Entry* LookupOrInsert(const Key& key, uint32_t hash);

  // If an entry with matching key is found, returns that entry.
  // If no matching entry is found, a new entry is inserted with
  // corresponding key, key hash, and value created by func.
  template <typename Func>
  Entry* LookupOrInsert(const Key& key, uint32_t hash, const Func& value_func);

  // Heterogeneous version of LookupOrInsert, which allows a
  // different lookup key type than the hashmap's key type.
  // The requirement is that MatchFun has an overload:
  //
  //   operator()(const LookupKey& lookup_key, const Key& entry_key)
  //
  // If an entry with matching key is found, returns that entry.
  // If no matching entry is found, a new entry is inserted with
  // a key created by key_func, key hash, and value created by
  // value_func.
  template <typename LookupKey, typename KeyFunc, typename ValueFunc>
  Entry* LookupOrInsert(const LookupKey& lookup_key, uint32_t hash,
                        const KeyFunc& key_func, const ValueFunc& value_func);

  Entry* InsertNew(const Key& key, uint32_t hash);

  // Removes the entry with matching key.
  // It returns the value of the deleted entry
  // or null if there is no value for such key.
  Value Remove(const Key& key, uint32_t hash);

  // Empties the hash map (occupancy() == 0).
  void Clear();

  // Empties the map and makes it unusable for allocation.
  void Invalidate() {
    DCHECK_NOT_NULL(impl_.map_);
    impl_.allocator().DeleteArray(impl_.map_, capacity());
    impl_ = Impl(impl_.match(), AllocationPolicy());
  }

  // The number of (non-empty) entries in the table.
  uint32_t occupancy() const { return impl_.occupancy_; }

  // The capacity of the table. The implementation
  // makes sure that occupancy is at most 80% of
  // the table capacity.
  uint32_t capacity() const { return impl_.capacity_; }

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

  AllocationPolicy allocator() const { return impl_.allocator(); }

 protected:
  void Initialize(uint32_t capacity);

 private:
  Entry* map_end() const { return impl_.map_ + impl_.capacity_; }
  template <typename LookupKey>
  Entry* Probe(const LookupKey& key, uint32_t hash) const;
  Entry* FillEmptyEntry(Entry* entry, const Key& key, const Value& value,
                        uint32_t hash);
  void Resize();

  // To support matcher and allocator that may not be possible to
  // default-construct, we have to store their instances. Using this to store
  // all internal state of the hash map and using private inheritance to store
  // matcher and allocator lets us take advantage of an empty base class
  // optimization to avoid extra space in the common case when MatchFun and
  // AllocationPolicy have no state.
  // TODO(ishell): Once we reach C++20, consider removing the Impl struct and
  // adding match and allocator as [[no_unique_address]] fields.
  struct Impl : private MatchFun, private AllocationPolicy {
    Impl(MatchFun match, AllocationPolicy allocator)
        : MatchFun(std::move(match)), AllocationPolicy(std::move(allocator)) {}

    Impl() = default;
    Impl(const Impl&) V8_NOEXCEPT = default;
    Impl(Impl&& other) V8_NOEXCEPT { *this = std::move(other); }

    Impl& operator=(const Impl& other) V8_NOEXCEPT = default;
    Impl& operator=(Impl&& other) V8_NOEXCEPT {
      MatchFun::operator=(std::move(other));
      AllocationPolicy::operator=(std::move(other));
      map_ = other.map_;
      capacity_ = other.capacity_;
      occupancy_ = other.occupancy_;

      other.map_ = nullptr;
      other.capacity_ = 0;
      other.occupancy_ = 0;
      return *this;
    }

    const MatchFun& match() const { return *this; }
    MatchFun& match() { return *this; }

    const AllocationPolicy& allocator() const { return *this; }
    AllocationPolicy& allocator() { return *this; }

    Entry* map_ = nullptr;
    uint32_t capacity_ = 0;
    uint32_t occupancy_ = 0;
  } impl_;
};
template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::
    TemplateHashMapImpl(uint32_t initial_capacity, MatchFun match,
                        AllocationPolicy allocator)
    : impl_(std::move(match), std::move(allocator)) {
  Initialize(initial_capacity);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::
    TemplateHashMapImpl(const TemplateHashMapImpl* original,
                        AllocationPolicy allocator)
    : impl_(original->impl_.match(), std::move(allocator)) {
  impl_.capacity_ = original->capacity();
  impl_.occupancy_ = original->occupancy();
  impl_.map_ = impl_.allocator().template AllocateArray<Entry>(capacity());
  memcpy(impl_.map_, original->impl_.map_, capacity() * sizeof(Entry));
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
TemplateHashMapImpl<Key, Value, MatchFun,
                    AllocationPolicy>::~TemplateHashMapImpl() {
  if (impl_.map_) impl_.allocator().DeleteArray(impl_.map_, capacity());
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
    const Key& key, uint32_t hash) {
  return LookupOrInsert(key, hash, []() { return Value(); });
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
template <typename Func>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::LookupOrInsert(
    const Key& key, uint32_t hash, const Func& value_func) {
  return LookupOrInsert(
      key, hash, [&key]() { return key; }, value_func);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
template <typename LookupKey, typename KeyFunc, typename ValueFunc>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::LookupOrInsert(
    const LookupKey& lookup_key, uint32_t hash, const KeyFunc& key_func,
    const ValueFunc& value_func) {
  // Find a matching entry.
  Entry* entry = Probe(lookup_key, hash);
  if (entry->exists()) {
    return entry;
  }

  return FillEmptyEntry(entry, key_func(), value_func(), hash);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::InsertNew(
    const Key& key, uint32_t hash) {
  Entry* entry = Probe(key, hash);
  return FillEmptyEntry(entry, key, Value(), hash);
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
  DCHECK(occupancy() < capacity());

  // p is the candidate entry to clear. q is used to scan forwards.
  Entry* q = p;  // Start at the entry to remove.
  while (true) {
    // Move q to the next entry.
    q = q + 1;
    if (q == map_end()) {
      q = impl_.map_;
    }

    // All entries between p and q have their initial position between p and q
    // and the entry p can be cleared without breaking the search for these
    // entries.
    if (!q->exists()) {
      break;
    }

    // Find the initial position for the entry at position q.
    Entry* r = impl_.map_ + (q->hash & (capacity() - 1));

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
  impl_.occupancy_--;
  return value;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
void TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Clear() {
  // Mark all entries as empty.
  for (size_t i = 0; i < capacity(); ++i) {
    impl_.map_[i].clear();
  }
  impl_.occupancy_ = 0;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Start() const {
  return Next(impl_.map_ - 1);
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Next(
    Entry* entry) const {
  const Entry* end = map_end();
  DCHECK(impl_.map_ - 1 <= entry && entry < end);
  for (entry++; entry < end; entry++) {
    if (entry->exists()) {
      return entry;
    }
  }
  return nullptr;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
template <typename LookupKey>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Probe(
    const LookupKey& key, uint32_t hash) const {
  DCHECK(base::bits::IsPowerOfTwo(capacity()));
  size_t i = hash & (capacity() - 1);
  DCHECK(i < capacity());

  DCHECK(occupancy() < capacity());  // Guarantees loop termination.
  Entry* map = impl_.map_;
  while (map[i].exists() &&
         !impl_.match()(hash, map[i].hash, key, map[i].key)) {
    i = (i + 1) & (capacity() - 1);
  }

  return &map[i];
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
typename TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Entry*
TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::FillEmptyEntry(
    Entry* entry, const Key& key, const Value& value, uint32_t hash) {
  DCHECK(!entry->exists());

  new (entry) Entry(key, value, hash);
  impl_.occupancy_++;

  // Grow the map if we reached >= 80% occupancy.
  if (occupancy() + occupancy() / 4 >= capacity()) {
    Resize();
    entry = Probe(key, hash);
  }

  return entry;
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
void TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Initialize(
    uint32_t capacity) {
  DCHECK(base::bits::IsPowerOfTwo(capacity));
  impl_.map_ = impl_.allocator().template AllocateArray<Entry>(capacity);
  if (impl_.map_ == nullptr) {
    FATAL("Out of memory: HashMap::Initialize");
    return;
  }
  impl_.capacity_ = capacity;
  Clear();
}

template <typename Key, typename Value, typename MatchFun,
          class AllocationPolicy>
void TemplateHashMapImpl<Key, Value, MatchFun, AllocationPolicy>::Resize() {
  Entry* old_map = impl_.map_;
  uint32_t old_capacity = capacity();
  uint32_t n = occupancy();

  // Allocate larger map.
  Initialize(capacity() * 2);

  // Rehash all current entries.
  for (Entry* entry = old_map; n > 0; entry++) {
    if (entry->exists()) {
      Entry* new_entry = Probe(entry->key, entry->hash);
      new_entry =
          FillEmptyEntry(new_entry, entry->key, entry->value, entry->hash);
      n--;
    }
  }

  // Delete old map.
  impl_.allocator().DeleteArray(old_map, old_capacity);
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

  explicit CustomMatcherTemplateHashMapImpl(
      MatchFun match, uint32_t capacity = Base::kDefaultHashMapCapacity,
      AllocationPolicy allocator = AllocationPolicy())
      : Base(capacity, HashEqualityThenKeyMatcher<void*, MatchFun>(match),
             allocator) {}

  explicit CustomMatcherTemplateHashMapImpl(
      const CustomMatcherTemplateHashMapImpl* original,
      AllocationPolicy allocator = AllocationPolicy())
      : Base(original, allocator) {}

  CustomMatcherTemplateHashMapImpl(const CustomMatcherTemplateHashMapImpl&) =
      delete;
  CustomMatcherTemplateHashMapImpl& operator=(
      const CustomMatcherTemplateHashMapImpl&) = delete;
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
  explicit PointerTemplateHashMapImpl(
      uint32_t capacity = Base::kDefaultHashMapCapacity,
      AllocationPolicy allocator = AllocationPolicy())
      : Base(capacity, KeyEqualityMatcher<void*>(), allocator) {}

  PointerTemplateHashMapImpl(const PointerTemplateHashMapImpl& other,
                             AllocationPolicy allocator = AllocationPolicy())
      : Base(&other, allocator) {}

  PointerTemplateHashMapImpl(PointerTemplateHashMapImpl&& other) V8_NOEXCEPT
      : Base(std::move(other)) {}

  PointerTemplateHashMapImpl& operator=(PointerTemplateHashMapImpl&& other)
      V8_NOEXCEPT {
    static_cast<Base&>(*this) = std::move(other);
    return *this;
  }
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
  static_assert(sizeof(Key*) == sizeof(void*));
  static_assert(sizeof(Value*) == sizeof(void*));
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

  explicit TemplateHashMap(MatchFun match,
                           AllocationPolicy allocator = AllocationPolicy())
      : Base(Base::kDefaultHashMapCapacity,
             HashEqualityThenKeyMatcher<void*, MatchFun>(match), allocator) {}

  Iterator begin() const { return Iterator(this, this->Start()); }
  Iterator end() const { return Iterator(this, nullptr); }
  Iterator find(Key* key, bool insert = false) {
    if (insert) {
      return Iterator(this, this->LookupOrInsert(key, key->Hash()));
    }
    return Iterator(this, this->Lookup(key, key->Hash()));
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_HASHMAP_H_
