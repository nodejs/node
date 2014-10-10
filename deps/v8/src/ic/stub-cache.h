// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STUB_CACHE_H_
#define V8_STUB_CACHE_H_

#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


// The stub cache is used for megamorphic property accesses.
// It maps (map, name, type) to property access handlers. The cache does not
// need explicit invalidation when a prototype chain is modified, since the
// handlers verify the chain.


class SCTableReference {
 public:
  Address address() const { return address_; }

 private:
  explicit SCTableReference(Address address) : address_(address) {}

  Address address_;

  friend class StubCache;
};


class StubCache {
 public:
  struct Entry {
    Name* key;
    Code* value;
    Map* map;
  };

  void Initialize();
  // Access cache for entry hash(name, map).
  Code* Set(Name* name, Map* map, Code* code);
  Code* Get(Name* name, Map* map, Code::Flags flags);
  // Clear the lookup table (@ mark compact collection).
  void Clear();
  // Collect all maps that match the name and flags.
  void CollectMatchingMaps(SmallMapList* types, Handle<Name> name,
                           Code::Flags flags, Handle<Context> native_context,
                           Zone* zone);
  // Generate code for probing the stub cache table.
  // Arguments extra, extra2 and extra3 may be used to pass additional scratch
  // registers. Set to no_reg if not needed.
  // If leave_frame is true, then exit a frame before the tail call.
  void GenerateProbe(MacroAssembler* masm, Code::Flags flags, bool leave_frame,
                     Register receiver, Register name, Register scratch,
                     Register extra, Register extra2 = no_reg,
                     Register extra3 = no_reg);

  enum Table { kPrimary, kSecondary };

  SCTableReference key_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->key));
  }

  SCTableReference map_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->map));
  }

  SCTableReference value_reference(StubCache::Table table) {
    return SCTableReference(
        reinterpret_cast<Address>(&first_entry(table)->value));
  }

  StubCache::Entry* first_entry(StubCache::Table table) {
    switch (table) {
      case StubCache::kPrimary:
        return StubCache::primary_;
      case StubCache::kSecondary:
        return StubCache::secondary_;
    }
    UNREACHABLE();
    return NULL;
  }

  Isolate* isolate() { return isolate_; }

  // Setting the entry size such that the index is shifted by Name::kHashShift
  // is convenient; shifting down the length field (to extract the hash code)
  // automatically discards the hash bit field.
  static const int kCacheIndexShift = Name::kHashShift;

 private:
  explicit StubCache(Isolate* isolate);

  // The stub cache has a primary and secondary level.  The two levels have
  // different hashing algorithms in order to avoid simultaneous collisions
  // in both caches.  Unlike a probing strategy (quadratic or otherwise) the
  // update strategy on updates is fairly clear and simple:  Any existing entry
  // in the primary cache is moved to the secondary cache, and secondary cache
  // entries are overwritten.

  // Hash algorithm for the primary table.  This algorithm is replicated in
  // assembler for every architecture.  Returns an index into the table that
  // is scaled by 1 << kCacheIndexShift.
  static int PrimaryOffset(Name* name, Code::Flags flags, Map* map) {
    STATIC_ASSERT(kCacheIndexShift == Name::kHashShift);
    // Compute the hash of the name (use entire hash field).
    DCHECK(name->HasHashCode());
    uint32_t field = name->hash_field();
    // Using only the low bits in 64-bit mode is unlikely to increase the
    // risk of collision even if the heap is spread over an area larger than
    // 4Gb (and not at all if it isn't).
    uint32_t map_low32bits =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(map));
    // We always set the in_loop bit to zero when generating the lookup code
    // so do it here too so the hash codes match.
    uint32_t iflags =
        (static_cast<uint32_t>(flags) & ~Code::kFlagsNotUsedInLookup);
    // Base the offset on a simple combination of name, flags, and map.
    uint32_t key = (map_low32bits + field) ^ iflags;
    return key & ((kPrimaryTableSize - 1) << kCacheIndexShift);
  }

  // Hash algorithm for the secondary table.  This algorithm is replicated in
  // assembler for every architecture.  Returns an index into the table that
  // is scaled by 1 << kCacheIndexShift.
  static int SecondaryOffset(Name* name, Code::Flags flags, int seed) {
    // Use the seed from the primary cache in the secondary cache.
    uint32_t name_low32bits =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name));
    // We always set the in_loop bit to zero when generating the lookup code
    // so do it here too so the hash codes match.
    uint32_t iflags =
        (static_cast<uint32_t>(flags) & ~Code::kFlagsNotUsedInLookup);
    uint32_t key = (seed - name_low32bits) + iflags;
    return key & ((kSecondaryTableSize - 1) << kCacheIndexShift);
  }

  // Compute the entry for a given offset in exactly the same way as
  // we do in generated code.  We generate an hash code that already
  // ends in Name::kHashShift 0s.  Then we multiply it so it is a multiple
  // of sizeof(Entry).  This makes it easier to avoid making mistakes
  // in the hashed offset computations.
  static Entry* entry(Entry* table, int offset) {
    const int multiplier = sizeof(*table) >> Name::kHashShift;
    return reinterpret_cast<Entry*>(reinterpret_cast<Address>(table) +
                                    offset * multiplier);
  }

  static const int kPrimaryTableBits = 11;
  static const int kPrimaryTableSize = (1 << kPrimaryTableBits);
  static const int kSecondaryTableBits = 9;
  static const int kSecondaryTableSize = (1 << kSecondaryTableBits);

 private:
  Entry primary_[kPrimaryTableSize];
  Entry secondary_[kSecondaryTableSize];
  Isolate* isolate_;

  friend class Isolate;
  friend class SCTableReference;

  DISALLOW_COPY_AND_ASSIGN(StubCache);
};
}
}  // namespace v8::internal

#endif  // V8_STUB_CACHE_H_
