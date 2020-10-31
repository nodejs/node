// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/util/internal/default_value_objectwriter.h>

#include <unordered_map>

#include <google/protobuf/util/internal/constants.h>
#include <google/protobuf/util/internal/utility.h>
#include <google/protobuf/stubs/map_util.h>

namespace google {
namespace protobuf {
namespace util {
using util::Status;
using util::StatusOr;
namespace converter {

namespace {
// Helper function to convert string value to given data type by calling the
// passed converter function on the DataPiece created from "value" argument.
// If value is empty or if conversion fails, the default_value is returned.
template <typename T>
T ConvertTo(StringPiece value,
            StatusOr<T> (DataPiece::*converter_fn)() const, T default_value) {
  if (value.empty()) return default_value;
  StatusOr<T> result = (DataPiece(value, true).*converter_fn)();
  return result.ok() ? result.ValueOrDie() : default_value;
}
}  // namespace

DefaultValueObjectWriter::DefaultValueObjectWriter(
    TypeResolver* type_resolver, const google::protobuf::Type& type,
    ObjectWriter* ow)
    : typeinfo_(TypeInfo::NewTypeInfo(type_resolver)),
      own_typeinfo_(true),
      type_(type),
      current_(nullptr),
      root_(nullptr),
      suppress_empty_list_(false),
      preserve_proto_field_names_(false),
      use_ints_for_enums_(false),
      field_scrub_callback_(nullptr),
      ow_(ow) {}

DefaultValueObjectWriter::~DefaultValueObjectWriter() {
  if (own_typeinfo_) {
    delete typeinfo_;
  }
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderBool(
    StringPiece name, bool value) {
  if (current_ == nullptr) {
    ow_->RenderBool(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderInt32(
    StringPiece name, int32 value) {
  if (current_ == nullptr) {
    ow_->RenderInt32(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderUint32(
    StringPiece name, uint32 value) {
  if (current_ == nullptr) {
    ow_->RenderUint32(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderInt64(
    StringPiece name, int64 value) {
  if (current_ == nullptr) {
    ow_->RenderInt64(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderUint64(
    StringPiece name, uint64 value) {
  if (current_ == nullptr) {
    ow_->RenderUint64(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderDouble(
    StringPiece name, double value) {
  if (current_ == nullptr) {
    ow_->RenderDouble(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderFloat(
    StringPiece name, float value) {
  if (current_ == nullptr) {
    ow_->RenderBool(name, value);
  } else {
    RenderDataPiece(name, DataPiece(value));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderString(
    StringPiece name, StringPiece value) {
  if (current_ == nullptr) {
    ow_->RenderString(name, value);
  } else {
    // Since StringPiece is essentially a pointer, takes a copy of "value" to
    // avoid ownership issues.
    string_values_.emplace_back(new std::string(value));
    RenderDataPiece(name, DataPiece(*string_values_.back(), true));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderBytes(
    StringPiece name, StringPiece value) {
  if (current_ == nullptr) {
    ow_->RenderBytes(name, value);
  } else {
    // Since StringPiece is essentially a pointer, takes a copy of "value" to
    // avoid ownership issues.
    string_values_.emplace_back(new std::string(value));
    RenderDataPiece(name, DataPiece(*string_values_.back(), false, true));
  }
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::RenderNull(
    StringPiece name) {
  if (current_ == nullptr) {
    ow_->RenderNull(name);
  } else {
    RenderDataPiece(name, DataPiece::NullData());
  }
  return this;
}

void DefaultValueObjectWriter::RegisterFieldScrubCallBack(
    FieldScrubCallBackPtr field_scrub_callback) {
  field_scrub_callback_.reset(field_scrub_callback.release());
}

DefaultValueObjectWriter::Node* DefaultValueObjectWriter::CreateNewNode(
    const std::string& name, const google::protobuf::Type* type, NodeKind kind,
    const DataPiece& data, bool is_placeholder,
    const std::vector<std::string>& path, bool suppress_empty_list,
    bool preserve_proto_field_names, bool use_ints_for_enums,
    FieldScrubCallBack* field_scrub_callback) {
  return new Node(name, type, kind, data, is_placeholder, path,
                  suppress_empty_list, preserve_proto_field_names,
                  use_ints_for_enums, field_scrub_callback);
}

DefaultValueObjectWriter::Node::Node(
    const std::string& name, const google::protobuf::Type* type, NodeKind kind,
    const DataPiece& data, bool is_placeholder,
    const std::vector<std::string>& path, bool suppress_empty_list,
    bool preserve_proto_field_names, bool use_ints_for_enums,
    FieldScrubCallBack* field_scrub_callback)
    : name_(name),
      type_(type),
      kind_(kind),
      is_any_(false),
      data_(data),
      is_placeholder_(is_placeholder),
      path_(path),
      suppress_empty_list_(suppress_empty_list),
      preserve_proto_field_names_(preserve_proto_field_names),
      use_ints_for_enums_(use_ints_for_enums),
      field_scrub_callback_(field_scrub_callback) {}

DefaultValueObjectWriter::Node* DefaultValueObjectWriter::Node::FindChild(
    StringPiece name) {
  if (name.empty() || kind_ != OBJECT) {
    return nullptr;
  }
  for (int i = 0; i < children_.size(); ++i) {
    Node* child = children_[i];
    if (child->name() == name) {
      return child;
    }
  }
  return nullptr;
}

void DefaultValueObjectWriter::Node::WriteTo(ObjectWriter* ow) {
  if (kind_ == PRIMITIVE) {
    ObjectWriter::RenderDataPieceTo(data_, name_, ow);
    return;
  }

  // Render maps. Empty maps are rendered as "{}".
  if (kind_ == MAP) {
    ow->StartObject(name_);
    WriteChildren(ow);
    ow->EndObject();
    return;
  }

  // Write out lists. If we didn't have any list in response, write out empty
  // list.
  if (kind_ == LIST) {
    // Suppress empty lists if requested.
    if (suppress_empty_list_ && is_placeholder_) return;

    ow->StartList(name_);
    WriteChildren(ow);
    ow->EndList();
    return;
  }

  // If is_placeholder_ = true, we didn't see this node in the response, so
  // skip output.
  if (is_placeholder_) return;

  ow->StartObject(name_);
  WriteChildren(ow);
  ow->EndObject();
}

void DefaultValueObjectWriter::Node::WriteChildren(ObjectWriter* ow) {
  for (int i = 0; i < children_.size(); ++i) {
    Node* child = children_[i];
    child->WriteTo(ow);
  }
}

const google::protobuf::Type* DefaultValueObjectWriter::Node::GetMapValueType(
    const google::protobuf::Type& found_type, const TypeInfo* typeinfo) {
  // If this field is a map, we should use the type of its "Value" as
  // the type of the child node.
  for (int i = 0; i < found_type.fields_size(); ++i) {
    const google::protobuf::Field& sub_field = found_type.fields(i);
    if (sub_field.number() != 2) {
      continue;
    }
    if (sub_field.kind() != google::protobuf::Field_Kind_TYPE_MESSAGE) {
      // This map's value type is not a message type. We don't need to
      // get the field_type in this case.
      break;
    }
    util::StatusOr<const google::protobuf::Type*> sub_type =
        typeinfo->ResolveTypeUrl(sub_field.type_url());
    if (!sub_type.ok()) {
      GOOGLE_LOG(WARNING) << "Cannot resolve type '" << sub_field.type_url() << "'.";
    } else {
      return sub_type.ValueOrDie();
    }
    break;
  }
  return nullptr;
}

void DefaultValueObjectWriter::Node::PopulateChildren(
    const TypeInfo* typeinfo) {
  // Ignores well known types that don't require automatically populating their
  // primitive children. For type "Any", we only populate its children when the
  // "@type" field is set.
  // TODO(tsun): remove "kStructValueType" from the list. It's being checked
  //     now because of a bug in the tool-chain that causes the "oneof_index"
  //     of kStructValueType to not be set correctly.
  if (type_ == nullptr || type_->name() == kAnyType ||
      type_->name() == kStructType || type_->name() == kTimestampType ||
      type_->name() == kDurationType || type_->name() == kStructValueType) {
    return;
  }
  std::vector<Node*> new_children;
  std::unordered_map<std::string, int> orig_children_map;

  // Creates a map of child nodes to speed up lookup.
  for (int i = 0; i < children_.size(); ++i) {
    InsertIfNotPresent(&orig_children_map, children_[i]->name_, i);
  }

  for (int i = 0; i < type_->fields_size(); ++i) {
    const google::protobuf::Field& field = type_->fields(i);

    // This code is checking if the field to be added to the tree should be
    // scrubbed or not by calling the field_scrub_callback_ callback function.
    std::vector<std::string> path;
    if (!path_.empty()) {
      path.insert(path.begin(), path_.begin(), path_.end());
    }
    path.push_back(field.name());
    if (field_scrub_callback_ != nullptr &&
        field_scrub_callback_->Run(path, &field)) {
      continue;
    }

    std::unordered_map<std::string, int>::iterator found =
        orig_children_map.find(field.name());
    // If the child field has already been set, we just add it to the new list
    // of children.
    if (found != orig_children_map.end()) {
      new_children.push_back(children_[found->second]);
      children_[found->second] = nullptr;
      continue;
    }

    const google::protobuf::Type* field_type = nullptr;
    bool is_map = false;
    NodeKind kind = PRIMITIVE;

    if (field.kind() == google::protobuf::Field_Kind_TYPE_MESSAGE) {
      kind = OBJECT;
      util::StatusOr<const google::protobuf::Type*> found_result =
          typeinfo->ResolveTypeUrl(field.type_url());
      if (!found_result.ok()) {
        // "field" is of an unknown type.
        GOOGLE_LOG(WARNING) << "Cannot resolve type '" << field.type_url() << "'.";
      } else {
        const google::protobuf::Type* found_type = found_result.ValueOrDie();
        is_map = IsMap(field, *found_type);

        if (!is_map) {
          field_type = found_type;
        } else {
          // If this field is a map, we should use the type of its "Value" as
          // the type of the child node.
          field_type = GetMapValueType(*found_type, typeinfo);
          kind = MAP;
        }
      }
    }

    if (!is_map &&
        field.cardinality() ==
            google::protobuf::Field_Cardinality_CARDINALITY_REPEATED) {
      kind = LIST;
    }

    // If oneof_index() != 0, the child field is part of a "oneof", which means
    // the child field is optional and we shouldn't populate its default
    // primitive value.
    if (field.oneof_index() != 0 && kind == PRIMITIVE) continue;

    // If the child field is of primitive type, sets its data to the default
    // value of its type.
    std::unique_ptr<Node> child(
        new Node(preserve_proto_field_names_ ? field.name() : field.json_name(),
                 field_type, kind,
                 kind == PRIMITIVE ? CreateDefaultDataPieceForField(
                                         field, typeinfo, use_ints_for_enums_)
                                   : DataPiece::NullData(),
                 true, path, suppress_empty_list_, preserve_proto_field_names_,
                 use_ints_for_enums_, field_scrub_callback_));
    new_children.push_back(child.release());
  }
  // Adds all leftover nodes in children_ to the beginning of new_child.
  for (int i = 0; i < children_.size(); ++i) {
    if (children_[i] == nullptr) {
      continue;
    }
    new_children.insert(new_children.begin(), children_[i]);
    children_[i] = nullptr;
  }
  children_.swap(new_children);
}

void DefaultValueObjectWriter::MaybePopulateChildrenOfAny(Node* node) {
  // If this is an "Any" node with "@type" already given and no other children
  // have been added, populates its children.
  if (node != nullptr && node->is_any() && node->type() != nullptr &&
      node->type()->name() != kAnyType && node->number_of_children() == 1) {
    node->PopulateChildren(typeinfo_);
  }
}

DataPiece DefaultValueObjectWriter::FindEnumDefault(
    const google::protobuf::Field& field, const TypeInfo* typeinfo,
    bool use_ints_for_enums) {
  if (!field.default_value().empty())
    return DataPiece(field.default_value(), true);

  const google::protobuf::Enum* enum_type =
      typeinfo->GetEnumByTypeUrl(field.type_url());
  if (!enum_type) {
    GOOGLE_LOG(WARNING) << "Could not find enum with type '" << field.type_url()
                 << "'";
    return DataPiece::NullData();
  }
  // We treat the first value as the default if none is specified.
  return enum_type->enumvalue_size() > 0
             ? (use_ints_for_enums
                    ? DataPiece(enum_type->enumvalue(0).number())
                    : DataPiece(enum_type->enumvalue(0).name(), true))
             : DataPiece::NullData();
}

DataPiece DefaultValueObjectWriter::CreateDefaultDataPieceForField(
    const google::protobuf::Field& field, const TypeInfo* typeinfo,
    bool use_ints_for_enums) {
  switch (field.kind()) {
    case google::protobuf::Field_Kind_TYPE_DOUBLE: {
      return DataPiece(ConvertTo<double>(
          field.default_value(), &DataPiece::ToDouble, static_cast<double>(0)));
    }
    case google::protobuf::Field_Kind_TYPE_FLOAT: {
      return DataPiece(ConvertTo<float>(
          field.default_value(), &DataPiece::ToFloat, static_cast<float>(0)));
    }
    case google::protobuf::Field_Kind_TYPE_INT64:
    case google::protobuf::Field_Kind_TYPE_SINT64:
    case google::protobuf::Field_Kind_TYPE_SFIXED64: {
      return DataPiece(ConvertTo<int64>(
          field.default_value(), &DataPiece::ToInt64, static_cast<int64>(0)));
    }
    case google::protobuf::Field_Kind_TYPE_UINT64:
    case google::protobuf::Field_Kind_TYPE_FIXED64: {
      return DataPiece(ConvertTo<uint64>(
          field.default_value(), &DataPiece::ToUint64, static_cast<uint64>(0)));
    }
    case google::protobuf::Field_Kind_TYPE_INT32:
    case google::protobuf::Field_Kind_TYPE_SINT32:
    case google::protobuf::Field_Kind_TYPE_SFIXED32: {
      return DataPiece(ConvertTo<int32>(
          field.default_value(), &DataPiece::ToInt32, static_cast<int32>(0)));
    }
    case google::protobuf::Field_Kind_TYPE_BOOL: {
      return DataPiece(
          ConvertTo<bool>(field.default_value(), &DataPiece::ToBool, false));
    }
    case google::protobuf::Field_Kind_TYPE_STRING: {
      return DataPiece(field.default_value(), true);
    }
    case google::protobuf::Field_Kind_TYPE_BYTES: {
      return DataPiece(field.default_value(), false, true);
    }
    case google::protobuf::Field_Kind_TYPE_UINT32:
    case google::protobuf::Field_Kind_TYPE_FIXED32: {
      return DataPiece(ConvertTo<uint32>(
          field.default_value(), &DataPiece::ToUint32, static_cast<uint32>(0)));
    }
    case google::protobuf::Field_Kind_TYPE_ENUM: {
      return FindEnumDefault(field, typeinfo, use_ints_for_enums);
    }
    default: {
      return DataPiece::NullData();
    }
  }
}

DefaultValueObjectWriter* DefaultValueObjectWriter::StartObject(
    StringPiece name) {
  if (current_ == nullptr) {
    std::vector<std::string> path;
    root_.reset(CreateNewNode(
        std::string(name), &type_, OBJECT, DataPiece::NullData(), false, path,
        suppress_empty_list_, preserve_proto_field_names_, use_ints_for_enums_,
        field_scrub_callback_.get()));
    root_->PopulateChildren(typeinfo_);
    current_ = root_.get();
    return this;
  }
  MaybePopulateChildrenOfAny(current_);
  Node* child = current_->FindChild(name);
  if (current_->kind() == LIST || current_->kind() == MAP || child == nullptr) {
    // If current_ is a list or a map node, we should create a new child and use
    // the type of current_ as the type of the new child.
    std::unique_ptr<Node> node(
        CreateNewNode(std::string(name),
                      ((current_->kind() == LIST || current_->kind() == MAP)
                           ? current_->type()
                           : nullptr),
                      OBJECT, DataPiece::NullData(), false,
                      child == nullptr ? current_->path() : child->path(),
                      suppress_empty_list_, preserve_proto_field_names_,
                      use_ints_for_enums_, field_scrub_callback_.get()));
    child = node.get();
    current_->AddChild(node.release());
  }

  child->set_is_placeholder(false);
  if (child->kind() == OBJECT && child->number_of_children() == 0) {
    child->PopulateChildren(typeinfo_);
  }

  stack_.push(current_);
  current_ = child;
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::EndObject() {
  if (stack_.empty()) {
    // The root object ends here. Writes out the tree.
    WriteRoot();
    return this;
  }
  current_ = stack_.top();
  stack_.pop();
  return this;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::StartList(
    StringPiece name) {
  if (current_ == nullptr) {
    std::vector<std::string> path;
    root_.reset(CreateNewNode(
        std::string(name), &type_, LIST, DataPiece::NullData(), false, path,
        suppress_empty_list_, preserve_proto_field_names_, use_ints_for_enums_,
        field_scrub_callback_.get()));
    current_ = root_.get();
    return this;
  }
  MaybePopulateChildrenOfAny(current_);
  Node* child = current_->FindChild(name);
  if (child == nullptr || child->kind() != LIST) {
    std::unique_ptr<Node> node(CreateNewNode(
        std::string(name), nullptr, LIST, DataPiece::NullData(), false,
        child == nullptr ? current_->path() : child->path(),
        suppress_empty_list_, preserve_proto_field_names_, use_ints_for_enums_,
        field_scrub_callback_.get()));
    child = node.get();
    current_->AddChild(node.release());
  }
  child->set_is_placeholder(false);

  stack_.push(current_);
  current_ = child;
  return this;
}

void DefaultValueObjectWriter::WriteRoot() {
  root_->WriteTo(ow_);
  root_.reset(nullptr);
  current_ = nullptr;
}

DefaultValueObjectWriter* DefaultValueObjectWriter::EndList() {
  if (stack_.empty()) {
    WriteRoot();
    return this;
  }
  current_ = stack_.top();
  stack_.pop();
  return this;
}

void DefaultValueObjectWriter::RenderDataPiece(StringPiece name,
                                               const DataPiece& data) {
  MaybePopulateChildrenOfAny(current_);
  if (current_->type() != nullptr && current_->type()->name() == kAnyType &&
      name == "@type") {
    util::StatusOr<std::string> data_string = data.ToString();
    if (data_string.ok()) {
      const std::string& string_value = data_string.ValueOrDie();
      // If the type of current_ is "Any" and its "@type" field is being set
      // here, sets the type of current_ to be the type specified by the
      // "@type".
      util::StatusOr<const google::protobuf::Type*> found_type =
          typeinfo_->ResolveTypeUrl(string_value);
      if (!found_type.ok()) {
        GOOGLE_LOG(WARNING) << "Failed to resolve type '" << string_value << "'.";
      } else {
        current_->set_type(found_type.ValueOrDie());
      }
      current_->set_is_any(true);
      // If the "@type" field is placed after other fields, we should populate
      // other children of primitive type now. Otherwise, we should wait until
      // the first value field is rendered before we populate the children,
      // because the "value" field of a Any message could be omitted.
      if (current_->number_of_children() > 1 && current_->type() != nullptr) {
        current_->PopulateChildren(typeinfo_);
      }
    }
  }
  Node* child = current_->FindChild(name);
  if (child == nullptr || child->kind() != PRIMITIVE) {
    // No children are found, creates a new child.
    std::unique_ptr<Node> node(
        CreateNewNode(std::string(name), nullptr, PRIMITIVE, data, false,
                      child == nullptr ? current_->path() : child->path(),
                      suppress_empty_list_, preserve_proto_field_names_,
                      use_ints_for_enums_, field_scrub_callback_.get()));
    current_->AddChild(node.release());
  } else {
    child->set_data(data);
    child->set_is_placeholder(false);
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
