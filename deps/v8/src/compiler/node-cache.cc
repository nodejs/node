// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

#define INITIAL_SIZE 16
#define LINEAR_PROBE 5

template <typename Key>
int32_t NodeCacheHash(Key key) {
  UNIMPLEMENTED();
  return 0;
}

template <>
inline int32_t NodeCacheHash(int32_t key) {
  return ComputeIntegerHash(key, 0);
}


template <>
inline int32_t NodeCacheHash(int64_t key) {
  return ComputeLongHash(key);
}


template <>
inline int32_t NodeCacheHash(double key) {
  return ComputeLongHash(bit_cast<int64_t>(key));
}


template <>
inline int32_t NodeCacheHash(void* key) {
  return ComputePointerHash(key);
}


template <typename Key>
bool NodeCache<Key>::Resize(Zone* zone) {
  if (size_ >= max_) return false;  // Don't grow past the maximum size.

  // Allocate a new block of entries 4x the size.
  Entry* old_entries = entries_;
  int old_size = size_ + LINEAR_PROBE;
  size_ = size_ * 4;
  int num_entries = size_ + LINEAR_PROBE;
  entries_ = zone->NewArray<Entry>(num_entries);
  memset(entries_, 0, sizeof(Entry) * num_entries);

  // Insert the old entries into the new block.
  for (int i = 0; i < old_size; i++) {
    Entry* old = &old_entries[i];
    if (old->value_ != NULL) {
      int hash = NodeCacheHash(old->key_);
      int start = hash & (size_ - 1);
      int end = start + LINEAR_PROBE;
      for (int j = start; j < end; j++) {
        Entry* entry = &entries_[j];
        if (entry->value_ == NULL) {
          entry->key_ = old->key_;
          entry->value_ = old->value_;
          break;
        }
      }
    }
  }
  return true;
}


template <typename Key>
Node** NodeCache<Key>::Find(Zone* zone, Key key) {
  int32_t hash = NodeCacheHash(key);
  if (entries_ == NULL) {
    // Allocate the initial entries and insert the first entry.
    int num_entries = INITIAL_SIZE + LINEAR_PROBE;
    entries_ = zone->NewArray<Entry>(num_entries);
    size_ = INITIAL_SIZE;
    memset(entries_, 0, sizeof(Entry) * num_entries);
    Entry* entry = &entries_[hash & (INITIAL_SIZE - 1)];
    entry->key_ = key;
    return &entry->value_;
  }

  while (true) {
    // Search up to N entries after (linear probing).
    int start = hash & (size_ - 1);
    int end = start + LINEAR_PROBE;
    for (int i = start; i < end; i++) {
      Entry* entry = &entries_[i];
      if (entry->key_ == key) return &entry->value_;
      if (entry->value_ == NULL) {
        entry->key_ = key;
        return &entry->value_;
      }
    }

    if (!Resize(zone)) break;  // Don't grow past the maximum size.
  }

  // If resized to maximum and still didn't find space, overwrite an entry.
  Entry* entry = &entries_[hash & (size_ - 1)];
  entry->key_ = key;
  entry->value_ = NULL;
  return &entry->value_;
}


template class NodeCache<int64_t>;
template class NodeCache<int32_t>;
template class NodeCache<void*>;
}
}
}  // namespace v8::internal::compiler
