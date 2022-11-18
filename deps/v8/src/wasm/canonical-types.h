// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_CANONICAL_TYPES_H_
#define V8_WASM_CANONICAL_TYPES_H_

#include <unordered_map>

#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

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
  TypeCanonicalizer() = default;

  // Singleton class; no copying or moving allowed.
  TypeCanonicalizer(const TypeCanonicalizer& other) = delete;
  TypeCanonicalizer& operator=(const TypeCanonicalizer& other) = delete;
  TypeCanonicalizer(TypeCanonicalizer&& other) = delete;
  TypeCanonicalizer& operator=(TypeCanonicalizer&& other) = delete;

  // Registers the last {size} types of {module} as a recursive group, and
  // possibly canonicalizes it if an identical one has been found.
  // Modifies {module->isorecursive_canonical_type_ids}.
  V8_EXPORT_PRIVATE void AddRecursiveGroup(WasmModule* module, uint32_t size);

  // Adds a module-independent signature as a recursive group, and canonicalizes
  // it if an identical is found. Returns the canonical index of the added
  // signature.
  V8_EXPORT_PRIVATE uint32_t AddRecursiveGroup(const FunctionSig* sig);

  // Returns if the type at {sub_index} in {sub_module} is a subtype of the
  // type at {super_index} in {super_module} after canonicalization.
  V8_EXPORT_PRIVATE bool IsCanonicalSubtype(uint32_t sub_index,
                                            uint32_t super_index,
                                            const WasmModule* sub_module,
                                            const WasmModule* super_module);

 private:
  using TypeInModule = std::pair<const WasmModule*, uint32_t>;
  struct CanonicalType {
    TypeDefinition type_def;
    bool is_relative_supertype;

    bool operator==(const CanonicalType& other) const {
      return type_def == other.type_def &&
             is_relative_supertype == other.is_relative_supertype;
    }

    bool operator!=(const CanonicalType& other) const {
      return type_def != other.type_def ||
             is_relative_supertype != other.is_relative_supertype;
    }

    // TODO(manoskouk): Improve this.
    size_t hash_value() const {
      return base::hash_combine(base::hash_value(type_def.kind),
                                base::hash_value(is_relative_supertype));
    }
  };
  struct CanonicalGroup {
    struct hash {
      size_t operator()(const CanonicalGroup& group) const {
        return group.hash_value();
      }
    };

    bool operator==(const CanonicalGroup& other) const {
      return types == other.types;
    }

    bool operator!=(const CanonicalGroup& other) const {
      return types != other.types;
    }

    size_t hash_value() const {
      size_t result = 0;
      for (const CanonicalType& type : types) {
        result = base::hash_combine(result, type.hash_value());
      }
      return result;
    }

    std::vector<CanonicalType> types;
  };

  int FindCanonicalGroup(CanonicalGroup&) const;

  // Canonicalize all types present in {type} (including supertype) according to
  // {CanonicalizeValueType}.
  CanonicalType CanonicalizeTypeDef(const WasmModule* module,
                                    TypeDefinition type,
                                    uint32_t recursive_group_start);

  // An indexed type gets mapped to a {ValueType::CanonicalWithRelativeIndex}
  // if its index points inside the new canonical group; otherwise, the index
  // gets mapped to its canonical representative.
  ValueType CanonicalizeValueType(const WasmModule* module, ValueType type,
                                  uint32_t recursive_group_start) const;

  std::vector<uint32_t> canonical_supertypes_;
  // group -> canonical id of first type
  std::unordered_map<CanonicalGroup, uint32_t, CanonicalGroup::hash>
      canonical_groups_;
  AccountingAllocator allocator_;
  Zone zone_{&allocator_, "canonical type zone"};
  base::Mutex mutex_;
};

// Returns a reference to the TypeCanonicalizer shared by the entire process.
V8_EXPORT_PRIVATE TypeCanonicalizer* GetTypeCanonicalizer();

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_CANONICAL_TYPES_H_
