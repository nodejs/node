// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"

#include "src/base/platform/mutex.h"
#include "src/wasm/wasm-module.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

using CacheKey =
    std::tuple<uint32_t, uint32_t, const WasmModule*, const WasmModule*>;

struct CacheKeyHasher {
  size_t operator()(CacheKey key) const {
    static constexpr size_t large_prime = 14887;
    return std::get<0>(key) + (std::get<1>(key) * large_prime) +
           (reinterpret_cast<size_t>(std::get<2>(key)) * large_prime *
            large_prime) +
           (reinterpret_cast<size_t>(std::get<3>(key)) * large_prime *
            large_prime * large_prime);
  }
};

class TypeJudgementCache {
 public:
  TypeJudgementCache()
      : zone_(new AccountingAllocator(), "type judgement zone"),
        subtyping_cache_(&zone_),
        type_equivalence_cache_(&zone_) {}

  static TypeJudgementCache* instance() {
    static base::LazyInstance<TypeJudgementCache>::type instance_ =
        LAZY_INSTANCE_INITIALIZER;
    return instance_.Pointer();
  }

  base::RecursiveMutex* type_cache_mutex() { return &type_cache_mutex_; }
  bool is_cached_subtype(uint32_t subtype, uint32_t supertype,
                         const WasmModule* sub_module,
                         const WasmModule* super_module) const {
    return subtyping_cache_.count(std::make_tuple(
               subtype, supertype, sub_module, super_module)) == 1;
  }
  void cache_subtype(uint32_t subtype, uint32_t supertype,
                     const WasmModule* sub_module,
                     const WasmModule* super_module) {
    subtyping_cache_.emplace(subtype, supertype, sub_module, super_module);
  }
  void uncache_subtype(uint32_t subtype, uint32_t supertype,
                       const WasmModule* sub_module,
                       const WasmModule* super_module) {
    subtyping_cache_.erase(
        std::make_tuple(subtype, supertype, sub_module, super_module));
  }
  bool is_cached_equivalent_type(uint32_t type1, uint32_t type2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) const {
    if (type1 > type2) std::swap(type1, type2);
    if (reinterpret_cast<uintptr_t>(module1) >
        reinterpret_cast<uintptr_t>(module2)) {
      std::swap(module1, module2);
    }
    return type_equivalence_cache_.count(
               std::make_tuple(type1, type2, module1, module2)) == 1;
  }
  void cache_type_equivalence(uint32_t type1, uint32_t type2,
                              const WasmModule* module1,
                              const WasmModule* module2) {
    if (type1 > type2) std::swap(type1, type2);
    if (reinterpret_cast<uintptr_t>(module1) >
        reinterpret_cast<uintptr_t>(module2)) {
      std::swap(module1, module2);
    }
    type_equivalence_cache_.emplace(type1, type2, module1, module2);
  }
  void uncache_type_equivalence(uint32_t type1, uint32_t type2,
                                const WasmModule* module1,
                                const WasmModule* module2) {
    if (type1 > type2) std::swap(type1, type2);
    if (reinterpret_cast<uintptr_t>(module1) >
        reinterpret_cast<uintptr_t>(module2)) {
      std::swap(module1, module2);
    }
    type_equivalence_cache_.erase(
        std::make_tuple(type1, type2, module1, module2));
  }

 private:
  Zone zone_;
  ZoneUnorderedSet<CacheKey, CacheKeyHasher>
      // Cache for discovered subtyping pairs.
      subtyping_cache_,
      // Cache for discovered equivalent type pairs.
      // Indexes and modules are stored in increasing order.
      type_equivalence_cache_;
  // The above two caches are used from background compile jobs, so they
  // must be protected from concurrent modifications:
  base::RecursiveMutex type_cache_mutex_;
};

bool ArrayEquivalentIndices(uint32_t type_index_1, uint32_t type_index_2,
                            const WasmModule* module1,
                            const WasmModule* module2) {
  const ArrayType* sub_array = module1->types[type_index_1].array_type;
  const ArrayType* super_array = module2->types[type_index_2].array_type;
  if (sub_array->mutability() != super_array->mutability()) return false;

  // Temporarily cache type equivalence for the recursive call.
  TypeJudgementCache::instance()->cache_type_equivalence(
      type_index_1, type_index_2, module1, module2);
  if (EquivalentTypes(sub_array->element_type(), super_array->element_type(),
                      module1, module2)) {
    return true;
  } else {
    TypeJudgementCache::instance()->uncache_type_equivalence(
        type_index_1, type_index_2, module1, module2);
    // TODO(7748): Consider caching negative results as well.
    return false;
  }
}

bool StructEquivalentIndices(uint32_t type_index_1, uint32_t type_index_2,
                             const WasmModule* module1,
                             const WasmModule* module2) {
  const StructType* sub_struct = module1->types[type_index_1].struct_type;
  const StructType* super_struct = module2->types[type_index_2].struct_type;

  if (sub_struct->field_count() != super_struct->field_count()) {
    return false;
  }

  // Temporarily cache type equivalence for the recursive call.
  TypeJudgementCache::instance()->cache_type_equivalence(
      type_index_1, type_index_2, module1, module2);
  for (uint32_t i = 0; i < sub_struct->field_count(); i++) {
    if (sub_struct->mutability(i) != super_struct->mutability(i) ||
        !EquivalentTypes(sub_struct->field(i), super_struct->field(i), module1,
                         module2)) {
      TypeJudgementCache::instance()->uncache_type_equivalence(
          type_index_1, type_index_2, module1, module2);
      return false;
    }
  }
  return true;
}

bool FunctionEquivalentIndices(uint32_t type_index_1, uint32_t type_index_2,
                               const WasmModule* module1,
                               const WasmModule* module2) {
  const FunctionSig* sig1 = module1->types[type_index_1].function_sig;
  const FunctionSig* sig2 = module2->types[type_index_2].function_sig;

  if (sig1->parameter_count() != sig2->parameter_count() ||
      sig1->return_count() != sig2->return_count()) {
    return false;
  }

  auto iter1 = sig1->all();
  auto iter2 = sig2->all();

  // Temporarily cache type equivalence for the recursive call.
  TypeJudgementCache::instance()->cache_type_equivalence(
      type_index_1, type_index_2, module1, module2);
  for (int i = 0; i < iter1.size(); i++) {
    if (!EquivalentTypes(iter1[i], iter2[i], module1, module2)) {
      TypeJudgementCache::instance()->uncache_type_equivalence(
          type_index_1, type_index_2, module1, module2);
      return false;
    }
  }
  return true;
}

V8_INLINE bool EquivalentIndices(uint32_t index1, uint32_t index2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) {
  DCHECK(index1 != index2 || module1 != module2);
  uint8_t kind1 = module1->type_kinds[index1];

  if (kind1 != module2->type_kinds[index2]) return false;

  base::RecursiveMutexGuard type_cache_access(
      TypeJudgementCache::instance()->type_cache_mutex());
  if (TypeJudgementCache::instance()->is_cached_equivalent_type(
          index1, index2, module1, module2)) {
    return true;
  }

  if (kind1 == kWasmStructTypeCode) {
    return StructEquivalentIndices(index1, index2, module1, module2);
  } else if (kind1 == kWasmArrayTypeCode) {
    return ArrayEquivalentIndices(index1, index2, module1, module2);
  } else {
    DCHECK_EQ(kind1, kWasmFunctionTypeCode);
    return FunctionEquivalentIndices(index1, index2, module1, module2);
  }
}

bool StructIsSubtypeOf(uint32_t subtype_index, uint32_t supertype_index,
                       const WasmModule* sub_module,
                       const WasmModule* super_module) {
  const StructType* sub_struct = sub_module->types[subtype_index].struct_type;
  const StructType* super_struct =
      super_module->types[supertype_index].struct_type;

  if (sub_struct->field_count() < super_struct->field_count()) {
    return false;
  }

  TypeJudgementCache::instance()->cache_subtype(subtype_index, supertype_index,
                                                sub_module, super_module);
  for (uint32_t i = 0; i < super_struct->field_count(); i++) {
    bool sub_mut = sub_struct->mutability(i);
    bool super_mut = super_struct->mutability(i);
    if (sub_mut != super_mut ||
        (sub_mut &&
         !EquivalentTypes(sub_struct->field(i), super_struct->field(i),
                          sub_module, super_module)) ||
        (!sub_mut && !IsSubtypeOf(sub_struct->field(i), super_struct->field(i),
                                  sub_module, super_module))) {
      TypeJudgementCache::instance()->uncache_subtype(
          subtype_index, supertype_index, sub_module, super_module);
      return false;
    }
  }
  return true;
}

bool ArrayIsSubtypeOf(uint32_t subtype_index, uint32_t supertype_index,
                      const WasmModule* sub_module,
                      const WasmModule* super_module) {
  const ArrayType* sub_array = sub_module->types[subtype_index].array_type;
  const ArrayType* super_array =
      super_module->types[supertype_index].array_type;
  bool sub_mut = sub_array->mutability();
  bool super_mut = super_array->mutability();
  TypeJudgementCache::instance()->cache_subtype(subtype_index, supertype_index,
                                                sub_module, super_module);
  if (sub_mut != super_mut ||
      (sub_mut &&
       !EquivalentTypes(sub_array->element_type(), super_array->element_type(),
                        sub_module, super_module)) ||
      (!sub_mut &&
       !IsSubtypeOf(sub_array->element_type(), super_array->element_type(),
                    sub_module, super_module))) {
    TypeJudgementCache::instance()->uncache_subtype(
        subtype_index, supertype_index, sub_module, super_module);
    return false;
  } else {
    return true;
  }
}

// TODO(7748): Expand this with function subtyping when it is introduced.
bool FunctionIsSubtypeOf(uint32_t subtype_index, uint32_t supertype_index,
                         const WasmModule* sub_module,
                         const WasmModule* super_module) {
  return FunctionEquivalentIndices(subtype_index, supertype_index, sub_module,
                                   super_module);
}

}  // namespace

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
    case kRttWithDepth:
      return (supertype.kind() == kRtt &&
              ((sub_module == super_module &&
                subtype.ref_index() == supertype.ref_index()) ||
               EquivalentIndices(subtype.ref_index(), supertype.ref_index(),
                                 sub_module, super_module))) ||
             (supertype.kind() == kRttWithDepth &&
              supertype.depth() == subtype.depth() &&
              EquivalentIndices(subtype.ref_index(), supertype.ref_index(),
                                sub_module, super_module));
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
    case HeapType::kExtern:
    case HeapType::kEq:
      return sub_heap == super_heap || super_heap == HeapType::kAny;
    case HeapType::kAny:
      return super_heap == HeapType::kAny;
    case HeapType::kI31:
    case HeapType::kData:
      return super_heap == sub_heap || super_heap == HeapType::kEq ||
             super_heap == HeapType::kAny;
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
    case HeapType::kExtern:
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

  uint8_t sub_kind = sub_module->type_kinds[sub_index];

  if (sub_kind != super_module->type_kinds[super_index]) return false;

  // Accessing the caches for subtyping and equivalence from multiple background
  // threads is protected by a lock.
  base::RecursiveMutexGuard type_cache_access(
      TypeJudgementCache::instance()->type_cache_mutex());
  if (TypeJudgementCache::instance()->is_cached_subtype(
          sub_index, super_index, sub_module, super_module)) {
    return true;
  }

  if (sub_kind == kWasmStructTypeCode) {
    return StructIsSubtypeOf(sub_index, super_index, sub_module, super_module);
  } else if (sub_kind == kWasmArrayTypeCode) {
    return ArrayIsSubtypeOf(sub_index, super_index, sub_module, super_module);
  } else {
    DCHECK_EQ(sub_kind, kWasmFunctionTypeCode);
    return FunctionIsSubtypeOf(sub_index, super_index, sub_module,
                               super_module);
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

  DCHECK_IMPLIES(type1.has_depth(), type2.has_depth());  // Due to 'if' above.
  if (type1.has_depth() && type1.depth() != type2.depth()) return false;

  DCHECK(type1.has_index() && module1->has_type(type1.ref_index()) &&
         type2.has_index() && module2->has_type(type2.ref_index()));

  return EquivalentIndices(type1.ref_index(), type2.ref_index(), module1,
                           module2);
}

ValueType CommonSubtype(ValueType a, ValueType b, const WasmModule* module) {
  if (a == b) return a;
  if (IsSubtypeOf(a, b, module)) return a;
  if (IsSubtypeOf(b, a, module)) return b;
  return kWasmBottom;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
