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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/unknown_field_set.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/parse_context.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/stl_util.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

const UnknownFieldSet* UnknownFieldSet::default_instance() {
  static auto instance = internal::OnShutdownDelete(new UnknownFieldSet());
  return instance;
}

void UnknownFieldSet::ClearFallback() {
  GOOGLE_DCHECK(!fields_.empty());
  int n = fields_.size();
  do {
    (fields_)[--n].Delete();
  } while (n > 0);
  fields_.clear();
}

void UnknownFieldSet::InternalMergeFrom(const UnknownFieldSet& other) {
  int other_field_count = other.field_count();
  if (other_field_count > 0) {
    fields_.reserve(fields_.size() + other_field_count);
    for (int i = 0; i < other_field_count; i++) {
      fields_.push_back((other.fields_)[i]);
      fields_.back().DeepCopy((other.fields_)[i]);
    }
  }
}

void UnknownFieldSet::MergeFrom(const UnknownFieldSet& other) {
  int other_field_count = other.field_count();
  if (other_field_count > 0) {
    fields_.reserve(fields_.size() + other_field_count);
    for (int i = 0; i < other_field_count; i++) {
      fields_.push_back((other.fields_)[i]);
      fields_.back().DeepCopy((other.fields_)[i]);
    }
  }
}

// A specialized MergeFrom for performance when we are merging from an UFS that
// is temporary and can be destroyed in the process.
void UnknownFieldSet::MergeFromAndDestroy(UnknownFieldSet* other) {
  if (fields_.empty()) {
    fields_ = std::move(other->fields_);
  } else {
    fields_.insert(fields_.end(),
                   std::make_move_iterator(other->fields_.begin()),
                   std::make_move_iterator(other->fields_.end()));
  }
  other->fields_.clear();
}

void UnknownFieldSet::MergeToInternalMetdata(
    const UnknownFieldSet& other,
    internal::InternalMetadataWithArena* metadata) {
  metadata->mutable_unknown_fields()->MergeFrom(other);
}

size_t UnknownFieldSet::SpaceUsedExcludingSelfLong() const {
  if (fields_.empty()) return 0;

  size_t total_size = sizeof(fields_) + sizeof(UnknownField) * fields_.size();

  for (int i = 0; i < fields_.size(); i++) {
    const UnknownField& field = (fields_)[i];
    switch (field.type()) {
      case UnknownField::TYPE_LENGTH_DELIMITED:
        total_size += sizeof(*field.data_.length_delimited_.string_value) +
                      internal::StringSpaceUsedExcludingSelfLong(
                          *field.data_.length_delimited_.string_value);
        break;
      case UnknownField::TYPE_GROUP:
        total_size += field.data_.group_->SpaceUsedLong();
        break;
      default:
        break;
    }
  }
  return total_size;
}

size_t UnknownFieldSet::SpaceUsedLong() const {
  return sizeof(*this) + SpaceUsedExcludingSelf();
}

void UnknownFieldSet::AddVarint(int number, uint64 value) {
  UnknownField field;
  field.number_ = number;
  field.SetType(UnknownField::TYPE_VARINT);
  field.data_.varint_ = value;
  fields_.push_back(field);
}

void UnknownFieldSet::AddFixed32(int number, uint32 value) {
  UnknownField field;
  field.number_ = number;
  field.SetType(UnknownField::TYPE_FIXED32);
  field.data_.fixed32_ = value;
  fields_.push_back(field);
}

void UnknownFieldSet::AddFixed64(int number, uint64 value) {
  UnknownField field;
  field.number_ = number;
  field.SetType(UnknownField::TYPE_FIXED64);
  field.data_.fixed64_ = value;
  fields_.push_back(field);
}

std::string* UnknownFieldSet::AddLengthDelimited(int number) {
  UnknownField field;
  field.number_ = number;
  field.SetType(UnknownField::TYPE_LENGTH_DELIMITED);
  field.data_.length_delimited_.string_value = new std::string;
  fields_.push_back(field);
  return field.data_.length_delimited_.string_value;
}


UnknownFieldSet* UnknownFieldSet::AddGroup(int number) {
  UnknownField field;
  field.number_ = number;
  field.SetType(UnknownField::TYPE_GROUP);
  field.data_.group_ = new UnknownFieldSet;
  fields_.push_back(field);
  return field.data_.group_;
}

void UnknownFieldSet::AddField(const UnknownField& field) {
  fields_.push_back(field);
  fields_.back().DeepCopy(field);
}

void UnknownFieldSet::DeleteSubrange(int start, int num) {
  // Delete the specified fields.
  for (int i = 0; i < num; ++i) {
    (fields_)[i + start].Delete();
  }
  // Slide down the remaining fields.
  for (int i = start + num; i < fields_.size(); ++i) {
    (fields_)[i - num] = (fields_)[i];
  }
  // Pop off the # of deleted fields.
  for (int i = 0; i < num; ++i) {
    fields_.pop_back();
  }
}

void UnknownFieldSet::DeleteByNumber(int number) {
  int left = 0;  // The number of fields left after deletion.
  for (int i = 0; i < fields_.size(); ++i) {
    UnknownField* field = &(fields_)[i];
    if (field->number() == number) {
      field->Delete();
    } else {
      if (i != left) {
        (fields_)[left] = (fields_)[i];
      }
      ++left;
    }
  }
  fields_.resize(left);
}

bool UnknownFieldSet::MergeFromCodedStream(io::CodedInputStream* input) {
  UnknownFieldSet other;
  if (internal::WireFormat::SkipMessage(input, &other) &&
      input->ConsumedEntireMessage()) {
    MergeFromAndDestroy(&other);
    return true;
  } else {
    return false;
  }
}

bool UnknownFieldSet::ParseFromCodedStream(io::CodedInputStream* input) {
  Clear();
  return MergeFromCodedStream(input);
}

bool UnknownFieldSet::ParseFromZeroCopyStream(io::ZeroCopyInputStream* input) {
  io::CodedInputStream coded_input(input);
  return (ParseFromCodedStream(&coded_input) &&
          coded_input.ConsumedEntireMessage());
}

bool UnknownFieldSet::ParseFromArray(const void* data, int size) {
  io::ArrayInputStream input(data, size);
  return ParseFromZeroCopyStream(&input);
}

void UnknownField::Delete() {
  switch (type()) {
    case UnknownField::TYPE_LENGTH_DELIMITED:
      delete data_.length_delimited_.string_value;
      break;
    case UnknownField::TYPE_GROUP:
      delete data_.group_;
      break;
    default:
      break;
  }
}

void UnknownField::DeepCopy(const UnknownField& other) {
  switch (type()) {
    case UnknownField::TYPE_LENGTH_DELIMITED:
      data_.length_delimited_.string_value =
          new std::string(*data_.length_delimited_.string_value);
      break;
    case UnknownField::TYPE_GROUP: {
      UnknownFieldSet* group = new UnknownFieldSet();
      group->InternalMergeFrom(*data_.group_);
      data_.group_ = group;
      break;
    }
    default:
      break;
  }
}


void UnknownField::SerializeLengthDelimitedNoTag(
    io::CodedOutputStream* output) const {
  GOOGLE_DCHECK_EQ(TYPE_LENGTH_DELIMITED, type());
  const std::string& data = *data_.length_delimited_.string_value;
  output->WriteVarint32(data.size());
  output->WriteRawMaybeAliased(data.data(), data.size());
}

uint8* UnknownField::SerializeLengthDelimitedNoTagToArray(uint8* target) const {
  GOOGLE_DCHECK_EQ(TYPE_LENGTH_DELIMITED, type());
  const std::string& data = *data_.length_delimited_.string_value;
  target = io::CodedOutputStream::WriteVarint32ToArray(data.size(), target);
  target = io::CodedOutputStream::WriteStringToArray(data, target);
  return target;
}

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
namespace internal {
const char* PackedEnumParser(void* object, const char* ptr, ParseContext* ctx,
                             bool (*is_valid)(int),
                             InternalMetadataWithArena* metadata,
                             int field_num) {
  return ctx->ReadPackedVarint(
      ptr, [object, is_valid, metadata, field_num](uint64 val) {
        if (is_valid(val)) {
          static_cast<RepeatedField<int>*>(object)->Add(val);
        } else {
          WriteVarint(field_num, val, metadata->mutable_unknown_fields());
        }
      });
}
const char* PackedEnumParserArg(void* object, const char* ptr,
                                ParseContext* ctx,
                                bool (*is_valid)(const void*, int),
                                const void* data,
                                InternalMetadataWithArena* metadata,
                                int field_num) {
  return ctx->ReadPackedVarint(
      ptr, [object, is_valid, data, metadata, field_num](uint64 val) {
        if (is_valid(data, val)) {
          static_cast<RepeatedField<int>*>(object)->Add(val);
        } else {
          WriteVarint(field_num, val, metadata->mutable_unknown_fields());
        }
      });
}

class UnknownFieldParserHelper {
 public:
  explicit UnknownFieldParserHelper(UnknownFieldSet* unknown)
      : unknown_(unknown) {}

  void AddVarint(uint32 num, uint64 value) { unknown_->AddVarint(num, value); }
  void AddFixed64(uint32 num, uint64 value) {
    unknown_->AddFixed64(num, value);
  }
  const char* ParseLengthDelimited(uint32 num, const char* ptr,
                                   ParseContext* ctx) {
    std::string* s = unknown_->AddLengthDelimited(num);
    int size = ReadSize(&ptr);
    GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
    return ctx->ReadString(ptr, size, s);
  }
  const char* ParseGroup(uint32 num, const char* ptr, ParseContext* ctx) {
    UnknownFieldParserHelper child(unknown_->AddGroup(num));
    return ctx->ParseGroup(&child, ptr, num * 8 + 3);
  }
  void AddFixed32(uint32 num, uint32 value) {
    unknown_->AddFixed32(num, value);
  }

  const char* _InternalParse(const char* ptr, ParseContext* ctx) {
    return WireFormatParser(*this, ptr, ctx);
  }

 private:
  UnknownFieldSet* unknown_;
};

const char* UnknownGroupParse(UnknownFieldSet* unknown, const char* ptr,
                              ParseContext* ctx) {
  UnknownFieldParserHelper field_parser(unknown);
  return WireFormatParser(field_parser, ptr, ctx);
}

const char* UnknownFieldParse(uint64 tag, UnknownFieldSet* unknown,
                              const char* ptr, ParseContext* ctx) {
  UnknownFieldParserHelper field_parser(unknown);
  return FieldParser(tag, field_parser, ptr, ctx);
}

const char* UnknownFieldParse(uint32 tag, InternalMetadataWithArena* metadata,
                              const char* ptr, ParseContext* ctx) {
  return UnknownFieldParse(tag, metadata->mutable_unknown_fields(), ptr, ctx);
}

}  // namespace internal
#endif  // GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

}  // namespace protobuf
}  // namespace google
