// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_EXTERNAL_REFERENCE_TABLE_H_
#define V8_CODEGEN_EXTERNAL_REFERENCE_TABLE_H_

#include <vector>

#include "src/builtins/accessors.h"
#include "src/builtins/builtins.h"
#include "src/codegen/external-reference.h"
#include "src/logging/counters-definitions.h"

namespace v8 {
namespace internal {

class Isolate;

// ExternalReferenceTable is a helper class that defines the relationship
// between external references and their encodings. It is used to build
// hashmaps in ExternalReferenceEncoder and ExternalReferenceDecoder.
class ExternalReferenceTable {
 public:
  // For the nullptr ref, see the constructor.
  static constexpr int kSpecialReferenceCount = 1;
  static constexpr int kExternalReferenceCount =
      ExternalReference::kExternalReferenceCount;
  static constexpr int kBuiltinsReferenceCount =
#define COUNT_C_BUILTIN(...) +1
      BUILTIN_LIST_C(COUNT_C_BUILTIN);
#undef COUNT_C_BUILTIN
  static constexpr int kRuntimeReferenceCount =
      Runtime::kNumFunctions -
      Runtime::kNumInlineFunctions;  // Don't count dupe kInline... functions.
  static constexpr int kIsolateAddressReferenceCount = kIsolateAddressCount;
  static constexpr int kAccessorReferenceCount =
      Accessors::kAccessorInfoCount + Accessors::kAccessorSetterCount;
  // The number of stub cache external references, see AddStubCache.
  static constexpr int kStubCacheReferenceCount = 12;
  static constexpr int kStatsCountersReferenceCount =
#define SC(...) +1
      STATS_COUNTER_NATIVE_CODE_LIST(SC);
#undef SC
  static constexpr int kSize =
      kSpecialReferenceCount + kExternalReferenceCount +
      kBuiltinsReferenceCount + kRuntimeReferenceCount +
      kIsolateAddressReferenceCount + kAccessorReferenceCount +
      kStubCacheReferenceCount + kStatsCountersReferenceCount;
  static constexpr uint32_t kEntrySize =
      static_cast<uint32_t>(kSystemPointerSize);
  static constexpr uint32_t kSizeInBytes = kSize * kEntrySize + 2 * kUInt32Size;

  Address address(uint32_t i) const { return ref_addr_[i]; }
  const char* name(uint32_t i) const { return ref_name_[i]; }

  bool is_initialized() const { return is_initialized_ != 0; }

  static const char* ResolveSymbol(void* address);

  static constexpr uint32_t OffsetOfEntry(uint32_t i) {
    // Used in CodeAssembler::LookupExternalReference.
    return i * kEntrySize;
  }

  const char* NameFromOffset(uint32_t offset) {
    DCHECK_EQ(offset % kEntrySize, 0);
    DCHECK_LT(offset, kSizeInBytes);
    int index = offset / kEntrySize;
    return name(index);
  }

  ExternalReferenceTable() = default;
  ExternalReferenceTable(const ExternalReferenceTable&) = delete;
  ExternalReferenceTable& operator=(const ExternalReferenceTable&) = delete;
  void Init(Isolate* isolate);

 private:
  void Add(Address address, int* index);

  void AddReferences(Isolate* isolate, int* index);
  void AddBuiltins(int* index);
  void AddRuntimeFunctions(int* index);
  void AddIsolateAddresses(Isolate* isolate, int* index);
  void AddAccessors(int* index);
  void AddStubCache(Isolate* isolate, int* index);

  Address GetStatsCounterAddress(StatsCounter* counter);
  void AddNativeCodeStatsCounters(Isolate* isolate, int* index);

  STATIC_ASSERT(sizeof(Address) == kEntrySize);
  Address ref_addr_[kSize];
  static const char* const ref_name_[kSize];

  // Not bool to guarantee deterministic size.
  uint32_t is_initialized_ = 0;

  // Redirect disabled stats counters to this field. This is done to make sure
  // we can have a snapshot that includes native counters even when the embedder
  // isn't collecting them.
  // This field is uint32_t since the MacroAssembler and CodeStubAssembler
  // accesses this field as a uint32_t.
  uint32_t dummy_stats_counter_ = 0;
};

STATIC_ASSERT(ExternalReferenceTable::kSizeInBytes ==
              sizeof(ExternalReferenceTable));

}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_EXTERNAL_REFERENCE_TABLE_H_
