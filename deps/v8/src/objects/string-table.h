// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_TABLE_H_
#define V8_OBJECTS_STRING_TABLE_H_

#include "src/common/assert-scope.h"
#include "src/objects/string.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// A generic key for lookups into the string table, which allows heteromorphic
// lookup and on-demand creation of new strings.
class StringTableKey {
 public:
  virtual ~StringTableKey() = default;
  inline StringTableKey(uint32_t raw_hash_field, int length);

  uint32_t raw_hash_field() const {
    DCHECK_NE(0, raw_hash_field_);
    return raw_hash_field_;
  }

  inline uint32_t hash() const;
  int length() const { return length_; }

 protected:
  inline void set_raw_hash_field(uint32_t raw_hash_field);

 private:
  uint32_t raw_hash_field_ = 0;
  int length_;
};

class SeqOneByteString;

// StringTable, for internalizing strings. The Lookup methods are designed to be
// thread-safe, in combination with GC safepoints.
//
// The string table layout is defined by its Data implementation class, see
// StringTable::Data for details.
class V8_EXPORT_PRIVATE StringTable {
 public:
  static constexpr Smi empty_element() { return Smi::FromInt(0); }
  static constexpr Smi deleted_element() { return Smi::FromInt(1); }

  explicit StringTable(Isolate* isolate);
  ~StringTable();

  int Capacity() const;
  int NumberOfElements() const;

  // Find string in the string table. If it is not there yet, it is
  // added. The return value is the string found.
  Handle<String> LookupString(Isolate* isolate, Handle<String> key);

  // Find string in the string table, using the given key. If the string is not
  // there yet, it is created (by the key) and added. The return value is the
  // string found.
  template <typename StringTableKey, typename IsolateT>
  Handle<String> LookupKey(IsolateT* isolate, StringTableKey* key);

  // {raw_string} must be a tagged String pointer.
  // Returns a tagged pointer: either a Smi if the string is an array index, an
  // internalized string, or a Smi sentinel.
  static Address TryStringToIndexOrLookupExisting(Isolate* isolate,
                                                  Address raw_string);

  void Print(PtrComprCageBase cage_base) const;
  size_t GetCurrentMemoryUsage() const;

  // The following methods must be called either while holding the write lock,
  // or while in a Heap safepoint.
  void IterateElements(RootVisitor* visitor);
  void DropOldData();
  void NotifyElementsRemoved(int count);

  void VerifyIfOwnedBy(Isolate* isolate);

 private:
  class Data;

  Data* EnsureCapacity(PtrComprCageBase cage_base, int additional_elements);

  std::atomic<Data*> data_;
  // Write mutex is mutable so that readers of concurrently mutated values (e.g.
  // NumberOfElements) are allowed to lock it while staying const.
  mutable base::Mutex write_mutex_;
  Isolate* isolate_;
};

// Mapping from forwarding indices (stored in a string's hash field) to
// internalized strings.
// The table is organised in "blocks". As writes only append new entries, the
// organisation in blocks allows lock-free writes. We need a lock only for
// growing the table (adding more blocks). When the vector holding the blocks
// needs to grow, we keep a copy of the old vector alive to allow concurrent
// reads while the vector is relocated.
class StringForwardingTable {
 public:
  // Capacity for the first block.
  static constexpr int kInitialBlockSize = 16;
  static_assert(base::bits::IsPowerOfTwo(kInitialBlockSize));
  static constexpr int kInitialBlockSizeHighestBit =
      kBitsPerInt - base::bits::CountLeadingZeros32(kInitialBlockSize) - 1;
  // Initial capacity in the block vector.
  static constexpr int kInitialBlockVectorCapacity = 4;
  static constexpr Smi deleted_element() { return Smi::FromInt(0); }

  explicit StringForwardingTable(Isolate* isolate);
  ~StringForwardingTable();

  inline int Size() const;
  // Returns the index of the added string pair.
  int Add(Isolate* isolate, String string, String forward_to);
  String GetForwardString(Isolate* isolate, int index) const;
  static Address GetForwardStringAddress(Isolate* isolate, int index);
  void IterateElements(RootVisitor* visitor);
  void Reset();
  void UpdateAfterEvacuation();

 private:
  class Block;
  class BlockVector;

  // Returns the block for a given index and sets the index within this block
  // as out parameter.
  static inline uint32_t BlockForIndex(int index, uint32_t* index_in_block_out);
  static inline uint32_t IndexInBlock(int index, uint32_t block);
  static inline uint32_t CapacityForBlock(uint32_t block);

  void InitializeBlockVector();
  // Ensure that |block| exists in the BlockVector already. If not, a new block
  // is created (with capacity double the capacity of the last block) and
  // inserted into the BlockVector. The BlockVector itself might grow (to double
  // the capacity).
  BlockVector* EnsureCapacity(uint32_t block);

  Isolate* isolate_;
  std::atomic<BlockVector*> blocks_;
  // We need a vector of BlockVectors to keep old BlockVectors alive when we
  // grow the table, due to concurrent reads that may still hold a pointer to
  // them. |block_vector_sotrage_| is only accessed while we grow with the mutex
  // held. All regular access go through |block_|, which holds a pointer to the
  // current BlockVector.
  std::vector<std::unique_ptr<BlockVector>> block_vector_storage_;
  std::atomic<int> next_free_index_;
  base::Mutex grow_mutex_;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_TABLE_H_
