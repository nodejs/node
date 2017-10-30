// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/identity-map.h"

#include "src/base/functional.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

static const int kInitialIdentityMapSize = 4;
static const int kResizeFactor = 4;

IdentityMapBase::~IdentityMapBase() {
  // Clear must be called by the subclass to avoid calling the virtual
  // DeleteArray function from the destructor.
  DCHECK_NULL(keys_);
}

void IdentityMapBase::Clear() {
  if (keys_) {
    DCHECK(!is_iterable());
    heap_->UnregisterStrongRoots(keys_);
    DeleteArray(keys_);
    DeleteArray(values_);
    keys_ = nullptr;
    values_ = nullptr;
    size_ = 0;
    capacity_ = 0;
    mask_ = 0;
  }
}

void IdentityMapBase::EnableIteration() {
  CHECK(!is_iterable());
  is_iterable_ = true;
}

void IdentityMapBase::DisableIteration() {
  CHECK(is_iterable());
  is_iterable_ = false;
}

int IdentityMapBase::ScanKeysFor(Object* address) const {
  int start = Hash(address) & mask_;
  Object* not_mapped = heap_->not_mapped_symbol();
  for (int index = start; index < capacity_; index++) {
    if (keys_[index] == address) return index;  // Found.
    if (keys_[index] == not_mapped) return -1;  // Not found.
  }
  for (int index = 0; index < start; index++) {
    if (keys_[index] == address) return index;  // Found.
    if (keys_[index] == not_mapped) return -1;  // Not found.
  }
  return -1;
}

int IdentityMapBase::InsertKey(Object* address) {
  Object* not_mapped = heap_->not_mapped_symbol();
  while (true) {
    int start = Hash(address) & mask_;
    int limit = capacity_ / 2;
    // Search up to {limit} entries.
    for (int index = start; --limit > 0; index = (index + 1) & mask_) {
      if (keys_[index] == address) return index;  // Found.
      if (keys_[index] == not_mapped) {           // Free entry.
        size_++;
        DCHECK_LE(size_, capacity_);
        keys_[index] = address;
        return index;
      }
    }
    // Should only have to resize once, since we grow 4x.
    Resize(capacity_ * kResizeFactor);
  }
  UNREACHABLE();
}

void* IdentityMapBase::DeleteIndex(int index) {
  void* ret_value = values_[index];
  Object* not_mapped = heap_->not_mapped_symbol();
  DCHECK_NE(keys_[index], not_mapped);
  keys_[index] = not_mapped;
  values_[index] = nullptr;
  size_--;
  DCHECK_GE(size_, 0);

  if (size_ * kResizeFactor < capacity_ / kResizeFactor) {
    Resize(capacity_ / kResizeFactor);
    return ret_value;  // No need to fix collisions as resize reinserts keys.
  }

  // Move any collisions to their new correct location.
  int next_index = index;
  for (;;) {
    next_index = (next_index + 1) & mask_;
    Object* key = keys_[next_index];
    if (key == not_mapped) break;

    int expected_index = Hash(key) & mask_;
    if (index < next_index) {
      if (index < expected_index && expected_index <= next_index) continue;
    } else {
      DCHECK_GT(index, next_index);
      if (index < expected_index || expected_index <= next_index) continue;
    }

    DCHECK_EQ(not_mapped, keys_[index]);
    DCHECK_NULL(values_[index]);
    std::swap(keys_[index], keys_[next_index]);
    std::swap(values_[index], values_[next_index]);
    index = next_index;
  }

  return ret_value;
}

int IdentityMapBase::Lookup(Object* key) const {
  int index = ScanKeysFor(key);
  if (index < 0 && gc_counter_ != heap_->gc_count()) {
    // Miss; rehash if there was a GC, then lookup again.
    const_cast<IdentityMapBase*>(this)->Rehash();
    index = ScanKeysFor(key);
  }
  return index;
}

int IdentityMapBase::LookupOrInsert(Object* key) {
  // Perform an optimistic lookup.
  int index = ScanKeysFor(key);
  if (index < 0) {
    // Miss; rehash if there was a GC, then insert.
    if (gc_counter_ != heap_->gc_count()) Rehash();
    index = InsertKey(key);
  }
  DCHECK_GE(index, 0);
  return index;
}

int IdentityMapBase::Hash(Object* address) const {
  CHECK_NE(address, heap_->not_mapped_symbol());
  uintptr_t raw_address = reinterpret_cast<uintptr_t>(address);
  return static_cast<int>(hasher_(raw_address));
}

// Searches this map for the given key using the object's address
// as the identity, returning:
//    found => a pointer to the storage location for the value
//    not found => a pointer to a new storage location for the value
IdentityMapBase::RawEntry IdentityMapBase::GetEntry(Object* key) {
  CHECK(!is_iterable());  // Don't allow insertion while iterable.
  if (capacity_ == 0) {
    // Allocate the initial storage for keys and values.
    capacity_ = kInitialIdentityMapSize;
    mask_ = kInitialIdentityMapSize - 1;
    gc_counter_ = heap_->gc_count();

    keys_ = reinterpret_cast<Object**>(NewPointerArray(capacity_));
    Object* not_mapped = heap_->not_mapped_symbol();
    for (int i = 0; i < capacity_; i++) keys_[i] = not_mapped;
    values_ = NewPointerArray(capacity_);
    memset(values_, 0, sizeof(void*) * capacity_);

    heap_->RegisterStrongRoots(keys_, keys_ + capacity_);
  }
  int index = LookupOrInsert(key);
  return &values_[index];
}

// Searches this map for the given key using the object's address
// as the identity, returning:
//    found => a pointer to the storage location for the value
//    not found => {nullptr}
IdentityMapBase::RawEntry IdentityMapBase::FindEntry(Object* key) const {
  // Don't allow find by key while iterable (might rehash).
  CHECK(!is_iterable());
  if (size_ == 0) return nullptr;
  // Remove constness since lookup might have to rehash.
  int index = Lookup(key);
  return index >= 0 ? &values_[index] : nullptr;
}

// Deletes the given key from the map using the object's address as the
// identity, returning:
//    found => the value
//    not found => {nullptr}
void* IdentityMapBase::DeleteEntry(Object* key) {
  CHECK(!is_iterable());  // Don't allow deletion by key while iterable.
  if (size_ == 0) return nullptr;
  int index = Lookup(key);
  if (index < 0) return nullptr;  // No entry found.
  return DeleteIndex(index);
}

IdentityMapBase::RawEntry IdentityMapBase::EntryAtIndex(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, capacity_);
  DCHECK_NE(keys_[index], heap_->not_mapped_symbol());
  CHECK(is_iterable());  // Must be iterable to access by index;
  return &values_[index];
}

int IdentityMapBase::NextIndex(int index) const {
  DCHECK_LE(-1, index);
  DCHECK_LE(index, capacity_);
  CHECK(is_iterable());  // Must be iterable to access by index;
  Object* not_mapped = heap_->not_mapped_symbol();
  for (++index; index < capacity_; ++index) {
    if (keys_[index] != not_mapped) {
      return index;
    }
  }
  return capacity_;
}

void IdentityMapBase::Rehash() {
  CHECK(!is_iterable());  // Can't rehash while iterating.
  // Record the current GC counter.
  gc_counter_ = heap_->gc_count();
  // Assume that most objects won't be moved.
  std::vector<std::pair<Object*, void*>> reinsert;
  // Search the table looking for keys that wouldn't be found with their
  // current hashcode and evacuate them.
  int last_empty = -1;
  Object* not_mapped = heap_->not_mapped_symbol();
  for (int i = 0; i < capacity_; i++) {
    if (keys_[i] == not_mapped) {
      last_empty = i;
    } else {
      int pos = Hash(keys_[i]) & mask_;
      if (pos <= last_empty || pos > i) {
        // Evacuate an entry that is in the wrong place.
        reinsert.push_back(std::pair<Object*, void*>(keys_[i], values_[i]));
        keys_[i] = not_mapped;
        values_[i] = nullptr;
        last_empty = i;
        size_--;
      }
    }
  }
  // Reinsert all the key/value pairs that were in the wrong place.
  for (auto pair : reinsert) {
    int index = InsertKey(pair.first);
    DCHECK_GE(index, 0);
    values_[index] = pair.second;
  }
}

void IdentityMapBase::Resize(int new_capacity) {
  CHECK(!is_iterable());  // Can't resize while iterating.
  // Resize the internal storage and reinsert all the key/value pairs.
  DCHECK_GT(new_capacity, size_);
  int old_capacity = capacity_;
  Object** old_keys = keys_;
  void** old_values = values_;

  capacity_ = new_capacity;
  mask_ = capacity_ - 1;
  gc_counter_ = heap_->gc_count();
  size_ = 0;

  keys_ = reinterpret_cast<Object**>(NewPointerArray(capacity_));
  Object* not_mapped = heap_->not_mapped_symbol();
  for (int i = 0; i < capacity_; i++) keys_[i] = not_mapped;
  values_ = NewPointerArray(capacity_);
  memset(values_, 0, sizeof(void*) * capacity_);

  for (int i = 0; i < old_capacity; i++) {
    if (old_keys[i] == not_mapped) continue;
    int index = InsertKey(old_keys[i]);
    DCHECK_GE(index, 0);
    values_[index] = old_values[i];
  }

  // Unregister old keys and register new keys.
  heap_->UnregisterStrongRoots(old_keys);
  heap_->RegisterStrongRoots(keys_, keys_ + capacity_);

  // Delete old storage;
  DeleteArray(old_keys);
  DeleteArray(old_values);
}

}  // namespace internal
}  // namespace v8
