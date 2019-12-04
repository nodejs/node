// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {
void GenerateClassDebugReader(const ClassType& type, std::ostream& h_contents,
                              std::ostream& cc_contents, std::ostream& visitor,
                              std::unordered_set<const ClassType*>* done) {
  // Make sure each class only gets generated once.
  if (!type.IsExtern() || !done->insert(&type).second) return;
  const ClassType* super_type = type.GetSuperClass();

  // We must emit the classes in dependency order. If the super class hasn't
  // been emitted yet, go handle it first.
  if (super_type != nullptr) {
    GenerateClassDebugReader(*super_type, h_contents, cc_contents, visitor,
                             done);
  }

  const std::string name = type.name();
  const std::string super_name =
      super_type == nullptr ? "Object" : super_type->name();
  h_contents << "\nclass Tq" << name << " : public Tq" << super_name << " {\n";
  h_contents << " public:\n";
  h_contents << "  inline Tq" << name << "(uintptr_t address) : Tq"
             << super_name << "(address) {}\n";
  h_contents << "  std::vector<std::unique_ptr<ObjectProperty>> "
                "GetProperties(d::MemoryAccessor accessor) const override;\n";
  h_contents << "  const char* GetName() const override;\n";
  h_contents << "  void Visit(TqObjectVisitor* visitor) const override;\n";

  cc_contents << "\nconst char* Tq" << name << "::GetName() const {\n";
  cc_contents << "  return \"v8::internal::" << name << "\";\n";
  cc_contents << "}\n";

  cc_contents << "\nvoid Tq" << name
              << "::Visit(TqObjectVisitor* visitor) const {\n";
  cc_contents << "  visitor->Visit" << name << "(this);\n";
  cc_contents << "}\n";

  visitor << "  virtual void Visit" << name << "(const Tq" << name
          << "* object) {\n";
  visitor << "    Visit" << super_name << "(object);\n";
  visitor << "  }\n";

  std::stringstream get_props_impl;

  for (const Field& field : type.fields()) {
    const Type* field_type = field.name_and_type.type;
    if (field_type == TypeOracle::GetVoidType()) continue;
    const std::string& field_name = field.name_and_type.name;
    bool is_field_tagged = field_type->IsSubtypeOf(TypeOracle::GetTaggedType());
    base::Optional<const ClassType*> field_class_type =
        field_type->ClassSupertype();
    size_t field_size = 0;
    std::string field_size_string;
    std::tie(field_size, field_size_string) = field.GetFieldSizeInformation();

    std::string field_value_type;
    std::string field_value_type_compressed;
    std::string field_cc_type;
    std::string field_cc_type_compressed;
    if (is_field_tagged) {
      field_value_type = "uintptr_t";
      field_value_type_compressed = "i::Tagged_t";
      field_cc_type = "v8::internal::" + (field_class_type.has_value()
                                              ? (*field_class_type)->name()
                                              : "Object");
      field_cc_type_compressed =
          COMPRESS_POINTERS_BOOL ? "v8::internal::TaggedValue" : field_cc_type;
    } else {
      const Type* constexpr_version = field_type->ConstexprVersion();
      if (constexpr_version == nullptr) {
        Error("Type '", field_type->ToString(),
              "' requires a constexpr representation");
        continue;
      }
      field_cc_type = constexpr_version->GetGeneratedTypeName();
      field_cc_type_compressed = field_cc_type;
      // Note that we need constexpr names to resolve correctly in the global
      // namespace, because we're passing them as strings to a debugging
      // extension. We can verify this during build of the debug helper, because
      // we use this type for a local variable below, and generate this code in
      // a disjoint namespace. However, we can't emit a useful error at this
      // point. Instead we'll emit a comment that might be helpful.
      field_value_type =
          field_cc_type +
          " /*Failing? Ensure constexpr type name is fully qualified and "
          "necessary #includes are in debug-helper-internal.h*/";
      field_value_type_compressed = field_value_type;
    }

    const std::string field_getter =
        "Get" + CamelifyString(field_name) + "Value";
    const std::string address_getter =
        "Get" + CamelifyString(field_name) + "Address";

    std::string indexed_field_info;
    std::string index_param;
    std::string index_offset;
    if (field.index) {
      const Type* index_type = (*field.index)->name_and_type.type;
      std::string index_type_name;
      std::string index_value;
      if (index_type == TypeOracle::GetSmiType()) {
        index_type_name = "uintptr_t";
        index_value =
            "i::PlatformSmiTagging::SmiToInt(indexed_field_count.value)";
      } else if (!index_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
        const Type* constexpr_index = index_type->ConstexprVersion();
        if (constexpr_index == nullptr) {
          Error("Type '", index_type->ToString(),
                "' requires a constexpr representation");
          continue;
        }
        index_type_name = constexpr_index->GetGeneratedTypeName();
        index_value = "indexed_field_count.value";
      } else {
        Error("Unsupported index type: ", index_type);
        continue;
      }
      get_props_impl << "  Value<" << index_type_name
                     << "> indexed_field_count = Get"
                     << CamelifyString((*field.index)->name_and_type.name)
                     << "Value(accessor);\n";
      indexed_field_info =
          ", " + index_value + ", GetArrayKind(indexed_field_count.validity)";
      index_param = ", size_t offset";
      index_offset = " + offset * sizeof(value)";
    }
    get_props_impl
        << "  result.push_back(v8::base::make_unique<ObjectProperty>(\""
        << field_name << "\", \"" << field_cc_type_compressed << "\", \""
        << field_cc_type << "\", " << address_getter << "()"
        << indexed_field_info << "));\n";

    h_contents << "  uintptr_t " << address_getter << "() const;\n";
    h_contents << "  Value<" << field_value_type << "> " << field_getter
               << "(d::MemoryAccessor accessor " << index_param << ") const;\n";
    cc_contents << "\nuintptr_t Tq" << name << "::" << address_getter
                << "() const {\n";
    cc_contents << "  return address_ - i::kHeapObjectTag + " << field.offset
                << ";\n";
    cc_contents << "}\n";
    cc_contents << "\nValue<" << field_value_type << "> Tq" << name
                << "::" << field_getter << "(d::MemoryAccessor accessor"
                << index_param << ") const {\n";
    cc_contents << "  " << field_value_type_compressed << " value{};\n";
    cc_contents << "  d::MemoryAccessResult validity = accessor("
                << address_getter << "()" << index_offset
                << ", reinterpret_cast<uint8_t*>(&value), sizeof(value));\n";
    cc_contents << "  return {validity, "
                << (is_field_tagged ? "Decompress(value, address_)" : "value")
                << "};\n";
    cc_contents << "}\n";
  }

  h_contents << "};\n";

  cc_contents << "\nstd::vector<std::unique_ptr<ObjectProperty>> Tq" << name
              << "::GetProperties(d::MemoryAccessor accessor) const {\n";
  cc_contents << "  std::vector<std::unique_ptr<ObjectProperty>> result = Tq"
              << super_name << "::GetProperties(accessor);\n";
  cc_contents << get_props_impl.str();
  cc_contents << "  return result;\n";
  cc_contents << "}\n";
}
}  // namespace

void ImplementationVisitor::GenerateClassDebugReaders(
    const std::string& output_directory) {
  const std::string file_name = "class-debug-readers-tq";
  std::stringstream h_contents;
  std::stringstream cc_contents;
  h_contents << "// Provides the ability to read object properties in\n";
  h_contents << "// postmortem or remote scenarios, where the debuggee's\n";
  h_contents << "// memory is not part of the current process's address\n";
  h_contents << "// space and must be read using a callback function.\n\n";
  {
    IncludeGuardScope include_guard(h_contents, file_name + ".h");

    h_contents << "#include <cstdint>\n";
    h_contents << "#include <vector>\n";
    h_contents
        << "\n#include \"tools/debug_helper/debug-helper-internal.h\"\n\n";

    cc_contents << "#include \"torque-generated/" << file_name << ".h\"\n";
    cc_contents << "#include \"include/v8-internal.h\"\n\n";
    cc_contents << "namespace i = v8::internal;\n\n";

    NamespaceScope h_namespaces(h_contents, {"v8_debug_helper_internal"});
    NamespaceScope cc_namespaces(cc_contents, {"v8_debug_helper_internal"});

    std::stringstream visitor;
    visitor << "\nclass TqObjectVisitor {\n";
    visitor << " public:\n";
    visitor << "  virtual void VisitObject(const TqObject* object) {}\n";

    std::unordered_set<const ClassType*> done;
    for (const TypeAlias* alias : GlobalContext::GetClasses()) {
      const ClassType* type = ClassType::DynamicCast(alias->type());
      GenerateClassDebugReader(*type, h_contents, cc_contents, visitor, &done);
    }

    visitor << "};\n";
    h_contents << visitor.str();
  }
  WriteFile(output_directory + "/" + file_name + ".h", h_contents.str());
  WriteFile(output_directory + "/" + file_name + ".cc", cc_contents.str());
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
