// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/ostreams.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module-builder.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm {

// Introduce a separate representation for recursion groups to be used by this
// fuzz test.
namespace test {

// Provide operator<<(std::ostream&, T> for types with T.Print(std::ostream*).
template <typename T>
  requires requires(const T& t, std::ostream& os) { t.Print(os); }
std::ostream& operator<<(std::ostream& os, const T& t) {
  t.Print(os);
  return os;
}

// `reftype` in the spec.
struct RefType {
  // The fuzzer domain initially chooses an unconstrained `index`.
  // `FixupRefTypes` will restrict this index to be within [0,
  // current_rec_group_end) and will potentially set `index_in_recgroup`.
  // `CanonicalizeIndexes` will replace `index` by a canonical index if it
  // references a type from an earlier recgroup (i.e. if `index_in_recgroup` is
  // not set).
  uint32_t index;
  bool shared;
  Nullability nullable;

  // `ref_type_kind` and `index_in_recgroup` will be set by `FixupRefType`.
  RefTypeKind ref_type_kind;
  std::optional<uint32_t> index_in_recgroup;

  // Construction only passes the first three fields.
  RefType(uint32_t index, bool shared, Nullability nullable)
      : index(index), shared(shared), nullable(nullable) {}

  void FixupRefType(const std::vector<RefTypeKind>& available_ref_types,
                    uint32_t recgroup_start) {
    DCHECK(!available_ref_types.empty());
    // Restrict the index to anything until the end of the current rec group.
    index %= base::checked_cast<uint32_t>(available_ref_types.size());
    // Populate the ref type kind, which is encoded in the `ValueType` returned
    // by `value_type()`.
    ref_type_kind = available_ref_types[index];
    DCHECK(ref_type_kind == RefTypeKind::kFunction ||
           ref_type_kind == RefTypeKind::kStruct ||
           ref_type_kind == RefTypeKind::kArray);
    if (index >= recgroup_start) index_in_recgroup = index - recgroup_start;
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    if (index_in_recgroup.has_value()) return;
    // Update the index to the canonical index from a previous recgroup.
    DCHECK_LT(index, canonical_indexes.size());
    index = canonical_indexes[index];
  }

  ValueType value_type() const {
    return ValueType::RefMaybeNull(ModuleTypeIndex{index}, nullable, shared,
                                   ref_type_kind);
  }

  void Print(std::ostream& os) const {
    if (nullable) os << "null ";
    if (shared) os << "shared ";
    if (index_in_recgroup.has_value()) {
      os << "recref " << index_in_recgroup.value();
    } else {
      os << "ref " << index;
    }
  }
};

// `numtype` in the spec.
struct NumType {
  ValueType type;

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
    if (auto* ref_type = std::get_if<RefType>(this)) {
      ref_type->FixupRefType(available_ref_types, recgroup_start);
    }
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    if (auto* ref_type = std::get_if<RefType>(this)) {
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

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    for (FieldType& field_type : field_types) {
      field_type.CanonicalizeIndexes(canonical_indexes);
    }
  }

  static RefTypeKind ref_type_kind() { return RefTypeKind::kStruct; }

  void BuildType(Zone* zone, WasmModuleBuilder* builder) const {
    // TODO(381687256): Populate final and supertype.
    constexpr bool kNotFinal = false;
    constexpr ModuleTypeIndex kNoSupertype = ModuleTypeIndex::Invalid();
    // If this fails, we need to constrain vector sizes in the fuzzer domains.
    DCHECK_GE(kMaxUInt32, field_types.size());
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
    bool is_shared = false;      // TODO(42204563): Add support.
    builder->AddStructType(
        zone->New<wasm::StructType>(field_count, kNoOffsets, reps, mutabilities,
                                    is_descriptor, is_shared),
        kNotFinal, kNoSupertype);
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

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    field_type.val_type.CanonicalizeIndexes(canonical_indexes);
  }

  void BuildType(Zone* zone, WasmModuleBuilder* builder) const {
    // TODO(381687256): Populate final and supertype.
    constexpr bool kNotFinal = false;
    constexpr ModuleTypeIndex kNoSupertype = ModuleTypeIndex::Invalid();
    builder->AddArrayType(
        zone->New<wasm::ArrayType>(field_type.val_type.value_type(),
                                   field_type.mutability),
        kNotFinal, kNoSupertype);
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

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    for (ValType& t : params) t.CanonicalizeIndexes(canonical_indexes);
    for (ValType& t : returns) t.CanonicalizeIndexes(canonical_indexes);
  }

  void BuildType(Zone* zone, WasmModuleBuilder* builder) const {
    // TODO(381687256): Populate final and supertype.
    constexpr bool kNotFinal = false;
    constexpr ModuleTypeIndex kNoSupertype = ModuleTypeIndex::Invalid();
    FunctionSig::Builder sig_builder(zone, returns.size(), params.size());
    for (ValType param : params) sig_builder.AddParam(param.value_type());
    for (ValType ret : returns) sig_builder.AddReturn(ret.value_type());
    FunctionSig* sig = sig_builder.Get();
    builder->ForceAddSignature(sig, kNotFinal, kNoSupertype);
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

// `rectype` in the spec.
struct RecursionGroup {
  std::vector<CompType> types;
  // If {single_type} is false, this type will be outside any recursion group.
  // This is only allowed if {types.size() == 1}.
  bool single_type = false;

  void UpdateAvailableRefTypesAndFixupRefTypes(
      std::vector<RefTypeKind>& available_ref_types) {
    uint32_t recgroup_start =
        base::checked_cast<uint32_t>(available_ref_types.size());
    for (const CompType& type : types) {
      available_ref_types.push_back(
          std::visit([](auto& t) { return t.ref_type_kind(); }, type));
    }
    for (CompType& type : types) {
      std::visit(
          [&](auto& t) {
            t.FixupRefTypes(available_ref_types, recgroup_start);
          },
          type);
    }
  }

  void CanonicalizeIndexes(const std::vector<uint32_t>& canonical_indexes) {
    for (CompType& type : types) {
      std::visit([&](auto& t) { t.CanonicalizeIndexes(canonical_indexes); },
                 type);
    }
  }

  void BuildTypes(Zone* zone, WasmModuleBuilder* builder) const {
    auto build_type = [zone, builder](const auto& t) {
      t.BuildType(zone, builder);
    };
    if (single_type) {
      DCHECK_EQ(1, types.size());
      std::visit(build_type, types[0]);
    } else {
      builder->StartRecursiveTypeGroup();
      for (const CompType& type : types) {
        std::visit(build_type, type);
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

  void FixupRefTypes() {
    std::vector<RefTypeKind> available_ref_types;
    for (RecursionGroup& rec_group : rec_groups) {
      rec_group.UpdateAvailableRefTypesAndFixupRefTypes(available_ref_types);
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
  // reftype ::= ref null? heaptype
  // heaptype ::= absheaptype | typeidx
  // TODO(381687256): Introduce abstract heap types.
  auto ref_type_domain = fuzztest::ConstructorOf<test::RefType>(
      /* index */ fuzztest::NonNegative<int>(),
      // TODO(clemensb): Support shared types; the bit currently gets lost in
      // the module builder.
      /* shared */ fuzztest::Just(false),  // fuzztest::Arbitrary<bool>(),
      /* nullability */
      fuzztest::ElementOf({Nullability::kNullable, Nullability::kNonNullable}));

  // numtype ::= i32 | i64 | f32 | f64
  auto num_type_domain = fuzztest::StructOf<test::NumType>(
      fuzztest::ElementOf<ValueType>({kWasmI32, kWasmI64, kWasmF32, kWasmF64}));
  // valtype ::= numtype | vectype | reftype
  // TODO(381687256): Add vectype (kS128) on SIMD-enabled hosts.
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

  // rectype ::= rec subtype*
  // subtype ::= sub final? typeidx* comptype
  // TODO(381687256): support final types and subtyping.
  auto recgroup_domain = fuzztest::OneOf(
      // A single type declared outside any recursion group.
      fuzztest::StructOf<test::RecursionGroup>(
          fuzztest::VectorOf(comptype_domain).WithSize(1),
          fuzztest::Just(true)),
      // An actual recursion group of arbitrary size.
      fuzztest::StructOf<test::RecursionGroup>(
          fuzztest::VectorOf(comptype_domain), fuzztest::Just(false)));

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

    // After module generation by the fuzzer, fix up ref types to include the
    // correct ref type kind and restrict indexes to the valid index space.
    test_module.FixupRefTypes();

    WasmModuleBuilder builder(&zone_);
    test_module.BuildTypes(&zone_, &builder);
    ZoneBuffer buffer{&zone_};
    builder.WriteTo(&buffer);

    WasmDetectedFeatures detected_features;
    bool kValidateModule = true;
    ModuleResult result =
        DecodeWasmModule(enabled_features_, base::VectorOf(buffer),
                         kValidateModule, kWasmOrigin, &detected_features);

    // If this fails due to too many types, we need to constrain vector sizes in
    // the fuzzer domains.
    ASSERT_TRUE(result.ok());
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
        // This canonical ID can be used by successive recgroups.
        canonical_indexes.push_back(first_canonical_id.index + i);
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
