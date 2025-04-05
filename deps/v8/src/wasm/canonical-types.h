// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_CANONICAL_TYPES_H_
#define V8_WASM_CANONICAL_TYPES_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <unordered_map>

#include "src/base/bounds.h"
#include "src/base/hashing.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-module.h"

namespace v8::internal::wasm {

class CanonicalTypeNamesProvider;

// We use ValueType instances constructed from canonical type indices, so we
// can't let them get bigger than what we have storage space for.
// We could raise this limit using unused bits in the ValueType, but that
// doesn't seem urgent, as we have no evidence of the current limit being
// an actual limitation in practice.
static constexpr size_t kMaxCanonicalTypes = kV8MaxWasmTypes;
// We don't want any valid modules to fail canonicalization.
static_assert(kMaxCanonicalTypes >= kV8MaxWasmTypes);
// We want the invalid index to fail any range checks.
static_assert(kInvalidCanonicalIndex > kMaxCanonicalTypes);
// Ensure that ValueType can hold all canonical type indexes.
static_assert(kMaxCanonicalTypes <= (1 << CanonicalValueType::kNumIndexBits));

// A singleton class, responsible for isorecursive canonicalization of wasm
// types.
// A recursive group is a subsequence of types explicitly marked in the type
// section of a wasm module. Identical recursive groups have to be canonicalized
// to a single canonical group. Respective types in two identical groups are
// considered identical for all purposes.
// Two groups are considered identical if they have the same shape, and all
// type indices referenced in the same position in both groups reference:
// - identical types, if those do not belong to the rec. group,
// - types in the same relative position in the group, if those belong to the
//   rec. group.
class TypeCanonicalizer {
 public:
  static constexpr CanonicalTypeIndex kPredefinedArrayI8Index{0};
  static constexpr CanonicalTypeIndex kPredefinedArrayI16Index{1};
  static constexpr uint32_t kNumberOfPredefinedTypes = 2;

  TypeCanonicalizer();

  // Singleton class; no copying or moving allowed.
  TypeCanonicalizer(const TypeCanonicalizer& other) = delete;
  TypeCanonicalizer& operator=(const TypeCanonicalizer& other) = delete;
  TypeCanonicalizer(TypeCanonicalizer&& other) = delete;
  TypeCanonicalizer& operator=(TypeCanonicalizer&& other) = delete;

  // Register the last {size} types of {module} as a recursive group and
  // possibly canonicalize it if an identical one has been found.
  // Modifies {module->isorecursive_canonical_type_ids}.
  V8_EXPORT_PRIVATE void AddRecursiveGroup(WasmModule* module, uint32_t size);

  // Same as above, but for a group of size 1 (using the last type in the
  // module).
  V8_EXPORT_PRIVATE void AddRecursiveSingletonGroup(WasmModule* module);

  // Adds a module-independent signature as a recursive group, and canonicalizes
  // it if an identical is found. Returns the canonical index of the added
  // signature.
  V8_EXPORT_PRIVATE CanonicalTypeIndex
  AddRecursiveGroup(const FunctionSig* sig);

  // Retrieve back a type from a canonical index later.
  V8_EXPORT_PRIVATE const CanonicalSig* LookupFunctionSignature(
      CanonicalTypeIndex index) const;
  V8_EXPORT_PRIVATE const CanonicalStructType* LookupStruct(
      CanonicalTypeIndex index) const;
  V8_EXPORT_PRIVATE const CanonicalArrayType* LookupArray(
      CanonicalTypeIndex index) const;

  // Returns if {canonical_sub_index} is a canonical subtype of
  // {canonical_super_index}.
  V8_EXPORT_PRIVATE bool IsCanonicalSubtype(CanonicalTypeIndex sub_index,
                                            CanonicalTypeIndex super_index);

  // Returns if the type at {sub_index} in {sub_module} is a subtype of the
  // type at {super_index} in {super_module} after canonicalization.
  V8_EXPORT_PRIVATE bool IsCanonicalSubtype(ModuleTypeIndex sub_index,
                                            ModuleTypeIndex super_index,
                                            const WasmModule* sub_module,
                                            const WasmModule* super_module);

  // Deletes recursive groups. Used by fuzzers to avoid accumulating memory, and
  // used by specific tests e.g. for serialization / deserialization.
  V8_EXPORT_PRIVATE void EmptyStorageForTesting();

  size_t EstimateCurrentMemoryConsumption() const;

  V8_EXPORT_PRIVATE size_t GetCurrentNumberOfTypes() const;

  // Prepares wasm for the provided canonical type index. This reserves enough
  // space in the canonical rtts and the JSToWasm wrappers on the isolate roots.
  V8_EXPORT_PRIVATE static void PrepareForCanonicalTypeId(
      Isolate* isolate, CanonicalTypeIndex id);
  // Reset the canonical rtts and JSToWasm wrappers on the isolate roots for
  // testing purposes (in production cases canonical type ids are never freed).
  V8_EXPORT_PRIVATE static void ClearWasmCanonicalTypesForTesting(
      Isolate* isolate);

  V8_EXPORT_PRIVATE bool IsFunctionSignature(CanonicalTypeIndex index) const;
  V8_EXPORT_PRIVATE bool IsStruct(CanonicalTypeIndex index) const;
  V8_EXPORT_PRIVATE bool IsArray(CanonicalTypeIndex index) const;

  bool IsHeapSubtype(CanonicalTypeIndex sub, CanonicalTypeIndex super) const;
  bool IsCanonicalSubtype_Locked(CanonicalTypeIndex sub_index,
                                 CanonicalTypeIndex super_index) const;

  CanonicalTypeIndex FindIndex_Slow(const CanonicalSig* sig) const;

#if DEBUG
  // Check whether a supposedly-canonicalized function signature does indeed
  // live in this class's storage. Useful for guarding casts of signatures
  // that are entering the typed world.
  V8_EXPORT_PRIVATE bool Contains(const CanonicalSig* sig) const;
#endif

 private:
  struct CanonicalType {
    enum Kind : int8_t { kFunction, kStruct, kArray, kCont };

    union {
      const CanonicalSig* function_sig = nullptr;
      const CanonicalStructType* struct_type;
      const CanonicalArrayType* array_type;
      const CanonicalContType* cont_type;
    };
    CanonicalTypeIndex supertype{kNoSuperType};
    CanonicalTypeIndex descriptor{kNoType};
    CanonicalTypeIndex describes{kNoType};
    Kind kind = kFunction;
    bool is_final = false;
    bool is_shared = false;
    uint8_t subtyping_depth = 0;

    constexpr CanonicalType(const CanonicalSig* sig,
                            CanonicalTypeIndex supertype, bool is_final,
                            bool is_shared)
        : function_sig(sig),
          supertype(supertype),
          kind(kFunction),
          is_final(is_final),
          is_shared(is_shared) {}

    constexpr CanonicalType(const CanonicalStructType* type,
                            CanonicalTypeIndex supertype,
                            CanonicalTypeIndex descriptor,
                            CanonicalTypeIndex describes, bool is_final,
                            bool is_shared)
        : struct_type(type),
          supertype(supertype),
          descriptor(descriptor),
          describes(describes),
          kind(kStruct),
          is_final(is_final),
          is_shared(is_shared) {}

    constexpr CanonicalType(const CanonicalArrayType* type,
                            CanonicalTypeIndex supertype, bool is_final,
                            bool is_shared)
        : array_type(type),
          supertype(supertype),
          kind(kArray),
          is_final(is_final),
          is_shared(is_shared) {}

    constexpr CanonicalType(const CanonicalContType* type,
                            CanonicalTypeIndex supertype, bool is_final,
                            bool is_shared)
        : cont_type(type),
          supertype(supertype),
          kind(kCont),
          is_final(is_final),
          is_shared(is_shared) {}

    constexpr CanonicalType() = default;
  };

  // Define the range of a recursion group; for use in {CanonicalHashing} and
  // {CanonicalEquality}.
  struct RecursionGroupRange {
    const CanonicalTypeIndex first;
    const CanonicalTypeIndex last;

    bool Contains(CanonicalTypeIndex index) const {
      return base::IsInRange(index.index, first.index, last.index);
    }
  };

  // Support for hashing of recursion groups, where type indexes have to be
  // hashed relative to the recursion group.
  struct CanonicalHashing {
    base::Hasher hasher;
    const RecursionGroupRange recgroup;

    explicit CanonicalHashing(RecursionGroupRange recgroup)
        : recgroup{recgroup} {}

    uint32_t MakeGroupRelative(CanonicalTypeIndex index) {
      uint32_t is_relative = recgroup.Contains(index) ? 1 : 0;
      uint32_t relative = index.index - is_relative * recgroup.first.index;
      return (relative << 1) | is_relative;
    }

    void Add(CanonicalType type) {
      // Add several pieces of information in a single number, for faster
      // hashing.
      uint32_t supertype_index = MakeGroupRelative(type.supertype);
      static_assert(kMaxCanonicalTypes <= kMaxUInt32 >> 3);
      uint32_t metadata =
          (supertype_index << 2) | (type.is_shared << 1) | (type.is_final << 0);
      hasher.Add(metadata);
      switch (type.kind) {
        case CanonicalType::kFunction:
          Add(*type.function_sig);
          break;
        case CanonicalType::kStruct: {
          uint32_t descriptor_index = MakeGroupRelative(type.descriptor);
          uint32_t describes_index = MakeGroupRelative(type.describes);
          // {MakeGroupRelative} returns a value at most 1 bit bigger than
          // the max canonical index, which currently fits into 20 bits. So
          // by shifting left by 11, we're not truncating anything. (And even
          // if we did, it's just a hash, collisions are OK.)
          uint32_t desc = (descriptor_index << 11) ^ (describes_index);
          hasher.Add(desc);
          Add(*type.struct_type);
          break;
        }
        case CanonicalType::kArray:
          Add(*type.array_type);
          break;
        case CanonicalType::kCont:
          Add(*type.cont_type);
          break;
      }
    }

    void Add(CanonicalValueType value_type) {
      if (value_type.has_index() && recgroup.Contains(value_type.ref_index())) {
        // For relative indexed types, add their nullability, exactness, and
        // the relative index to the hash.
        // Shift the relative index by {kMaxCanonicalTypes} to map it to a
        // different index space (note that collisions in hashing are OK
        // though).
        static_assert(kMaxCanonicalTypes <= kMaxUInt32 / 2);
        // TODO(403372470): Add the 'exact' bit.
        hasher.Add((value_type.is_exact() << 1) | value_type.is_nullable());
        hasher.Add((value_type.ref_index().index - recgroup.first.index) +
                   kMaxCanonicalTypes);
      } else {
        hasher.Add(value_type);
      }
    }

    void Add(const CanonicalSig& sig) {
      hasher.Add(sig.parameter_count());
      for (CanonicalValueType type : sig.all()) Add(type);
    }

    void Add(const CanonicalStructType& struct_type) {
      hasher.AddRange(struct_type.mutabilities());
      for (const ValueTypeBase& field : struct_type.fields()) {
        Add(CanonicalValueType{field});
      }
    }

    void Add(const CanonicalArrayType& array_type) {
      hasher.Add(array_type.mutability());
      Add(array_type.element_type());
    }

    void Add(const CanonicalContType& cont_type) {
      CanonicalTypeIndex cont_index = cont_type.contfun_typeindex();

      if (recgroup.Contains(cont_index)) {
        hasher.Add(cont_index.index - recgroup.first.index +
                   kMaxCanonicalTypes);
      } else {
        hasher.Add(cont_index.index);  // Not relative to this rec group
      }
    }

    size_t hash() const { return hasher.hash(); }
  };

  // Support for equality checking of recursion groups, where type indexes have
  // to be compared relative to their respective recursion group.
  struct CanonicalEquality {
    // Recursion group bounds for LHS and RHS.
    const RecursionGroupRange recgroup1;
    const RecursionGroupRange recgroup2;

    CanonicalEquality(RecursionGroupRange recgroup1,
                      RecursionGroupRange recgroup2)
        : recgroup1{recgroup1}, recgroup2{recgroup2} {}

    bool EqualTypeIndex(CanonicalTypeIndex index1,
                        CanonicalTypeIndex index2) const {
      const bool relative_index = recgroup1.Contains(index1);
      if (relative_index != recgroup2.Contains(index2)) return false;
      if (relative_index) {
        // Compare relative type indexes within the respective recgroups.
        uint32_t rel_type1 = index1.index - recgroup1.first.index;
        uint32_t rel_type2 = index2.index - recgroup2.first.index;
        if (rel_type1 != rel_type2) return false;
      } else if (index1 != index2) {
        return false;
      }
      return true;
    }

    bool EqualType(const CanonicalType& type1,
                   const CanonicalType& type2) const {
      if (!EqualTypeIndex(type1.supertype, type2.supertype)) return false;
      if (type1.is_final != type2.is_final) return false;
      if (type1.is_shared != type2.is_shared) return false;
      switch (type1.kind) {
        case CanonicalType::kFunction:
          return type2.kind == CanonicalType::kFunction &&
                 EqualSig(*type1.function_sig, *type2.function_sig);
        case CanonicalType::kStruct:
          return type2.kind == CanonicalType::kStruct &&
                 EqualTypeIndex(type1.descriptor, type2.descriptor) &&
                 EqualTypeIndex(type1.describes, type2.describes) &&
                 EqualStructType(*type1.struct_type, *type2.struct_type);
        case CanonicalType::kArray:
          return type2.kind == CanonicalType::kArray &&
                 EqualArrayType(*type1.array_type, *type2.array_type);
        case CanonicalType::kCont:
          return type2.kind == CanonicalType::kCont &&
                 EqualContType(*type1.cont_type, *type2.cont_type);
      }
    }

    bool EqualTypes(base::Vector<const CanonicalType> types1,
                    base::Vector<const CanonicalType> types2) const {
      return std::equal(types1.begin(), types1.end(), types2.begin(),
                        types2.end(),
                        std::bind_front(&CanonicalEquality::EqualType, this));
    }

    bool EqualValueType(CanonicalValueType type1,
                        CanonicalValueType type2) const {
      const bool indexed = type1.has_index();
      if (indexed != type2.has_index()) return false;
      if (indexed) {
        return EqualTypeIndex(type1.ref_index(), type2.ref_index());
      }
      return type1 == type2;
    }

    bool EqualSig(const CanonicalSig& sig1, const CanonicalSig& sig2) const {
      if (sig1.parameter_count() != sig2.parameter_count()) return false;
      return std::equal(
          sig1.all().begin(), sig1.all().end(), sig2.all().begin(),
          sig2.all().end(),
          std::bind_front(&CanonicalEquality::EqualValueType, this));
    }

    bool EqualStructType(const CanonicalStructType& type1,
                         const CanonicalStructType& type2) const {
      return
          // Compare fields, including a check that the size is the same.
          std::equal(
              type1.fields().begin(), type1.fields().end(),
              type2.fields().begin(), type2.fields().end(),
              std::bind_front(&CanonicalEquality::EqualValueType, this)) &&
          // Compare mutabilities, skipping the check for the size.
          std::equal(type1.mutabilities().begin(), type1.mutabilities().end(),
                     type2.mutabilities().begin());
    }

    bool EqualArrayType(const CanonicalArrayType& type1,
                        const CanonicalArrayType& type2) const {
      return type1.mutability() == type2.mutability() &&
             EqualValueType(type1.element_type(), type2.element_type());
    }

    bool EqualContType(const CanonicalContType& type1,
                       const CanonicalContType& type2) const {
      return EqualTypeIndex(type1.contfun_typeindex(),
                            type2.contfun_typeindex());
    }
  };

  struct CanonicalGroup {
    CanonicalGroup(Zone* zone, size_t size, CanonicalTypeIndex first)
        : types(zone->AllocateVector<CanonicalType>(size)), first(first) {
      // size >= 2; otherwise a `CanonicalSingletonGroup` should have been used.
      DCHECK_LE(2, size);
    }

    bool operator==(const CanonicalGroup& other) const {
      CanonicalTypeIndex last{first.index +
                              static_cast<uint32_t>(types.size() - 1)};
      CanonicalTypeIndex other_last{
          other.first.index + static_cast<uint32_t>(other.types.size() - 1)};
      CanonicalEquality equality{{first, last}, {other.first, other_last}};
      return equality.EqualTypes(types, other.types);
    }

    size_t hash_value() const {
      CanonicalTypeIndex last{first.index +
                              static_cast<uint32_t>(types.size()) - 1};
      CanonicalHashing hasher{{first, last}};
      for (CanonicalType t : types) {
        hasher.Add(t);
      }
      return hasher.hash();
    }

    // The storage of this vector is the TypeCanonicalizer's zone_.
    const base::Vector<CanonicalType> types;
    const CanonicalTypeIndex first;
  };

  struct CanonicalSingletonGroup {
    bool operator==(const CanonicalSingletonGroup& other) const {
      CanonicalEquality equality{{index, index}, {other.index, other.index}};
      return equality.EqualType(type, other.type);
    }

    size_t hash_value() const {
      CanonicalHashing hasher{{index, index}};
      hasher.Add(type);
      return hasher.hash();
    }

    CanonicalType type;
    CanonicalTypeIndex index;
  };

  // Conceptually a vector of CanonicalType. Modification generally requires
  // synchronization, read-only access can be done without locking.
  class CanonicalTypeVector {
    static constexpr uint32_t kSegmentSize = 1024;
    static constexpr uint32_t kNumSegments =
        (kMaxCanonicalTypes + kSegmentSize - 1) / kSegmentSize;
    static_assert(kSegmentSize * kNumSegments >= kMaxCanonicalTypes);
    static_assert(
        kNumSegments <= 1024,
        "Reconsider this data structures when increasing kMaxCanonicalTypes");

   public:
    const CanonicalType* operator[](CanonicalTypeIndex index) const {
      uint32_t segment_idx = index.index / kSegmentSize;
      // Only check against the static constant here; uninitialized segments are
      // {nullptr}, so accessing them crashes.
      SBXCHECK_GT(kNumSegments, segment_idx);
      Segment* segment = segments_[segment_idx].load(std::memory_order_relaxed);
      const CanonicalType* type = (*segment)[index.index % kSegmentSize];
      // DCHECK is sufficient as returning {nullptr} is safe.
      DCHECK_NOT_NULL(type);
      return type;
    }

    const CanonicalType* operator[](CanonicalValueType type) const {
      DCHECK(type.has_index());
      return (*this)[type.ref_index()];
    }

    void reserve(uint32_t size, Zone* zone) {
      DCHECK_GE(kMaxCanonicalTypes, size);
      uint32_t segment_idx = size / kSegmentSize;
      // Stop on the first segment (iterating backwards) which already exists.
      while (!segments_[segment_idx].load(std::memory_order_relaxed)) {
        segments_[segment_idx].store(zone->New<Segment>(),
                                     std::memory_order_relaxed);
        if (segment_idx-- == 0) break;
      }
    }

    void set(CanonicalTypeIndex index, const CanonicalType* type) {
      // No checking needed, this is only executed after CheckMaxCanonicalIndex.
      uint32_t segment_idx = index.index / kSegmentSize;
      Segment* segment = segments_[segment_idx].load(std::memory_order_relaxed);
      segment->set(index.index % kSegmentSize, type);
    }

    void ClearForTesting() {
      for (uint32_t i = 0; i < kNumSegments; ++i) {
        if (segments_[i].load(std::memory_order_relaxed) == nullptr) break;
        segments_[i].store(nullptr, std::memory_order_relaxed);
      }
    }

    const CanonicalTypeIndex FindIndex_Slow(const CanonicalSig* sig) const {
      for (uint32_t i = 0; i < kNumSegments; ++i) {
        Segment* segment = segments_[i].load(std::memory_order_relaxed);
        // If callers have a CanonicalSig* to pass into this function, the
        // type canonicalizer must know about this sig, hence we must find it
        // before hitting a `nullptr` segment.
        DCHECK_NOT_NULL(segment);
        for (uint32_t k = 0; k < kSegmentSize; ++k) {
          const CanonicalType* type = (*segment)[k];
          // Again: We expect to find the signature before hitting uninitialized
          // slots.
          DCHECK_NOT_NULL(type);
          if (type->kind == CanonicalType::kFunction &&
              type->function_sig == sig) {
            return CanonicalTypeIndex{i * kSegmentSize + k};
          }
        }
      }
      UNREACHABLE();
    }

   private:
    class Segment {
     public:
      const CanonicalType* operator[](uint32_t index) const {
        DCHECK_GT(kSegmentSize, index);
        return content_[index].load(std::memory_order_relaxed);
      }

      void set(uint32_t index, const CanonicalType* type) {
        DCHECK_GT(kSegmentSize, index);
        DCHECK_NULL(content_[index].load(std::memory_order_relaxed));
        content_[index].store(type, std::memory_order_relaxed);
      }

     private:
      std::atomic<const CanonicalType*> content_[kSegmentSize]{};
    };

    std::atomic<Segment*> segments_[kNumSegments]{};
  };

  void AddPredefinedArrayTypes();

  CanonicalTypeIndex FindCanonicalGroup(const CanonicalGroup&) const;
  CanonicalTypeIndex FindCanonicalGroup(const CanonicalSingletonGroup&) const;

  // Canonicalize the module-specific type at `module_type_idx` within the
  // recursion group starting at `recursion_group_start`, using
  // `canonical_recgroup_start` as the start offset of types within the
  // recursion group.
  CanonicalType CanonicalizeTypeDef(
      const WasmModule* module, ModuleTypeIndex module_type_idx,
      ModuleTypeIndex recgroup_start,
      CanonicalTypeIndex canonical_recgroup_start);

  void CheckMaxCanonicalIndex() const;

  std::vector<CanonicalTypeIndex> canonical_supertypes_;
  // Set of all known canonical recgroups of size >=2.
  std::unordered_set<CanonicalGroup> canonical_groups_;
  // Set of all known canonical recgroups of size 1.
  std::unordered_set<CanonicalSingletonGroup> canonical_singleton_groups_;
  // Maps canonical indices back to the types.
  CanonicalTypeVector canonical_types_;
  AccountingAllocator allocator_;
  Zone zone_{&allocator_, "canonical type zone"};
  mutable base::Mutex mutex_;
};

// Returns a reference to the TypeCanonicalizer shared by the entire process.
V8_EXPORT_PRIVATE TypeCanonicalizer* GetTypeCanonicalizer();

}  // namespace v8::internal::wasm

#endif  // V8_WASM_CANONICAL_TYPES_H_
