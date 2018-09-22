// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-cache.h"

#include <cstring>

#include "src/globals.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

enum { kInitialSize = 16u, kLinearProbe = 5u };

}  // namespace


template <typename Key, typename Hash, typename Pred>
struct NodeCache<Key, Hash, Pred>::Entry {
  Key key_;
  Node* value_;
};


template <typename Key, typename Hash, typename Pred>
bool NodeCache<Key, Hash, Pred>::Resize(Zone* zone) {
  if (size_ >= max_) return false;  // Don't grow past the maximum size.

  // Allocate a new block of entries 4x the size.
  Entry* old_entries = entries_;
  size_t old_size = size_ + kLinearProbe;
  size_ *= 4;
  size_t num_entries = size_ + kLinearProbe;
  entries_ = zone->NewArray<Entry>(num_entries);
  memset(static_cast<void*>(entries_), 0, sizeof(Entry) * num_entries);

  // Insert the old entries into the new block.
  for (size_t i = 0; i < old_size; ++i) {
    Entry* old = &old_entries[i];
    if (old->value_) {
      size_t hash = hash_(old->key_);
      size_t start = hash & (size_ - 1);
      size_t end = start + kLinearProbe;
      for (size_t j = start; j < end; ++j) {
        Entry* entry = &entries_[j];
        if (!entry->value_) {
          entry->key_ = old->key_;
          entry->value_ = old->value_;
          break;
        }
      }
    }
  }
  return true;
}


template <typename Key, typename Hash, typename Pred>
Node** NodeCache<Key, Hash, Pred>::Find(Zone* zone, Key key) {
  size_t hash = hash_(key);
  if (!entries_) {
    // Allocate the initial entries and insert the first entry.
    size_t num_entries = kInitialSize + kLinearProbe;
    entries_ = zone->NewArray<Entry>(num_entries);
    size_ = kInitialSize;
    memset(static_cast<void*>(entries_), 0, sizeof(Entry) * num_entries);
    Entry* entry = &entries_[hash & (kInitialSize - 1)];
    entry->key_ = key;
    return &entry->value_;
  }

  for (;;) {
    // Search up to N entries after (linear probing).
    size_t start = hash & (size_ - 1);
    size_t end = start + kLinearProbe;
    for (size_t i = start; i < end; i++) {
      Entry* entry = &entries_[i];
      if (pred_(entry->key_, key)) return &entry->value_;
      if (!entry->value_) {
        entry->key_ = key;
        return &entry->value_;
      }
    }

    if (!Resize(zone)) break;  // Don't grow past the maximum size.
  }

  // If resized to maximum and still didn't find space, overwrite an entry.
  Entry* entry = &entries_[hash & (size_ - 1)];
  entry->key_ = key;
  entry->value_ = nullptr;
  return &entry->value_;
}


template <typename Key, typename Hash, typename Pred>
void NodeCache<Key, Hash, Pred>::GetCachedNodes(ZoneVector<Node*>* nodes) {
  if (entries_) {
    for (size_t i = 0; i < size_ + kLinearProbe; i++) {
      if (entries_[i].value_) nodes->push_back(entries_[i].value_);
    }
  }
}


// -----------------------------------------------------------------------------
// Instantiations

template class V8_EXPORT_PRIVATE NodeCache<int32_t>;
template class V8_EXPORT_PRIVATE NodeCache<int64_t>;

template class V8_EXPORT_PRIVATE NodeCache<RelocInt32Key>;
template class V8_EXPORT_PRIVATE NodeCache<RelocInt64Key>;

}  // namespace compiler
}  // namespace internal
}  // namespace v8
