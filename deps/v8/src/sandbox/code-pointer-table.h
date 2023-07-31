// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_POINTER_TABLE_H_
#define V8_SANDBOX_CODE_POINTER_TABLE_H_

#include "include/v8config.h"
#include "src/base/atomicops.h"
#include "src/base/memory.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/sandbox/external-entity-table.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

class Isolate;

/**
 * The entries of a CodePointerTable.
 */
struct CodePointerTableEntry {
  // Make this entry a code pointer entry containing the given pointer.
  inline void MakeCodePointerEntry(Address value);

  // Load code pointer stored in this entry.
  // This entry must be a code pointer entry.
  inline Address GetCodePointer() const;

  // Store the given code pointer in this entry.
  // This entry must be a code pointer entry.
  inline void SetCodePointer(Address value);

  // Make this entry a freelist entry, containing the index of the next entry
  // on the freelist.
  inline void MakeFreelistEntry(uint32_t next_entry_index);

  // Get the index of the next entry on the freelist. This method may be
  // called even when the entry is not a freelist entry. However, the result
  // is only valid if this is a freelist entry. This behaviour is required
  // for efficient entry allocation, see TryAllocateEntryFromFreelist.
  inline uint32_t GetNextFreelistEntryIndex() const;

 private:
  friend class CodePointerTable;

  // Freelist entries contain the index of the next free entry in their lower 32
  // bits and this tag in the upper 32 bits.
  static constexpr Address kFreeEntryTag = 0xffffffffULL << 32;

  std::atomic<Address> pointer_;
};

static_assert(sizeof(CodePointerTableEntry) == 8);

/**
 * A table containing pointers to code.
 *
 * When the sandbox is enabled, a code pointer table (CPT) can be used to ensure
 * basic control-flow integrity in the absence of special hardware support (such
 * as landing pad instructions): by referencing code through an index into a
 * CPT, and ensuring that only valid code entrypoints are stored inside the
 * table, it is then guaranteed that any indirect control-flow transfer ends up
 * on a valid entrypoint as long as an attacker is still confined to the
 * sandbox.
 */
class V8_EXPORT_PRIVATE CodePointerTable
    : public ExternalEntityTable<CodePointerTableEntry,
                                 kCodePointerTableReservationSize> {
 public:
  // Size of a CodePointerTable, for layout computation in IsolateData.
  static int constexpr kSize = 4 * kSystemPointerSize;
  static_assert(kMaxCodePointers == kMaxCapacity);

  CodePointerTable() = default;
  CodePointerTable(const CodePointerTable&) = delete;
  CodePointerTable& operator=(const CodePointerTable&) = delete;

  // Initializes this table by reserving the backing memory.
  void Initialize();

  // Resets this table and deletes all associated memory.
  void TearDown();

  // Retrieves the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline Address Get(CodePointerHandle handle) const;

  // Sets the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline void Set(CodePointerHandle handle, Address value);

  // Allocates a new entry in the table. The caller must provide the initial
  // value and tag.
  //
  // This method is atomic and can be called from background threads.
  inline CodePointerHandle AllocateAndInitializeEntry(Address initial_value);

  // The address of the backing buffer, for use in JIT compilers.
  Address buffer_address() const { return reinterpret_cast<Address>(buffer_); }

 private:
  inline uint32_t HandleToIndex(CodePointerHandle handle) const;
  inline CodePointerHandle IndexToHandle(uint32_t index) const;
};

static_assert(sizeof(CodePointerTable) == CodePointerTable::kSize);

V8_EXPORT_PRIVATE CodePointerTable* GetProcessWideCodePointerTable();

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_CODE_POINTER_TABLE_H_
