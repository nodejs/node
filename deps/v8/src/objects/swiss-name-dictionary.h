// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SWISS_NAME_DICTIONARY_H_
#define V8_OBJECTS_SWISS_NAME_DICTIONARY_H_

#include <cstdint>

#include "src/base/export-template.h"
#include "src/common/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/internal-index.h"
#include "src/objects/js-objects.h"
#include "src/objects/swiss-hash-table-helpers.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// A property backing store based on Swiss Tables/Abseil's flat_hash_map. The
// implementation is heavily based on Abseil's raw_hash_set.h.
//
// Memory layout (see below for detailed description of parts):
//   Prefix:                      [table type dependent part, can have 0 size]
//   Capacity:                    4 bytes, raw int32_t
//   Meta table pointer:          kTaggedSize bytes
//   Data table:                  2 * |capacity| * |kTaggedSize| bytes
//   Ctrl table:                  |capacity| + |kGroupWidth| uint8_t entries
//   PropertyDetails table:       |capacity| uint_8 entries
//
// Note that because of |kInitialCapacity| == 4 there is no need for padding.
//
// Description of parts directly contained in SwissNameDictionary allocation:
//   Prefix:
//     In case of SwissNameDictionary:
//       identity hash: 4 bytes, raw int32_t
//   Meta table pointer: kTaggedSize bytes.
//     See below for explanation of the meta table.
//   Data table:
//     For each logical bucket of the hash table, contains the corresponding key
//     and value.
//   Ctrl table:
//     The control table is used to implement a Swiss Table: Each byte is either
//     Ctrl::kEmpty, Ctrl::kDeleted, or in case of a bucket denoting a present
//     entry in the hash table, the 7 lowest bits of the key's hash. The first
//     |capacity| entries are the actual control table. The additional
//     |kGroupWidth| bytes contain a copy of the first min(capacity,
//     kGroupWidth) bytes of the table.
//   PropertyDetails table:
//     Each byte contains the PropertyDetails for the corresponding bucket of
//     the ctrl table. Entries may contain unitialized data if the corresponding
//     bucket hasn't been used before.
//
// Meta table:
//   The meta table (not to be confused with the control table used in any
//   Swiss Table design!) is a separate ByteArray. Here, the "X" in "uintX_t"
//   depends on the capacity of the swiss table. For capacities <= 256 we have X
//   = 8, for 256 < |capacity| <= 2^16 we have X = 16, and otherwise X = 32 (see
//   MetaTableSizePerEntryFor). It contais the following data:
//     Number of Entries: uintX_t.
//     Number of Deleted Entries: uintX_t.
//     Enumeration table: max_load_factor * Capacity() entries of type uintX_t:
//       The i-th entry in the enumeration table
//       contains the number of the bucket representing the i-th entry of the
//       table in enumeration order. Entries may contain unitialized data if the
//       corresponding bucket  hasn't been used before.
class SwissNameDictionary : public HeapObject {
 public:
  using Group = swiss_table::Group;

  template <typename LocalIsolate>
  inline InternalIndex FindEntry(LocalIsolate* isolate, Object key);

  // This is to make the interfaces of NameDictionary::FindEntry and
  // OrderedNameDictionary::FindEntry compatible.
  // TODO(emrich) clean this up: NameDictionary uses Handle<Object>
  // for FindEntry keys due to its Key typedef, but that's also used
  // for adding, where we do need handles.
  template <typename LocalIsolate>
  inline InternalIndex FindEntry(LocalIsolate* isolate, Handle<Object> key);

  static inline bool IsKey(ReadOnlyRoots roots, Object key_candidate);
  inline bool ToKey(ReadOnlyRoots roots, InternalIndex entry, Object* out_key);

  inline Object KeyAt(InternalIndex entry);
  inline Name NameAt(InternalIndex entry);
  inline Object ValueAt(InternalIndex entry);
  inline PropertyDetails DetailsAt(InternalIndex entry);

  inline void ValueAtPut(InternalIndex entry, Object value);
  inline void DetailsAtPut(InternalIndex entry, PropertyDetails value);

  inline int NumberOfElements();
  inline int NumberOfDeletedElements();

  inline int Capacity();
  inline int UsedCapacity();

  template <typename LocalIsolate>
  void Initialize(LocalIsolate* isolate, ByteArray meta_table, int capacity);

  inline void SetHash(int hash);
  inline int Hash();

  class IndexIterator {
   public:
    inline IndexIterator(Handle<SwissNameDictionary> dict, int start);

    inline IndexIterator& operator++();

    inline bool operator==(const IndexIterator& b) const;
    inline bool operator!=(const IndexIterator& b) const;

    inline InternalIndex operator*();

   private:
    int used_capacity_;
    int enum_index_;

    // This may be an empty handle, but only if the capacity of the table is
    // 0 and pointer compression is disabled.
    Handle<SwissNameDictionary> dict_;
  };

  class IndexIterable {
   public:
    inline explicit IndexIterable(Handle<SwissNameDictionary> dict);

    inline IndexIterator begin();
    inline IndexIterator end();

   private:
    // This may be an empty handle, but only if the capacity of the table is
    // 0 and pointer compression is disabled.
    Handle<SwissNameDictionary> dict_;
  };

  inline IndexIterable IterateEntriesOrdered();
  inline IndexIterable IterateEntries();

  // For the given enumeration index, returns the entry (= bucket of the Swiss
  // Table) containing the data for the mapping with that enumeration index.
  // The returned bucket may be deleted.
  inline int EntryForEnumerationIndex(int enumeration_index);

  inline static constexpr bool IsValidCapacity(int capacity);
  inline static int CapacityFor(int at_least_space_for);

  // Given a capacity, how much of it can we fill before resizing?
  inline static constexpr int MaxUsableCapacity(int capacity);

  // The maximum allowed capacity for any SwissNameDictionary.
  inline static constexpr int MaxCapacity();

  // Returns total size in bytes required for a table of given capacity.
  inline static constexpr int SizeFor(int capacity);

  inline static constexpr int MetaTableSizePerEntryFor(int capacity);
  inline static constexpr int MetaTableSizeFor(int capacity);

  inline static constexpr int DataTableSize(int capacity);
  inline static constexpr int CtrlTableSize(int capacity);

  // Indicates that IterateEntries() returns entries ordered.
  static constexpr bool kIsOrderedDictionaryType = true;

  static const int kGroupWidth = Group::kWidth;

  class BodyDescriptor;

  // Note that 0 is also a valid capacity. Changing this value to a smaller one
  // may make some padding necessary in the data layout.
  static constexpr int kInitialCapacity = kSwissNameDictionaryInitialCapacity;

  // Defines how many kTaggedSize sized values are associcated which each entry
  // in the data table.
  static constexpr int kDataTableEntryCount = 2;
  static constexpr int kDataTableKeyEntryIndex = 0;
  static constexpr int kDataTableValueEntryIndex = kDataTableKeyEntryIndex + 1;

  static constexpr int kMetaTableElementCountOffset = 0;
  static constexpr int kMetaTableDeletedElementCountOffset = 1;
  static constexpr int kMetaTableEnumerationTableStartOffset = 2;

  // The maximum capacity of any SwissNameDictionary whose meta table can use 1
  // byte per entry.
  static constexpr int kMax1ByteMetaTableCapacity = (1 << 8);
  // The maximum capacity of any SwissNameDictionary whose meta table can use 2
  // bytes per entry.
  static constexpr int kMax2ByteMetaTableCapacity = (1 << 16);

  // TODO(v8:11388) We would like to use Torque-generated constants here, but
  // those are currently incorrect.
  // Offset into the overall table, starting at HeapObject standard fields,
  // in bytes. This means that the map is stored at offset 0.
  using Offset = int;
  inline static constexpr Offset PrefixOffset();
  inline static constexpr Offset CapacityOffset();
  inline static constexpr Offset MetaTablePointerOffset();
  inline static constexpr Offset DataTableStartOffset();
  inline static constexpr Offset DataTableEndOffset(int capacity);
  inline static constexpr Offset CtrlTableStartOffset(int capacity);
  inline static constexpr Offset PropertyDetailsTableStartOffset(int capacity);

#if VERIFY_HEAP
  void SwissNameDictionaryVerify(Isolate* isolate, bool slow_checks);
#endif
  DECL_VERIFIER(SwissNameDictionary)
  DECL_PRINTER(SwissNameDictionary)
  DECL_CAST(SwissNameDictionary)
  OBJECT_CONSTRUCTORS(SwissNameDictionary, HeapObject);

 private:
  using ctrl_t = swiss_table::ctrl_t;
  using Ctrl = swiss_table::Ctrl;

  // Returns table of byte-encoded PropertyDetails (without enumeration index
  // stored in PropertyDetails).
  inline uint8_t* PropertyDetailsTable();

  // Sets key and value to the hole for the given entry.
  inline void ClearDataTableEntry(Isolate* isolate, int entry);
  inline void SetKey(int entry, Object key);

  inline void DetailsAtPut(int entry, PropertyDetails value);
  inline void ValueAtPut(int entry, Object value);

  inline PropertyDetails DetailsAt(int entry);
  inline Object ValueAtRaw(int entry);
  inline Object KeyAt(int entry);

  inline bool ToKey(ReadOnlyRoots roots, int entry, Object* out_key);

  // Use |set_ctrl| for modifications whenever possible, since that function
  // correctly maintains the copy of the first group at the end of the ctrl
  // table.
  inline ctrl_t* CtrlTable();

  inline static bool IsEmpty(ctrl_t c);
  inline static bool IsFull(ctrl_t c);
  inline static bool IsDeleted(ctrl_t c);
  inline static bool IsEmptyOrDeleted(ctrl_t c);

  // Sets the a control byte, taking the necessary copying of the first group
  // into account.
  inline void SetCtrl(int entry, ctrl_t h);
  inline ctrl_t GetCtrl(int entry);

  inline Object LoadFromDataTable(int entry, int data_offset);
  inline Object LoadFromDataTable(IsolateRoot root, int entry, int data_offset);
  inline void StoreToDataTable(int entry, int data_offset, Object data);
  inline void StoreToDataTableNoBarrier(int entry, int data_offset,
                                        Object data);

  inline void SetCapacity(int capacity);
  inline void SetNumberOfElements(int elements);
  inline void SetNumberOfDeletedElements(int deleted_elements);

  static inline swiss_table::ProbeSequence<Group::kWidth> probe(uint32_t hash,
                                                                int capacity);

  // Sets that the entry with the given |enumeration_index| is stored at the
  // given bucket of the data table.
  inline void SetEntryForEnumerationIndex(int enumeration_index, int entry);

  DECL_ACCESSORS(meta_table, ByteArray)
  inline void SetMetaTableField(int field_index, int value);
  inline int GetMetaTableField(int field_index);

  template <typename T>
  inline static void SetMetaTableField(ByteArray meta_table, int field_index,
                                       int value);
  template <typename T>
  inline static int GetMetaTableField(ByteArray meta_table, int field_index);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SWISS_NAME_DICTIONARY_H_
