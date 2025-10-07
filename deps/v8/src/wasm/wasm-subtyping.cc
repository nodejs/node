// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"

#include "src/wasm/canonical-types.h"
#include "src/wasm/wasm-module.h"

namespace v8::internal::wasm {

namespace {

V8_INLINE bool EquivalentIndices(ModuleTypeIndex index1, ModuleTypeIndex index2,
                                 const WasmModule* module) {
  if (index1 == index2) return true;
  return module->canonical_type_id(index1) == module->canonical_type_id(index2);
}

bool ValidStructSubtypeDefinition(ModuleTypeIndex subtype_index,
                                  ModuleTypeIndex supertype_index,
                                  const WasmModule* module) {
  const TypeDefinition& sub_def = module->type(subtype_index);
  const TypeDefinition& super_def = module->type(supertype_index);
  const StructType* sub_struct = sub_def.struct_type;
  const StructType* super_struct = super_def.struct_type;

  if (sub_struct->field_count() < super_struct->field_count()) {
    return false;
  }

  for (uint32_t i = 0; i < super_struct->field_count(); i++) {
    bool sub_mut = sub_struct->mutability(i);
    bool super_mut = super_struct->mutability(i);
    if (sub_mut != super_mut ||
        (sub_mut && !EquivalentTypes(sub_struct->field(i),
                                     super_struct->field(i), module)) ||
        (!sub_mut &&
         !IsSubtypeOf(sub_struct->field(i), super_struct->field(i), module))) {
      return false;
    }
  }
  if (sub_def.descriptor.valid()) {
    // If a type has a descriptor, its supertype must either have no descriptor,
    // or the supertype's descriptor must be a supertype of the subtype's
    // descriptor.
    if (super_def.descriptor.valid() &&
        !IsHeapSubtypeOf(module->heap_type(sub_def.descriptor),
                         module->heap_type(super_def.descriptor), module)) {
      return false;
    }
  } else {
    // If a type has no descriptor, its supertype must not have one either.
    if (super_def.descriptor.valid()) return false;
  }
  // A type and its supertype must either both be descriptors, or both not.
  if (sub_def.describes.valid() != super_def.describes.valid()) {
    return false;
  }
  return true;
}

bool ValidArraySubtypeDefinition(ModuleTypeIndex subtype_index,
                                 ModuleTypeIndex supertype_index,
                                 const WasmModule* module) {
  const ArrayType* sub_array = module->type(subtype_index).array_type;
  const ArrayType* super_array = module->type(supertype_index).array_type;
  bool sub_mut = sub_array->mutability();
  bool super_mut = super_array->mutability();

  return (sub_mut && super_mut &&
          EquivalentTypes(sub_array->element_type(),
                          super_array->element_type(), module)) ||
         (!sub_mut && !super_mut &&
          IsSubtypeOf(sub_array->element_type(), super_array->element_type(),
                      module));
}

bool ValidFunctionSubtypeDefinition(ModuleTypeIndex subtype_index,
                                    ModuleTypeIndex supertype_index,
                                    const WasmModule* module) {
  const FunctionSig* sub_func = module->type(subtype_index).function_sig;
  const FunctionSig* super_func = module->type(supertype_index).function_sig;

  if (sub_func->parameter_count() != super_func->parameter_count() ||
      sub_func->return_count() != super_func->return_count()) {
    return false;
  }

  for (uint32_t i = 0; i < sub_func->parameter_count(); i++) {
    // Contravariance for params.
    if (!IsSubtypeOf(super_func->parameters()[i], sub_func->parameters()[i],
                     module)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < sub_func->return_count(); i++) {
    // Covariance for returns.
    if (!IsSubtypeOf(sub_func->returns()[i], super_func->returns()[i],
                     module)) {
      return false;
    }
  }

  return true;
}

bool ValidContinuationSubtypeDefinition(ModuleTypeIndex subtype_index,
                                        ModuleTypeIndex supertype_index,
                                        const WasmModule* module) {
  const ContType* sub_cont = module->type(subtype_index).cont_type;
  const ContType* super_cont = module->type(supertype_index).cont_type;

  return IsHeapSubtypeOf(module->heap_type(sub_cont->contfun_typeindex()),
                         module->heap_type(super_cont->contfun_typeindex()),
                         module);
}

// For some purposes, we can treat all custom structs like the generic
// "structref", etc.
StandardType UpcastToStandardType(ValueTypeBase type) {
  if (!type.has_index()) return type.standard_type();
  switch (type.ref_type_kind()) {
    case RefTypeKind::kStruct:
      return StandardType::kStruct;
    case RefTypeKind::kArray:
      return StandardType::kArray;
    case RefTypeKind::kFunction:
      return StandardType::kFunc;
    case RefTypeKind::kCont:
      return StandardType::kCont;
    case RefTypeKind::kOther:
      UNREACHABLE();
  }
}

// Format: subtype, supertype
#define FOREACH_SUBTYPING(V) /*                               force 80 cols */ \
  /* anyref hierarchy */                                                       \
  V(Eq, Any)                                                                   \
  V(Struct, Eq)                                                                \
  V(None, Struct)                                                              \
  V(Array, Eq)                                                                 \
  V(None, Array)                                                               \
  V(I31, Eq)                                                                   \
  V(None, I31)                                                                 \
  V(String, Any)                                                               \
  V(None, String)                                                              \
  /* funcref hierarchy */                                                      \
  V(NoFunc, Func)                                                              \
  /* extern hierarchy */                                                       \
  V(ExternString, Extern)                                                      \
  V(NoExtern, ExternString)                                                    \
  /* exnref hierarchy */                                                       \
  V(NoExn, Exn)                                                                \
  /* cont hierarchy */                                                         \
  V(NoCont, Cont)

static constexpr uint32_t kNumStandardTypes =
    value_type_impl::kNumberOfStandardTypes;

static constexpr std::array<GenericKind, kNumStandardTypes>
    kValueTypeLookupMap = base::make_array<kNumStandardTypes>([](size_t i) {
#define ENTRY(name, ...)                                 \
  if (static_cast<size_t>(StandardType::k##name) == i) { \
    return GenericKind::k##name;                         \
  }
      FOREACH_GENERIC_TYPE(ENTRY)
#undef ENTRY
      // Numeric types fall through to here. We won't use these values.
      return GenericKind::kVoid;
    });
constexpr GenericKind ToGenericKind(StandardType type) {
  DCHECK_LT(static_cast<int>(type), static_cast<int>(StandardType::kI32));
  return kValueTypeLookupMap[static_cast<size_t>(type)];
}

////////////////////////////////////////////////////////////////////////////////
// Construct a dense index space for nontrivial subtypes, to save space in
// the 2D lookup maps we'll build for them.

constexpr int NotInSubtypeRelation(StandardType type) {
#define CASE(sub, super) \
  if (type == StandardType::k##sub || type == StandardType::k##super) return 0;
  FOREACH_SUBTYPING(CASE)
#undef CASE
  return 1;
}
static constexpr uint32_t kNumNonTrivial =
#define CASE(name, ...) (NotInSubtypeRelation(StandardType::k##name) ? 0 : 1) +
    FOREACH_GENERIC_TYPE(CASE)
// We could include numeric types here, but they don't have nontrivial
// subtyping relations anyway.
#undef CASE
        0;
enum class CondensedIndices : int {
// "kFooAdjustNext" is a dummy entry to control whether the next "kBar" will
// get its own unique value: when NotInSubtypeRelation(kFoo), then we'll end
// up with kFoo = N, kFooAdJustNext = N-1, kBar = N, and we'll never use
// kFoo later, so have effectively skipped it from the condensed space.
#define ENTRY(name, ...)    \
  k##name,                  \
      k##name##AdjustNext = \
          k##name - NotInSubtypeRelation(StandardType::k##name),
  FOREACH_GENERIC_TYPE(ENTRY)
#undef ENTRY
};
static constexpr uint8_t kNotRelatedSentinel = 0xFF;
static_assert(kNumStandardTypes < kNotRelatedSentinel);

constexpr uint8_t ComputeCondensedIndex(StandardType type) {
  switch (type) {
#define BAILOUT(name, ...) case StandardType::k##name:
    FOREACH_NUMERIC_VALUE_TYPE(BAILOUT)
    return kNotRelatedSentinel;
#undef BAILOUT
#define CASE(name, ...)                                         \
  case StandardType::k##name:                                   \
    if (NotInSubtypeRelation(type)) return kNotRelatedSentinel; \
    return static_cast<uint8_t>(CondensedIndices::k##name);
    FOREACH_GENERIC_TYPE(CASE)
#undef CASE
  }
}
constexpr StandardType ComputeStandardType(uint8_t condensed_index) {
#define CASE(name, ...)                                                \
  if (ComputeCondensedIndex(StandardType::k##name) == condensed_index) \
    return StandardType::k##name;
  FOREACH_GENERIC_TYPE(CASE)
#undef CASE
  UNREACHABLE();
}

static constexpr std::array<uint8_t, kNumStandardTypes>
    kCondensedIndexLookupMap =
        base::make_array<kNumStandardTypes>([](size_t i) {
          return ComputeCondensedIndex(static_cast<StandardType>(i));
        });
static constexpr std::array<uint8_t, kNumNonTrivial> kCondensedToStandardMap =
    base::make_array<kNumNonTrivial>(
        [](size_t i) { return static_cast<uint8_t>(ComputeStandardType(i)); });

constexpr uint8_t CondensedIndex(StandardType type) {
  return kCondensedIndexLookupMap[static_cast<size_t>(type)];
}
constexpr StandardType CondensedToStandard(uint8_t condensed) {
  return static_cast<StandardType>(kCondensedToStandardMap[condensed]);
}

////////////////////////////////////////////////////////////////////////////////
// Statically compute transitive subtype relations.

// Inputs are "condensed".
constexpr bool ComputeIsSubtype(size_t sub, size_t super) {
  if (sub == super) return true;
#define CASE(a, b)                                              \
  {                                                             \
    size_t raw_a = static_cast<size_t>(CondensedIndices::k##a); \
    size_t raw_b = static_cast<size_t>(CondensedIndices::k##b); \
    if (sub == raw_a) {                                         \
      if (super == raw_b) return true;                          \
      if (ComputeIsSubtype(raw_b, super)) return true;          \
    }                                                           \
  }
  FOREACH_SUBTYPING(CASE)
#undef CASE
  return false;
}

// A 2D map (type, type) -> bool indicating transitive subtype relationships.
// Keyed by "condensed" indices to save space.
static constexpr std::array<std::array<bool, kNumNonTrivial>, kNumNonTrivial>
    kSubtypeLookupMap2 = base::make_array<kNumNonTrivial>([](size_t sub) {
      return base::make_array<kNumNonTrivial>(
          [sub](size_t super) { return ComputeIsSubtype(sub, super); });
    });

// The "public" accessor for looking up subtyping of standard types.
constexpr bool SubtypeLookup(StandardType sub, StandardType super) {
  // Note: this implementation strikes a compromise between time and space.
  // We could put everything into the lookup map, but that would significantly
  // increase its size (at the time of this writing: by about 4x), with
  // mostly-sparse additional entries.
  if (sub == StandardType::kBottom) return true;
  if (super == StandardType::kTop) return true;
  uint8_t sub_condensed = CondensedIndex(sub);
  if (sub_condensed == kNotRelatedSentinel) return sub == super;
  uint8_t super_condensed = CondensedIndex(super);
  if (super_condensed == kNotRelatedSentinel) return false;
  return kSubtypeLookupMap2[sub_condensed][super_condensed];
}

////////////////////////////////////////////////////////////////////////////////
// Statically compute common ancestors.

// Inputs are "condensed", result is a full StandardType because it might
// be kTop.
constexpr StandardType ComputeCommonAncestor(size_t t1, size_t t2) {
  if (kSubtypeLookupMap2[t1][t2]) return CondensedToStandard(t2);
  if (kSubtypeLookupMap2[t2][t1]) return CondensedToStandard(t1);
#define CASE(a, b)                                                            \
  if (t1 == static_cast<size_t>(CondensedIndices::k##a)) {                    \
    return ComputeCommonAncestor(static_cast<size_t>(CondensedIndices::k##b), \
                                 t2);                                         \
  }
  FOREACH_SUBTYPING(CASE)
#undef CASE
  return StandardType::kTop;
}

// A 2D map (type, type) -> type caching the lowest common supertype.
// Keyed by "condensed" indices to save space.
static constexpr std::array<std::array<StandardType, kNumNonTrivial>,
                            kNumNonTrivial>
    kCommonAncestorLookupMap = base::make_array<kNumNonTrivial>([](size_t sub) {
      return base::make_array<kNumNonTrivial>(
          [sub](size_t super) { return ComputeCommonAncestor(sub, super); });
    });

// The "public" accessor for looking up common supertypes of standard types.
constexpr StandardType CommonAncestorLookup(StandardType t1, StandardType t2) {
  if (t1 == StandardType::kBottom) return t2;
  if (t2 == StandardType::kBottom) return t1;
  if (t1 == StandardType::kTop) return t1;
  if (t2 == StandardType::kTop) return t2;
  uint8_t t1_condensed = CondensedIndex(t1);
  if (t1_condensed == kNotRelatedSentinel) {
    return t2 == t1 ? t1 : StandardType::kTop;
  }
  uint8_t t2_condensed = CondensedIndex(t2);
  if (t2_condensed == kNotRelatedSentinel) return StandardType::kTop;
  return kCommonAncestorLookupMap[t1_condensed][t2_condensed];
}

////////////////////////////////////////////////////////////////////////////////
// Null-related things.

HeapType NullSentinelImpl(HeapType type) {
  StandardType standard = UpcastToStandardType(type);
  constexpr StandardType candidates[] = {
#define NULLTYPE(name, ...) StandardType::k##name,
      FOREACH_NONE_TYPE(NULLTYPE)
#undef NULLTYPE
  };
  for (StandardType candidate : candidates) {
    if (SubtypeLookup(candidate, standard)) {
      return HeapType::Generic(ToGenericKind(candidate), type.is_shared());
    }
  }
  if (type.is_string_view()) {
    // TODO(12868): This special case reflects unresolved discussion. If string
    // views aren't nullable, they shouldn't really show up here at all.
    return HeapType::Generic(GenericKind::kNone, type.is_shared());
  }
  UNREACHABLE();
}

bool IsNullSentinel(ValueTypeBase type) {
  DCHECK(!type.is_numeric());
  if (!type.is_abstract_ref()) return false;
  return IsNullKind(type.generic_kind());
}

bool IsGenericSubtypeOfIndexedTypes(ValueTypeBase type) {
  DCHECK(type.is_generic());
  GenericKind kind = type.generic_kind();
  return IsNullKind(kind) || kind == GenericKind::kBottom;
}

}  // namespace

bool ValidSubtypeDefinition(ModuleTypeIndex subtype_index,
                            ModuleTypeIndex supertype_index,
                            const WasmModule* module) {
  const TypeDefinition& subtype = module->type(subtype_index);
  const TypeDefinition& supertype = module->type(supertype_index);
  if (subtype.kind != supertype.kind) return false;
  if (supertype.is_final) return false;
  if (subtype.is_shared != supertype.is_shared) return false;
  switch (subtype.kind) {
    case TypeDefinition::kFunction:
      return ValidFunctionSubtypeDefinition(subtype_index, supertype_index,
                                            module);
    case TypeDefinition::kStruct:
      return ValidStructSubtypeDefinition(subtype_index, supertype_index,
                                          module);
    case TypeDefinition::kArray:
      return ValidArraySubtypeDefinition(subtype_index, supertype_index,
                                         module);
    case TypeDefinition::kCont:
      return ValidContinuationSubtypeDefinition(subtype_index, supertype_index,
                                                module);
  }
}

namespace {
// Common parts of the implementation for ValueType and CanonicalValueType.
std::optional<bool> IsSubtypeOf_CommonImpl(ValueTypeBase subtype,
                                           ValueTypeBase supertype) {
  DCHECK(!subtype.is_numeric() && !supertype.is_numeric());

  if (subtype.is_shared() != supertype.is_shared()) return false;
  if (subtype.is_bottom()) return true;
  if (supertype.has_index()) {
    if (subtype.has_index()) {
      // Only exact types can possibly be subtypes of other exact types.
      if (supertype.is_exact() && !subtype.is_exact()) return false;
      // If both types are indexed, the specialized implementations need to
      // take care of it.
      return {};
    }
    // Subtype is generic. It can only be a subtype if it is a none-type.
    if (!IsGenericSubtypeOfIndexedTypes(subtype)) return false;
  }
  return SubtypeLookup(UpcastToStandardType(subtype),
                       UpcastToStandardType(supertype));
}

}  // namespace

V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(
    HeapType subtype, HeapType supertype, const WasmModule* sub_module,
    const WasmModule* super_module) {
  DCHECK(subtype != supertype || sub_module != super_module);

  std::optional<bool> result = IsSubtypeOf_CommonImpl(subtype, supertype);
  if (result.has_value()) return result.value();
  DCHECK(subtype.has_index() && supertype.has_index());
  ModuleTypeIndex sub_index = subtype.ref_index();
  CanonicalTypeIndex super_canon =
      super_module->canonical_type_id(supertype.ref_index());
  // Comparing canonicalized type indices handles both different modules
  // and different recgroups in the same module.
  if (supertype.is_exact()) {
    return sub_module->canonical_type_id(sub_index) == super_canon;
  }
  do {
    if (sub_module->canonical_type_id(sub_index) == super_canon) return true;
    sub_index = sub_module->supertype(sub_index);
  } while (sub_index.valid());
  return false;
}

V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(
    ValueType subtype, ValueType supertype, const WasmModule* sub_module,
    const WasmModule* super_module) {
  // Note: it seems tempting to handle <top> and the numeric types
  // with the lookup-based mechanism we'll ultimately call. But that would
  // require guarding the checks for nullability and sharedness behind
  // checks for being reftypes, so it would end up being more complicated,
  // not less.
  if (supertype.is_top()) return true;
  if (subtype.is_numeric()) return subtype == supertype;
  if (supertype.is_numeric()) return subtype.is_bottom();
  if (subtype.is_nullable() && !supertype.is_nullable()) return false;
  HeapType sub_heap = subtype.heap_type();
  HeapType super_heap = supertype.heap_type();
  if (sub_heap == super_heap && sub_module == super_module) return true;
  return IsSubtypeOfImpl(sub_heap, super_heap, sub_module, super_module);
}

V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(
    CanonicalValueType subtype, CanonicalValueType supertype) {
  DCHECK_NE(subtype, supertype);  // Caller has checked.
  if (supertype.is_top()) return true;
  if (subtype.is_numeric()) return false;
  if (supertype.is_numeric()) return subtype.is_bottom();
  if (subtype.is_nullable() && !supertype.is_nullable()) return false;

  std::optional<bool> result = IsSubtypeOf_CommonImpl(subtype, supertype);
  if (result.has_value()) return result.value();
  DCHECK(subtype.has_index() && supertype.has_index());
  CanonicalTypeIndex sub_index = subtype.ref_index();
  CanonicalTypeIndex super_index = supertype.ref_index();
  // Can happen despite subtype != supertype, e.g. when nullability differs.
  if (sub_index == super_index) return true;
  if (supertype.is_exact()) return false;
  return GetTypeCanonicalizer()->IsHeapSubtype(sub_index, super_index);
}

V8_NOINLINE bool EquivalentTypes(ValueType type1, ValueType type2,
                                 const WasmModule* module) {
  if (type1 == type2) return true;
  if (!type1.has_index() || !type2.has_index()) return type1 == type2;
  if (type1.nullability() != type2.nullability()) return false;
  if (type1.is_exact() != type2.is_exact()) return false;

  DCHECK(type1.has_index() && module->has_type(type1.ref_index()) &&
         type2.has_index() && module->has_type(type2.ref_index()));

  return EquivalentIndices(type1.ref_index(), type2.ref_index(), module);
}

V8_NOINLINE bool EquivalentTypes(ValueType type1, ValueType type2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) {
  if (type1 == type2 && module1 == module2) return true;
  if (!type1.has_index() || !type2.has_index()) return type1 == type2;
  if (type1.nullability() != type2.nullability()) return false;
  if (type1.is_exact() != type2.is_exact()) return false;

  DCHECK(type1.has_index() && module1->has_type(type1.ref_index()) &&
         type2.has_index() && module2->has_type(type2.ref_index()));

  return module1->canonical_type_id(type1.ref_index()) ==
         module2->canonical_type_id(type2.ref_index());
}

namespace {
// Returns the least common ancestor of two type indices, as a type index in
// {module1}.
HeapType CommonAncestor(HeapType type1, HeapType type2,
                        const WasmModule* module) {
  DCHECK(type1.has_index() && type2.has_index());
  bool both_shared = type1.is_shared();
  if (both_shared != type2.is_shared()) return HeapType{kWasmTop};

  ModuleTypeIndex type_index1 = type1.ref_index();
  ModuleTypeIndex type_index2 = type2.ref_index();
  {
    int depth1 = GetSubtypingDepth(module, type_index1);
    int depth2 = GetSubtypingDepth(module, type_index2);
    while (depth1 > depth2) {
      type_index1 = module->supertype(type_index1);
      depth1--;
    }
    while (depth2 > depth1) {
      type_index2 = module->supertype(type_index2);
      depth2--;
    }
  }
  DCHECK_NE(type_index1, kNoSuperType);
  DCHECK_NE(type_index2, kNoSuperType);
  while (type_index1 != kNoSuperType &&
         !EquivalentIndices(type_index1, type_index2, module)) {
    type_index1 = module->supertype(type_index1);
    type_index2 = module->supertype(type_index2);
  }
  DCHECK_EQ(type_index1 == kNoSuperType, type_index2 == kNoSuperType);
  RefTypeKind kind1 = type1.ref_type_kind();
  if (type_index1 != kNoSuperType) {
    DCHECK_EQ(kind1, type2.ref_type_kind());
    return HeapType::Index(type_index1, both_shared, kind1);
  }
  // No indexed type was found as common ancestor, so we can treat both types
  // as generic.
  StandardType generic_ancestor = CommonAncestorLookup(
      UpcastToStandardType(type1), UpcastToStandardType(type2));
  return HeapType::Generic(ToGenericKind(generic_ancestor), both_shared);
}

// Returns the least common ancestor of an abstract heap type {type1}, and
// another heap type {type2}.
HeapType CommonAncestorWithAbstract(HeapType heap1, HeapType heap2) {
  DCHECK(heap1.is_abstract_ref());
  bool is_shared = heap1.is_shared();
  if (is_shared != heap2.is_shared()) return HeapType{kWasmTop};

  // If {heap2} is an indexed type, then {heap1} could be a subtype of it if
  // it is a none-type. In that case, {heap2} is the common ancestor.
  std::optional<bool> is_sub = IsSubtypeOf_CommonImpl(heap1, heap2);
  DCHECK(is_sub.has_value());  // Guaranteed by {heap1.is_abstract_ref()}.
  if (is_sub.value()) return heap2;

  // Otherwise, we can treat {heap2} like its generic supertype (e.g. any
  // indexed struct is "rounded up" to structref).
  StandardType generic_ancestor = CommonAncestorLookup(
      UpcastToStandardType(heap1), UpcastToStandardType(heap2));
  return HeapType::Generic(ToGenericKind(generic_ancestor), is_shared);
}

Exactness UnionExactness(ValueType type1, ValueType type2,
                         const WasmModule* module) {
  // The union of two types could be exact in these cases:
  // - if both types are exact and the same type index
  // - if one type is an exact indexed type, and the other is the matching
  //   none-type.
  if (type1.is_exact()) {
    if (type2.is_exact()) {
      bool same =
          EquivalentIndices(type1.ref_index(), type2.ref_index(), module);
      return same ? Exactness::kExact : Exactness::kAnySubtype;
    }
    // Possibly compatible, actual subtyping check will follow.
    if (IsNullSentinel(type2)) return Exactness::kExact;
  } else if (type2.is_exact()) {
    if (IsNullSentinel(type1)) return Exactness::kExact;
  }
  return Exactness::kAnySubtype;
}

}  // namespace

V8_EXPORT_PRIVATE TypeInModule Union(ValueType type1, ValueType type2,
                                     const WasmModule* module) {
  if (type1 == kWasmTop || type2 == kWasmTop) return {kWasmTop, module};
  if (type1 == kWasmBottom) return {type2, module};
  if (type2 == kWasmBottom) return {type1, module};
  if (!type1.is_ref() || !type2.is_ref()) {
    return {type1 == type2 ? type1 : kWasmTop, module};
  }
  Nullability nullability =
      type1.is_nullable() || type2.is_nullable() ? kNullable : kNonNullable;
  Exactness exactness = UnionExactness(type1, type2, module);
  HeapType heap1 = type1.heap_type();
  HeapType heap2 = type2.heap_type();
  if (heap1 == heap2) {
    return {type1.AsNullable(nullability).AsExactIfIndexed(exactness), module};
  }
  HeapType result_type = kWasmBottom;
  if (heap1.is_abstract_ref()) {
    result_type = CommonAncestorWithAbstract(heap1, heap2);
  } else if (heap2.is_abstract_ref()) {
    result_type = CommonAncestorWithAbstract(heap2, heap1);
  } else {
    result_type = CommonAncestor(heap1, heap2, module);
  }
  // The type could only be kBottom if the input was kBottom but any kBottom
  // HeapType should be "normalized" to kWasmBottom ValueType.
  DCHECK_NE(result_type, kWasmBottom);
  if (result_type.is_top()) return {kWasmTop, module};
  return {ValueType::RefMaybeNull(result_type, nullability).AsExact(exactness),
          module};
}

TypeInModule Intersection(ValueType type1, ValueType type2,
                          const WasmModule* module) {
  if (type1 == kWasmTop) return {type2, module};
  if (type2 == kWasmTop) return {type1, module};
  if (!type1.is_ref() || !type2.is_ref()) {
    return {type1 == type2 ? type1 : kWasmBottom, module};
  }
  Nullability nullability =
      type1.is_nullable() && type2.is_nullable() ? kNullable : kNonNullable;
  // non-nullable null type is not a valid type.
  if (nullability == kNonNullable &&
      (IsNullSentinel(type1) || IsNullSentinel(type2))) {
    return {kWasmBottom, module};
  }
  if (IsHeapSubtypeOf(type1.heap_type(), type2.heap_type(), module)) {
    return TypeInModule{type1.AsNullable(nullability), module};
  }
  if (IsHeapSubtypeOf(type2.heap_type(), type1.heap_type(), module)) {
    return TypeInModule{type2.AsNullable(nullability), module};
  }
  if (nullability == kNonNullable) {
    return {kWasmBottom, module};
  }
  // Check for common null representation.
  ValueType null_type1 = ToNullSentinel({type1, module});
  if (null_type1 == ToNullSentinel({type2, module})) {
    return {null_type1, module};
  }
  return {kWasmBottom, module};
}

ValueType ToNullSentinel(TypeInModule type) {
  HeapType null_heap = NullSentinelImpl(type.type.heap_type());
  DCHECK(IsHeapSubtypeOf(null_heap, type.type.heap_type(), type.module));
  return ValueType::RefNull(null_heap);
}

bool IsSameTypeHierarchy(HeapType type1, HeapType type2,
                         const WasmModule* module) {
  return NullSentinelImpl(type1) == NullSentinelImpl(type2);
}

}  // namespace v8::internal::wasm
