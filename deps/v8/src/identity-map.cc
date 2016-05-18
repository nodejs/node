// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/identity-map.h"

#include "src/base/functional.h"
#include "src/heap/heap-inl.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

static const int kInitialIdentityMapSize = 4;
static const int kResizeFactor = 4;

IdentityMapBase::~IdentityMapBase() { Clear(); }

void IdentityMapBase::Clear() {
  if (keys_) {
    heap_->UnregisterStrongRoots(keys_);
    keys_ = nullptr;
    values_ = nullptr;
    size_ = 0;
    mask_ = 0;
  }
}

IdentityMapBase::RawEntry IdentityMapBase::Lookup(Object* key) {
  int index = LookupIndex(key);
  return index >= 0 ? &values_[index] : nullptr;
}


IdentityMapBase::RawEntry IdentityMapBase::Insert(Object* key) {
  int index = InsertIndex(key);
  DCHECK_GE(index, 0);
  return &values_[index];
}


int IdentityMapBase::Hash(Object* address) {
  CHECK_NE(address, heap_->not_mapped_symbol());
  uintptr_t raw_address = reinterpret_cast<uintptr_t>(address);
  return static_cast<int>(hasher_(raw_address));
}


int IdentityMapBase::LookupIndex(Object* address) {
  int start = Hash(address) & mask_;
  Object* not_mapped = heap_->not_mapped_symbol();
  for (int index = start; index < size_; index++) {
    if (keys_[index] == address) return index;  // Found.
    if (keys_[index] == not_mapped) return -1;  // Not found.
  }
  for (int index = 0; index < start; index++) {
    if (keys_[index] == address) return index;  // Found.
    if (keys_[index] == not_mapped) return -1;  // Not found.
  }
  return -1;
}


int IdentityMapBase::InsertIndex(Object* address) {
  Object* not_mapped = heap_->not_mapped_symbol();
  while (true) {
    int start = Hash(address) & mask_;
    int limit = size_ / 2;
    // Search up to {limit} entries.
    for (int index = start; --limit > 0; index = (index + 1) & mask_) {
      if (keys_[index] == address) return index;  // Found.
      if (keys_[index] == not_mapped) {           // Free entry.
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
IdentityMapBase::RawEntry IdentityMapBase::GetEntry(Object* key) {
  RawEntry result;
  if (size_ == 0) {
    // Allocate the initial storage for keys and values.
    size_ = kInitialIdentityMapSize;
    mask_ = kInitialIdentityMapSize - 1;
    gc_counter_ = heap_->gc_count();

    keys_ = zone_->NewArray<Object*>(size_);
    Object* not_mapped = heap_->not_mapped_symbol();
    for (int i = 0; i < size_; i++) keys_[i] = not_mapped;
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
IdentityMapBase::RawEntry IdentityMapBase::FindEntry(Object* key) {
  if (size_ == 0) return nullptr;

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
  Object* not_mapped = heap_->not_mapped_symbol();
  for (int i = 0; i < size_; i++) {
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
      }
    }
  }
  // Reinsert all the key/value pairs that were in the wrong place.
  for (auto pair : reinsert) {
    int index = InsertIndex(pair.first);
    DCHECK_GE(index, 0);
    DCHECK_NE(heap_->not_mapped_symbol(), values_[index]);
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
  Object* not_mapped = heap_->not_mapped_symbol();
  for (int i = 0; i < size_; i++) keys_[i] = not_mapped;
  values_ = zone_->NewArray<void*>(size_);
  memset(values_, 0, sizeof(void*) * size_);

  for (int i = 0; i < old_size; i++) {
    if (old_keys[i] == not_mapped) continue;
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
