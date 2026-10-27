// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/ostreams.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-subtyping.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm {

// Introduce a separate representation for recursion groups to be used by this
// fuzz test.
namespace test {

constexpr std::array kAllGenericKinds = {
    GenericKind::kFunc,     GenericKind::kNoFunc, GenericKind::kAny,
    GenericKind::kEq,       GenericKind::kI31,    GenericKind::kStruct,
    GenericKind::kArray,    GenericKind::kNone,   GenericKind::kExtern,
    GenericKind::kNoExtern, GenericKind::kExn,    GenericKind::kNoExn};

// Provide operator<<(std::ostream&, T> for types with T.Print(std::ostream*).
template <typename T>
  requires requires(const T& t, std::ostream& os) { t.Print(os); }
std::ostream& operator<<(std::ostream& os, const T& t) {
  t.Print(os);
  return os;
}

struct SubType;

// Variance for type adaptation when deriving a subtype from a supertype.
enum class Variance { kInvariant, kCovariant, kContravariant };

// `reftype` in the spec (`ref null? heaptype`, where `heaptype` is either
// `absheaptype` or `typeidx`).
struct RefType {
  // For indexed heap types (`uint32_t`), the fuzzer domain initially chooses an
  // unconstrained index. `FixupRefType` will restrict this index to be within
  // [0, current_rec_group_end) and will potentially set `index_in_recgroup`.
  // `CanonicalizeIndexes` will replace the index by a canonical index if it
  // references a type from an earlier recgroup (i.e. if `index_in_recgroup` is
  // not set).
  std::variant<GenericKind, uint32_t> heap_type;
  SharedFlag shared;
  Nullability nullable;

  // `ref_type_kind` and `index_in_recgroup` will be set by `FixupRefType` for
  // indexed heap types.
  RefTypeKind ref_type_kind = RefTypeKind::kOther;
  std::optional<uint32_t> index_in_recgroup;

  // Construction only passes the first three fields.
  RefType(std::variant<GenericKind, uint32_t> heap_type, SharedFlag shared,
          Nullability nullable)
      : heap_type(heap_type), shared(shared), nullable(nullable) {}

  void FixupRefType(const std::vector<RefTypeKind>& available_ref_types,
                    uint32_t recgroup_start) {
    uint32_t* index = std::get_if<uint32_t>(&heap_type);
    if (index == nullptr) {
      // Abstract heap types never have a `RefTypeKind` or recgroup offset.
      DCHECK_EQ(RefTypeKind::kOther, ref_type_kind);
      DCHECK(!index_in_recgroup.has_value());
      return;
    }
    DCHECK(!available_ref_types.empty());
    // Restrict the index to anything until the end of the current rec group.
    *index %= base::checked_cast<uint32_t>(available_ref_types.size());
    // Populate the ref type kind, which is encoded in the `ValueType` returned
    // by `value_type()`.
    ref_type_kind = available_ref_types[*index];
    DCHECK(ref_type_kind == RefTypeKind::kFunction ||
           ref_type_kind == RefTypeKind::kStruct ||
           ref_type_kind == RefTypeKind::kArray);
    if (*index >= recgroup_start) {
      index_in_recgroup = *index - recgroup_start;
    } else {
      // `this` may have been copied from a supertype in an earlier recgroup;
      // clear any inherited intra-recgroup offset.
      index_in_recgroup.reset();
    }
  }

  void AdaptToSupertype(const RefType& super_ref, Variance variance,
                        const std::vector<RefTypeKind>& available_ref_types,
                        const std::vector<const SubType*>& previous_types);

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    if (index_in_recgroup.has_value()) return;
    if (uint32_t* index = std::get_if<uint32_t>(&heap_type)) {
      // Update the index to the canonical index from a previous recgroup.
      DCHECK_LT(*index, canonical_indexes.size());
      *index = canonical_indexes[*index];
    }
  }

  ValueType value_type() const {
    if (const uint32_t* index = std::get_if<uint32_t>(&heap_type)) {
      return ValueType::RefMaybeNull(ModuleTypeIndex{*index}, nullable, shared,
                                     ref_type_kind);
    }
    return ValueType::Generic(std::get<GenericKind>(heap_type), nullable,
                              shared);
  }

  void Print(std::ostream& os) const {
    if (nullable) os << "null ";
    if (shared) os << "shared ";
    if (index_in_recgroup.has_value()) {
      os << "recref " << index_in_recgroup.value();
    } else if (const uint32_t* index = std::get_if<uint32_t>(&heap_type)) {
      os << "ref " << *index;
    } else {
      os << "ref " << value_type().generic_heaptype_name();
    }
  }
};

// `numtype` or `vectype` in the spec.
struct NumType {
  ValueType type;

  void Fixup() {
    // Fall back to `i32` if the host CPU does not support SIMD, as the module
    // decoder rejects `v128` without hardware SIMD support.
    if (type == kWasmS128 && !CheckHardwareSupportsSimd()) {
      type = kWasmI32;
    }
  }

  void Print(std::ostream& os) const { os << type.name(); }

  ValueType value_type() const {
    DCHECK(type.is_numeric());
    return type;
  }
};

// `valtype` in the spec.
struct ValType : public std::variant<NumType, RefType> {
  explicit ValType(std::variant<NumType, RefType> v)
      : std::variant<NumType, RefType>(v) {}

  void FixupRefType(const std::vector<RefTypeKind>& available_ref_types,
                    uint32_t recgroup_start) {
    if (RefType* ref_type = std::get_if<RefType>(this)) {
      ref_type->FixupRefType(available_ref_types, recgroup_start);
    } else {
      std::get<NumType>(*this).Fixup();
    }
  }

  void AdaptToSupertype(const ValType& super_val, Variance variance,
                        const std::vector<RefTypeKind>& available_ref_types,
                        const std::vector<const SubType*>& previous_types) {
    const RefType* super_ref = std::get_if<RefType>(&super_val);
    RefType* sub_ref = std::get_if<RefType>(this);
    if (super_ref != nullptr && sub_ref != nullptr &&
        variance != Variance::kInvariant) {
      sub_ref->AdaptToSupertype(*super_ref, variance, available_ref_types,
                                previous_types);
      return;
    }
    // Numeric types and invariant positions must match the supertype's value
    // type; also fall back to copying if only one of the two is a `RefType`.
    *this = super_val;
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    if (RefType* ref_type = std::get_if<RefType>(this)) {
      ref_type->CanonicalizeIndexes(canonical_indexes);
    }
  }

  ValueType value_type() const {
    return std::visit([](auto& t) { return t.value_type(); }, *this);
  }

  void Print(std::ostream& os) const {
    std::visit([&os](auto& t) { t.Print(os); }, *this);
  }
};

// `fieldtype` in the spec.
struct FieldType {
  ValType val_type;
  bool mutability;

  void FixupRefType(const std::vector<RefTypeKind>& available_ref_types,
                    uint32_t recgroup_start) {
    val_type.FixupRefType(available_ref_types, recgroup_start);
  }

  void AdaptToSupertype(const FieldType& super_field,
                        const std::vector<RefTypeKind>& available_ref_types,
                        const std::vector<const SubType*>& previous_types) {
    mutability = super_field.mutability;
    // Mutable fields are invariant; immutable fields are covariant.
    Variance variance =
        mutability ? Variance::kInvariant : Variance::kCovariant;
    val_type.AdaptToSupertype(super_field.val_type, variance,
                              available_ref_types, previous_types);
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    val_type.CanonicalizeIndexes(canonical_indexes);
  }

  void Print(std::ostream& os) const {
    os << (mutability ? "mut " : "");
    std::visit([&os](auto& t) { os << t; }, val_type);
  }
};

// `structtype` in the spec.
struct StructType {
  std::vector<FieldType> field_types;

  void FixupRefTypes(const std::vector<RefTypeKind>& available_ref_types,
                     uint32_t recgroup_start) {
    for (FieldType& field_type : field_types) {
      field_type.FixupRefType(available_ref_types, recgroup_start);
    }
  }

  void AdaptToSupertype(const StructType& supertype,
                        const std::vector<RefTypeKind>& available_ref_types,
                        const std::vector<const SubType*>& previous_types) {
    // Struct subtyping allows depth subtyping on existing fields and width
    // subtyping by appending extra fields (which remain at the end of
    // `field_types` if `field_types.size() > supertype.field_types.size()`).
    size_t generated_fields =
        std::min(field_types.size(), supertype.field_types.size());
    for (size_t i = 0; i < generated_fields; ++i) {
      field_types[i].AdaptToSupertype(supertype.field_types[i],
                                      available_ref_types, previous_types);
    }
    if (field_types.size() < supertype.field_types.size()) {
      field_types.insert(field_types.end(),
                         supertype.field_types.begin() + generated_fields,
                         supertype.field_types.end());
    }
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    for (FieldType& field_type : field_types) {
      field_type.CanonicalizeIndexes(canonical_indexes);
    }
  }

  static RefTypeKind ref_type_kind() { return RefTypeKind::kStruct; }

  void BuildType(Zone* zone, WasmModuleBuilder* builder, bool is_final,
                 ModuleTypeIndex supertype) const {
    // If this fails, we need to constrain vector sizes in the fuzzer domains.
    DCHECK_GE(kV8MaxWasmStructFields, field_types.size());
    uint32_t field_count = static_cast<uint32_t>(field_types.size());
    // Offsets are not used and never accessed, hence we can pass nullptr.
    constexpr uint32_t* kNoOffsets = nullptr;
    ValueType* reps = zone->AllocateArray<ValueType>(field_count);
    bool* mutabilities = zone->AllocateArray<bool>(field_count);
    for (uint32_t i = 0; i < field_count; ++i) {
      reps[i] = field_types[i].val_type.value_type();
      mutabilities[i] = field_types[i].mutability;
    }
    bool is_descriptor = false;  // TODO(403372470): Add support.
    // TODO(42204563): Add support for SharedFlag{true}.
    builder->AddStructType(
        zone->New<wasm::StructType>(field_count, kNoOffsets, reps, mutabilities,
                                    is_descriptor, SharedFlag{false}),
        is_final, supertype);
  }

  void Print(std::ostream& os) const {
    os << "struct(" << PrintCollection(field_types).WithoutBrackets() << ")";
  }
};

// `arraytype` in the spec.
struct ArrayType {
  FieldType field_type;

  static RefTypeKind ref_type_kind() { return RefTypeKind::kArray; }

  void FixupRefTypes(const std::vector<RefTypeKind>& available_ref_types,
                     uint32_t recgroup_start) {
    field_type.val_type.FixupRefType(available_ref_types, recgroup_start);
  }

  void AdaptToSupertype(const ArrayType& supertype,
                        const std::vector<RefTypeKind>& available_ref_types,
                        const std::vector<const SubType*>& previous_types) {
    field_type.AdaptToSupertype(supertype.field_type, available_ref_types,
                                previous_types);
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    field_type.val_type.CanonicalizeIndexes(canonical_indexes);
  }

  void BuildType(Zone* zone, WasmModuleBuilder* builder, bool is_final,
                 ModuleTypeIndex supertype) const {
    builder->AddArrayType(
        zone->New<wasm::ArrayType>(field_type.val_type.value_type(),
                                   field_type.mutability, SharedFlag{false}),
        is_final, supertype);
  }

  void Print(std::ostream& os) const { os << "array(" << field_type << ")"; }
};

// `functype` in the spec.
struct FunctionType {
  std::vector<ValType> params;
  std::vector<ValType> returns;

  static RefTypeKind ref_type_kind() { return RefTypeKind::kFunction; }

  void FixupRefTypes(const std::vector<RefTypeKind>& available_ref_types,
                     uint32_t recgroup_start) {
    for (ValType& t : params) {
      t.FixupRefType(available_ref_types, recgroup_start);
    }
    for (ValType& t : returns) {
      t.FixupRefType(available_ref_types, recgroup_start);
    }
  }

  void AdaptToSupertype(const FunctionType& supertype,
                        const std::vector<RefTypeKind>& available_ref_types,
                        const std::vector<const SubType*>& previous_types) {
    // Function subtyping is contravariant in parameters and covariant in
    // returns, with matching arities.
    AdaptVector(params, supertype.params, Variance::kContravariant,
                available_ref_types, previous_types);
    AdaptVector(returns, supertype.returns, Variance::kCovariant,
                available_ref_types, previous_types);
  }

  static void AdaptVector(std::vector<ValType>& sub_vec,
                          const std::vector<ValType>& super_vec,
                          Variance variance,
                          const std::vector<RefTypeKind>& available_ref_types,
                          const std::vector<const SubType*>& previous_types) {
    // Adapt the overlapping prefix and resize to match the supertype's arity.
    size_t generated = std::min(sub_vec.size(), super_vec.size());
    for (size_t i = 0; i < generated; ++i) {
      sub_vec[i].AdaptToSupertype(super_vec[i], variance, available_ref_types,
                                  previous_types);
    }
    if (sub_vec.size() > super_vec.size()) {
      sub_vec.erase(sub_vec.begin() + super_vec.size(), sub_vec.end());
    } else if (sub_vec.size() < super_vec.size()) {
      sub_vec.insert(sub_vec.end(), super_vec.begin() + generated,
                     super_vec.end());
    }
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    for (ValType& t : params) t.CanonicalizeIndexes(canonical_indexes);
    for (ValType& t : returns) t.CanonicalizeIndexes(canonical_indexes);
  }

  void BuildType(Zone* zone, WasmModuleBuilder* builder, bool is_final,
                 ModuleTypeIndex supertype) const {
    FunctionSig::Builder sig_builder(zone, returns.size(), params.size());
    for (ValType param : params) sig_builder.AddParam(param.value_type());
    for (ValType ret : returns) sig_builder.AddReturn(ret.value_type());
    FunctionSig* sig = sig_builder.Get();
    builder->ForceAddSignature(sig, is_final, supertype);
  }

  void Print(std::ostream& os) const {
    os << "func params (" << PrintCollection(params).WithoutBrackets()
       << ") returns (" << PrintCollection(returns).WithoutBrackets() << ")";
  }
};

// `comptype` in the spec.
using CompType = std::variant<FunctionType, StructType, ArrayType>;

std::ostream& operator<<(std::ostream& os, const CompType& type) {
  // Call operator<< on the contained type.
  std::visit([&os](auto& t) { os << t; }, type);
  return os;
}

// `subtype` in the spec (`sub final? typeidx* comptype`).
struct SubType {
  CompType type;
  bool is_final;
  // The fuzzer domain initially chooses either `kNoSuperType.index` or an
  // unconstrained non-negative index.
  // `Fixup` will restrict this index to a valid earlier supertype index (or
  // reset it to `kNoSuperType.index` if none exists) and will potentially set
  // `supertype_in_recgroup`.
  // `CanonicalizeIndexes` will replace `supertype` by a canonical index if it
  // references a type from an earlier recgroup (i.e. if `supertype_in_recgroup`
  // is not set).
  uint32_t supertype;

  // `supertype_in_recgroup` and `subtyping_depth` will be set by `Fixup`.
  std::optional<uint32_t> supertype_in_recgroup;
  uint32_t subtyping_depth = 0;

  // Construction only passes the first three fields.
  SubType(CompType type, bool is_final, uint32_t supertype)
      : type(std::move(type)), is_final(is_final), supertype(supertype) {}

  RefTypeKind ref_type_kind() const {
    return std::visit([](const auto& t) { return t.ref_type_kind(); }, type);
  }

  void Fixup(const std::vector<RefTypeKind>& available_ref_types,
             const std::vector<const SubType*>& previous_types,
             uint32_t recgroup_start) {
    if (supertype != kNoSuperType.index) {
      // Collect all valid non-final earlier types of the same kind and within
      // the maximum subtyping depth.
      RefTypeKind kind = ref_type_kind();
      base::SmallVector<uint32_t, 16> candidates;
      for (uint32_t i = 0; i < previous_types.size(); ++i) {
        const SubType* cand = previous_types[i];
        if (!cand->is_final &&
            cand->subtyping_depth < kV8MaxRttSubtypingDepth &&
            cand->ref_type_kind() == kind) {
          candidates.push_back(i);
        }
      }
      if (candidates.empty()) {
        supertype = kNoSuperType.index;
      } else {
        supertype = candidates[supertype % candidates.size()];
        const SubType* super = previous_types[supertype];
        subtyping_depth = super->subtyping_depth + 1;
        if (supertype >= recgroup_start) {
          supertype_in_recgroup = supertype - recgroup_start;
        }
        // Adapt to the supertype before fixing up reference types so that any
        // fields/parameters/returns added by subtyping are also fixed up.
        std::visit(
            [&]<typename T>(T& t) {
              t.AdaptToSupertype(std::get<T>(super->type), available_ref_types,
                                 previous_types);
            },
            type);
      }
    }

    std::visit(
        [&](auto& t) { t.FixupRefTypes(available_ref_types, recgroup_start); },
        type);
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    if (supertype != kNoSuperType.index && !supertype_in_recgroup.has_value()) {
      // Update the supertype index to the canonical index from an earlier
      // recgroup.
      DCHECK_LT(supertype, canonical_indexes.size());
      supertype = canonical_indexes[supertype];
    }
    std::visit([&](auto& t) { t.CanonicalizeIndexes(canonical_indexes); },
               type);
  }

  void BuildType(Zone* zone, WasmModuleBuilder* builder) const {
    std::visit(
        [&](const auto& t) {
          t.BuildType(zone, builder, is_final, ModuleTypeIndex{supertype});
        },
        type);
  }

  void Print(std::ostream& os) const {
    if (is_final) os << "final ";
    if (supertype_in_recgroup.has_value()) {
      os << "sub rec " << supertype_in_recgroup.value() << " ";
    } else if (supertype != kNoSuperType.index) {
      os << "sub " << supertype << " ";
    }
    os << type;
  }
};

void RefType::AdaptToSupertype(
    const RefType& super_ref, Variance variance,
    const std::vector<RefTypeKind>& available_ref_types,
    const std::vector<const SubType*>& previous_types) {
  shared = super_ref.shared;
  // Non-nullable covariant references must remain non-nullable; nullable
  // contravariant references must remain nullable.
  if (variance == Variance::kCovariant &&
      super_ref.nullable == Nullability::kNonNullable) {
    nullable = Nullability::kNonNullable;
  } else if (variance == Variance::kContravariant &&
             super_ref.nullable == Nullability::kNullable) {
    nullable = Nullability::kNullable;
  }

  HeapType super_heap = super_ref.value_type().heap_type();
  if (uint32_t* sub_idx = std::get_if<uint32_t>(&heap_type)) {
    base::SmallVector<uint32_t, 8> candidates;
    if (const uint32_t* super_idx =
            std::get_if<uint32_t>(&super_ref.heap_type)) {
      candidates.push_back(*super_idx);
      if (variance == Variance::kCovariant) {
        // Collect all earlier types that transitively have `*super_idx` as a
        // supertype.
        for (uint32_t i = *super_idx + 1; i < previous_types.size(); ++i) {
          for (uint32_t cur = previous_types[i]->supertype;
               cur != kNoSuperType.index;
               cur = previous_types[cur]->supertype) {
            if (cur == *super_idx) {
              candidates.push_back(i);
              break;
            }
          }
        }
      } else {
        DCHECK_EQ(Variance::kContravariant, variance);
        // If `*super_idx` is not a forward reference within the supertype's
        // recgroup, collect all of its transitive supertypes.
        if (*super_idx < previous_types.size()) {
          for (uint32_t cur = previous_types[*super_idx]->supertype;
               cur != kNoSuperType.index;
               cur = previous_types[cur]->supertype) {
            candidates.push_back(cur);
          }
        }
      }
    } else {
      // `super_ref` has an abstract heap type; check which indexed types up to
      // the end of the current recgroup are valid subtypes/supertypes (e.g. a
      // struct/array/func type refining `any`, `eq`, `struct`, `array`, or
      // `func`).
      for (uint32_t i = 0; i < available_ref_types.size(); ++i) {
        HeapType cand_heap =
            HeapType::Index(ModuleTypeIndex{i}, shared, available_ref_types[i]);
        bool match = variance == Variance::kCovariant
                         ? IsHeapSubtypeOf(cand_heap, super_heap, nullptr)
                         : IsHeapSubtypeOf(super_heap, cand_heap, nullptr);
        if (match) candidates.push_back(i);
      }
    }
    if (!candidates.empty()) {
      *sub_idx = candidates[*sub_idx % candidates.size()];
      return;
    }
  }

  // Either `this` was generated as an abstract heap type, or no indexed type
  // matched (e.g. `super_ref` is `i31`, `none`, `extern`, or contravariant from
  // an abstract supertype). Pick a matching abstract heap type using a
  // deterministic seed derived from `this`.
  uint32_t seed = std::holds_alternative<uint32_t>(heap_type)
                      ? std::get<uint32_t>(heap_type)
                      : static_cast<uint32_t>(value_type().standard_type());
  base::SmallVector<GenericKind, 8> generic_candidates;
  for (GenericKind cand : kAllGenericKinds) {
    HeapType cand_heap = HeapType::Generic(cand, shared);
    bool match = variance == Variance::kCovariant
                     ? IsHeapSubtypeOf(cand_heap, super_heap, nullptr)
                     : IsHeapSubtypeOf(super_heap, cand_heap, nullptr);
    if (match) generic_candidates.push_back(cand);
  }
  DCHECK(!generic_candidates.empty());
  heap_type = generic_candidates[seed % generic_candidates.size()];
}

// `rectype` in the spec.
struct RecursionGroup {
  std::vector<SubType> types;
  // If {single_type} is false, this type will be outside any recursion group.
  // This is only allowed if {types.size() == 1}.
  bool single_type = false;

  void UpdateAvailableTypesAndFixup(
      std::vector<RefTypeKind>& available_ref_types,
      std::vector<const SubType*>& previous_types) {
    uint32_t recgroup_start =
        base::checked_cast<uint32_t>(available_ref_types.size());
    for (const SubType& type : types) {
      available_ref_types.push_back(type.ref_type_kind());
    }
    for (SubType& type : types) {
      type.Fixup(available_ref_types, previous_types, recgroup_start);
      previous_types.push_back(&type);
    }
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    for (SubType& type : types) {
      type.CanonicalizeIndexes(canonical_indexes);
    }
  }

  void BuildTypes(Zone* zone, WasmModuleBuilder* builder) const {
    if (single_type) {
      DCHECK_EQ(1, types.size());
      types[0].BuildType(zone, builder);
    } else {
      builder->StartRecursiveTypeGroup();
      for (const SubType& type : types) {
        type.BuildType(zone, builder);
      }
      builder->EndRecursiveTypeGroup();
    }
  }

  void Print(std::ostream& os) const {
    // Note: {single_type} is not included here because it makes no difference
    // for canonicalization.
    os << "recgroup(" << PrintCollection(types).WithoutBrackets() << ")";
  }
};

// A module with a number of types organized in recursion groups.
struct Module {
  std::vector<RecursionGroup> rec_groups;

  void Fixup() {
    std::vector<RefTypeKind> available_ref_types;
    std::vector<const SubType*> previous_types;
    for (RecursionGroup& rec_group : rec_groups) {
      rec_group.UpdateAvailableTypesAndFixup(available_ref_types,
                                             previous_types);
    }
  }

  void BuildTypes(Zone* zone, WasmModuleBuilder* builder) const {
    for (const RecursionGroup& rec_group : rec_groups) {
      rec_group.BuildTypes(zone, builder);
    }
  }
};

struct PrintRecgroupsWithSameId {
  const std::map<std::string, CanonicalTypeIndex>& canonical_types;
  CanonicalTypeIndex index;
};

std::ostream& operator<<(std::ostream& os, const PrintRecgroupsWithSameId& p) {
  for (auto& [str, id] : p.canonical_types) {
    if (id == p.index) os << "- " << str << "\n";
  }
  return os;
}

}  // namespace test

class TypeCanonicalizerTest
    : public fuzztest::PerFuzzTestFixtureAdapter<TestWithPlatform> {
 public:
  TypeCanonicalizerTest() : zone_(&allocator_, "TypeCanonicalizerTest") {}
  ~TypeCanonicalizerTest() override = default;

  void TestCanonicalization(const std::vector<test::Module>&);

 private:
  void Reset() {
    wasm::GetTypeCanonicalizer()->EmptyStorageForTesting();
    zone_.Reset();
  }

  AccountingAllocator allocator_;
  Zone zone_;
  const WasmEnabledFeatures enabled_features_ =
      WasmEnabledFeatures::FromFlags();
};

// FuzzTest domain construction.

static fuzztest::Domain<test::Module> ArbitraryModule() {
  // absheaptype
  auto abs_heap_type_domain = fuzztest::ElementOf(test::kAllGenericKinds);
  // heaptype ::= absheaptype | typeidx
  auto heap_type_domain = fuzztest::VariantOf(
      abs_heap_type_domain, fuzztest::InRange<uint32_t>(0, kMaxInt));
  // reftype ::= ref null? heaptype
  auto ref_type_domain = fuzztest::ConstructorOf<test::RefType>(
      heap_type_domain,
      // TODO(clemensb): Support shared types; the bit currently gets lost in
      // the module builder.
      /* shared */
      fuzztest::Just(SharedFlag{false}),  // fuzztest::Arbitrary<bool>(),
      /* nullability */
      fuzztest::ElementOf({Nullability::kNullable, Nullability::kNonNullable}));

  // numtype ::= i32 | i64 | f32 | f64
  // vectype ::= v128
  auto num_type_domain =
      fuzztest::StructOf<test::NumType>(fuzztest::ElementOf<ValueType>(
          {kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmS128}));
  // valtype ::= numtype | vectype | reftype
  auto val_type_domain = fuzztest::ConstructorOf<test::ValType>(
      fuzztest::VariantOf(num_type_domain, ref_type_domain));
  // resulttype ::= [vec(valtype)]
  auto result_type_domain = fuzztest::VectorOf(val_type_domain);

  // packedtype ::= i8 | i16
  auto packed_type_domain =
      fuzztest::ConstructorOf<test::ValType>(fuzztest::StructOf<test::NumType>(
          fuzztest::ElementOf<ValueType>({kWasmI8, kWasmI16})));
  // storagetype ::= valtype | packedtype
  auto storage_type_domain =
      fuzztest::OneOf(val_type_domain, packed_type_domain);

  // fieldtype ::= mut storagetype
  auto field_type_domain = fuzztest::StructOf<test::FieldType>(
      storage_type_domain, /* mutability */ fuzztest::Arbitrary<bool>());
  // structtype ::= fieldtype*
  auto struct_type_domain = fuzztest::StructOf<test::StructType>(
      fuzztest::VectorOf(field_type_domain));

  // arraytype ::= fieldtype
  auto array_type_domain =
      fuzztest::StructOf<test::ArrayType>(field_type_domain);

  // functype ::= resulttype -> resulttype
  auto function_type_domain = fuzztest::StructOf<test::FunctionType>(
      result_type_domain, result_type_domain);

  // comptype ::= func functype | struct structtype | array arraytype
  auto comptype_domain = fuzztest::VariantOf<test::CompType>(
      function_type_domain, struct_type_domain, array_type_domain);

  // subtype ::= sub final? typeidx* comptype
  auto subtype_domain = fuzztest::ConstructorOf<test::SubType>(
      comptype_domain,
      /* is_final */ fuzztest::Arbitrary<bool>(),
      /* supertype */
      fuzztest::OneOf(fuzztest::Just(kNoSuperType.index),
                      fuzztest::InRange<uint32_t>(0, kMaxInt)));

  // rectype ::= rec subtype*
  auto recgroup_domain = fuzztest::OneOf(
      // A single type declared outside any recursion group.
      fuzztest::StructOf<test::RecursionGroup>(
          fuzztest::VectorOf(subtype_domain).WithSize(1), fuzztest::Just(true)),
      // An actual recursion group of arbitrary size.
      fuzztest::StructOf<test::RecursionGroup>(
          fuzztest::VectorOf(subtype_domain), fuzztest::Just(false)));

  auto module_domain =
      fuzztest::StructOf<test::Module>(fuzztest::VectorOf(recgroup_domain));
  return module_domain;
}

// Fuzz tests.

void TypeCanonicalizerTest::TestCanonicalization(
    const std::vector<test::Module>& test_modules) {
  // For each test, reset the type canonicalizer such that individual inputs are
  // independent of each other.
  Reset();

  // Keep a map of all recgroups in all modules to check that canonicalization
  // works as expected. The key is a text representation of the respective type
  // or recursion group; we expect same text to mean identical group.
  std::map<std::string, CanonicalTypeIndex> canonical_types;
  // Also remember all seen canonical type IDs so we can check that we don't
  // accidentally canonicalize different types to the same index.
  std::unordered_set<CanonicalTypeIndex, base::hash<CanonicalTypeIndex>>
      seen_canonical_indexes;

  // Note: We make a copy of the module here intentionally. We need to modify
  // the internal representation to fix up ref type indexes but Fuzztest is
  // handing us a const reference only.
  for (test::Module test_module : test_modules) {
    // The `canonical_indexes` vector will be populated when processing the
    // individual rec groups. It contains the canonical IDs of all previously
    // processed types.
    // Note: This vector is pretty identical to
    // `WasmModule::isorecursive_canonical_type_ids`, but
    // 1) it's filled rec-group by rec-group, and
    // 2) we want to avoid relying on `WasmModule` internals.
    std::vector<uint32_t> canonical_indexes;

    // After module generation by the fuzzer, fix up ref types and supertypes
    // to restrict indexes to the valid index space and ensure valid subtyping.
    test_module.Fixup();

    WasmModuleBuilder builder(&zone_);
    test_module.BuildTypes(&zone_, &builder);
    ZoneBuffer buffer{&zone_};
    builder.WriteTo(&buffer);

    WasmDetectedFeatures detected_features;
    bool kValidateModule = true;
    ModuleResult result =
        DecodeWasmModule(enabled_features_, base::VectorOf(buffer),
                         kValidateModule, &detected_features);

    // If this fails due to too many types, we need to constrain vector sizes in
    // the fuzzer domains.
    ASSERT_TRUE(result.ok()) << result.error().message();
    std::shared_ptr<WasmModule> module = std::move(result).value();
    size_t total_types = 0;
    for (const test::RecursionGroup& rec_group : test_module.rec_groups) {
      total_types += rec_group.types.size();
    }
    ASSERT_EQ(module->types.size(), total_types);

    size_t num_previous_types = 0;
    for (test::RecursionGroup& rec_group : test_module.rec_groups) {
      // Skip empty recursion groups; they do not get a canonical ID assigned,
      // so we cannot check anything for them (except that they do not confuse
      // canonicalization of surrounding types or groups).
      if (rec_group.types.empty()) continue;

      DCHECK_EQ(seen_canonical_indexes.size(), canonical_types.size());
      DCHECK_EQ(num_previous_types, canonical_indexes.size());
      rec_group.CanonicalizeIndexes(canonical_indexes);

      // The total number of types must be within kV8MaxWasmTypes.
      ASSERT_GE(kMaxUInt32, num_previous_types);
      ModuleTypeIndex first_type_id{
          base::checked_cast<uint32_t>(num_previous_types)};
      num_previous_types += rec_group.types.size();
      CanonicalTypeIndex first_canonical_id =
          module->canonical_type_id(first_type_id);

      for (uint32_t i = 1; i < rec_group.types.size(); ++i) {
        // Canonical IDs are consecutive within the recursion group.
        ASSERT_EQ(CanonicalTypeIndex{first_canonical_id.index + i},
                  module->canonical_type_id(
                      ModuleTypeIndex{first_type_id.index + i}));
      }
      for (uint32_t i = 0; i < rec_group.types.size(); ++i) {
        CanonicalTypeIndex canonical_id{first_canonical_id.index + i};
        // This canonical ID can be used by successive recgroups.
        canonical_indexes.push_back(canonical_id.index);

        // Verify that the canonicalized subtype is recognized as a heap subtype
        // of its canonical supertype (and not vice versa).
        const test::SubType& subtype = rec_group.types[i];
        if (subtype.supertype != kNoSuperType.index) {
          CanonicalTypeIndex canonical_supertype{
              subtype.supertype_in_recgroup.has_value()
                  ? first_canonical_id.index +
                        subtype.supertype_in_recgroup.value()
                  : subtype.supertype};
          ASSERT_TRUE(GetTypeCanonicalizer()->IsHeapSubtype(
              canonical_id, canonical_supertype));
          ASSERT_FALSE(GetTypeCanonicalizer()->IsHeapSubtype(
              canonical_supertype, canonical_id));
        }
      }
      std::string recgroup_str = (std::ostringstream{} << rec_group).str();
      auto [it, added] = canonical_types.insert(
          std::make_pair(recgroup_str, first_canonical_id));
      if (added) {
        // If this is a new recgroup, check that it gets a unique canonical ID
        // (we do not accidentally canonicalize different types).
        ASSERT_TRUE(seen_canonical_indexes.insert(first_canonical_id).second)
            << "A new type should get a new canonical ID assigned.\n"
            << "Recgroups with the same ID (" << first_canonical_id << "):\n"
            << test::PrintRecgroupsWithSameId{canonical_types,
                                              first_canonical_id};
      } else {
        // If this recgroup equals an existing one, we should get the same
        // canonical ID.
        ASSERT_EQ(it->second, first_canonical_id)
            << "Identical recgroups should get canonicalized:\n"
            << recgroup_str;
      }
    }
  }
}

V8_FUZZ_TEST_F(TypeCanonicalizerTest, TestCanonicalization)
    .WithDomains(fuzztest::VectorOf(ArbitraryModule()));

}  // namespace v8::internal::wasm
