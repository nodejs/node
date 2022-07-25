// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"

#include "src/base/platform/mutex.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/wasm-module.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

V8_INLINE bool EquivalentIndices(uint32_t index1, uint32_t index2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) {
  DCHECK(index1 != index2 || module1 != module2);
  if (!FLAG_wasm_type_canonicalization) return false;
  return module1->isorecursive_canonical_type_ids[index1] ==
         module2->isorecursive_canonical_type_ids[index2];
}

bool ValidStructSubtypeDefinition(uint32_t subtype_index,
                                  uint32_t supertype_index,
                                  const WasmModule* sub_module,
                                  const WasmModule* super_module) {
  const StructType* sub_struct = sub_module->types[subtype_index].struct_type;
  const StructType* super_struct =
      super_module->types[supertype_index].struct_type;

  if (sub_struct->field_count() < super_struct->field_count()) {
    return false;
  }

  for (uint32_t i = 0; i < super_struct->field_count(); i++) {
    bool sub_mut = sub_struct->mutability(i);
    bool super_mut = super_struct->mutability(i);
    if (sub_mut != super_mut ||
        (sub_mut &&
         !EquivalentTypes(sub_struct->field(i), super_struct->field(i),
                          sub_module, super_module)) ||
        (!sub_mut && !IsSubtypeOf(sub_struct->field(i), super_struct->field(i),
                                  sub_module, super_module))) {
      return false;
    }
  }
  return true;
}

bool ValidArraySubtypeDefinition(uint32_t subtype_index,
                                 uint32_t supertype_index,
                                 const WasmModule* sub_module,
                                 const WasmModule* super_module) {
  const ArrayType* sub_array = sub_module->types[subtype_index].array_type;
  const ArrayType* super_array =
      super_module->types[supertype_index].array_type;
  bool sub_mut = sub_array->mutability();
  bool super_mut = super_array->mutability();

  return (sub_mut && super_mut &&
          EquivalentTypes(sub_array->element_type(),
                          super_array->element_type(), sub_module,
                          super_module)) ||
         (!sub_mut && !super_mut &&
          IsSubtypeOf(sub_array->element_type(), super_array->element_type(),
                      sub_module, super_module));
}

bool ValidFunctionSubtypeDefinition(uint32_t subtype_index,
                                    uint32_t supertype_index,
                                    const WasmModule* sub_module,
                                    const WasmModule* super_module) {
  const FunctionSig* sub_func = sub_module->types[subtype_index].function_sig;
  const FunctionSig* super_func =
      super_module->types[supertype_index].function_sig;

  if (sub_func->parameter_count() != super_func->parameter_count() ||
      sub_func->return_count() != super_func->return_count()) {
    return false;
  }

  for (uint32_t i = 0; i < sub_func->parameter_count(); i++) {
    // Contravariance for params.
    if (!IsSubtypeOf(super_func->parameters()[i], sub_func->parameters()[i],
                     super_module, sub_module)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < sub_func->return_count(); i++) {
    // Covariance for returns.
    if (!IsSubtypeOf(sub_func->returns()[i], super_func->returns()[i],
                     sub_module, super_module)) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool ValidSubtypeDefinition(uint32_t subtype_index, uint32_t supertype_index,
                            const WasmModule* sub_module,
                            const WasmModule* super_module) {
  TypeDefinition::Kind sub_kind = sub_module->types[subtype_index].kind;
  TypeDefinition::Kind super_kind = super_module->types[supertype_index].kind;
  if (sub_kind != super_kind) return false;
  switch (sub_kind) {
    case TypeDefinition::kFunction:
      return ValidFunctionSubtypeDefinition(subtype_index, supertype_index,
                                            sub_module, super_module);
    case TypeDefinition::kStruct:
      return ValidStructSubtypeDefinition(subtype_index, supertype_index,
                                          sub_module, super_module);
    case TypeDefinition::kArray:
      return ValidArraySubtypeDefinition(subtype_index, supertype_index,
                                         sub_module, super_module);
  }
}

V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(
    ValueType subtype, ValueType supertype, const WasmModule* sub_module,
    const WasmModule* super_module) {
  DCHECK(subtype != supertype || sub_module != super_module);

  switch (subtype.kind()) {
    case kI32:
    case kI64:
    case kF32:
    case kF64:
    case kS128:
    case kI8:
    case kI16:
    case kVoid:
    case kBottom:
      return subtype == supertype;
    case kRtt:
      return supertype.kind() == kRtt &&
             EquivalentIndices(subtype.ref_index(), supertype.ref_index(),
                               sub_module, super_module);
    case kRef:
    case kOptRef:
      break;
  }

  DCHECK(subtype.is_object_reference());

  bool compatible_references = subtype.is_nullable()
                                   ? supertype.is_nullable()
                                   : supertype.is_object_reference();
  if (!compatible_references) return false;

  DCHECK(supertype.is_object_reference());

  // Now check that sub_heap and super_heap are subtype-related.

  HeapType sub_heap = subtype.heap_type();
  HeapType super_heap = supertype.heap_type();

  switch (sub_heap.representation()) {
    case HeapType::kFunc:
      // funcref is a subtype of anyref (aka externref) under wasm-gc.
      return sub_heap == super_heap ||
             (FLAG_experimental_wasm_gc && super_heap == HeapType::kAny);
    case HeapType::kEq:
      return sub_heap == super_heap || super_heap == HeapType::kAny;
    case HeapType::kAny:
      return super_heap == HeapType::kAny;
    case HeapType::kI31:
    case HeapType::kData:
      return super_heap == sub_heap || super_heap == HeapType::kEq ||
             super_heap == HeapType::kAny;
    case HeapType::kArray:
      return super_heap == HeapType::kArray || super_heap == HeapType::kData ||
             super_heap == HeapType::kEq || super_heap == HeapType::kAny;
    case HeapType::kBottom:
      UNREACHABLE();
    default:
      break;
  }

  DCHECK(sub_heap.is_index());
  uint32_t sub_index = sub_heap.ref_index();
  DCHECK(sub_module->has_type(sub_index));

  switch (super_heap.representation()) {
    case HeapType::kFunc:
      return sub_module->has_signature(sub_index);
    case HeapType::kEq:
    case HeapType::kData:
      return !sub_module->has_signature(sub_index);
    case HeapType::kArray:
      return sub_module->has_array(sub_index);
    case HeapType::kI31:
      return false;
    case HeapType::kAny:
      return true;
    case HeapType::kBottom:
      UNREACHABLE();
    default:
      break;
  }

  DCHECK(super_heap.is_index());
  uint32_t super_index = super_heap.ref_index();
  DCHECK(super_module->has_type(super_index));
  // The {IsSubtypeOf} entry point already has a fast path checking ValueType
  // equality; here we catch (ref $x) being a subtype of (ref null $x).
  if (sub_module == super_module && sub_index == super_index) return true;

  if (FLAG_wasm_type_canonicalization) {
    return GetTypeCanonicalizer()->IsCanonicalSubtype(sub_index, super_index,
                                                      sub_module, super_module);
  } else {
    uint32_t explicit_super = sub_module->supertype(sub_index);
    while (true) {
      if (explicit_super == super_index) return true;
      // Reached the end of the explicitly defined inheritance chain.
      if (explicit_super == kNoSuperType) return false;
      explicit_super = sub_module->supertype(explicit_super);
    }
  }
}

V8_NOINLINE bool EquivalentTypes(ValueType type1, ValueType type2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) {
  if (type1 == type2 && module1 == module2) return true;
  if (!type1.has_index()) return type1 == type2;
  if (type1.kind() != type2.kind()) return false;

  DCHECK(type1.has_index() && type2.has_index() &&
         (type1 != type2 || module1 != module2));

  DCHECK(type1.has_index() && module1->has_type(type1.ref_index()) &&
         type2.has_index() && module2->has_type(type2.ref_index()));

  return EquivalentIndices(type1.ref_index(), type2.ref_index(), module1,
                           module2);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
