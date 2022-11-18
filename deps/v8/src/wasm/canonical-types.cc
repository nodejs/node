// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/canonical-types.h"

#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {

TypeCanonicalizer* GetTypeCanonicalizer() {
  return GetWasmEngine()->type_canonicalizer();
}

void TypeCanonicalizer::AddRecursiveGroup(WasmModule* module, uint32_t size) {
  // Multiple threads could try to register recursive groups concurrently.
  // TODO(manoskouk): Investigate if we can fine-grain the synchronization.
  base::MutexGuard mutex_guard(&mutex_);
  DCHECK_GE(module->types.size(), size);
  uint32_t start_index = static_cast<uint32_t>(module->types.size()) - size;
  CanonicalGroup group;
  group.types.resize(size);
  for (uint32_t i = 0; i < size; i++) {
    group.types[i] = CanonicalizeTypeDef(module, module->types[start_index + i],
                                         start_index);
  }
  int canonical_index = FindCanonicalGroup(group);
  if (canonical_index >= 0) {
    // Identical group found. Map new types to the old types's canonical
    // representatives.
    for (uint32_t i = 0; i < size; i++) {
      module->isorecursive_canonical_type_ids[start_index + i] =
          canonical_index + i;
    }
  } else {
    // Identical group not found. Add new canonical representatives for the new
    // types.
    uint32_t first_canonical_index =
        static_cast<uint32_t>(canonical_supertypes_.size());
    canonical_supertypes_.resize(first_canonical_index + size);
    for (uint32_t i = 0; i < size; i++) {
      CanonicalType& canonical_type = group.types[i];
      // Compute the canonical index of the supertype: If it is relative, we
      // need to add {first_canonical_index}.
      canonical_supertypes_[first_canonical_index + i] =
          canonical_type.is_relative_supertype
              ? canonical_type.type_def.supertype + first_canonical_index
              : canonical_type.type_def.supertype;
      module->isorecursive_canonical_type_ids[start_index + i] =
          first_canonical_index + i;
    }
    canonical_groups_.emplace(group, first_canonical_index);
  }
}

uint32_t TypeCanonicalizer::AddRecursiveGroup(const FunctionSig* sig) {
  base::MutexGuard mutex_guard(&mutex_);
// Types in the signature must be module-independent.
#if DEBUG
  for (ValueType type : sig->all()) DCHECK(!type.has_index());
#endif
  CanonicalGroup group;
  group.types.resize(1);
  group.types[0].type_def = TypeDefinition(sig, kNoSuperType);
  group.types[0].is_relative_supertype = false;
  int canonical_index = FindCanonicalGroup(group);
  if (canonical_index < 0) {
    canonical_index = static_cast<int>(canonical_supertypes_.size());
    // We need to copy the signature in the local zone, or else we risk
    // storing a dangling pointer in the future.
    auto builder = FunctionSig::Builder(&zone_, sig->return_count(),
                                        sig->parameter_count());
    for (auto type : sig->returns()) builder.AddReturn(type);
    for (auto type : sig->parameters()) builder.AddParam(type);
    const FunctionSig* allocated_sig = builder.Build();
    group.types[0].type_def = TypeDefinition(allocated_sig, kNoSuperType);
    group.types[0].is_relative_supertype = false;
    canonical_groups_.emplace(group, canonical_index);
    canonical_supertypes_.emplace_back(kNoSuperType);
  }
  return canonical_index;
}

ValueType TypeCanonicalizer::CanonicalizeValueType(
    const WasmModule* module, ValueType type,
    uint32_t recursive_group_start) const {
  if (!type.has_index()) return type;
  return type.ref_index() >= recursive_group_start
             ? ValueType::CanonicalWithRelativeIndex(
                   type.kind(), type.ref_index() - recursive_group_start)
             : ValueType::FromIndex(
                   type.kind(),
                   module->isorecursive_canonical_type_ids[type.ref_index()]);
}

bool TypeCanonicalizer::IsCanonicalSubtype(uint32_t sub_index,
                                           uint32_t super_index,
                                           const WasmModule* sub_module,
                                           const WasmModule* super_module) {
  // Multiple threads could try to register and access recursive groups
  // concurrently.
  // TODO(manoskouk): Investigate if we can improve this synchronization.
  base::MutexGuard mutex_guard(&mutex_);
  uint32_t canonical_super =
      super_module->isorecursive_canonical_type_ids[super_index];
  uint32_t canonical_sub =
      sub_module->isorecursive_canonical_type_ids[sub_index];
  while (canonical_sub != kNoSuperType) {
    if (canonical_sub == canonical_super) return true;
    canonical_sub = canonical_supertypes_[canonical_sub];
  }
  return false;
}

TypeCanonicalizer::CanonicalType TypeCanonicalizer::CanonicalizeTypeDef(
    const WasmModule* module, TypeDefinition type,
    uint32_t recursive_group_start) {
  uint32_t canonical_supertype = kNoSuperType;
  bool is_relative_supertype = false;
  if (type.supertype < recursive_group_start) {
    canonical_supertype =
        module->isorecursive_canonical_type_ids[type.supertype];
  } else if (type.supertype != kNoSuperType) {
    canonical_supertype = type.supertype - recursive_group_start;
    is_relative_supertype = true;
  }
  TypeDefinition result;
  switch (type.kind) {
    case TypeDefinition::kFunction: {
      const FunctionSig* original_sig = type.function_sig;
      FunctionSig::Builder builder(&zone_, original_sig->return_count(),
                                   original_sig->parameter_count());
      for (ValueType ret : original_sig->returns()) {
        builder.AddReturn(
            CanonicalizeValueType(module, ret, recursive_group_start));
      }
      for (ValueType param : original_sig->parameters()) {
        builder.AddParam(
            CanonicalizeValueType(module, param, recursive_group_start));
      }
      result = TypeDefinition(builder.Build(), canonical_supertype);
      break;
    }
    case TypeDefinition::kStruct: {
      const StructType* original_type = type.struct_type;
      StructType::Builder builder(&zone_, original_type->field_count());
      for (uint32_t i = 0; i < original_type->field_count(); i++) {
        builder.AddField(CanonicalizeValueType(module, original_type->field(i),
                                               recursive_group_start),
                         original_type->mutability(i));
      }
      result = TypeDefinition(builder.Build(), canonical_supertype);
      break;
    }
    case TypeDefinition::kArray: {
      ValueType element_type = CanonicalizeValueType(
          module, type.array_type->element_type(), recursive_group_start);
      result = TypeDefinition(
          zone_.New<ArrayType>(element_type, type.array_type->mutability()),
          canonical_supertype);
      break;
    }
  }

  return {result, is_relative_supertype};
}

// Returns the index of the canonical representative of the first type in this
// group, or -1 if an identical group does not exist.
int TypeCanonicalizer::FindCanonicalGroup(CanonicalGroup& group) const {
  auto element = canonical_groups_.find(group);
  return element == canonical_groups_.end() ? -1 : element->second;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
