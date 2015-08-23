// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/heap/heap.h"
#include "src/heap/identity-map.h"

namespace v8 {
namespace internal {

static const int kInitialIdentityMapSize = 4;
static const int kResizeFactor = 4;

IdentityMapBase::~IdentityMapBase() {
  if (keys_) {
    Heap::OptionalRelocationLock lock(heap_, concurrent_);
    heap_->UnregisterStrongRoots(keys_);
  }
}


IdentityMapBase::RawEntry IdentityMapBase::Lookup(Handle<Object> key) {
  AllowHandleDereference for_lookup;
  int index = LookupIndex(*key);
  return index >= 0 ? &values_[index] : nullptr;
}


IdentityMapBase::RawEntry IdentityMapBase::Insert(Handle<Object> key) {
  AllowHandleDereference for_lookup;
  int index = InsertIndex(*key);
  DCHECK_GE(index, 0);
  return &values_[index];
}


int IdentityMapBase::Hash(Object* address) {
  uintptr_t raw_address = reinterpret_cast<uintptr_t>(address);
  CHECK_NE(0U, raw_address);  // Cannot store Smi 0 as a key here, sorry.
  // Xor some of the upper bits, since the lower 2 or 3 are usually aligned.
  return static_cast<int>((raw_address >> 11) ^ raw_address);
}


int IdentityMapBase::LookupIndex(Object* address) {
  int start = Hash(address) & mask_;
  for (int index = start; index < size_; index++) {
    if (keys_[index] == address) return index;  // Found.
    if (keys_[index] == nullptr) return -1;     // Not found.
  }
  for (int index = 0; index < start; index++) {
    if (keys_[index] == address) return index;  // Found.
    if (keys_[index] == nullptr) return -1;     // Not found.
  }
  return -1;
}


int IdentityMapBase::InsertIndex(Object* address) {
  while (true) {
    int start = Hash(address) & mask_;
    int limit = size_ / 2;
    // Search up to {limit} entries.
    for (int index = start; --limit > 0; index = (index + 1) & mask_) {
      if (keys_[index] == address) return index;  // Found.
      if (keys_[index] == nullptr) {              // Free entry.
        keys_[index] = address;
        return index;
      }
    }
    Resize();  // Should only have to resize once, since we grow 4x.
  }
  UNREACHABLE();
  return -1;
}


// Searches this map for the given key using the object's address
// as the identity, returning:
//    found => a pointer to the storage location for the value
//    not found => a pointer to a new storage location for the value
IdentityMapBase::RawEntry IdentityMapBase::GetEntry(Handle<Object> key) {
  Heap::OptionalRelocationLock lock(heap_, concurrent_);
  RawEntry result;
  if (size_ == 0) {
    // Allocate the initial storage for keys and values.
    size_ = kInitialIdentityMapSize;
    mask_ = kInitialIdentityMapSize - 1;
    gc_counter_ = heap_->gc_count();

    keys_ = zone_->NewArray<Object*>(size_);
    memset(keys_, 0, sizeof(Object*) * size_);
    values_ = zone_->NewArray<void*>(size_);
    memset(values_, 0, sizeof(void*) * size_);

    heap_->RegisterStrongRoots(keys_, keys_ + size_);
    result = Insert(key);
  } else {
    // Perform an optimistic lookup.
    result = Lookup(key);
    if (result == nullptr) {
      // Miss; rehash if there was a GC, then insert.
      if (gc_counter_ != heap_->gc_count()) Rehash();
      result = Insert(key);
    }
  }
  return result;
}


// Searches this map for the given key using the object's address
// as the identity, returning:
//    found => a pointer to the storage location for the value
//    not found => {nullptr}
IdentityMapBase::RawEntry IdentityMapBase::FindEntry(Handle<Object> key) {
  if (size_ == 0) return nullptr;

  Heap::OptionalRelocationLock lock(heap_, concurrent_);
  RawEntry result = Lookup(key);
  if (result == nullptr && gc_counter_ != heap_->gc_count()) {
    Rehash();  // Rehash is expensive, so only do it in case of a miss.
    result = Lookup(key);
  }
  return result;
}


void IdentityMapBase::Rehash() {
  // Record the current GC counter.
  gc_counter_ = heap_->gc_count();
  // Assume that most objects won't be moved.
  ZoneVector<std::pair<Object*, void*>> reinsert(zone_);
  // Search the table looking for keys that wouldn't be found with their
  // current hashcode and evacuate them.
  int last_empty = -1;
  for (int i = 0; i < size_; i++) {
    if (keys_[i] == nullptr) {
      last_empty = i;
    } else {
      int pos = Hash(keys_[i]) & mask_;
      if (pos <= last_empty || pos > i) {
        // Evacuate an entry that is in the wrong place.
        reinsert.push_back(std::pair<Object*, void*>(keys_[i], values_[i]));
        keys_[i] = nullptr;
        values_[i] = nullptr;
        last_empty = i;
      }
    }
  }
  // Reinsert all the key/value pairs that were in the wrong place.
  for (auto pair : reinsert) {
    int index = InsertIndex(pair.first);
    DCHECK_GE(index, 0);
    DCHECK_NULL(values_[index]);
    values_[index] = pair.second;
  }
}


void IdentityMapBase::Resize() {
  // Grow the internal storage and reinsert all the key/value pairs.
  int old_size = size_;
  Object** old_keys = keys_;
  void** old_values = values_;

  size_ = size_ * kResizeFactor;
  mask_ = size_ - 1;
  gc_counter_ = heap_->gc_count();

  CHECK_LE(size_, (1024 * 1024 * 16));  // that would be extreme...

  keys_ = zone_->NewArray<Object*>(size_);
  memset(keys_, 0, sizeof(Object*) * size_);
  values_ = zone_->NewArray<void*>(size_);
  memset(values_, 0, sizeof(void*) * size_);

  for (int i = 0; i < old_size; i++) {
    if (old_keys[i] == nullptr) continue;
    int index = InsertIndex(old_keys[i]);
    DCHECK_GE(index, 0);
    values_[index] = old_values[i];
  }

  // Unregister old keys and register new keys.
  heap_->UnregisterStrongRoots(old_keys);
  heap_->RegisterStrongRoots(keys_, keys_ + size_);
}
}  // namespace internal
}  // namespace v8
