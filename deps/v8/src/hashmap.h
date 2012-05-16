// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_HASHMAP_H_
#define V8_HASHMAP_H_

#include "allocation.h"
#include "checks.h"
#include "utils.h"

namespace v8 {
namespace internal {

template<class AllocationPolicy>
class TemplateHashMapImpl {
 public:
  typedef bool (*MatchFun) (void* key1, void* key2);

  // initial_capacity is the size of the initial hash map;
  // it must be a power of 2 (and thus must not be 0).
  TemplateHashMapImpl(MatchFun match, uint32_t initial_capacity = 8);

  ~TemplateHashMapImpl();

  // HashMap entries are (key, value, hash) triplets.
  // Some clients may not need to use the value slot
  // (e.g. implementers of sets, where the key is the value).
  struct Entry {
    void* key;
    void* value;
    uint32_t hash;  // the full hash value for key
  };

  // If an entry with matching key is found, Lookup()
  // returns that entry. If no matching entry is found,
  // but insert is set, a new entry is inserted with
  // corresponding key, key hash, and NULL value.
  // Otherwise, NULL is returned.
  Entry* Lookup(void* key, uint32_t hash, bool insert);

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

 private:
  MatchFun match_;
  Entry* map_;
  uint32_t capacity_;
  uint32_t occupancy_;

  Entry* map_end() const { return map_ + capacity_; }
  Entry* Probe(void* key, uint32_t hash);
  void Initialize(uint32_t capacity);
  void Resize();
};

typedef TemplateHashMapImpl<FreeStoreAllocationPolicy> HashMap;

template<class P>
TemplateHashMapImpl<P>::TemplateHashMapImpl(MatchFun match,
                    uint32_t initial_capacity) {
  match_ = match;
  Initialize(initial_capacity);
}


template<class P>
TemplateHashMapImpl<P>::~TemplateHashMapImpl() {
  P::Delete(map_);
}


template<class P>
typename TemplateHashMapImpl<P>::Entry* TemplateHashMapImpl<P>::Lookup(
    void* key, uint32_t hash, bool insert) {
  // Find a matching entry.
  Entry* p = Probe(key, hash);
  if (p->key != NULL) {
    return p;
  }

  // No entry found; insert one if necessary.
  if (insert) {
    p->key = key;
    p->value = NULL;
    p->hash = hash;
    occupancy_++;

    // Grow the map if we reached >= 80% occupancy.
    if (occupancy_ + occupancy_/4 >= capacity_) {
      Resize();
      p = Probe(key, hash);
    }

    return p;
  }

  // No entry found and none inserted.
  return NULL;
}


template<class P>
void* TemplateHashMapImpl<P>::Remove(void* key, uint32_t hash) {
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
  ASSERT(occupancy_ < capacity_);

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


template<class P>
void TemplateHashMapImpl<P>::Clear() {
  // Mark all entries as empty.
  const Entry* end = map_end();
  for (Entry* p = map_; p < end; p++) {
    p->key = NULL;
  }
  occupancy_ = 0;
}


template<class P>
typename TemplateHashMapImpl<P>::Entry* TemplateHashMapImpl<P>::Start() const {
  return Next(map_ - 1);
}


template<class P>
typename TemplateHashMapImpl<P>::Entry* TemplateHashMapImpl<P>::Next(Entry* p)
    const {
  const Entry* end = map_end();
  ASSERT(map_ - 1 <= p && p < end);
  for (p++; p < end; p++) {
    if (p->key != NULL) {
      return p;
    }
  }
  return NULL;
}


template<class P>
typename TemplateHashMapImpl<P>::Entry* TemplateHashMapImpl<P>::Probe(void* key,
                                                            uint32_t hash) {
  ASSERT(key != NULL);

  ASSERT(IsPowerOf2(capacity_));
  Entry* p = map_ + (hash & (capacity_ - 1));
  const Entry* end = map_end();
  ASSERT(map_ <= p && p < end);

  ASSERT(occupancy_ < capacity_);  // Guarantees loop termination.
  while (p->key != NULL && (hash != p->hash || !match_(key, p->key))) {
    p++;
    if (p >= end) {
      p = map_;
    }
  }

  return p;
}


template<class P>
void TemplateHashMapImpl<P>::Initialize(uint32_t capacity) {
  ASSERT(IsPowerOf2(capacity));
  map_ = reinterpret_cast<Entry*>(P::New(capacity * sizeof(Entry)));
  if (map_ == NULL) {
    v8::internal::FatalProcessOutOfMemory("HashMap::Initialize");
    return;
  }
  capacity_ = capacity;
  Clear();
}


template<class P>
void TemplateHashMapImpl<P>::Resize() {
  Entry* map = map_;
  uint32_t n = occupancy_;

  // Allocate larger map.
  Initialize(capacity_ * 2);

  // Rehash all current entries.
  for (Entry* p = map; n > 0; p++) {
    if (p->key != NULL) {
      Lookup(p->key, p->hash, true)->value = p->value;
      n--;
    }
  }

  // Delete old map.
  P::Delete(map);
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
      typename TemplateHashMapImpl<AllocationPolicy>::MatchFun match)
    : TemplateHashMapImpl<AllocationPolicy>(match) { }

  Iterator begin() const { return Iterator(this, this->Start()); }
  Iterator end() const { return Iterator(this, NULL); }
  Iterator find(Key* key, bool insert = false) {
    return Iterator(this, this->Lookup(key, key->Hash(), insert));
  }
};

} }  // namespace v8::internal

#endif  // V8_HASHMAP_H_
