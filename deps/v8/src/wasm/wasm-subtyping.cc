// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"

#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

bool IsEquivalent(ValueType type1, ValueType type2, const WasmModule* module);

bool IsArrayTypeEquivalent(uint32_t type_index_1, uint32_t type_index_2,
                           const WasmModule* module) {
  if (module->type_kinds[type_index_1] != kWasmArrayTypeCode ||
      module->type_kinds[type_index_2] != kWasmArrayTypeCode) {
    return false;
  }

  const ArrayType* sub_array = module->types[type_index_1].array_type;
  const ArrayType* super_array = module->types[type_index_2].array_type;
  if (sub_array->mutability() != super_array->mutability()) return false;

  // Temporarily cache type equivalence for the recursive call.
  module->cache_type_equivalence(type_index_1, type_index_2);
  if (IsEquivalent(sub_array->element_type(), super_array->element_type(),
                   module)) {
    return true;
  } else {
    module->uncache_type_equivalence(type_index_1, type_index_2);
    // TODO(7748): Consider caching negative results as well.
    return false;
  }
}

bool IsStructTypeEquivalent(uint32_t type_index_1, uint32_t type_index_2,
                            const WasmModule* module) {
  if (module->type_kinds[type_index_1] != kWasmStructTypeCode ||
      module->type_kinds[type_index_2] != kWasmStructTypeCode) {
    return false;
  }
  const StructType* sub_struct = module->types[type_index_1].struct_type;
  const StructType* super_struct = module->types[type_index_2].struct_type;

  if (sub_struct->field_count() != super_struct->field_count()) {
    return false;
  }

  // Temporarily cache type equivalence for the recursive call.
  module->cache_type_equivalence(type_index_1, type_index_2);
  for (uint32_t i = 0; i < sub_struct->field_count(); i++) {
    if (sub_struct->mutability(i) != super_struct->mutability(i) ||
        !IsEquivalent(sub_struct->field(i), super_struct->field(i), module)) {
      module->uncache_type_equivalence(type_index_1, type_index_2);
      return false;
    }
  }
  return true;
}

bool IsEquivalent(ValueType type1, ValueType type2, const WasmModule* module) {
  if (type1 == type2) return true;
  if (type1.kind() != type2.kind()) return false;
  // At this point, the types can only be both rtts, refs, or optrefs,
  // but with different indexed types.

  // Rtts need to have the same depth.
  if (type1.has_depth() && type1.depth() != type2.depth()) return false;
  // In all three cases, the indexed types have to be equivalent.
  if (module->is_cached_equivalent_type(type1.ref_index(), type2.ref_index())) {
    return true;
  }
  return IsArrayTypeEquivalent(type1.ref_index(), type2.ref_index(), module) ||
         IsStructTypeEquivalent(type1.ref_index(), type2.ref_index(), module);
}

bool IsStructSubtype(uint32_t subtype_index, uint32_t supertype_index,
                     const WasmModule* module) {
  if (module->type_kinds[subtype_index] != kWasmStructTypeCode ||
      module->type_kinds[supertype_index] != kWasmStructTypeCode) {
    return false;
  }
  const StructType* sub_struct = module->types[subtype_index].struct_type;
  const StructType* super_struct = module->types[supertype_index].struct_type;

  if (sub_struct->field_count() < super_struct->field_count()) {
    return false;
  }

  module->cache_subtype(subtype_index, supertype_index);
  for (uint32_t i = 0; i < super_struct->field_count(); i++) {
    bool sub_mut = sub_struct->mutability(i);
    bool super_mut = super_struct->mutability(i);
    if (sub_mut != super_mut ||
        (sub_mut &&
         !IsEquivalent(sub_struct->field(i), super_struct->field(i), module)) ||
        (!sub_mut &&
         !IsSubtypeOf(sub_struct->field(i), super_struct->field(i), module))) {
      module->uncache_subtype(subtype_index, supertype_index);
      return false;
    }
  }
  return true;
}

bool IsArraySubtype(uint32_t subtype_index, uint32_t supertype_index,
                    const WasmModule* module) {
  if (module->type_kinds[subtype_index] != kWasmArrayTypeCode ||
      module->type_kinds[supertype_index] != kWasmArrayTypeCode) {
    return false;
  }
  const ArrayType* sub_array = module->types[subtype_index].array_type;
  const ArrayType* super_array = module->types[supertype_index].array_type;
  bool sub_mut = sub_array->mutability();
  bool super_mut = super_array->mutability();
  module->cache_subtype(subtype_index, supertype_index);
  if (sub_mut != super_mut ||
      (sub_mut && !IsEquivalent(sub_array->element_type(),
                                super_array->element_type(), module)) ||
      (!sub_mut && !IsSubtypeOf(sub_array->element_type(),
                                super_array->element_type(), module))) {
    module->uncache_subtype(subtype_index, supertype_index);
    return false;
  } else {
    return true;
  }
}
}  // namespace

// TODO(7748): Extend this with function and any-heap subtyping.
V8_EXPORT_PRIVATE bool IsSubtypeOfHeap(HeapType subtype, HeapType supertype,
                                       const WasmModule* module) {
  DCHECK(!module->has_signature(subtype) && !module->has_signature(supertype));
  if (subtype == supertype) {
    return true;
  }
  // eqref is a supertype of all reference types except funcref.
  if (supertype == kHeapEq) {
    return subtype != kHeapFunc;
  }
  // At the moment, generic heap types are not subtyping-related otherwise.
  if (is_generic_heap_type(subtype) || is_generic_heap_type(supertype)) {
    return false;
  }

  if (module->is_cached_subtype(subtype, supertype)) {
    return true;
  }
  return IsStructSubtype(subtype, supertype, module) ||
         IsArraySubtype(subtype, supertype, module);
}

// TODO(7748): Extend this with function subtyping.
ValueType CommonSubtype(ValueType a, ValueType b, const WasmModule* module) {
  if (a == b) return a;
  if (IsSubtypeOf(a, b, module)) return a;
  if (IsSubtypeOf(b, a, module)) return b;
  return kWasmBottom;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
