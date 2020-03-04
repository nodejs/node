// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

constexpr char kTqObjectOverrideDecls[] =
    R"(  std::vector<std::unique_ptr<ObjectProperty>> GetProperties(
      d::MemoryAccessor accessor) const override;
  const char* GetName() const override;
  void Visit(TqObjectVisitor* visitor) const override;
  bool IsSuperclassOf(const TqObject* other) const override;
)";

constexpr char kObjectClassListDefinition[] = R"(
const d::ClassList kObjectClassList {
  sizeof(kObjectClassNames) / sizeof(const char*),
  kObjectClassNames,
};
)";

namespace {
enum TypeStorage {
  kAsStoredInHeap,
  kUncompressed,
};

// A convenient way to keep track of several different ways that we might need
// to represent a field's type in the generated C++.
class DebugFieldType {
 public:
  explicit DebugFieldType(const Field& field) : field_(field) {}

  bool IsTagged() const {
    return field_.name_and_type.type->IsSubtypeOf(TypeOracle::GetTaggedType());
  }

  // Returns the type that should be used for this field's value within code
  // that is compiled as part of the debug helper library. In particular, this
  // simplifies any tagged type to a plain uintptr_t because the debug helper
  // compiles without most of the V8 runtime code.
  std::string GetValueType(TypeStorage storage) const {
    if (IsTagged()) {
      return storage == kAsStoredInHeap ? "i::Tagged_t" : "uintptr_t";
    }
    // Note that we need constexpr names to resolve correctly in the global
    // namespace, because we're passing them as strings to a debugging
    // extension. We can verify this during build of the debug helper, because
    // we use this type for a local variable below, and generate this code in
    // a disjoint namespace. However, we can't emit a useful error at this
    // point. Instead we'll emit a comment that might be helpful.
    return GetOriginalType(storage) +
           " /*Failing? Ensure constexpr type name is fully qualified and "
           "necessary #includes are in debug-helper-internal.h*/";
  }

  // Returns the type that should be used to represent a field's type to
  // debugging tools that have full V8 symbols. The types returned from this
  // method are fully qualified and may refer to object types that are not
  // included in the compilation of the debug helper library.
  std::string GetOriginalType(TypeStorage storage) const {
    if (field_.name_and_type.type->IsStructType()) {
      // There's no meaningful type we could use here, because the V8 symbols
      // don't have any definition of a C++ struct matching this struct type.
      return "";
    }
    if (IsTagged()) {
      if (storage == kAsStoredInHeap && COMPRESS_POINTERS_BOOL) {
        return "v8::internal::TaggedValue";
      }
      base::Optional<const ClassType*> field_class_type =
          field_.name_and_type.type->ClassSupertype();
      return "v8::internal::" +
             (field_class_type.has_value()
                  ? (*field_class_type)->GetGeneratedTNodeTypeName()
                  : "Object");
    }
    const Type* constexpr_version =
        field_.name_and_type.type->ConstexprVersion();
    if (constexpr_version == nullptr) {
      Error("Type '", field_.name_and_type.type->ToString(),
            "' requires a constexpr representation")
          .Position(field_.pos);
      return "";
    }
    return constexpr_version->GetGeneratedTypeName();
  }

  // Returns the field's size in bytes.
  size_t GetSize() const {
    size_t size = 0;
    std::tie(size, std::ignore) = field_.GetFieldSizeInformation();
    return size;
  }

  // Returns the name of the function for getting this field's address.
  std::string GetAddressGetter() {
    return "Get" + CamelifyString(field_.name_and_type.name) + "Address";
  }

 private:
  const Field& field_;
};

// Emits a function to get the address of a field within a class, based on the
// member variable {address_}, which is a tagged pointer. Example
// implementation:
//
// uintptr_t TqFixedArray::GetObjectsAddress() const {
//   return address_ - i::kHeapObjectTag + 16;
// }
void GenerateFieldAddressAccessor(const Field& field,
                                  const std::string& class_name,
                                  std::ostream& h_contents,
                                  std::ostream& cc_contents) {
  DebugFieldType debug_field_type(field);

  const std::string address_getter = debug_field_type.GetAddressGetter();

  h_contents << "  uintptr_t " << address_getter << "() const;\n";
  cc_contents << "\nuintptr_t Tq" << class_name << "::" << address_getter
              << "() const {\n";
  cc_contents << "  return address_ - i::kHeapObjectTag + " << field.offset
              << ";\n";
  cc_contents << "}\n";
}

// Emits a function to get the value of a field, or the value from an indexed
// position within an array field, based on the member variable {address_},
// which is a tagged pointer, and the parameter {accessor}, a function pointer
// that allows for fetching memory from the debuggee. The returned result
// includes both a "validity", indicating whether the memory could be fetched,
// and the fetched value. If the field contains tagged data, then these
// functions call EnsureDecompressed to expand compressed data. Example:
//
// Value<uintptr_t> TqMap::GetPrototypeValue(d::MemoryAccessor accessor) const {
//   i::Tagged_t value{};
//   d::MemoryAccessResult validity = accessor(
//       GetPrototypeAddress(),
//       reinterpret_cast<uint8_t*>(&value),
//       sizeof(value));
//   return {validity, EnsureDecompressed(value, address_)};
// }
//
// For array fields, an offset parameter is included. Example:
//
// Value<uintptr_t> TqFixedArray::GetObjectsValue(d::MemoryAccessor accessor,
//                                                size_t offset) const {
//   i::Tagged_t value{};
//   d::MemoryAccessResult validity = accessor(
//       GetObjectsAddress() + offset * sizeof(value),
//       reinterpret_cast<uint8_t*>(&value),
//       sizeof(value));
//   return {validity, EnsureDecompressed(value, address_)};
// }
void GenerateFieldValueAccessor(const Field& field,
                                const std::string& class_name,
                                std::ostream& h_contents,
                                std::ostream& cc_contents) {
  // Currently not implemented for struct fields.
  if (field.name_and_type.type->IsStructType()) return;

  DebugFieldType debug_field_type(field);

  const std::string address_getter = debug_field_type.GetAddressGetter();
  const std::string field_getter =
      "Get" + CamelifyString(field.name_and_type.name) + "Value";

  std::string index_param;
  std::string index_offset;
  if (field.index) {
    index_param = ", size_t offset";
    index_offset = " + offset * sizeof(value)";
  }

  if (!field.name_and_type.type->IsStructType()) {
    std::string field_value_type = debug_field_type.GetValueType(kUncompressed);
    h_contents << "  Value<" << field_value_type << "> " << field_getter
               << "(d::MemoryAccessor accessor " << index_param << ") const;\n";
    cc_contents << "\nValue<" << field_value_type << "> Tq" << class_name
                << "::" << field_getter << "(d::MemoryAccessor accessor"
                << index_param << ") const {\n";
    cc_contents << "  " << debug_field_type.GetValueType(kAsStoredInHeap)
                << " value{};\n";
    cc_contents << "  d::MemoryAccessResult validity = accessor("
                << address_getter << "()" << index_offset
                << ", reinterpret_cast<uint8_t*>(&value), sizeof(value));\n";
    cc_contents << "  return {validity, "
                << (debug_field_type.IsTagged()
                        ? "EnsureDecompressed(value, address_)"
                        : "value")
                << "};\n";
    cc_contents << "}\n";
  }
}

// Emits a portion of the member function GetProperties that is responsible for
// adding data about the current field to a result vector called "result".
// Example output:
//
// result.push_back(std::make_unique<ObjectProperty>(
//     "prototype",                                     // Field name
//     "v8::internal::HeapObject",                      // Field type
//     "v8::internal::HeapObject",                      // Decompressed type
//     GetPrototypeAddress(),                           // Field address
//     1,                                               // Number of values
//     8,                                               // Size of value
//     std::vector<std::unique_ptr<StructProperty>>(),  // Struct fields
//     d::PropertyKind::kSingle));                      // Field kind
//
// In builds with pointer compression enabled, the field type for tagged values
// is "v8::internal::TaggedValue" (a four-byte class) and the decompressed type
// is a normal Object subclass that describes the expanded eight-byte type.
//
// If the field is an array, then its length is fetched from the debuggee. This
// could fail if the debuggee has incomplete memory, so the "validity" from that
// fetch is used to determine the result PropertyKind, which will say whether
// the array's length is known.
//
// If the field's type is a struct, then a local variable is created and filled
// with descriptions of each of the struct's fields. The type and decompressed
// type in the ObjectProperty are set to the empty string, to indicate to the
// caller that the struct fields vector should be used instead.
//
// The following example is an array of structs, so it uses both of the optional
// components described above:
//
// std::vector<std::unique_ptr<StructProperty>> descriptors_struct_field_list;
// descriptors_struct_field_list.push_back(std::make_unique<StructProperty>(
//     "key",                                // Struct field name
//     "v8::internal::PrimitiveHeapObject",  // Struct field type
//     "v8::internal::PrimitiveHeapObject",  // Struct field decompressed type
//     0));                                  // Offset within struct data
// // The line above is repeated for other struct fields. Omitted here.
// Value<uint16_t> indexed_field_count =
//     GetNumberOfAllDescriptorsValue(accessor);  // Fetch the array length.
// result.push_back(std::make_unique<ObjectProperty>(
//     "descriptors",                                 // Field name
//     "",                                            // Field type
//     "",                                            // Decompressed type
//     GetDescriptorsAddress(),                       // Field address
//     indexed_field_count.value,                     // Number of values
//     24,                                            // Size of value
//     std::move(descriptors_struct_field_list),      // Struct fields
//     GetArrayKind(indexed_field_count.validity)));  // Field kind
void GenerateGetPropsChunkForField(const Field& field,
                                   std::ostream& get_props_impl) {
  DebugFieldType debug_field_type(field);

  // If the current field is a struct, create a vector describing its fields.
  std::string struct_field_list =
      "std::vector<std::unique_ptr<StructProperty>>()";
  if (const StructType* field_struct_type =
          StructType::DynamicCast(field.name_and_type.type)) {
    struct_field_list = field.name_and_type.name + "_struct_field_list";
    get_props_impl << "  std::vector<std::unique_ptr<StructProperty>> "
                   << struct_field_list << ";\n";
    for (const Field& struct_field : field_struct_type->fields()) {
      DebugFieldType struct_field_type(struct_field);
      get_props_impl << "  " << struct_field_list
                     << ".push_back(std::make_unique<StructProperty>(\""
                     << struct_field.name_and_type.name << "\", \""
                     << struct_field_type.GetOriginalType(kAsStoredInHeap)
                     << "\", \""
                     << struct_field_type.GetOriginalType(kUncompressed)
                     << "\", " << struct_field.offset << "));\n";
    }
    struct_field_list = "std::move(" + struct_field_list + ")";
  }

  // The number of values and property kind for non-indexed properties:
  std::string count_value = "1";
  std::string property_kind = "d::PropertyKind::kSingle";

  // If the field is indexed, emit a fetch of the array length, and change
  // count_value and property_kind to be the correct values for an array.
  if (field.index) {
    const Type* index_type = field.index->type;
    std::string index_type_name;
    if (index_type == TypeOracle::GetSmiType()) {
      index_type_name = "uintptr_t";
      count_value =
          "i::PlatformSmiTagging::SmiToInt(indexed_field_count.value)";
    } else if (!index_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
      const Type* constexpr_index = index_type->ConstexprVersion();
      if (constexpr_index == nullptr) {
        Error("Type '", index_type->ToString(),
              "' requires a constexpr representation");
        return;
      }
      index_type_name = constexpr_index->GetGeneratedTypeName();
      count_value = "indexed_field_count.value";
    } else {
      Error("Unsupported index type: ", index_type);
      return;
    }
    get_props_impl << "  Value<" << index_type_name
                   << "> indexed_field_count = Get"
                   << CamelifyString(field.index->name) << "Value(accessor);\n";
    property_kind = "GetArrayKind(indexed_field_count.validity)";
  }

  get_props_impl << "  result.push_back(std::make_unique<ObjectProperty>(\""
                 << field.name_and_type.name << "\", \""
                 << debug_field_type.GetOriginalType(kAsStoredInHeap)
                 << "\", \"" << debug_field_type.GetOriginalType(kUncompressed)
                 << "\", " << debug_field_type.GetAddressGetter() << "(), "
                 << count_value << ", " << debug_field_type.GetSize() << ", "
                 << struct_field_list << ", " << property_kind << "));\n";
}

// For any Torque-defined class Foo, this function generates a class TqFoo which
// allows for convenient inspection of objects of type Foo in a crash dump or
// time travel session (where we can't just run the object printer). The
// generated class looks something like this:
//
// class TqFoo : public TqParentOfFoo {
//  public:
//   // {address} is an uncompressed tagged pointer.
//   inline TqFoo(uintptr_t address) : TqParentOfFoo(address) {}
//
//   // Creates and returns a list of this object's properties.
//   std::vector<std::unique_ptr<ObjectProperty>> GetProperties(
//       d::MemoryAccessor accessor) const override;
//
//   // Returns the name of this class, "v8::internal::Foo".
//   const char* GetName() const override;
//
//   // Visitor pattern; implementation just calls visitor->VisitFoo(this).
//   void Visit(TqObjectVisitor* visitor) const override;
//
//   // Returns whether Foo is a superclass of the other object's type.
//   bool IsSuperclassOf(const TqObject* other) const override;
//
//   // Field accessors omitted here (see other comments above).
// };
//
// Four output streams are written:
//
// h_contents:  A header file which gets the class definition above.
// cc_contents: A cc file which gets implementations of that class's members.
// visitor:     A stream that is accumulating the definition of the class
//              TqObjectVisitor. Each class Foo gets its own virtual method
//              VisitFoo in TqObjectVisitor.
// class_names: A stream that is accumulating a list of strings including fully-
//              qualified names for every Torque-defined class type.
void GenerateClassDebugReader(const ClassType& type, std::ostream& h_contents,
                              std::ostream& cc_contents, std::ostream& visitor,
                              std::ostream& class_names,
                              std::unordered_set<const ClassType*>* done) {
  // Make sure each class only gets generated once.
  if (!done->insert(&type).second) return;
  const ClassType* super_type = type.GetSuperClass();

  // We must emit the classes in dependency order. If the super class hasn't
  // been emitted yet, go handle it first.
  if (super_type != nullptr) {
    GenerateClassDebugReader(*super_type, h_contents, cc_contents, visitor,
                             class_names, done);
  }

  // Classes with undefined layout don't grant any particular value here and may
  // not correspond with actual C++ classes, so skip them.
  if (type.HasUndefinedLayout()) return;

  const std::string name = type.name();
  const std::string super_name =
      super_type == nullptr ? "Object" : super_type->name();
  h_contents << "\nclass Tq" << name << " : public Tq" << super_name << " {\n";
  h_contents << " public:\n";
  h_contents << "  inline Tq" << name << "(uintptr_t address) : Tq"
             << super_name << "(address) {}\n";
  h_contents << kTqObjectOverrideDecls;

  cc_contents << "\nconst char* Tq" << name << "::GetName() const {\n";
  cc_contents << "  return \"v8::internal::" << name << "\";\n";
  cc_contents << "}\n";

  cc_contents << "\nvoid Tq" << name
              << "::Visit(TqObjectVisitor* visitor) const {\n";
  cc_contents << "  visitor->Visit" << name << "(this);\n";
  cc_contents << "}\n";

  cc_contents << "\nbool Tq" << name
              << "::IsSuperclassOf(const TqObject* other) const {\n";
  cc_contents
      << "  return GetName() != other->GetName() && dynamic_cast<const Tq"
      << name << "*>(other) != nullptr;\n";
  cc_contents << "}\n";

  // By default, the visitor method for this class just calls the visitor method
  // for this class's parent. This allows custom visitors to only override a few
  // classes they care about without needing to know about the entire hierarchy.
  visitor << "  virtual void Visit" << name << "(const Tq" << name
          << "* object) {\n";
  visitor << "    Visit" << super_name << "(object);\n";
  visitor << "  }\n";

  class_names << "  \"v8::internal::" << name << "\",\n";

  std::stringstream get_props_impl;

  for (const Field& field : type.fields()) {
    if (field.name_and_type.type == TypeOracle::GetVoidType()) continue;
    GenerateFieldAddressAccessor(field, name, h_contents, cc_contents);
    GenerateFieldValueAccessor(field, name, h_contents, cc_contents);
    GenerateGetPropsChunkForField(field, get_props_impl);
  }

  h_contents << "};\n";

  cc_contents << "\nstd::vector<std::unique_ptr<ObjectProperty>> Tq" << name
              << "::GetProperties(d::MemoryAccessor accessor) const {\n";
  // Start by getting the fields from the parent class.
  cc_contents << "  std::vector<std::unique_ptr<ObjectProperty>> result = Tq"
              << super_name << "::GetProperties(accessor);\n";
  // Then add the fields from this class.
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

    h_contents << "// Unset a windgi.h macro that causes conflicts.\n";
    h_contents << "#ifdef GetBValue\n";
    h_contents << "#undef GetBValue\n";
    h_contents << "#endif\n\n";

    cc_contents << "#include \"torque-generated/" << file_name << ".h\"\n";
    cc_contents << "#include \"include/v8-internal.h\"\n\n";
    cc_contents << "namespace i = v8::internal;\n\n";

    NamespaceScope h_namespaces(h_contents, {"v8_debug_helper_internal"});
    NamespaceScope cc_namespaces(cc_contents, {"v8_debug_helper_internal"});

    std::stringstream visitor;
    visitor << "\nclass TqObjectVisitor {\n";
    visitor << " public:\n";
    visitor << "  virtual void VisitObject(const TqObject* object) {}\n";

    std::stringstream class_names;

    std::unordered_set<const ClassType*> done;
    for (const TypeAlias* alias : GlobalContext::GetClasses()) {
      const ClassType* type = ClassType::DynamicCast(alias->type());
      GenerateClassDebugReader(*type, h_contents, cc_contents, visitor,
                               class_names, &done);
    }

    visitor << "};\n";
    h_contents << visitor.str();

    cc_contents << "\nconst char* kObjectClassNames[] {\n";
    cc_contents << class_names.str();
    cc_contents << "};\n";
    cc_contents << kObjectClassListDefinition;
  }
  WriteFile(output_directory + "/" + file_name + ".h", h_contents.str());
  WriteFile(output_directory + "/" + file_name + ".cc", cc_contents.str());
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
