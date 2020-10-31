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

#include <google/protobuf/util/internal/proto_writer.h>

#include <functional>
#include <stack>

#include <google/protobuf/stubs/once.h>
#include <google/protobuf/stubs/time.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/util/internal/field_mask_utility.h>
#include <google/protobuf/util/internal/object_location_tracker.h>
#include <google/protobuf/util/internal/constants.h>
#include <google/protobuf/util/internal/utility.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/statusor.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

using io::CodedOutputStream;
using ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite;
using util::Status;
using util::StatusOr;
using util::error::INVALID_ARGUMENT;


ProtoWriter::ProtoWriter(TypeResolver* type_resolver,
                         const google::protobuf::Type& type,
                         strings::ByteSink* output, ErrorListener* listener)
    : master_type_(type),
      typeinfo_(TypeInfo::NewTypeInfo(type_resolver)),
      own_typeinfo_(true),
      done_(false),
      ignore_unknown_fields_(false),
      ignore_unknown_enum_values_(false),
      use_lower_camel_for_enums_(false),
      case_insensitive_enum_parsing_(true),
      element_(nullptr),
      size_insert_(),
      output_(output),
      buffer_(),
      adapter_(&buffer_),
      stream_(new CodedOutputStream(&adapter_)),
      listener_(listener),
      invalid_depth_(0),
      tracker_(new ObjectLocationTracker()) {}

ProtoWriter::ProtoWriter(const TypeInfo* typeinfo,
                         const google::protobuf::Type& type,
                         strings::ByteSink* output, ErrorListener* listener)
    : master_type_(type),
      typeinfo_(typeinfo),
      own_typeinfo_(false),
      done_(false),
      ignore_unknown_fields_(false),
      ignore_unknown_enum_values_(false),
      use_lower_camel_for_enums_(false),
      case_insensitive_enum_parsing_(true),
      element_(nullptr),
      size_insert_(),
      output_(output),
      buffer_(),
      adapter_(&buffer_),
      stream_(new CodedOutputStream(&adapter_)),
      listener_(listener),
      invalid_depth_(0),
      tracker_(new ObjectLocationTracker()) {}

ProtoWriter::~ProtoWriter() {
  if (own_typeinfo_) {
    delete typeinfo_;
  }
  if (element_ == nullptr) return;
  // Cleanup explicitly in order to avoid destructor stack overflow when input
  // is deeply nested.
  // Cast to BaseElement to avoid doing additional checks (like missing fields)
  // during pop().
  std::unique_ptr<BaseElement> element(
      static_cast<BaseElement*>(element_.get())->pop<BaseElement>());
  while (element != nullptr) {
    element.reset(element->pop<BaseElement>());
  }
}

namespace {

// Writes an INT32 field, including tag to the stream.
inline Status WriteInt32(int field_number, const DataPiece& data,
                         CodedOutputStream* stream) {
  StatusOr<int32> i32 = data.ToInt32();
  if (i32.ok()) {
    WireFormatLite::WriteInt32(field_number, i32.ValueOrDie(), stream);
  }
  return i32.status();
}

// writes an SFIXED32 field, including tag, to the stream.
inline Status WriteSFixed32(int field_number, const DataPiece& data,
                            CodedOutputStream* stream) {
  StatusOr<int32> i32 = data.ToInt32();
  if (i32.ok()) {
    WireFormatLite::WriteSFixed32(field_number, i32.ValueOrDie(), stream);
  }
  return i32.status();
}

// Writes an SINT32 field, including tag, to the stream.
inline Status WriteSInt32(int field_number, const DataPiece& data,
                          CodedOutputStream* stream) {
  StatusOr<int32> i32 = data.ToInt32();
  if (i32.ok()) {
    WireFormatLite::WriteSInt32(field_number, i32.ValueOrDie(), stream);
  }
  return i32.status();
}

// Writes a FIXED32 field, including tag, to the stream.
inline Status WriteFixed32(int field_number, const DataPiece& data,
                           CodedOutputStream* stream) {
  StatusOr<uint32> u32 = data.ToUint32();
  if (u32.ok()) {
    WireFormatLite::WriteFixed32(field_number, u32.ValueOrDie(), stream);
  }
  return u32.status();
}

// Writes a UINT32 field, including tag, to the stream.
inline Status WriteUInt32(int field_number, const DataPiece& data,
                          CodedOutputStream* stream) {
  StatusOr<uint32> u32 = data.ToUint32();
  if (u32.ok()) {
    WireFormatLite::WriteUInt32(field_number, u32.ValueOrDie(), stream);
  }
  return u32.status();
}

// Writes an INT64 field, including tag, to the stream.
inline Status WriteInt64(int field_number, const DataPiece& data,
                         CodedOutputStream* stream) {
  StatusOr<int64> i64 = data.ToInt64();
  if (i64.ok()) {
    WireFormatLite::WriteInt64(field_number, i64.ValueOrDie(), stream);
  }
  return i64.status();
}

// Writes an SFIXED64 field, including tag, to the stream.
inline Status WriteSFixed64(int field_number, const DataPiece& data,
                            CodedOutputStream* stream) {
  StatusOr<int64> i64 = data.ToInt64();
  if (i64.ok()) {
    WireFormatLite::WriteSFixed64(field_number, i64.ValueOrDie(), stream);
  }
  return i64.status();
}

// Writes an SINT64 field, including tag, to the stream.
inline Status WriteSInt64(int field_number, const DataPiece& data,
                          CodedOutputStream* stream) {
  StatusOr<int64> i64 = data.ToInt64();
  if (i64.ok()) {
    WireFormatLite::WriteSInt64(field_number, i64.ValueOrDie(), stream);
  }
  return i64.status();
}

// Writes a FIXED64 field, including tag, to the stream.
inline Status WriteFixed64(int field_number, const DataPiece& data,
                           CodedOutputStream* stream) {
  StatusOr<uint64> u64 = data.ToUint64();
  if (u64.ok()) {
    WireFormatLite::WriteFixed64(field_number, u64.ValueOrDie(), stream);
  }
  return u64.status();
}

// Writes a UINT64 field, including tag, to the stream.
inline Status WriteUInt64(int field_number, const DataPiece& data,
                          CodedOutputStream* stream) {
  StatusOr<uint64> u64 = data.ToUint64();
  if (u64.ok()) {
    WireFormatLite::WriteUInt64(field_number, u64.ValueOrDie(), stream);
  }
  return u64.status();
}

// Writes a DOUBLE field, including tag, to the stream.
inline Status WriteDouble(int field_number, const DataPiece& data,
                          CodedOutputStream* stream) {
  StatusOr<double> d = data.ToDouble();
  if (d.ok()) {
    WireFormatLite::WriteDouble(field_number, d.ValueOrDie(), stream);
  }
  return d.status();
}

// Writes a FLOAT field, including tag, to the stream.
inline Status WriteFloat(int field_number, const DataPiece& data,
                         CodedOutputStream* stream) {
  StatusOr<float> f = data.ToFloat();
  if (f.ok()) {
    WireFormatLite::WriteFloat(field_number, f.ValueOrDie(), stream);
  }
  return f.status();
}

// Writes a BOOL field, including tag, to the stream.
inline Status WriteBool(int field_number, const DataPiece& data,
                        CodedOutputStream* stream) {
  StatusOr<bool> b = data.ToBool();
  if (b.ok()) {
    WireFormatLite::WriteBool(field_number, b.ValueOrDie(), stream);
  }
  return b.status();
}

// Writes a BYTES field, including tag, to the stream.
inline Status WriteBytes(int field_number, const DataPiece& data,
                         CodedOutputStream* stream) {
  StatusOr<std::string> c = data.ToBytes();
  if (c.ok()) {
    WireFormatLite::WriteBytes(field_number, c.ValueOrDie(), stream);
  }
  return c.status();
}

// Writes a STRING field, including tag, to the stream.
inline Status WriteString(int field_number, const DataPiece& data,
                          CodedOutputStream* stream) {
  StatusOr<std::string> s = data.ToString();
  if (s.ok()) {
    WireFormatLite::WriteString(field_number, s.ValueOrDie(), stream);
  }
  return s.status();
}

// Given a google::protobuf::Type, returns the set of all required fields.
std::set<const google::protobuf::Field*> GetRequiredFields(
    const google::protobuf::Type& type) {
  std::set<const google::protobuf::Field*> required;
  for (int i = 0; i < type.fields_size(); i++) {
    const google::protobuf::Field& field = type.fields(i);
    if (field.cardinality() ==
        google::protobuf::Field_Cardinality_CARDINALITY_REQUIRED) {
      required.insert(&field);
    }
  }
  return required;
}

}  // namespace

ProtoWriter::ProtoElement::ProtoElement(const TypeInfo* typeinfo,
                                        const google::protobuf::Type& type,
                                        ProtoWriter* enclosing)
    : BaseElement(nullptr),
      ow_(enclosing),
      parent_field_(nullptr),
      typeinfo_(typeinfo),
      proto3_(type.syntax() == google::protobuf::SYNTAX_PROTO3),
      type_(type),
      size_index_(-1),
      array_index_(-1),
      // oneof_indices_ values are 1-indexed (0 means not present).
      oneof_indices_(type.oneofs_size() + 1) {
  if (!proto3_) {
    required_fields_ = GetRequiredFields(type_);
  }
}

ProtoWriter::ProtoElement::ProtoElement(ProtoWriter::ProtoElement* parent,
                                        const google::protobuf::Field* field,
                                        const google::protobuf::Type& type,
                                        bool is_list)
    : BaseElement(parent),
      ow_(this->parent()->ow_),
      parent_field_(field),
      typeinfo_(this->parent()->typeinfo_),
      proto3_(type.syntax() == google::protobuf::SYNTAX_PROTO3),
      type_(type),
      size_index_(!is_list && field->kind() ==
                                  google::protobuf::Field_Kind_TYPE_MESSAGE
                      ? ow_->size_insert_.size()
                      : -1),
      array_index_(is_list ? 0 : -1),
      // oneof_indices_ values are 1-indexed (0 means not present).
      oneof_indices_(type_.oneofs_size() + 1) {
  if (!is_list) {
    if (ow_->IsRepeated(*field)) {
      // Update array_index_ if it is an explicit list.
      if (this->parent()->array_index_ >= 0) this->parent()->array_index_++;
    } else if (!proto3_) {
      // For required fields tracking.
      this->parent()->RegisterField(field);
    }

    if (field->kind() == google::protobuf::Field_Kind_TYPE_MESSAGE) {
      if (!proto3_) {
        required_fields_ = GetRequiredFields(type_);
      }
      int start_pos = ow_->stream_->ByteCount();
      // length of serialized message is the final buffer position minus
      // starting buffer position, plus length adjustments for size fields
      // of any nested messages. We start with -start_pos here, so we only
      // need to add the final buffer position to it at the end.
      SizeInfo info = {start_pos, -start_pos};
      ow_->size_insert_.push_back(info);
    }
  }
}

ProtoWriter::ProtoElement* ProtoWriter::ProtoElement::pop() {
  if (!proto3_) {
    // Calls the registered error listener for any required field(s) not yet
    // seen.
    for (std::set<const google::protobuf::Field*>::iterator it =
             required_fields_.begin();
         it != required_fields_.end(); ++it) {
      ow_->MissingField((*it)->name());
    }
  }
  // Computes the total number of proto bytes used by a message, also adjusts
  // the size of all parent messages by the length of this size field.
  // If size_index_ < 0, this is not a message, so no size field is added.
  if (size_index_ >= 0) {
    // Add the final buffer position to compute the total length of this
    // serialized message. The stored value (before this addition) already
    // contains the total length of the size fields of all nested messages
    // minus the initial buffer position.
    ow_->size_insert_[size_index_].size += ow_->stream_->ByteCount();
    // Calculate the length required to serialize the size field of the
    // message, and propagate this additional size information upward to
    // all enclosing messages.
    int size = ow_->size_insert_[size_index_].size;
    int length = CodedOutputStream::VarintSize32(size);
    for (ProtoElement* e = parent(); e != nullptr; e = e->parent()) {
      // Only nested messages have size field, lists do not have size field.
      if (e->size_index_ >= 0) {
        ow_->size_insert_[e->size_index_].size += length;
      }
    }
  }
  return BaseElement::pop<ProtoElement>();
}

void ProtoWriter::ProtoElement::RegisterField(
    const google::protobuf::Field* field) {
  if (!required_fields_.empty() &&
      field->cardinality() ==
          google::protobuf::Field_Cardinality_CARDINALITY_REQUIRED) {
    required_fields_.erase(field);
  }
}

std::string ProtoWriter::ProtoElement::ToString() const {
  std::string loc = "";

  // first populate a stack with the nodes since we need to process them
  // from root to leaf when generating the string location
  const ProtoWriter::ProtoElement* now = this;
  std::stack<const ProtoWriter::ProtoElement*> element_stack;
  while (now->parent() != nullptr) {
    element_stack.push(now);
    now = now->parent();
  }

  // now pop each node from the stack and append to the location string
  while (!element_stack.empty()) {
    now = element_stack.top();
    element_stack.pop();

    if (!ow_->IsRepeated(*(now->parent_field_)) ||
        now->parent()->parent_field_ != now->parent_field_) {
      std::string name = now->parent_field_->name();
      int i = 0;
      while (i < name.size() && (ascii_isalnum(name[i]) || name[i] == '_')) ++i;
      if (i > 0 && i == name.size()) {  // safe field name
        if (loc.empty()) {
          loc = name;
        } else {
          StrAppend(&loc, ".", name);
        }
      } else {
        StrAppend(&loc, "[\"", CEscape(name), "\"]");
      }
    }

    int array_index_now = now->array_index_;
    if (ow_->IsRepeated(*(now->parent_field_)) && array_index_now > 0) {
      StrAppend(&loc, "[", array_index_now - 1, "]");
    }
  }

  return loc;
}

bool ProtoWriter::ProtoElement::IsOneofIndexTaken(int32 index) {
  return oneof_indices_[index];
}

void ProtoWriter::ProtoElement::TakeOneofIndex(int32 index) {
  oneof_indices_[index] = true;
}

void ProtoWriter::InvalidName(StringPiece unknown_name,
                              StringPiece message) {
  listener_->InvalidName(location(), ToSnakeCase(unknown_name), message);
}

void ProtoWriter::InvalidValue(StringPiece type_name,
                               StringPiece value) {
  listener_->InvalidValue(location(), type_name, value);
}

void ProtoWriter::MissingField(StringPiece missing_name) {
  listener_->MissingField(location(), missing_name);
}

ProtoWriter* ProtoWriter::StartObject(StringPiece name) {
  // Starting the root message. Create the root ProtoElement and return.
  if (element_ == nullptr) {
    if (!name.empty()) {
      InvalidName(name, "Root element should not be named.");
    }
    element_.reset(new ProtoElement(typeinfo_, master_type_, this));
    return this;
  }

  const google::protobuf::Field* field = nullptr;
  field = BeginNamed(name, false);
  if (field == nullptr) return this;

  // Check to see if this field is a oneof and that no oneof in that group has
  // already been set.
  if (!ValidOneof(*field, name)) {
    ++invalid_depth_;
    return this;
  }

  const google::protobuf::Type* type = LookupType(field);
  if (type == nullptr) {
    ++invalid_depth_;
    InvalidName(name, StrCat("Missing descriptor for field: ",
                                   field->type_url()));
    return this;
  }

  return StartObjectField(*field, *type);
}

ProtoWriter* ProtoWriter::EndObject() {
  if (invalid_depth_ > 0) {
    --invalid_depth_;
    return this;
  }

  if (element_ != nullptr) {
    element_.reset(element_->pop());
  }


  // If ending the root element,
  // then serialize the full message with calculated sizes.
  if (element_ == nullptr) {
    WriteRootMessage();
  }
  return this;
}

ProtoWriter* ProtoWriter::StartList(StringPiece name) {
  const google::protobuf::Field* field = BeginNamed(name, true);
  if (field == nullptr) return this;

  if (!ValidOneof(*field, name)) {
    ++invalid_depth_;
    return this;
  }

  const google::protobuf::Type* type = LookupType(field);
  if (type == nullptr) {
    ++invalid_depth_;
    InvalidName(name, StrCat("Missing descriptor for field: ",
                                   field->type_url()));
    return this;
  }

  return StartListField(*field, *type);
}

ProtoWriter* ProtoWriter::EndList() {
  if (invalid_depth_ > 0) {
    --invalid_depth_;
  } else if (element_ != nullptr) {
    element_.reset(element_->pop());
  }
  return this;
}

ProtoWriter* ProtoWriter::RenderDataPiece(StringPiece name,
                                          const DataPiece& data) {
  Status status;
  if (invalid_depth_ > 0) return this;

  const google::protobuf::Field* field = Lookup(name);
  if (field == nullptr) return this;

  if (!ValidOneof(*field, name)) return this;

  const google::protobuf::Type* type = LookupType(field);
  if (type == nullptr) {
    InvalidName(name, StrCat("Missing descriptor for field: ",
                                   field->type_url()));
    return this;
  }

  return RenderPrimitiveField(*field, *type, data);
}

bool ProtoWriter::ValidOneof(const google::protobuf::Field& field,
                             StringPiece unnormalized_name) {
  if (element_ == nullptr) return true;

  if (field.oneof_index() > 0) {
    if (element_->IsOneofIndexTaken(field.oneof_index())) {
      InvalidValue(
          "oneof",
          StrCat(
              "oneof field '", element_->type().oneofs(field.oneof_index() - 1),
              "' is already set. Cannot set '", unnormalized_name, "'"));
      return false;
    }
    element_->TakeOneofIndex(field.oneof_index());
  }
  return true;
}

bool ProtoWriter::IsRepeated(const google::protobuf::Field& field) {
  return field.cardinality() ==
         google::protobuf::Field_Cardinality_CARDINALITY_REPEATED;
}

ProtoWriter* ProtoWriter::StartObjectField(const google::protobuf::Field& field,
                                           const google::protobuf::Type& type) {
    WriteTag(field);
  element_.reset(new ProtoElement(element_.release(), &field, type, false));
  return this;
}

ProtoWriter* ProtoWriter::StartListField(const google::protobuf::Field& field,
                                         const google::protobuf::Type& type) {
  element_.reset(new ProtoElement(element_.release(), &field, type, true));
  return this;
}

Status ProtoWriter::WriteEnum(int field_number, const DataPiece& data,
                              const google::protobuf::Enum* enum_type,
                              CodedOutputStream* stream,
                              bool use_lower_camel_for_enums,
                              bool case_insensitive_enum_parsing,
                              bool ignore_unknown_values) {
  bool is_unknown_enum_value = false;
  StatusOr<int> e = data.ToEnum(enum_type, use_lower_camel_for_enums,
                                case_insensitive_enum_parsing,
                                ignore_unknown_values, &is_unknown_enum_value);
  if (e.ok() && !is_unknown_enum_value) {
    WireFormatLite::WriteEnum(field_number, e.ValueOrDie(), stream);
  }
  return e.status();
}

ProtoWriter* ProtoWriter::RenderPrimitiveField(
    const google::protobuf::Field& field, const google::protobuf::Type& type,
    const DataPiece& data) {
  Status status;

  // Pushing a ProtoElement and then pop it off at the end for 2 purposes:
  // error location reporting and required field accounting.
  //
  // For proto3, since there is no required field tracking, we only need to push
  // ProtoElement for error cases.
  if (!element_->proto3()) {
    element_.reset(new ProtoElement(element_.release(), &field, type, false));
  }

  if (field.kind() == google::protobuf::Field_Kind_TYPE_UNKNOWN ||
      field.kind() == google::protobuf::Field_Kind_TYPE_MESSAGE) {
    // Push a ProtoElement for location reporting purposes.
    if (element_->proto3()) {
      element_.reset(new ProtoElement(element_.release(), &field, type, false));
    }
    InvalidValue(field.type_url().empty()
                     ? google::protobuf::Field_Kind_Name(field.kind())
                     : field.type_url(),
                 data.ValueAsStringOrDefault(""));
    element_.reset(element()->pop());
    return this;
  }

  switch (field.kind()) {
    case google::protobuf::Field_Kind_TYPE_INT32: {
      status = WriteInt32(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_SFIXED32: {
      status = WriteSFixed32(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_SINT32: {
      status = WriteSInt32(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_FIXED32: {
      status = WriteFixed32(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_UINT32: {
      status = WriteUInt32(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_INT64: {
      status = WriteInt64(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_SFIXED64: {
      status = WriteSFixed64(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_SINT64: {
      status = WriteSInt64(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_FIXED64: {
      status = WriteFixed64(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_UINT64: {
      status = WriteUInt64(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_DOUBLE: {
      status = WriteDouble(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_FLOAT: {
      status = WriteFloat(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_BOOL: {
      status = WriteBool(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_BYTES: {
      status = WriteBytes(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_STRING: {
      status = WriteString(field.number(), data, stream_.get());
      break;
    }
    case google::protobuf::Field_Kind_TYPE_ENUM: {
      status = WriteEnum(
          field.number(), data, typeinfo_->GetEnumByTypeUrl(field.type_url()),
          stream_.get(), use_lower_camel_for_enums_,
          case_insensitive_enum_parsing_, ignore_unknown_enum_values_);
      break;
    }
    default:  // TYPE_GROUP or TYPE_MESSAGE
      status = Status(util::error::INVALID_ARGUMENT,
                      data.ToString().ValueOrDie());
  }

  if (!status.ok()) {
    // Push a ProtoElement for location reporting purposes.
    if (element_->proto3()) {
      element_.reset(new ProtoElement(element_.release(), &field, type, false));
    }
    InvalidValue(google::protobuf::Field_Kind_Name(field.kind()),
                 status.message());
    element_.reset(element()->pop());
    return this;
  }

  if (!element_->proto3()) element_.reset(element()->pop());

  return this;
}

const google::protobuf::Field* ProtoWriter::BeginNamed(StringPiece name,
                                                       bool is_list) {
  if (invalid_depth_ > 0) {
    ++invalid_depth_;
    return nullptr;
  }
  const google::protobuf::Field* field = Lookup(name);
  if (field == nullptr) {
    ++invalid_depth_;
    // InvalidName() already called in Lookup().
    return nullptr;
  }
  if (is_list && !IsRepeated(*field)) {
    ++invalid_depth_;
    InvalidName(name, "Proto field is not repeating, cannot start list.");
    return nullptr;
  }
  return field;
}

const google::protobuf::Field* ProtoWriter::Lookup(
    StringPiece unnormalized_name) {
  ProtoElement* e = element();
  if (e == nullptr) {
    InvalidName(unnormalized_name, "Root element must be a message.");
    return nullptr;
  }
  if (unnormalized_name.empty()) {
    // Objects in repeated field inherit the same field descriptor.
    if (e->parent_field() == nullptr) {
      InvalidName(unnormalized_name, "Proto fields must have a name.");
    } else if (!IsRepeated(*e->parent_field())) {
      InvalidName(unnormalized_name, "Proto fields must have a name.");
      return nullptr;
    }
    return e->parent_field();
  }
  const google::protobuf::Field* field =
      typeinfo_->FindField(&e->type(), unnormalized_name);
  if (field == nullptr && !ignore_unknown_fields_) {
    InvalidName(unnormalized_name, "Cannot find field.");
  }
  return field;
}

const google::protobuf::Type* ProtoWriter::LookupType(
    const google::protobuf::Field* field) {
  return ((field->kind() == google::protobuf::Field_Kind_TYPE_MESSAGE ||
           field->kind() == google::protobuf::Field_Kind_TYPE_GROUP)
              ? typeinfo_->GetTypeByTypeUrl(field->type_url())
              : &element_->type());
}

void ProtoWriter::WriteRootMessage() {
  GOOGLE_DCHECK(!done_);
  int curr_pos = 0;
  // Calls the destructor of CodedOutputStream to remove any uninitialized
  // memory from the Cord before we read it.
  stream_.reset(nullptr);
  const void* data;
  int length;
  io::ArrayInputStream input_stream(buffer_.data(), buffer_.size());
  while (input_stream.Next(&data, &length)) {
    if (length == 0) continue;
    int num_bytes = length;
    // Write up to where we need to insert the size field.
    // The number of bytes we may write is the smaller of:
    //   - the current fragment size
    //   - the distance to the next position where a size field needs to be
    //     inserted.
    if (!size_insert_.empty() &&
        size_insert_.front().pos - curr_pos < num_bytes) {
      num_bytes = size_insert_.front().pos - curr_pos;
    }
    output_->Append(static_cast<const char*>(data), num_bytes);
    if (num_bytes < length) {
      input_stream.BackUp(length - num_bytes);
    }
    curr_pos += num_bytes;
    // Insert the size field.
    //   size_insert_.front():      the next <index, size> pair to be written.
    //   size_insert_.front().pos:  position of the size field.
    //   size_insert_.front().size: the size (integer) to be inserted.
    if (!size_insert_.empty() && curr_pos == size_insert_.front().pos) {
      // Varint32 occupies at most 10 bytes.
      uint8 insert_buffer[10];
      uint8* insert_buffer_pos = CodedOutputStream::WriteVarint32ToArray(
          size_insert_.front().size, insert_buffer);
      output_->Append(reinterpret_cast<const char*>(insert_buffer),
                      insert_buffer_pos - insert_buffer);
      size_insert_.pop_front();
    }
  }
  output_->Flush();
  stream_.reset(new CodedOutputStream(&adapter_));
  done_ = true;
}

void ProtoWriter::WriteTag(const google::protobuf::Field& field) {
  WireFormatLite::WireType wire_type = WireFormatLite::WireTypeForFieldType(
      static_cast<WireFormatLite::FieldType>(field.kind()));
  stream_->WriteTag(WireFormatLite::MakeTag(field.number(), wire_type));
}


}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
