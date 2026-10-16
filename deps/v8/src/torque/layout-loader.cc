// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/layout-loader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>

#include "src/torque/ast.h"
#include "src/torque/global-context.h"
#include "src/torque/ls/json-parser.h"
#include "src/torque/torque-parser.h"
#include "src/torque/type-oracle.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8::internal::torque {

namespace {

constexpr int kSupportedSchemaVersion = 1;

using ls::JsonArray;
using ls::JsonObject;
using ls::JsonValue;

const JsonValue* Lookup(const JsonObject& object, const std::string& key) {
  auto it = object.find(key);
  return it == object.end() ? nullptr : &it->second;
}

// A malformed layout JSON aborts compilation instead of diagnosing one class.
const JsonValue& Require(const JsonObject& object, const std::string& key,
                         const std::string& context) {
  const JsonValue* value = Lookup(object, key);
  if (value == nullptr) {
    ReportError("layout JSON: ", context, ": missing key \"", key, "\"");
  }
  return *value;
}

size_t RequireSize(const JsonObject& object, const std::string& key,
                   const std::string& context) {
  const JsonValue& value = Require(object, key, context);
  // JSON numbers above 2^53 are not exact. Casting a value outside size_t's
  // range is undefined, including in 32-bit Torque builds.
  constexpr double kMaxExactInteger = 9007199254740992.0;  // 2^53
  constexpr double kBound =
      std::min(kMaxExactInteger,
               static_cast<double>(std::numeric_limits<size_t>::max()));
  if (!value.IsNumber() || value.ToNumber() < 0 || value.ToNumber() > kBound ||
      value.ToNumber() != std::trunc(value.ToNumber())) {
    ReportError("layout JSON: ", context, ": key \"", key,
                "\" is not a non-negative integer");
  }
  return static_cast<size_t>(value.ToNumber());
}

const std::string& RequireString(const JsonObject& object,
                                 const std::string& key,
                                 const std::string& context) {
  const JsonValue& value = Require(object, key, context);
  if (!value.IsString()) {
    ReportError("layout JSON: ", context, ": key \"", key,
                "\" is not a string");
  }
  return value.ToString();
}

std::string TorqueClassNameFromCppName(const std::string& cpp_name,
                                       const std::string& context) {
  size_t separator = cpp_name.rfind("::");
  if (separator == std::string::npos || separator + 2 == cpp_name.size()) {
    ReportError("layout JSON: ", context, ": C++ class name \"", cpp_name,
                "\" is not fully qualified");
  }
  return cpp_name.substr(separator + 2);
}

std::string TorqueBitFieldStructName(const JsonObject& storage,
                                     const std::string& context) {
  std::string name = RequireString(storage, "name", context);
  size_t pos;
  while ((pos = name.find("::")) != std::string::npos) {
    name.erase(pos, 2);
  }
  return name;
}

// Map primitive C++ types to their Torque representation. Torque already knows
// all other class, struct, and namespace-scope alias names.
struct PrimitiveTypeName {
  const char* cpp;
  const char* torque;
};

constexpr PrimitiveTypeName kPrimitiveTypeNames[] = {
    {"double", "float64"},   {"float", "float32"},     {"intptr_t", "intptr"},
    {"ptrdiff_t", "intptr"}, {"uintptr_t", "uintptr"}, {"size_t", "uintptr"},
    {"int8_t", "int8"},      {"uint8_t", "uint8"},     {"int16_t", "int16"},
    {"uint16_t", "uint16"},  {"int32_t", "int32"},     {"uint32_t", "uint32"},
    {"int64_t", "int64"},    {"uint64_t", "uint64"},   {"Address", "RawPtr"},
};

// Class templates the layout JSON may name a tagged slot by, mapped to the
// Torque type of their instantiations. Torque has no generic class types, so
// the mapping targets the common base class: every CppGCManaged<T> is a
// CppGCManagedBase, and T is a C++ payload type rather than a heap object.
// Must be kept in sync with _CLASS_TEMPLATE_ARGUMENTS in
// tools/metagen/layout_extract.py; both sides error on an unknown name.
constexpr PrimitiveTypeName kClassTemplateNames[] = {
    {"CppGCManaged", "CppGCManagedBase"},
};

// Instantiations Torque refines past that base class, keyed by the
// fully-qualified payload. An instantiation with no entry here falls back to
// kClassTemplateNames.
constexpr PrimitiveTypeName kClassTemplateInstantiations[] = {
    {"CppGCManaged<v8::internal::wasm::NativeModule>",
     "ManagedWasmNativeModule"},
    {"CppGCManaged<v8::internal::BackingStore>", "ManagedBackingStore"},
};

// Tagged types the two languages agree on but spell differently. The Torque
// spelling may be namespace-qualified.
//
// JSCallable is the widest C++ approximation of Torque's Callable: C++ cannot
// say "a JSProxy or JSObject with the callable bit set", so it names JSProxy
// and JSObject outright. A slot typed JSCallable holds a Callable.
// TODO(jgruber): Give C++ a precise Callable type (with the appropriate
// subtyping and cast helpers) so this entry can go away.
//
// IteratorRecord is nested in its owning class because the CSA struct already
// claims v8::internal::IteratorRecord.
// TODO(jgruber): Resolve the name clash with the CSA struct so the C++ and
// Torque IteratorRecord can share a name and this entry can go away.
constexpr PrimitiveTypeName kTaggedTypeNames[] = {
    {"JSCallable", "Callable"},
    {"IteratorRecord", "iterator::IteratorRecord"},
};

// Convert a base::BitField alias name to a Torque field name.
//
// Torque derives the C++ alias name from the Torque field name: CamelifyString
// plus "Bit" for a single bit and "Bits" for more (see
// ImplementationVisitor::GenerateBitFields). The layout JSON records the C++
// name, so reverse that conversion and check that converting back produces
// the original C++ name (e.g. "IsJSObjectBit" fails this check).
std::string TorqueBitFieldName(const std::string& cpp_name, size_t num_bits,
                               const std::string& context) {
  const std::string suffix = num_bits == 1 ? "Bit" : "Bits";
  if (cpp_name.size() <= suffix.size() ||
      cpp_name.compare(cpp_name.size() - suffix.size(), suffix.size(),
                       suffix) != 0) {
    ReportError("layout JSON: ", context, ": bit field \"", cpp_name, "\" of ",
                num_bits, " bit(s) does not end in \"", suffix, "\"");
  }
  std::string name =
      SnakeifyString(cpp_name.substr(0, cpp_name.size() - suffix.size()));
  if (CamelifyString(name) + suffix != cpp_name) {
    ReportError("layout JSON: ", context, ": bit field \"", cpp_name,
                "\" cannot be derived from a Torque field name; \"", name,
                "\" would be written \"", CamelifyString(name), suffix, "\"");
  }
  return name;
}

std::string TorqueTypeName(const std::string& cpp_name) {
  for (const PrimitiveTypeName& entry : kPrimitiveTypeNames) {
    if (cpp_name == entry.cpp) return entry.torque;
  }
  for (const PrimitiveTypeName& entry : kClassTemplateNames) {
    if (cpp_name == entry.cpp) return entry.torque;
  }
  for (const PrimitiveTypeName& entry : kTaggedTypeNames) {
    if (cpp_name == entry.cpp) return entry.torque;
  }
  return cpp_name;
}

// Split a possibly namespace-qualified Torque type name into its qualification
// and its identifier.
std::vector<std::string> SplitQualification(std::string* name) {
  std::vector<std::string> namespaces;
  size_t start = 0;
  for (size_t sep = name->find("::"); sep != std::string::npos;
       sep = name->find("::", start)) {
    namespaces.push_back(name->substr(start, sep - start));
    start = sep + 2;
  }
  if (!namespaces.empty()) *name = name->substr(start);
  return namespaces;
}

// Metagen preserves V8_TQ_* annotations without interpreting them. Define their
// syntax and valid locations here.
enum class AnnotationOn { kField, kTail, kClass };

struct AnnotationSpec {
  const char* name;
  bool takes_argument;
  AnnotationOn on;
};

constexpr AnnotationSpec kAnnotations[] = {
    {"V8_TQ_CONST", false, AnnotationOn::kField},
    {"V8_TQ_RELAXED", false, AnnotationOn::kField},
    {"V8_TQ_ACQ_REL", false, AnnotationOn::kField},
    {"V8_TQ_CUSTOM_WEAK", false, AnnotationOn::kField},
    {"V8_TQ_NAME", true, AnnotationOn::kField},
    {"V8_TQ_EXTENT_NAME", true, AnnotationOn::kField},
    {"V8_TQ_TYPE", true, AnnotationOn::kField},
    {"V8_TQ_TAIL_NAME", true, AnnotationOn::kClass},
    {"V8_TQ_TAIL_LENGTH", true, AnnotationOn::kClass},
    {"V8_TQ_NO_TAIL", false, AnnotationOn::kClass},
    {"V8_TQ_TAIL_SECTIONS", true, AnnotationOn::kClass},
};

// Tails accept field annotations except the name overrides. V8_TQ_TAIL_NAME
// names the tail instead.
bool AnnotationAllowedOn(const AnnotationSpec& spec, AnnotationOn on) {
  if (spec.on == on) return true;
  return on == AnnotationOn::kTail && spec.on == AnnotationOn::kField &&
         std::string_view(spec.name) != "V8_TQ_NAME" &&
         std::string_view(spec.name) != "V8_TQ_EXTENT_NAME";
}

const JsonObject* FindAnnotation(const JsonObject& record,
                                 std::string_view name,
                                 const std::string& context) {
  const JsonValue* list = Lookup(record, "annotations");
  if (list == nullptr) return nullptr;
  if (!list->IsArray()) {
    ReportError("layout JSON: ", context,
                ": \"annotations\" is not "
                "an array");
  }
  for (const JsonValue& entry : list->ToArray()) {
    if (!entry.IsObject()) {
      ReportError("layout JSON: ", context, ": annotation is not an object");
    }
    const JsonObject& annotation = entry.ToObject();
    if (RequireString(annotation, "name", context) == name) return &annotation;
  }
  return nullptr;
}

bool HasAnnotation(const JsonObject& record, std::string_view name,
                   const std::string& context) {
  return FindAnnotation(record, name, context) != nullptr;
}

const std::string* AnnotationArgument(const JsonObject& record,
                                      std::string_view name,
                                      const std::string& context) {
  const JsonObject* annotation = FindAnnotation(record, name, context);
  if (annotation == nullptr) return nullptr;
  return &RequireString(*annotation, "arg", context);
}

// Reject unknown and misplaced annotations instead of silently dropping them.
void ValidateAnnotations(const JsonObject& record, AnnotationOn on,
                         const std::string& context) {
  const JsonValue* list = Lookup(record, "annotations");
  if (list == nullptr) return;
  if (!list->IsArray()) {
    ReportError("layout JSON: ", context,
                ": \"annotations\" is not "
                "an array");
  }
  for (const JsonValue& entry : list->ToArray()) {
    if (!entry.IsObject()) {
      ReportError("layout JSON: ", context, ": annotation is not an object");
    }
    const JsonObject& annotation = entry.ToObject();
    const std::string& name = RequireString(annotation, "name", context);
    const AnnotationSpec* spec = nullptr;
    for (const AnnotationSpec& candidate : kAnnotations) {
      if (name == candidate.name) {
        spec = &candidate;
        break;
      }
    }
    if (spec == nullptr) {
      ReportError("layout JSON: ", context, ": unknown annotation ", name);
    }
    if (!AnnotationAllowedOn(*spec, on)) {
      ReportError("layout JSON: ", context, ": ", name, " is not allowed here");
    }
    bool has_argument = Lookup(annotation, "arg") != nullptr;
    if (has_argument != spec->takes_argument) {
      ReportError(
          "layout JSON: ", context, ": ", name,
          spec->takes_argument ? " needs an argument" : " takes no argument");
    }
  }
}

std::string TorqueClassName(const JsonObject& record) {
  const std::string& cpp_name = RequireString(record, "cpp_name", "class");
  return TorqueClassNameFromCppName(cpp_name, cpp_name);
}

std::string TorqueFieldName(const JsonObject& record,
                            const std::string& context) {
  if (const std::string* override =
          AnnotationArgument(record, "V8_TQ_NAME", context)) {
    if (override->empty()) {
      ReportError("layout JSON: ", context, ": V8_TQ_NAME is empty");
    }
    return *override;
  }
  std::string name = RequireString(record, "cpp_name", context);
  while (!name.empty() && name.back() == '_') name.pop_back();
  if (name.empty()) {
    ReportError("layout JSON: ", context,
                ": C++ field name maps to an empty Torque name");
  }
  return name;
}

// Compare C++ storage and Torque field types by representation category.
enum class StorageCategory {
  kTagged,
  kProtectedTagged,
  kTrustedPointer,
  kExternalPointer,
  kCppHeapPointer,
  kScalar,
};

std::string_view CategoryName(StorageCategory category) {
  switch (category) {
    case StorageCategory::kTagged:
      return "tagged";
    case StorageCategory::kProtectedTagged:
      return "protected tagged";
    case StorageCategory::kTrustedPointer:
      return "trusted pointer";
    case StorageCategory::kExternalPointer:
      return "external pointer";
    case StorageCategory::kCppHeapPointer:
      return "cpp-heap pointer";
    case StorageCategory::kScalar:
      return "scalar";
  }
}

std::optional<StorageCategory> CategoryOfStorageKind(const std::string& kind) {
  if (kind == "tagged") return StorageCategory::kTagged;
  if (kind == "protected_tagged") return StorageCategory::kProtectedTagged;
  if (kind == "trusted_pointer") return StorageCategory::kTrustedPointer;
  if (kind == "external_pointer") return StorageCategory::kExternalPointer;
  if (kind == "cpp_heap_pointer") return StorageCategory::kCppHeapPointer;
  if (kind == "int" || kind == "enum" || kind == "bool" ||
      kind == "unaligned" || kind == "struct" || kind == "bitfield" ||
      kind == "js_dispatch_handle") {
    return StorageCategory::kScalar;
  }
  return std::nullopt;
}

StorageCategory CategoryOfTorqueType(const Type* type) {
  // Check specific pointer types before the general tagged category.
  if (type->IsSubtypeOf(TypeOracle::GetProtectedPointerType())) {
    return StorageCategory::kProtectedTagged;
  }
  if (type->IsSubtypeOf(TypeOracle::GetTrustedPointerType())) {
    return StorageCategory::kTrustedPointer;
  }
  if (type->IsSubtypeOf(TypeOracle::GetExternalPointerType())) {
    return StorageCategory::kExternalPointer;
  }
  if (type->IsSubtypeOf(TypeOracle::GetCppHeapPointerType())) {
    return StorageCategory::kCppHeapPointer;
  }
  if (type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
    return StorageCategory::kTagged;
  }
  return StorageCategory::kScalar;
}

std::string_view SynchronizationName(FieldSynchronization synchronization) {
  switch (synchronization) {
    case FieldSynchronization::kNone:
      return "none";
    case FieldSynchronization::kRelaxed:
      return "relaxed";
    case FieldSynchronization::kAcquireRelease:
      return "acquire_release";
  }
}

std::optional<size_t> ConstantIndexExtent(const Field& field) {
  if (!field.index || !field.index_is_constant) return std::nullopt;
  auto* literal = IntegerLiteralExpression::DynamicCast(field.index->expr);
  if (literal == nullptr) return std::nullopt;
  return literal->value.TryTo<size_t>();
}

// Build comparison keys from type syntax without resolving aliases. Sort union
// members because .tq unions and C++ UnionOf may use different orders.
void FlattenUnion(TypeExpression* type, std::vector<std::string>* members);

std::string TypeExpressionKey(TypeExpression* type) {
  if (auto* basic = BasicTypeExpression::DynamicCast(type)) {
    std::stringstream s;
    for (const std::string& ns : basic->namespace_qualification) {
      s << ns << "::";
    }
    s << basic->name->value;
    if (!basic->generic_arguments.empty()) {
      s << "<";
      bool first = true;
      for (TypeExpression* argument : basic->generic_arguments) {
        if (!first) s << ",";
        first = false;
        s << TypeExpressionKey(argument);
      }
      s << ">";
    }
    return s.str();
  }
  if (UnionTypeExpression::DynamicCast(type) != nullptr) {
    std::vector<std::string> members;
    FlattenUnion(type, &members);
    std::sort(members.begin(), members.end());
    std::stringstream s;
    for (size_t i = 0; i < members.size(); ++i) {
      if (i > 0) s << "|";
      s << members[i];
    }
    return s.str();
  }
  ReportError(
      "layout JSON: field type uses an expression form TypeExpressionKey "
      "does not support, so it cannot be compared");
}

void FlattenUnion(TypeExpression* type, std::vector<std::string>* members) {
  if (auto* u = UnionTypeExpression::DynamicCast(type)) {
    FlattenUnion(u->a, members);
    FlattenUnion(u->b, members);
    return;
  }
  members->push_back(TypeExpressionKey(type));
}

// Build comparison keys for array extent expressions. Reject unsupported
// forms so two unreadable extents cannot compare equal.
std::string ExpressionKey(Expression* expr) {
  if (auto* literal = IntegerLiteralExpression::DynamicCast(expr)) {
    std::stringstream s;
    // Extents are non-negative; IntegerLiteral::ToString is not linked
    // into torque.
    if (literal->value.is_negative()) s << "-";
    s << literal->value.absolute_value();
    return s.str();
  }
  if (auto* identifier = IdentifierExpression::DynamicCast(expr)) {
    std::stringstream s;
    for (const std::string& ns : identifier->namespace_qualification) {
      s << ns << "::";
    }
    s << identifier->name->value;
    if (!identifier->generic_arguments.empty()) {
      s << "<";
      bool first = true;
      for (TypeExpression* argument : identifier->generic_arguments) {
        if (!first) s << ",";
        first = false;
        s << TypeExpressionKey(argument);
      }
      s << ">";
    }
    return s.str();
  }
  if (auto* access = FieldAccessExpression::DynamicCast(expr)) {
    return ExpressionKey(access->object) + "." + access->field->value;
  }
  if (auto* call = CallExpression::DynamicCast(expr)) {
    std::stringstream s;
    // Render binary operators, which parse as calls, in their source form.
    const std::string& callee = call->callee->name->value;
    if (call->arguments.size() == 2 &&
        call->callee->namespace_qualification.empty() &&
        call->callee->generic_arguments.empty() &&
        std::none_of(callee.begin(), callee.end(), [](char c) {
          return c == '_' || std::isalnum(static_cast<unsigned char>(c));
        })) {
      s << "(" << ExpressionKey(call->arguments[0]) << " " << callee << " "
        << ExpressionKey(call->arguments[1]) << ")";
      return s.str();
    }
    s << ExpressionKey(call->callee) << "(";
    bool first = true;
    for (Expression* argument : call->arguments) {
      if (!first) s << ",";
      first = false;
      s << ExpressionKey(argument);
    }
    s << ")";
    return s.str();
  }
  if (auto* conditional = ConditionalExpression::DynamicCast(expr)) {
    return "(" + ExpressionKey(conditional->condition) + " ? " +
           ExpressionKey(conditional->if_true) + " : " +
           ExpressionKey(conditional->if_false) + ")";
  }
  ReportError(
      "layout JSON: array extent uses an expression form "
      "ExpressionKey does not support, so it cannot be compared");
}

std::string IndexKey(const ClassFieldIndexInfo& index) {
  std::string result = ExpressionKey(index.expr);
  if (index.optional) result += "?";
  return result;
}

// Non-named super types cannot identify a layout-record boundary.
std::string TorqueSuperName(TypeExpression* super) {
  auto* basic = BasicTypeExpression::DynamicCast(super);
  return basic == nullptr ? "" : basic->name->value;
}

std::string FieldKey(const ClassFieldExpression& field) {
  std::stringstream s;
  s << field.name_and_type.name->value;
  if (field.index.has_value()) {
    s << "[" << IndexKey(*field.index) << "]";
  }
  s << ": " << TypeExpressionKey(field.name_and_type.type);
  if (field.const_qualified) s << " const";
  if (field.synchronization != FieldSynchronization::kNone) {
    s << " " << SynchronizationName(field.synchronization);
  }
  if (field.custom_weak_marking) s << " customWeak";
  return s.str();
}

JsonValue LoadLayoutDocument(const std::string& layout_json_path) {
  std::ifstream stream(layout_json_path);
  if (!stream.good()) {
    ReportError("cannot open layout JSON: ", layout_json_path);
  }
  std::string content{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
  ls::JsonParserResult parse_result = ls::ParseJson(content);
  if (parse_result.error.has_value()) {
    ReportError("layout JSON ", layout_json_path,
                " is not valid JSON: ", parse_result.error->message);
  }
  if (!parse_result.value.IsObject()) {
    ReportError("layout JSON ", layout_json_path, ": expected an object");
  }
  const JsonObject& document = parse_result.value.ToObject();
  const JsonValue& version = Require(document, "schema_version", "document");
  if (!version.IsNumber() || version.ToNumber() != kSupportedSchemaVersion) {
    ReportError("layout JSON ", layout_json_path,
                ": unsupported schema version (expected ",
                kSupportedSchemaVersion, ")");
  }
  return std::move(parse_result.value);
}

// Offsets and sizes are valid only for the target configuration that produced
// them. Check it explicitly because a class without a .tq body has no fields
// that could catch a mismatch indirectly.
void VerifyConfig(const JsonValue& document_value,
                  const std::string& layout_json_path) {
  const JsonValue& config_value =
      Require(document_value.ToObject(), "config", "document");
  if (!config_value.IsObject()) {
    ReportError("layout JSON ", layout_json_path,
                ": \"config\" is not an "
                "object");
  }
  const JsonObject& config = config_value.ToObject();
  struct Expectation {
    const char* key;
    size_t expected;
  };
  const Expectation expectations[] = {
      {"tagged_size", TargetArchitecture::TaggedSize()},
      {"pointer_size", TargetArchitecture::RawPtrSize()},
      {"external_pointer_size", TargetArchitecture::ExternalPointerSize()},
      {"cpp_heap_pointer_size", TargetArchitecture::CppHeapPointerSize()},
      {"trusted_pointer_size", TargetArchitecture::TrustedPointerSize()},
  };
  for (const Expectation& expectation : expectations) {
    size_t actual = RequireSize(config, expectation.key, "config");
    if (actual != expectation.expected) {
      ReportError("layout JSON ", layout_json_path, " was harvested with ",
                  expectation.key, " ", actual, ", but Torque targets ",
                  expectation.expected,
                  ": the layout JSON does not match this build configuration");
    }
  }
}

struct RecordIndexes {
  std::map<std::string, const JsonObject*> by_cpp_name;
  std::map<std::string, const JsonObject*> by_torque_name;
};

RecordIndexes IndexRecords(const JsonValue& document_value) {
  const JsonObject& document = document_value.ToObject();
  const JsonValue& classes = Require(document, "classes", "document");
  if (!classes.IsArray()) {
    ReportError("layout JSON: \"classes\" is not an array");
  }
  RecordIndexes records;
  for (const JsonValue& record : classes.ToArray()) {
    if (!record.IsObject()) {
      ReportError("layout JSON: class record is not an object");
    }
    const JsonObject& object = record.ToObject();
    const std::string& cpp_name = RequireString(object, "cpp_name", "class");
    if (!records.by_cpp_name.emplace(cpp_name, &object).second) {
      ReportError("layout JSON: duplicate C++ class record \"", cpp_name, "\"");
    }
    ValidateAnnotations(object, AnnotationOn::kClass, cpp_name);
    const JsonValue* fields = Lookup(object, "fields");
    if (fields != nullptr && fields->IsArray()) {
      for (const JsonValue& field : fields->ToArray()) {
        if (!field.IsObject()) continue;
        ValidateAnnotations(field.ToObject(), AnnotationOn::kField, cpp_name);
      }
    }
    if (const JsonValue* tail = Lookup(object, "tail");
        tail != nullptr && tail->IsObject()) {
      ValidateAnnotations(tail->ToObject(), AnnotationOn::kTail, cpp_name);
    }
    std::string torque_name = TorqueClassName(object);
    auto [torque_it, torque_inserted] =
        records.by_torque_name.emplace(torque_name, &object);
    if (!torque_inserted) {
      ReportError("layout JSON: C++ classes ",
                  RequireString(*torque_it->second, "cpp_name", torque_name),
                  " and ", cpp_name, " both map to Torque class ", torque_name);
    }
  }
  return records;
}

bool OptionalBool(const JsonObject& record, const std::string& key,
                  const std::string& context) {
  const JsonValue* value = Lookup(record, key);
  if (value == nullptr) return false;
  if (!value->IsBool()) {
    ReportError("layout JSON: ", context, ": key \"", key,
                "\" is not a boolean");
  }
  return value->ToBool();
}

FieldSynchronization ReadSynchronization(const JsonObject& record,
                                         const std::string& context) {
  bool relaxed = HasAnnotation(record, "V8_TQ_RELAXED", context);
  bool acq_rel = HasAnnotation(record, "V8_TQ_ACQ_REL", context);
  if (relaxed && acq_rel) {
    ReportError("layout JSON: ", context,
                ": V8_TQ_RELAXED and V8_TQ_ACQ_REL on the same member");
  }
  if (relaxed) return FieldSynchronization::kRelaxed;
  if (acq_rel) return FieldSynchronization::kAcquireRelease;
  return FieldSynchronization::kNone;
}

const JsonObject* LookupTail(const JsonObject& record,
                             const std::string& name) {
  const JsonValue* tail = Lookup(record, "tail");
  if (tail == nullptr) return nullptr;
  if (!tail->IsObject()) {
    ReportError("layout JSON: ", name, ": \"tail\" is not an object");
  }
  return &tail->ToObject();
}

// How Torque represents the flexible tail, as specified by class annotations.
enum class TailRepresentation {
  // Torque fields end at the fixed header.
  kNone,
  // The tail is one indexed Torque field.
  kIndexedField,
  // Torque splits the tail into several indexed sections, as in ScopeInfo.
  kIndexedSections,
  // No V8_TQ_TAIL_* annotation: the tail cannot become a Torque field.
  kUnspecified,
};

TailRepresentation GetTailRepresentation(const JsonObject& class_record,
                                         const JsonObject* tail_record,
                                         const std::string& context) {
  bool has_name = HasAnnotation(class_record, "V8_TQ_TAIL_NAME", context);
  bool has_length = HasAnnotation(class_record, "V8_TQ_TAIL_LENGTH", context);
  bool no_tail = HasAnnotation(class_record, "V8_TQ_NO_TAIL", context);
  bool sections = HasAnnotation(class_record, "V8_TQ_TAIL_SECTIONS", context);
  if (has_name != has_length) {
    ReportError("layout JSON: ", context,
                ": V8_TQ_TAIL_NAME and V8_TQ_TAIL_LENGTH must be given "
                "together");
  }
  if (has_name + no_tail + sections > 1) {
    ReportError("layout JSON: ", context,
                ": V8_TQ_TAIL_NAME, V8_TQ_NO_TAIL and V8_TQ_TAIL_SECTIONS "
                "are exclusive");
  }
  if (tail_record == nullptr) {
    if (has_name || no_tail || sections) {
      ReportError("layout JSON: ", context, ": ",
                  has_name
                      ? "V8_TQ_TAIL_NAME"
                      : (no_tail ? "V8_TQ_NO_TAIL" : "V8_TQ_TAIL_SECTIONS"),
                  " without a flexible array member");
    }
    return TailRepresentation::kNone;
  }
  if (has_name) return TailRepresentation::kIndexedField;
  if (sections) return TailRepresentation::kIndexedSections;
  return no_tail ? TailRepresentation::kNone : TailRepresentation::kUnspecified;
}

// Fold C++-only intermediate bases into the first derived class Torque knows.
// Return contributing records from the outermost base to the derived class.
template <typename BoundaryPredicate>
std::vector<const JsonObject*> CollapseCppOnlyBases(
    const JsonObject& record, const std::string& name,
    const std::map<std::string, const JsonObject*>& records_by_cpp_name,
    BoundaryPredicate is_boundary) {
  std::vector<const JsonObject*> groups{&record};
  const JsonObject* outermost = &record;
  while (true) {
    const JsonValue& base = Require(*outermost, "base", name);
    if (base.tag == JsonValue::IS_NULL) break;
    if (!base.IsString()) {
      ReportError("layout JSON: ", name, ": \"base\" is not a string");
    }
    if (is_boundary(TorqueClassNameFromCppName(base.ToString(), name))) break;
    auto parent_it = records_by_cpp_name.find(base.ToString());
    if (parent_it == records_by_cpp_name.end()) {
      ReportError("layout JSON: ", name, ": C++ base ", base.ToString(),
                  " has no layout record");
    }
    outermost = parent_it->second;
    if (Lookup(*outermost, "tail") != nullptr) {
      ReportError("layout JSON: ", name, ": C++ base ", base.ToString(),
                  " has a flexible tail");
    }
    groups.insert(groups.begin(), outermost);
  }
  return groups;
}

const JsonArray& RequireFields(const JsonObject& group,
                               const std::string& name) {
  const JsonValue& fields_value = Require(group, "fields", name);
  if (!fields_value.IsArray()) {
    ReportError("layout JSON: ", name, ": \"fields\" is not an array");
  }
  return fields_value.ToArray();
}

const JsonObject& RequireFieldObject(const JsonValue& field,
                                     const std::string& name) {
  if (!field.IsObject()) {
    ReportError("layout JSON: ", name, ": field is not an object");
  }
  return field.ToObject();
}

std::vector<const JsonObject*> CollectFields(
    const std::vector<const JsonObject*>& groups, const std::string& name) {
  std::vector<const JsonObject*> fields;
  for (const JsonObject* group : groups) {
    for (const JsonValue& field : RequireFields(*group, name)) {
      fields.push_back(&RequireFieldObject(field, name));
    }
  }
  return fields;
}

class LayoutVerifier {
 public:
  void Run(const std::string& layout_json_path) {
    for (const ClassType* type : TypeOracle::GetClasses()) {
      classes_by_name_[type->name()] = type;
    }
    JsonValue document = LoadLayoutDocument(layout_json_path);
    VerifyConfig(document, layout_json_path);
    RecordIndexes records = IndexRecords(document);
    records_by_cpp_name_ = std::move(records.by_cpp_name);
    for (const auto& entry : records_by_cpp_name_) {
      VerifyClass(*entry.second);
    }
  }

 private:
  void VerifyClass(const JsonObject& record) {
    std::string name = TorqueClassName(record);

    // The layout JSON also contains classes Torque does not know.
    auto it = classes_by_name_.find(name);
    if (it == classes_by_name_.end()) return;
    const ClassType* type = it->second;
    if (!type->IsLayoutDefinedInCpp()) return;
    // Classes without a .tq body, such as BigIntBase and Code, have no fields.
    if (type->HasUndefinedLayout()) return;

    // Skip C++-only bases to find the base declared in Torque.
    std::vector<const JsonObject*> groups = CollapseCppOnlyBases(
        record, name, records_by_cpp_name_, [&](const std::string& base) {
          return classes_by_name_.count(base) > 0;
        });

    VerifyBase(type, *groups.front(), name);
    VerifyFields(type, groups, record, name);
  }

  void VerifyBase(const ClassType* type, const JsonObject& record,
                  const std::string& name) {
    const ClassType* super = type->GetSuperClass();
    const JsonValue& base = Require(record, "base", name);
    if (base.tag == JsonValue::IS_NULL) {
      if (super != nullptr) {
        Error("class ", name, ": C++ layout has no base, Torque extends ",
              super->name())
            .Position(type->GetPosition());
      }
      return;
    }
    if (!base.IsString()) {
      ReportError("layout JSON: ", name, ": \"base\" is not a string");
    }
    if (super == nullptr) {
      Error("class ", name, ": C++ layout has base ", base.ToString(),
            ", Torque has none")
          .Position(type->GetPosition());
      return;
    }
    std::string torque_base = TorqueClassNameFromCppName(base.ToString(), name);
    if (super->name() != torque_base) {
      Error("class ", name, ": C++ layout base is ", base.ToString(),
            ", Torque extends ", super->name())
          .Position(type->GetPosition());
      return;
    }
    size_t base_size = RequireSize(record, "base_size", name);
    auto super_size = super->size().SingleValue();
    if (!super_size.has_value() || *super_size != base_size) {
      Error("class ", name, ": sizeof(", base.ToString(), ") is ", base_size,
            " in C++, ",
            super_size.has_value() ? std::to_string(*super_size) : "dynamic",
            " in Torque")
          .Position(type->GetPosition());
    }
  }

  void VerifyFields(const ClassType* type,
                    const std::vector<const JsonObject*>& groups,
                    const JsonObject& record, const std::string& name) {
    std::vector<const JsonObject*> json_fields = CollectFields(groups, name);
    const JsonObject* tail_record = LookupTail(record, name);
    // For an unspecified tail representation, verify the fixed header and
    // require indexed fields after it.
    TailRepresentation tail_representation =
        GetTailRepresentation(record, tail_record, name);
    bool tail_is_indexed_field =
        tail_representation == TailRepresentation::kIndexedField;

    const std::vector<Field>& torque_fields = type->fields();
    if (tail_representation == TailRepresentation::kNone ||
        tail_representation == TailRepresentation::kIndexedField) {
      size_t expected_count =
          json_fields.size() + (tail_is_indexed_field ? 1 : 0);
      if (torque_fields.size() != expected_count) {
        Error("class ", name, ": C++ layout has ", expected_count,
              " field(s), Torque has ", torque_fields.size())
            .Position(type->GetPosition());
        return;
      }
    } else {
      if (torque_fields.size() < json_fields.size()) {
        Error("class ", name, ": C++ layout has ", json_fields.size(),
              " fixed field(s) before the tail, Torque has only ",
              torque_fields.size(), " field(s)")
            .Position(type->GetPosition());
        return;
      }
      for (size_t i = json_fields.size(); i < torque_fields.size(); ++i) {
        if (!torque_fields[i].index.has_value()) {
          Error("class ", name, ": Torque field ",
                torque_fields[i].name_and_type.name,
                " is not indexed but lies in the C++ flexible tail")
              .Position(torque_fields[i].pos);
          return;
        }
      }
    }

    for (size_t i = 0; i < json_fields.size(); ++i) {
      VerifyField(name, torque_fields[i], *json_fields[i]);
    }
    if (tail_is_indexed_field) {
      VerifyTail(name, torque_fields.back(), record, *tail_record);
    }

    size_t size = RequireSize(record, "size", name);
    auto torque_size = type->size().SingleValue();
    if (tail_record != nullptr) {
      // VerifyTail checks the flexible portion separately.
      if (type->header_size() != size) {
        Error("class ", name, ": C++ fixed size is ", size,
              ", Torque header size is ", type->header_size())
            .Position(type->GetPosition());
      }
    } else if (torque_size.has_value()) {
      if (*torque_size != size || type->header_size() != size) {
        Error("class ", name, ": sizeof is ", size, " in C++, ", *torque_size,
              " in Torque")
            .Position(type->GetPosition());
      }
    } else {
      // A named array extent can leave a fixed C++ layout dynamic in Torque.
      // Its element, offset, and total class size still constrain the field.
      bool last_is_named_const_array =
          !torque_fields.empty() && torque_fields.back().index.has_value() &&
          torque_fields.back().index_is_constant &&
          !ConstantIndexExtent(torque_fields.back()).has_value();
      const JsonObject* last_json =
          json_fields.empty() ? nullptr : json_fields.back();
      if (!last_is_named_const_array || last_json == nullptr) {
        Error("class ", name,
              ": C++ layout is fixed-size but the Torque size is dynamic")
            .Position(type->GetPosition());
      } else {
        size_t last_offset = RequireSize(*last_json, "offset", name);
        size_t last_size = RequireSize(*last_json, "size", name);
        if (type->header_size() != last_offset ||
            last_offset + last_size != size) {
          Error("class ", name,
                ": the named-constant-extent array must "
                "span the C++ layout from ",
                type->header_size(), " to ", size, ", but spans ", last_offset,
                " to ", last_offset + last_size)
              .Position(type->GetPosition());
        }
      }
    }
  }

  void VerifyField(const std::string& class_name, const Field& field,
                   const JsonObject& record) {
    std::string name = TorqueFieldName(record, class_name);
    std::string context = class_name + "." + name + " (member " +
                          RequireString(record, "cpp_name", name) + ")";

    if (field.name_and_type.name != name) {
      Error("field ", context, ": Torque declares ", field.name_and_type.name,
            " here")
          .Position(field.pos);
      return;
    }
    if (field.index.has_value() && !field.index_is_constant) {
      Error("field ", context,
            ": indexed in Torque but a fixed-offset field in C++")
          .Position(field.pos);
      return;
    }

    size_t offset = RequireSize(record, "offset", context);
    if (!field.offset.has_value() || *field.offset != offset) {
      Error(
          "field ", context, ": offset is ", offset, " in C++, ",
          field.offset.has_value() ? std::to_string(*field.offset) : "dynamic",
          " in Torque")
          .Position(field.pos);
    }

    size_t size = RequireSize(record, "size", context);
    size_t extent = 1;
    if (Lookup(record, "array_extent") != nullptr) {
      extent = RequireSize(record, "array_extent", context);
      if (!field.index.has_value() || !field.index_is_constant) {
        Error("field ", context, ": a [", extent,
              "] array in C++ but not in Torque")
            .Position(field.pos);
      } else if (auto torque_extent = ConstantIndexExtent(field);
                 torque_extent.has_value() && *torque_extent != extent) {
        // Torque does not expose the value of a named constant in this AST.
        Error("field ", context, ": a [", extent, "] array in C++, [",
              *torque_extent, "] in Torque")
            .Position(field.pos);
      }
    } else if (field.index.has_value()) {
      Error("field ", context, ": indexed in Torque but scalar in C++")
          .Position(field.pos);
    }
    size_t element_size = std::get<0>(field.GetFieldSizeInformation());
    if (element_size * extent != size) {
      Error("field ", context, ": ", size, " byte(s) in C++, ",
            element_size * extent, " in Torque")
          .Position(field.pos);
    }

    VerifyCategory(context, field, record, "storage");
    VerifyBitFields(context, field, record);
    VerifyFlags(context, field, record);
  }

  // Check the individual bit fields, not just the member holding them. The
  // @cppScope asserts already compare the .tq declaration against the
  // C++ BitField aliases, but nothing else checks that the layout JSON agrees
  // with both.
  void VerifyBitFields(const std::string& context, const Field& field,
                       const JsonObject& record) {
    const JsonValue* storage = Lookup(record, "storage");
    if (storage == nullptr || !storage->IsObject()) return;
    const JsonObject& s = storage->ToObject();
    const JsonValue* kind = Lookup(s, "kind");
    if (kind == nullptr || !kind->IsString() ||
        kind->ToString() != "bitfield") {
      return;
    }
    const auto* type =
        BitFieldStructType::DynamicCast(field.name_and_type.type);
    if (type == nullptr) {
      Error("field ", context, ": a bit field group in C++, ",
            field.name_and_type.type->ToString(), " in Torque")
          .Position(field.pos);
      return;
    }
    const std::string struct_name = TorqueBitFieldStructName(s, context);
    if (type->name() != struct_name) {
      Error("field ", context, ": the C++ bit field group is ",
            RequireString(s, "name", context), ", Torque uses ", type->name())
          .Position(field.pos);
      return;
    }
    const JsonValue& bits = Require(s, "bits", context);
    if (!bits.IsArray()) {
      ReportError("layout JSON: ", context, ": \"bits\" is not an array");
    }
    const std::vector<BitField>& torque_bits = type->fields();
    if (bits.ToArray().size() != torque_bits.size()) {
      Error("field ", context, ": C++ declares ", bits.ToArray().size(),
            " bit field(s), Torque has ", torque_bits.size())
          .Position(type->GetPosition());
      return;
    }
    for (size_t i = 0; i < torque_bits.size(); ++i) {
      const JsonValue& bit = bits.ToArray()[i];
      if (!bit.IsObject()) {
        ReportError("layout JSON: ", context, ": bit field is not an object");
      }
      const JsonObject& b = bit.ToObject();
      size_t num_bits = RequireSize(b, "num_bits", context);
      std::string name = TorqueBitFieldName(
          RequireString(b, "cpp_name", context), num_bits, context);
      const BitField& torque_bit = torque_bits[i];
      if (torque_bit.name_and_type.name != name) {
        Error("field ", context, ": C++ bit field ", i, " is ", name,
              ", Torque declares ", torque_bit.name_and_type.name)
            .Position(torque_bit.pos);
        continue;
      }
      size_t offset = RequireSize(b, "offset", context);
      if (static_cast<size_t>(torque_bit.offset) != offset ||
          static_cast<size_t>(torque_bit.num_bits) != num_bits) {
        Error("bit field ", context, ".", name, ": ", num_bits, " bit(s) at ",
              offset, " in C++, ", torque_bit.num_bits, " at ",
              torque_bit.offset, " in Torque")
            .Position(torque_bit.pos);
      }
    }
  }

  void VerifyTail(const std::string& class_name, const Field& field,
                  const JsonObject& class_record, const JsonObject& record) {
    std::string name =
        *AnnotationArgument(class_record, "V8_TQ_TAIL_NAME", class_name);
    std::string context = class_name + "." + name + " (member " +
                          RequireString(record, "cpp_name", name) + ")";

    if (field.name_and_type.name != name) {
      Error("tail ", context, ": Torque declares ", field.name_and_type.name,
            " here")
          .Position(field.pos);
      return;
    }
    if (!field.index.has_value() || field.index_is_constant) {
      Error("tail ", context,
            ": a flexible tail in C++ but not an indexed "
            "field in Torque")
          .Position(field.pos);
      return;
    }
    size_t offset = RequireSize(record, "offset", context);
    if (!field.offset.has_value() || *field.offset != offset) {
      Error("tail ", context, ": starts at ", offset, " in C++, ",
            field.offset.has_value() ? std::to_string(*field.offset)
                                     : "dynamic offset",
            " in Torque")
          .Position(field.pos);
    }
    size_t element_size = RequireSize(record, "element_size", context);
    size_t torque_element_size = std::get<0>(field.GetFieldSizeInformation());
    if (torque_element_size != element_size) {
      Error("tail ", context, ": element size is ", element_size, " in C++, ",
            torque_element_size, " in Torque")
          .Position(field.pos);
    }
    const std::string& length_field =
        *AnnotationArgument(class_record, "V8_TQ_TAIL_LENGTH", context);
    auto* index = IdentifierExpression::DynamicCast(field.index->expr);
    if (index == nullptr || index->name->value != length_field) {
      Error("tail ", context, ": indexed by ", length_field,
            " in C++, but Torque uses a different index expression")
          .Position(field.pos);
    }
    VerifyCategory(context, field, record, "element_storage");
  }

  void VerifyCategory(const std::string& context, const Field& field,
                      const JsonObject& record, const std::string& key) {
    const JsonValue& storage_value = Require(record, key, context);
    if (!storage_value.IsObject()) {
      ReportError("layout JSON: ", context, ": \"", key, "\" is not an object");
    }
    const JsonObject& storage = storage_value.ToObject();
    const std::string& kind = RequireString(storage, "kind", context);
    auto expected = CategoryOfStorageKind(kind);
    if (!expected.has_value()) {
      ReportError("layout JSON: ", context, ": unknown storage kind \"", kind,
                  "\"");
    }
    StorageCategory actual = CategoryOfTorqueType(field.name_and_type.type);
    if (actual != *expected) {
      Error("field ", context, ": ", CategoryName(*expected),
            " storage in C++, but the Torque type ", *field.name_and_type.type,
            " is ", CategoryName(actual))
          .Position(field.pos);
    }
  }

  void VerifyFlags(const std::string& context, const Field& field,
                   const JsonObject& record) {
    bool is_const = HasAnnotation(record, "V8_TQ_CONST", context);
    if (field.const_qualified != is_const) {
      Error("field ", context, ": ", is_const ? "const" : "not const",
            " in C++, ", field.const_qualified ? "const" : "not const",
            " in Torque")
          .Position(field.pos);
    }

    FieldSynchronization synchronization = ReadSynchronization(record, context);
    if (field.synchronization != synchronization) {
      Error("field ", context, ": synchronization is ",
            SynchronizationName(synchronization), " in C++, ",
            SynchronizationName(field.synchronization), " in Torque")
          .Position(field.pos);
    }

    bool custom_weak = HasAnnotation(record, "V8_TQ_CUSTOM_WEAK", context);
    if (field.custom_weak_marking != custom_weak) {
      Error("field ", context, ": @customWeakMarking differs between C++ (",
            custom_weak, ") and Torque (", field.custom_weak_marking, ")")
          .Position(field.pos);
    }
  }

  std::map<std::string, const ClassType*> classes_by_name_;
  std::map<std::string, const JsonObject*> records_by_cpp_name_;
};

class LayoutImporter {
 public:
  void Run(const std::string& layout_json_path,
           const std::string& positions_path) {
    JsonValue document = LoadLayoutDocument(layout_json_path);
    VerifyConfig(document, layout_json_path);
    RecordIndexes records = IndexRecords(document);
    records_by_cpp_name_ = std::move(records.by_cpp_name);
    records_by_torque_name_ = std::move(records.by_torque_name);
    if (!positions_path.empty()) LoadPositions(positions_path);
    CollectBitFieldStructs(CurrentAst::Get().declarations());
    Import(CurrentAst::Get().declarations());
    // Appended after the walk: Import() iterates the same declaration vector.
    std::vector<Declaration*>& declarations = CurrentAst::Get().declarations();
    declarations.insert(declarations.end(), new_bit_field_structs_.begin(),
                        new_bit_field_structs_.end());
    new_bit_field_structs_.clear();
  }

  // Collect existing bitfield structs before importing the JSON declarations.
  // Keep the .tq declarations for builds with v8_use_metagen_layouts=false;
  // when importing, check that they match the JSON before replacing them.
  void CollectBitFieldStructs(const std::vector<Declaration*>& declarations) {
    for (Declaration* declaration : declarations) {
      if (auto* ns = NamespaceDeclaration::DynamicCast(declaration)) {
        CollectBitFieldStructs(ns->declarations);
        continue;
      }
      if (auto* decl = BitFieldStructDeclaration::DynamicCast(declaration)) {
        bit_field_structs_.emplace(decl->name->value, decl);
      }
    }
  }

  void ImportBitFieldStructs(const JsonObject& record,
                             const std::string& cpp_name,
                             SourcePosition fallback) {
    const JsonValue* fields = Lookup(record, "fields");
    if (fields == nullptr || !fields->IsArray()) return;
    for (const JsonValue& field : fields->ToArray()) {
      if (!field.IsObject()) continue;
      const JsonValue* storage = Lookup(field.ToObject(), "storage");
      if (storage == nullptr || !storage->IsObject()) continue;
      const JsonObject& s = storage->ToObject();
      const JsonValue* kind = Lookup(s, "kind");
      if (kind == nullptr || !kind->IsString() ||
          kind->ToString() != "bitfield") {
        continue;
      }
      const std::string name = TorqueBitFieldStructName(s, cpp_name);
      SourcePosition position = MemberPosition(
          cpp_name, RequireString(field.ToObject(), "cpp_name", cpp_name),
          fallback);
      auto it = bit_field_structs_.find(name);
      if (it == bit_field_structs_.end()) {
        CurrentSourcePosition::Scope position_scope(position);
        auto* decl = MakeNode<BitFieldStructDeclaration>(
            MakeNode<Identifier>(name), BitFieldParent(cpp_name, s),
            BitFieldEntries(cpp_name, s), std::optional<std::string>{});
        bit_field_structs_.emplace(name, decl);
        new_bit_field_structs_.push_back(decl);
        continue;
      }
      // Declared in .tq, or already imported for another member: build at
      // the C++ position for diagnostics and compare, then rebuild at the
      // Torque position and replace the declaration.
      BitFieldStructDeclaration* decl = it->second;
      TypeExpression* parent;
      std::vector<BitFieldDeclaration> entries;
      {
        CurrentSourcePosition::Scope position_scope(position);
        parent = BitFieldParent(cpp_name, s);
        entries = BitFieldEntries(cpp_name, s);
      }
      if (!BitFieldStructsEquivalent(*decl, parent, entries)) continue;
      std::vector<SourcePosition> torque_positions;
      for (const BitFieldDeclaration& f : decl->fields) {
        torque_positions.push_back(f.name_and_type.name->pos);
      }
      CurrentSourcePosition::Scope position_scope(decl->pos);
      decl->parent = BitFieldParent(cpp_name, s);
      decl->fields = BitFieldEntries(cpp_name, s, &torque_positions);
    }
  }

  bool BitFieldStructsEquivalent(
      const BitFieldStructDeclaration& decl, TypeExpression* parent,
      const std::vector<BitFieldDeclaration>& built) {
    CurrentSourcePosition::Scope position_scope(decl.pos);
    const std::string& name = decl.name->value;
    if (TypeExpressionKey(decl.parent) != TypeExpressionKey(parent)) {
      Error("bitfield struct ", name, ": .tq extends ",
            TypeExpressionKey(decl.parent), ", C++ ", TypeExpressionKey(parent))
          .Position(decl.pos);
      return false;
    }
    if (decl.fields.size() != built.size()) {
      Error("bitfield struct ", name, ": .tq declares ", decl.fields.size(),
            " field(s), C++ ", built.size())
          .Position(decl.pos);
      return false;
    }
    bool equivalent = true;
    for (size_t i = 0; i < built.size(); ++i) {
      std::string a = BitFieldKey(decl.fields[i]);
      std::string b = BitFieldKey(built[i]);
      if (a != b) {
        Error("bitfield struct ", name, ": .tq field \"", a,
              "\" differs from C++ field \"", b, "\"")
            .Position(decl.fields[i].name_and_type.name->pos);
        equivalent = false;
      }
    }
    return equivalent;
  }

  static std::string BitFieldKey(const BitFieldDeclaration& field) {
    return field.name_and_type.name->value + ": " +
           TypeExpressionKey(field.name_and_type.type) + ": " +
           std::to_string(field.num_bits) + " bit";
  }

  // Convert the storage width in bytes to a Torque unsigned integer type.
  TypeExpression* BitFieldParent(const std::string& context,
                                 const JsonObject& storage) {
    return NamedType(
        "uint" + std::to_string(RequireSize(storage, "width", context) * 8));
  }

  // Build the fields at the current position, or at `positions` (one per
  // field) when replacing a .tq declaration.
  std::vector<BitFieldDeclaration> BitFieldEntries(
      const std::string& context, const JsonObject& storage,
      const std::vector<SourcePosition>* positions = nullptr) {
    const JsonValue& bits = Require(storage, "bits", context);
    if (!bits.IsArray() || bits.ToArray().empty()) {
      ReportError("layout JSON: ", context, ": bitfield without fields");
    }
    std::vector<BitFieldDeclaration> fields;
    size_t next_bit = 0;
    for (const JsonValue& bit : bits.ToArray()) {
      if (!bit.IsObject()) {
        ReportError("layout JSON: ", context, ": bit field is not an object");
      }
      const JsonObject& b = bit.ToObject();
      const std::string& cpp_name = RequireString(b, "cpp_name", context);
      size_t num_bits = RequireSize(b, "num_bits", context);
      // Torque derives each offset from declaration order; a gap there would
      // silently shift every field after it.
      size_t offset = RequireSize(b, "offset", context);
      if (offset != next_bit) {
        ReportError("layout JSON: ", context, ": bit field \"", cpp_name,
                    "\" starts at bit ", offset, ", expected ", next_bit);
      }
      next_bit = offset + num_bits;
      CurrentSourcePosition::Scope position_scope(
          positions != nullptr ? positions->at(fields.size())
                               : CurrentSourcePosition::Get());
      fields.push_back(BitFieldDeclaration{
          NameAndTypeExpression{
              MakeNode<Identifier>(
                  TorqueBitFieldName(cpp_name, num_bits, context)),
              NamedType(TorqueTypeName(RequireString(b, "type", context)))},
          static_cast<int>(num_bits)});
    }
    return fields;
  }

 private:
  // Fall back to the .tq declaration when the positions JSON has no entry.
  void LoadPositions(const std::string& path) {
    JsonValue document_value = LoadLayoutDocument(path);
    const JsonValue& classes =
        Require(document_value.ToObject(), "classes", "positions");
    if (!classes.IsArray()) {
      ReportError("layout JSON: positions \"classes\" is not an array");
    }
    for (const JsonValue& entry : classes.ToArray()) {
      if (!entry.IsObject()) {
        ReportError("layout JSON: positions entry is not an object");
      }
      const JsonObject& object = entry.ToObject();
      const std::string& cpp_name =
          RequireString(object, "cpp_name", "positions");
      const JsonValue& members = Require(object, "members", cpp_name);
      if (!members.IsObject()) {
        ReportError("layout JSON: ", cpp_name,
                    ": \"members\" is not an object");
      }
      std::map<std::string, SourcePosition>& class_positions =
          positions_[cpp_name];
      for (const auto& [member, value] : members.ToObject()) {
        if (!value.IsString()) {
          ReportError("layout JSON: ", cpp_name, ".", member,
                      ": position is not a string");
        }
        class_positions.emplace(member, ParsePosition(value.ToString()));
      }
    }
  }

  SourcePosition MemberPosition(const std::string& class_cpp_name,
                                const std::string& member_cpp_name,
                                SourcePosition fallback) {
    auto class_it = positions_.find(class_cpp_name);
    if (class_it == positions_.end()) return fallback;
    auto member_it = class_it->second.find(member_cpp_name);
    if (member_it == class_it->second.end()) return fallback;
    return member_it->second;
  }

  void Import(const std::vector<Declaration*>& declarations) {
    for (Declaration* declaration : declarations) {
      if (auto* ns = NamespaceDeclaration::DynamicCast(declaration)) {
        Import(ns->declarations);
        continue;
      }
      auto* decl = ClassDeclaration::DynamicCast(declaration);
      if (decl == nullptr) continue;
      if (!(decl->flags & ClassFlag::kCppObjectLayoutDefinition)) continue;
      ImportClass(decl);
    }
  }

  void ImportClass(ClassDeclaration* decl) {
    const std::string& name = decl->name->value;
    bool body_less = (decl->flags & ClassFlag::kUndefinedLayout) != 0;
    auto it = records_by_torque_name_.find(name);
    if (it == records_by_torque_name_.end()) {
      // Classes without a record, such as templates, keep their .tq fields.
      if (body_less) {
        Error("class ", name,
              " has no layout record; a @cppObjectLayoutDefinition "
              "class without a .tq body takes its fields from the layout JSON")
            .Position(decl->pos);
      }
      return;
    }
    const JsonObject& record = *it->second;

    const std::string torque_base = TorqueSuperName(decl->super);
    std::vector<const JsonObject*> groups = CollapseCppOnlyBases(
        record, name, records_by_cpp_name_,
        [&](const std::string& base) { return base == torque_base; });

    // Members of C++-only bases belong to this class, including any bit field
    // groups among them.
    for (const JsonObject* group : groups) {
      ImportBitFieldStructs(*group, RequireString(*group, "cpp_name", name),
                            decl->pos);
    }

    const JsonObject* tail_record = LookupTail(record, name);
    // Keep the .tq fields when the tail has no annotation. An omitted tail
    // contributes no field, but its fixed header still does.
    TailRepresentation tail_representation =
        GetTailRepresentation(record, tail_record, name);
    if (tail_representation == TailRepresentation::kUnspecified) return;
    if (tail_representation == TailRepresentation::kNone) tail_record = nullptr;

    // Use C++ positions for diagnostics and strict comparison first.
    std::vector<ClassFieldExpression> fields =
        BuildFields(name, record, groups, tail_record, tail_representation,
                    decl->pos, nullptr);

    // Replace a .tq field block only if the built fields are equivalent.
    if (!body_less && !FieldsEquivalent(name, decl->fields, fields)) return;

    // Rebuild accepted fields at Torque positions so generated accessors stay
    // in the same compilation units as on the legacy path.
    std::vector<SourcePosition> torque_positions;
    torque_positions.reserve(fields.size());
    if (body_less) {
      torque_positions.resize(fields.size(), decl->pos);
    } else {
      for (const ClassFieldExpression& field : decl->fields) {
        torque_positions.push_back(field.name_and_type.name->pos);
      }
    }
    fields = BuildFields(name, record, groups, tail_record, tail_representation,
                         decl->pos, &torque_positions);

    decl->fields = std::move(fields);
    decl->flags &= ~ClassFlags(ClassFlag::kUndefinedLayout);
  }

  std::vector<ClassFieldExpression> BuildFields(
      const std::string& class_name, const JsonObject& class_record,
      const std::vector<const JsonObject*>& groups,
      const JsonObject* tail_record, TailRepresentation tail_representation,
      SourcePosition default_position,
      const std::vector<SourcePosition>* positions) {
    std::vector<ClassFieldExpression> fields;
    std::map<std::string, std::string> cpp_names_by_torque_name;
    auto build = [&](const JsonObject& field_record,
                     const std::string& owner_cpp_name, bool is_tail) {
      std::string torque_name =
          is_tail
              ? *AnnotationArgument(class_record, "V8_TQ_TAIL_NAME", class_name)
              : TorqueFieldName(field_record, class_name);
      const std::string& cpp_name =
          RequireString(field_record, "cpp_name", class_name);
      auto [name_it, inserted] =
          cpp_names_by_torque_name.emplace(torque_name, cpp_name);
      if (!inserted) {
        ReportError("layout JSON: C++ fields ", name_it->second, " and ",
                    cpp_name, " in class ", class_name,
                    " both map to Torque field ", torque_name);
      }
      SourcePosition position =
          positions != nullptr
              ? positions->at(fields.size())
              : MemberPosition(owner_cpp_name, cpp_name, default_position);
      fields.push_back(
          is_tail ? BuildTail(class_name, class_record, field_record, position)
                  : BuildField(class_name, field_record, position));
    };
    for (const JsonObject* group : groups) {
      const std::string& owner = RequireString(*group, "cpp_name", class_name);
      for (const JsonValue& field : RequireFields(*group, class_name)) {
        build(RequireFieldObject(field, class_name), owner,
              /*is_tail=*/false);
      }
    }
    if (tail_representation == TailRepresentation::kIndexedField) {
      build(*tail_record, RequireString(*groups.back(), "cpp_name", class_name),
            /*is_tail=*/true);
    } else if (tail_representation == TailRepresentation::kIndexedSections) {
      // C++ does not encode the Torque expressions that divide this tail. Parse
      // them together at the tail position while still verifying its storage.
      SourcePosition position =
          positions != nullptr
              ? positions->at(fields.size())
              : MemberPosition(
                    RequireString(*groups.back(), "cpp_name", class_name),
                    RequireString(*tail_record, "cpp_name", class_name),
                    default_position);
      CurrentSourcePosition::Scope position_scope(position);
      for (ClassFieldExpression& section :
           ParseTorqueClassFields(*AnnotationArgument(
               class_record, "V8_TQ_TAIL_SECTIONS", class_name))) {
        fields.push_back(std::move(section));
      }
    }
    return fields;
  }

  bool FieldsEquivalent(const std::string& class_name,
                        const std::vector<ClassFieldExpression>& declared,
                        const std::vector<ClassFieldExpression>& built) {
    if (declared.size() != built.size()) {
      Error("class ", class_name, ": .tq declares ", declared.size(),
            " field(s), C++ ", built.size())
          .Position(declared.empty()
                        ? CurrentSourcePosition::Get()
                        : declared.front().name_and_type.name->pos);
      return false;
    }
    bool equivalent = true;
    for (size_t i = 0; i < declared.size(); ++i) {
      // Errors building comparison keys should point at this field.
      CurrentSourcePosition::Scope position_scope(
          declared[i].name_and_type.name->pos);
      std::string a = FieldKey(declared[i]);
      std::string b = FieldKey(built[i]);
      if (a != b) {
        Error("class ", class_name, ": .tq field \"", a,
              "\" differs from C++ field \"", b, "\"")
            .Position(declared[i].name_and_type.name->pos);
        equivalent = false;
      }
    }
    return equivalent;
  }

  ClassFieldExpression BuildField(const std::string& class_name,
                                  const JsonObject& record,
                                  SourcePosition position) {
    std::string name = TorqueFieldName(record, class_name);
    std::string context = class_name + "." + name;
    CurrentSourcePosition::Scope position_scope(position);

    std::optional<ClassFieldIndexInfo> index;
    if (Lookup(record, "array_extent") != nullptr) {
      size_t extent = RequireSize(record, "array_extent", context);
      Expression* extent_expr;
      if (const std::string* extent_name =
              AnnotationArgument(record, "V8_TQ_EXTENT_NAME", context)) {
        // The size checks verify the V8_TQ_EXTENT_NAME expression.
        extent_expr = ParseTorqueExpression(*extent_name);
      } else {
        extent_expr =
            MakeNode<IntegerLiteralExpression>(IntegerLiteral(false, extent));
      }
      index = ClassFieldIndexInfo{extent_expr, false};
    }
    TypeExpression* type = RequireSize(record, "size", context) == 0
                               ? NamedType("void")
                               : BuildFieldType(context, record, "storage");
    return {NameAndTypeExpression{MakeNode<Identifier>(name), type},
            std::move(index),
            {},
            HasAnnotation(record, "V8_TQ_CUSTOM_WEAK", context),
            HasAnnotation(record, "V8_TQ_CONST", context),
            ReadSynchronization(record, context)};
  }

  ClassFieldExpression BuildTail(const std::string& class_name,
                                 const JsonObject& class_record,
                                 const JsonObject& record,
                                 SourcePosition position) {
    std::string name =
        *AnnotationArgument(class_record, "V8_TQ_TAIL_NAME", class_name);
    std::string context = class_name + "." + name;
    CurrentSourcePosition::Scope position_scope(position);
    const std::string& length_field =
        *AnnotationArgument(class_record, "V8_TQ_TAIL_LENGTH", context);
    ClassFieldIndexInfo index{
        MakeNode<IdentifierExpression>(MakeNode<Identifier>(length_field)),
        false};
    return {NameAndTypeExpression{
                MakeNode<Identifier>(name),
                BuildFieldType(context, record, "element_storage")},
            std::move(index),
            {},
            HasAnnotation(record, "V8_TQ_CUSTOM_WEAK", context),
            HasAnnotation(record, "V8_TQ_CONST", context),
            ReadSynchronization(record, context)};
  }

  TypeExpression* BuildType(const std::string& context,
                            const JsonObject& record, const std::string& key) {
    const JsonValue& value = Require(record, key, context);
    if (!value.IsObject()) {
      ReportError("layout JSON: ", context, ": \"", key, "\" is not an object");
    }
    return BuildTypeExpression(context, value.ToObject());
  }

  TypeExpression* NamedType(const std::string& name,
                            TypeExpression* argument = nullptr) {
    std::vector<TypeExpression*> arguments;
    if (argument != nullptr) arguments.push_back(argument);
    std::string identifier = name;
    std::vector<std::string> namespaces = SplitQualification(&identifier);
    return MakeNode<BasicTypeExpression>(std::move(namespaces),
                                         MakeNode<Identifier>(identifier),
                                         std::move(arguments));
  }

  // Map C++ storage to the Torque field type.
  TypeExpression* DeriveType(const std::string& context,
                             const JsonObject& storage) {
    const std::string& kind = RequireString(storage, "kind", context);
    if (kind == "tagged" || kind == "unaligned") {
      return BuildType(context, storage, "arg");
    }
    if (kind == "protected_tagged") {
      return NamedType("ProtectedPointer", BuildType(context, storage, "arg"));
    }
    if (kind == "trusted_pointer") {
      // An object's own pointer-table entry has no pointee type.
      return Lookup(storage, "arg") == nullptr
                 ? NamedType("TrustedPointer")
                 : NamedType("TrustedPointer",
                             BuildType(context, storage, "arg"));
    }
    if (kind == "external_pointer") return NamedType("ExternalPointer");
    if (kind == "cpp_heap_pointer") return NamedType("CppHeapPointer");
    // Dispatch handles are typed int32 (see js-function.tq).
    if (kind == "js_dispatch_handle") return NamedType("int32");
    if (kind == "bool") return NamedType("bool");
    if (kind == "bitfield") {
      return NamedType(TorqueBitFieldStructName(storage, context));
    }
    if (kind == "enum" || kind == "struct") {
      return NamedType(TorqueTypeName(RequireString(storage, "name", context)));
    }
    if (kind == "int") {
      if (const JsonValue* alias = Lookup(storage, "pointer_width_alias")) {
        if (!alias->IsString()) {
          ReportError("layout JSON: ", context,
                      ": \"pointer_width_alias\" is not a string");
        }
        return NamedType(TorqueTypeName(alias->ToString()));
      }
      bool is_signed = OptionalBool(storage, "signed", context);
      size_t bits = RequireSize(storage, "width", context) * 8;
      return NamedType((is_signed ? "int" : "uint") + std::to_string(bits));
    }
    ReportError("layout JSON: ", context, ": unknown storage kind \"", kind,
                "\"");
  }

  TypeExpression* BuildFieldType(const std::string& context,
                                 const JsonObject& record,
                                 const std::string& storage_key) {
    if (const std::string* override =
            AnnotationArgument(record, "V8_TQ_TYPE", context)) {
      return ParseTorqueTypeExpression(*override);
    }
    const JsonValue& storage = Require(record, storage_key, context);
    if (!storage.IsObject()) {
      ReportError("layout JSON: ", context, ": \"", storage_key,
                  "\" is not an object");
    }
    return DeriveType(context, storage.ToObject());
  }

  // Map a supported C++ class template to a Torque type. Torque has no generic
  // class types, so known instantiations get specific names. For example,
  // CppGCManaged<BackingStore> becomes ManagedBackingStore. Other
  // instantiations use the template's Torque base type. Return nullptr if the
  // template is not supported.
  const char* ClassTemplateTorqueName(const std::string& name,
                                      const JsonObject& type) {
    const char* base_name = nullptr;
    for (const PrimitiveTypeName& entry : kClassTemplateNames) {
      if (name == entry.cpp) base_name = entry.torque;
    }
    if (base_name == nullptr) return nullptr;

    const JsonValue* args = Lookup(type, "args");
    if (args == nullptr || !args->IsArray() || args->ToArray().size() != 1 ||
        !args->ToArray()[0].IsObject()) {
      return base_name;
    }
    const JsonObject& payload = args->ToArray()[0].ToObject();
    const JsonValue* payload_name = Lookup(payload, "name");
    if (payload_name == nullptr || !payload_name->IsString()) return base_name;

    std::string key = name + "<";
    if (const JsonValue* namespaces = Lookup(payload, "namespaces")) {
      if (namespaces->IsArray()) {
        for (const JsonValue& part : namespaces->ToArray()) {
          if (part.IsString()) key += part.ToString() + "::";
        }
      }
    }
    key += payload_name->ToString() + ">";
    for (const PrimitiveTypeName& entry : kClassTemplateInstantiations) {
      if (key == entry.cpp) return entry.torque;
    }
    return base_name;
  }

  TypeExpression* BuildTypeExpression(const std::string& context,
                                      const JsonObject& type) {
    const std::string& kind = RequireString(type, "kind", context);
    if (kind == "name") {
      if (const char* torque = ClassTemplateTorqueName(
              RequireString(type, "name", context), type)) {
        return MakeNode<BasicTypeExpression>(std::vector<std::string>{},
                                             MakeNode<Identifier>(torque),
                                             std::vector<TypeExpression*>{});
      }
      std::vector<TypeExpression*> args;
      if (const JsonValue* args_value = Lookup(type, "args")) {
        if (!args_value->IsArray()) {
          ReportError("layout JSON: ", context,
                      ": type \"args\" is not an array");
        }
        for (const JsonValue& arg : args_value->ToArray()) {
          if (!arg.IsObject()) {
            ReportError("layout JSON: ", context,
                        ": type argument is not an object");
          }
          args.push_back(BuildTypeExpression(context, arg.ToObject()));
        }
      }
      std::vector<std::string> namespaces;
      if (const JsonValue* namespaces_value = Lookup(type, "namespaces")) {
        if (!namespaces_value->IsArray()) {
          ReportError("layout JSON: ", context,
                      ": type \"namespaces\" is not an array");
        }
        for (const JsonValue& part : namespaces_value->ToArray()) {
          if (!part.IsString()) {
            ReportError("layout JSON: ", context,
                        ": type namespace is not a string");
          }
          namespaces.push_back(part.ToString());
        }
      }
      std::string identifier =
          TorqueTypeName(RequireString(type, "name", context));
      for (std::string& part : SplitQualification(&identifier)) {
        namespaces.push_back(std::move(part));
      }
      return MakeNode<BasicTypeExpression>(std::move(namespaces),
                                           MakeNode<Identifier>(identifier),
                                           std::move(args));
    }
    if (kind == "union") {
      const JsonValue& members = Require(type, "members", context);
      if (!members.IsArray() || members.ToArray().size() < 2) {
        ReportError("layout JSON: ", context, ": malformed union type");
      }
      TypeExpression* result = nullptr;
      for (const JsonValue& member : members.ToArray()) {
        if (!member.IsObject()) {
          ReportError("layout JSON: ", context,
                      ": union member is not an object");
        }
        TypeExpression* next = BuildTypeExpression(context, member.ToObject());
        result = result == nullptr
                     ? next
                     : MakeNode<UnionTypeExpression>(result, next);
      }
      return result;
    }
    ReportError("layout JSON: ", context, ": unknown type kind \"", kind, "\"");
  }

  // Parse a 1-based "path:line:column" C++ source position.
  SourcePosition ParsePosition(const std::string& position) {
    size_t column_sep = position.rfind(':');
    size_t line_sep = column_sep == std::string::npos
                          ? std::string::npos
                          : position.rfind(':', column_sep - 1);
    if (line_sep == std::string::npos) {
      ReportError("layout JSON: malformed position \"", position, "\"");
    }
    std::string path = position.substr(0, line_sep);
    int line = std::atoi(position.substr(line_sep + 1).c_str());
    int column = std::atoi(position.substr(column_sep + 1).c_str());
    if (line <= 0 || column <= 0) {
      ReportError("layout JSON: malformed position \"", position, "\"");
    }
    auto it = source_ids_.find(path);
    if (it == source_ids_.end()) {
      it = source_ids_.emplace(path, SourceFileMap::AddSource(path)).first;
    }
    // Torque positions are 0-based; the JSON's are 1-based.
    LineAndColumn start =
        LineAndColumn::WithUnknownOffset(line - 1, column - 1);
    return SourcePosition{it->second, start, start};
  }

  std::map<std::string, const JsonObject*> records_by_cpp_name_;
  std::map<std::string, const JsonObject*> records_by_torque_name_;
  std::map<std::string, std::map<std::string, SourcePosition>> positions_;
  // Bitfield structs declared in .tq or imported from JSON, indexed by name.
  std::map<std::string, BitFieldStructDeclaration*> bit_field_structs_;
  std::vector<Declaration*> new_bit_field_structs_;
  std::map<std::string, SourceId> source_ids_;
};

}  // namespace

void VerifyCppLayouts(const std::string& layout_json_path) {
  LayoutVerifier().Run(layout_json_path);
}

void ImportCppLayouts(const std::string& layout_json_path,
                      const std::string& positions_path) {
  LayoutImporter().Run(layout_json_path, positions_path);
}

}  // namespace v8::internal::torque
