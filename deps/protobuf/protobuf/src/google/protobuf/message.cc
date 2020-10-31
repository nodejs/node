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

#include <iostream>
#include <stack>
#include <unordered_map>

#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/message.h>

#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/parse_context.h>
#include <google/protobuf/reflection_internal.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/map_field.h>
#include <google/protobuf/map_field_inl.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

namespace internal {

// TODO(gerbens) make this factorized better. This should not have to hop
// to reflection. Currently uses GeneratedMessageReflection and thus is
// defined in generated_message_reflection.cc
void RegisterFileLevelMetadata(const DescriptorTable* descriptor_table);

}  // namespace internal

using internal::ReflectionOps;
using internal::WireFormat;
using internal::WireFormatLite;

void Message::MergeFrom(const Message& from) {
  const Descriptor* descriptor = GetDescriptor();
  GOOGLE_CHECK_EQ(from.GetDescriptor(), descriptor)
      << ": Tried to merge from a message with a different type.  "
         "to: "
      << descriptor->full_name()
      << ", "
         "from: "
      << from.GetDescriptor()->full_name();
  ReflectionOps::Merge(from, this);
}

void Message::CheckTypeAndMergeFrom(const MessageLite& other) {
  MergeFrom(*down_cast<const Message*>(&other));
}

void Message::CopyFrom(const Message& from) {
  const Descriptor* descriptor = GetDescriptor();
  GOOGLE_CHECK_EQ(from.GetDescriptor(), descriptor)
      << ": Tried to copy from a message with a different type. "
         "to: "
      << descriptor->full_name()
      << ", "
         "from: "
      << from.GetDescriptor()->full_name();
  ReflectionOps::Copy(from, this);
}

std::string Message::GetTypeName() const {
  return GetDescriptor()->full_name();
}

void Message::Clear() { ReflectionOps::Clear(this); }

bool Message::IsInitialized() const {
  return ReflectionOps::IsInitialized(*this);
}

void Message::FindInitializationErrors(std::vector<std::string>* errors) const {
  return ReflectionOps::FindInitializationErrors(*this, "", errors);
}

std::string Message::InitializationErrorString() const {
  std::vector<std::string> errors;
  FindInitializationErrors(&errors);
  return Join(errors, ", ");
}

void Message::CheckInitialized() const {
  GOOGLE_CHECK(IsInitialized()) << "Message of type \"" << GetDescriptor()->full_name()
                         << "\" is missing required fields: "
                         << InitializationErrorString();
}

void Message::DiscardUnknownFields() {
  return ReflectionOps::DiscardUnknownFields(this);
}

#if !GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
bool Message::MergePartialFromCodedStream(io::CodedInputStream* input) {
  return WireFormat::ParseAndMergePartial(input, this);
}
#endif

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
namespace internal {

class ReflectionAccessor {
 public:
  static void* GetOffset(void* msg, const google::protobuf::FieldDescriptor* f,
                         const google::protobuf::Reflection* r) {
    return static_cast<char*>(msg) + r->schema_.GetFieldOffset(f);
  }

  static void* GetRepeatedEnum(const Reflection* reflection,
                               const FieldDescriptor* field, Message* msg) {
    return reflection->MutableRawRepeatedField(
        msg, field, FieldDescriptor::CPPTYPE_ENUM, 0, nullptr);
  }

  static InternalMetadataWithArena* MutableInternalMetadataWithArena(
      const Reflection* reflection, Message* msg) {
    return reflection->MutableInternalMetadataWithArena(msg);
  }
};

}  // namespace internal

void SetField(uint64 val, const FieldDescriptor* field, Message* msg,
              const Reflection* reflection) {
#define STORE_TYPE(CPPTYPE_METHOD)                        \
  do                                                      \
    if (field->is_repeated()) {                           \
      reflection->Add##CPPTYPE_METHOD(msg, field, value); \
    } else {                                              \
      reflection->Set##CPPTYPE_METHOD(msg, field, value); \
    }                                                     \
  while (0)

  switch (field->type()) {
#define HANDLE_TYPE(TYPE, CPPTYPE, CPPTYPE_METHOD) \
  case FieldDescriptor::TYPE_##TYPE: {             \
    CPPTYPE value = val;                           \
    STORE_TYPE(CPPTYPE_METHOD);                    \
    break;                                         \
  }

    // Varints
    HANDLE_TYPE(INT32, int32, Int32)
    HANDLE_TYPE(INT64, int64, Int64)
    HANDLE_TYPE(UINT32, uint32, UInt32)
    HANDLE_TYPE(UINT64, uint64, UInt64)
    case FieldDescriptor::TYPE_SINT32: {
      int32 value = WireFormatLite::ZigZagDecode32(val);
      STORE_TYPE(Int32);
      break;
    }
    case FieldDescriptor::TYPE_SINT64: {
      int64 value = WireFormatLite::ZigZagDecode64(val);
      STORE_TYPE(Int64);
      break;
    }
      HANDLE_TYPE(BOOL, bool, Bool)

      // Fixed
      HANDLE_TYPE(FIXED32, uint32, UInt32)
      HANDLE_TYPE(FIXED64, uint64, UInt64)
      HANDLE_TYPE(SFIXED32, int32, Int32)
      HANDLE_TYPE(SFIXED64, int64, Int64)

    case FieldDescriptor::TYPE_FLOAT: {
      float value;
      uint32 bit_rep = val;
      std::memcpy(&value, &bit_rep, sizeof(value));
      STORE_TYPE(Float);
      break;
    }
    case FieldDescriptor::TYPE_DOUBLE: {
      double value;
      uint64 bit_rep = val;
      std::memcpy(&value, &bit_rep, sizeof(value));
      STORE_TYPE(Double);
      break;
    }
    case FieldDescriptor::TYPE_ENUM: {
      int value = val;
      if (field->is_repeated()) {
        reflection->AddEnumValue(msg, field, value);
      } else {
        reflection->SetEnumValue(msg, field, value);
      }
      break;
    }
    default:
      GOOGLE_LOG(FATAL) << "Error in descriptors, primitve field with field type "
                 << field->type();
  }
#undef STORE_TYPE
#undef HANDLE_TYPE
}

bool ReflectiveValidator(const void* arg, int val) {
  auto d = static_cast<const EnumDescriptor*>(arg);
  return d->FindValueByNumber(val) != nullptr;
}

const char* ParsePackedField(const FieldDescriptor* field, Message* msg,
                             const Reflection* reflection, const char* ptr,
                             internal::ParseContext* ctx) {
  switch (field->type()) {
#define HANDLE_PACKED_TYPE(TYPE, CPPTYPE, METHOD_NAME) \
  case FieldDescriptor::TYPE_##TYPE:                   \
    return internal::Packed##METHOD_NAME##Parser(      \
        reflection->MutableRepeatedField<CPPTYPE>(msg, field), ptr, ctx)
    HANDLE_PACKED_TYPE(INT32, int32, Int32);
    HANDLE_PACKED_TYPE(INT64, int64, Int64);
    HANDLE_PACKED_TYPE(SINT32, int32, SInt32);
    HANDLE_PACKED_TYPE(SINT64, int64, SInt64);
    HANDLE_PACKED_TYPE(UINT32, uint32, UInt32);
    HANDLE_PACKED_TYPE(UINT64, uint64, UInt64);
    HANDLE_PACKED_TYPE(BOOL, bool, Bool);
    case FieldDescriptor::TYPE_ENUM: {
      auto object =
          internal::ReflectionAccessor::GetRepeatedEnum(reflection, field, msg);
      if (field->file()->syntax() == FileDescriptor::SYNTAX_PROTO3) {
        return internal::PackedEnumParser(object, ptr, ctx);
      } else {
        return internal::PackedEnumParserArg(
            object, ptr, ctx, ReflectiveValidator, field->enum_type(),
            internal::ReflectionAccessor::MutableInternalMetadataWithArena(
                reflection, msg),
            field->number());
      }
    }
      HANDLE_PACKED_TYPE(FIXED32, uint32, Fixed32);
      HANDLE_PACKED_TYPE(FIXED64, uint64, Fixed64);
      HANDLE_PACKED_TYPE(SFIXED32, int32, SFixed32);
      HANDLE_PACKED_TYPE(SFIXED64, int64, SFixed64);
      HANDLE_PACKED_TYPE(FLOAT, float, Float);
      HANDLE_PACKED_TYPE(DOUBLE, double, Double);
#undef HANDLE_PACKED_TYPE

    default:
      GOOGLE_LOG(FATAL) << "Type is not packable " << field->type();
      return nullptr;  // Make compiler happy
  }
}

const char* ParseLenDelim(int field_number, const FieldDescriptor* field,
                          Message* msg, const Reflection* reflection,
                          const char* ptr, internal::ParseContext* ctx) {
  if (WireFormat::WireTypeForFieldType(field->type()) !=
      WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
    GOOGLE_DCHECK(field->is_packable());
    return ParsePackedField(field, msg, reflection, ptr, ctx);
  }
  enum { kNone = 0, kVerify, kStrict } utf8_level = kNone;
  const char* field_name = nullptr;
  auto parse_string = [ptr, ctx, &utf8_level, &field_name](std::string* s) {
    switch (utf8_level) {
      case kNone:
        return internal::InlineGreedyStringParser(s, ptr, ctx);
      case kVerify:
        return internal::InlineGreedyStringParserUTF8Verify(s, ptr, ctx,
                                                            field_name);
      case kStrict:
        return internal::InlineGreedyStringParserUTF8(s, ptr, ctx, field_name);
    }
  };
  switch (field->type()) {
    case FieldDescriptor::TYPE_STRING: {
      bool enforce_utf8 = true;
      bool utf8_verification = true;
      if (enforce_utf8 &&
          field->file()->syntax() == FileDescriptor::SYNTAX_PROTO3) {
        utf8_level = kStrict;
      } else if (utf8_verification) {
        utf8_level = kVerify;
      }
      field_name = field->full_name().c_str();
      PROTOBUF_FALLTHROUGH_INTENDED;
    }
    case FieldDescriptor::TYPE_BYTES: {
      if (field->is_repeated()) {
        int index = reflection->FieldSize(*msg, field);
        // Add new empty value.
        reflection->AddString(msg, field, "");
        if (field->options().ctype() == FieldOptions::STRING ||
            field->is_extension()) {
          auto object =
              reflection->MutableRepeatedPtrField<std::string>(msg, field)
                  ->Mutable(index);
          return parse_string(object);
        } else {
          auto object =
              reflection->MutableRepeatedPtrField<std::string>(msg, field)
                  ->Mutable(index);
          return parse_string(object);
        }
      } else {
        // Clear value and make sure it's set.
        reflection->SetString(msg, field, "");
        if (field->options().ctype() == FieldOptions::STRING ||
            field->is_extension()) {
          // HACK around inability to get mutable_string in reflection
          std::string* object = &const_cast<std::string&>(
              reflection->GetStringReference(*msg, field, nullptr));
          return parse_string(object);
        } else {
          // HACK around inability to get mutable_string in reflection
          std::string* object = &const_cast<std::string&>(
              reflection->GetStringReference(*msg, field, nullptr));
          return parse_string(object);
        }
      }
      GOOGLE_LOG(FATAL) << "No other type than string supported";
    }
    case FieldDescriptor::TYPE_MESSAGE: {
      Message* object;
      if (field->is_repeated()) {
        object = reflection->AddMessage(msg, field, ctx->data().factory);
      } else {
        object = reflection->MutableMessage(msg, field, ctx->data().factory);
      }
      return ctx->ParseMessage(object, ptr);
    }
    default:
      GOOGLE_LOG(FATAL) << "Wrong type for length delim " << field->type();
  }
  return nullptr;  // Make compiler happy.
}

Message* GetGroup(int field_number, const FieldDescriptor* field, Message* msg,
                  const Reflection* reflection) {
  if (field->is_repeated()) {
    return reflection->AddMessage(msg, field, nullptr);
  } else {
    return reflection->MutableMessage(msg, field, nullptr);
  }
}

const char* Message::_InternalParse(const char* ptr,
                                    internal::ParseContext* ctx) {
  class ReflectiveFieldParser {
   public:
    ReflectiveFieldParser(Message* msg, internal::ParseContext* ctx)
        : ReflectiveFieldParser(msg, ctx, false) {}

    void AddVarint(uint32 num, uint64 value) {
      if (is_item_ && num == 2) {
        if (!payload_.empty()) {
          auto field = Field(value, 2);
          if (field && field->message_type()) {
            auto child = reflection_->MutableMessage(msg_, field);
            // TODO(gerbens) signal error
            child->ParsePartialFromString(payload_);
          } else {
            MutableUnknown()->AddLengthDelimited(value)->swap(payload_);
          }
          return;
        }
        type_id_ = value;
        return;
      }
      auto field = Field(num, 0);
      if (field) {
        SetField(value, field, msg_, reflection_);
      } else {
        MutableUnknown()->AddVarint(num, value);
      }
    }
    void AddFixed64(uint32 num, uint64 value) {
      auto field = Field(num, 1);
      if (field) {
        SetField(value, field, msg_, reflection_);
      } else {
        MutableUnknown()->AddFixed64(num, value);
      }
    }
    const char* ParseLengthDelimited(uint32 num, const char* ptr,
                                     internal::ParseContext* ctx) {
      if (is_item_ && num == 3) {
        if (type_id_ == 0) {
          return InlineGreedyStringParser(&payload_, ptr, ctx);
        }
        num = type_id_;
        type_id_ = 0;
      }
      auto field = Field(num, 2);
      if (field) {
        return ParseLenDelim(num, field, msg_, reflection_, ptr, ctx);
      } else {
        return InlineGreedyStringParser(
            MutableUnknown()->AddLengthDelimited(num), ptr, ctx);
      }
    }
    const char* ParseGroup(uint32 num, const char* ptr,
                           internal::ParseContext* ctx) {
      if (!is_item_ && descriptor_->options().message_set_wire_format() &&
          num == 1) {
        is_item_ = true;
        ptr = ctx->ParseGroup(this, ptr, num * 8 + 3);
        is_item_ = false;
        type_id_ = 0;
        return ptr;
      }
      auto field = Field(num, 3);
      if (field) {
        auto msg = GetGroup(num, field, msg_, reflection_);
        return ctx->ParseGroup(msg, ptr, num * 8 + 3);
      } else {
        return UnknownFieldParse(num * 8 + 3, MutableUnknown(), ptr, ctx);
      }
    }
    void AddFixed32(uint32 num, uint32 value) {
      auto field = Field(num, 5);
      if (field) {
        SetField(value, field, msg_, reflection_);
      } else {
        MutableUnknown()->AddFixed32(num, value);
      }
    }

    const char* _InternalParse(const char* ptr, internal::ParseContext* ctx) {
      // We're parsing the a MessageSetItem
      GOOGLE_DCHECK(is_item_);
      return internal::WireFormatParser(*this, ptr, ctx);
    }

   private:
    Message* msg_;
    const Descriptor* descriptor_;
    const Reflection* reflection_;
    internal::ParseContext* ctx_;
    UnknownFieldSet* unknown_ = nullptr;
    bool is_item_ = false;
    uint32 type_id_ = 0;
    std::string payload_;

    ReflectiveFieldParser(Message* msg, internal::ParseContext* ctx,
                          bool is_item)
        : msg_(msg),
          descriptor_(msg->GetDescriptor()),
          reflection_(msg->GetReflection()),
          ctx_(ctx),
          is_item_(is_item) {
      GOOGLE_CHECK(descriptor_) << msg->GetTypeName();
      GOOGLE_CHECK(reflection_) << msg->GetTypeName();
    }

    const FieldDescriptor* Field(int num, int wire_type) {
      auto field = descriptor_->FindFieldByNumber(num);

      // If that failed, check if the field is an extension.
      if (field == nullptr && descriptor_->IsExtensionNumber(num)) {
        const DescriptorPool* pool = ctx_->data().pool;
        if (pool == NULL) {
          field = reflection_->FindKnownExtensionByNumber(num);
        } else {
          field = pool->FindExtensionByNumber(descriptor_, num);
        }
      }
      if (field == nullptr) return nullptr;

      if (internal::WireFormat::WireTypeForFieldType(field->type()) !=
          wire_type) {
        if (field->is_packable()) {
          if (wire_type ==
              internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
            return field;
          }
        }
        return nullptr;
      }
      return field;
    }

    UnknownFieldSet* MutableUnknown() {
      if (unknown_) return unknown_;
      return unknown_ = reflection_->MutableUnknownFields(msg_);
    }
  };

  ReflectiveFieldParser field_parser(this, ctx);
  return internal::WireFormatParser(field_parser, ptr, ctx);
}
#endif  // GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

void Message::SerializeWithCachedSizes(io::CodedOutputStream* output) const {
  const internal::SerializationTable* table =
      static_cast<const internal::SerializationTable*>(InternalGetTable());
  if (table == 0) {
    WireFormat::SerializeWithCachedSizes(*this, GetCachedSize(), output);
  } else {
    internal::TableSerialize(*this, table, output);
  }
}

size_t Message::ByteSizeLong() const {
  size_t size = WireFormat::ByteSize(*this);
  SetCachedSize(internal::ToCachedSize(size));
  return size;
}

void Message::SetCachedSize(int /* size */) const {
  GOOGLE_LOG(FATAL) << "Message class \"" << GetDescriptor()->full_name()
             << "\" implements neither SetCachedSize() nor ByteSize().  "
                "Must implement one or the other.";
}

size_t Message::SpaceUsedLong() const {
  return GetReflection()->SpaceUsedLong(*this);
}

// =============================================================================
// MessageFactory

MessageFactory::~MessageFactory() {}

namespace {

class GeneratedMessageFactory : public MessageFactory {
 public:
  static GeneratedMessageFactory* singleton();

  void RegisterFile(const google::protobuf::internal::DescriptorTable* table);
  void RegisterType(const Descriptor* descriptor, const Message* prototype);

  // implements MessageFactory ---------------------------------------
  const Message* GetPrototype(const Descriptor* type) override;

 private:
  // Only written at static init time, so does not require locking.
  std::unordered_map<const char*, const google::protobuf::internal::DescriptorTable*,
                     hash<const char*>, streq>
      file_map_;

  internal::WrappedMutex mutex_;
  // Initialized lazily, so requires locking.
  std::unordered_map<const Descriptor*, const Message*> type_map_;
};

GeneratedMessageFactory* GeneratedMessageFactory::singleton() {
  static auto instance =
      internal::OnShutdownDelete(new GeneratedMessageFactory);
  return instance;
}

void GeneratedMessageFactory::RegisterFile(
    const google::protobuf::internal::DescriptorTable* table) {
  if (!InsertIfNotPresent(&file_map_, table->filename, table)) {
    GOOGLE_LOG(FATAL) << "File is already registered: " << table->filename;
  }
}

void GeneratedMessageFactory::RegisterType(const Descriptor* descriptor,
                                           const Message* prototype) {
  GOOGLE_DCHECK_EQ(descriptor->file()->pool(), DescriptorPool::generated_pool())
      << "Tried to register a non-generated type with the generated "
         "type registry.";

  // This should only be called as a result of calling a file registration
  // function during GetPrototype(), in which case we already have locked
  // the mutex.
  mutex_.AssertHeld();
  if (!InsertIfNotPresent(&type_map_, descriptor, prototype)) {
    GOOGLE_LOG(DFATAL) << "Type is already registered: " << descriptor->full_name();
  }
}


const Message* GeneratedMessageFactory::GetPrototype(const Descriptor* type) {
  {
    ReaderMutexLock lock(&mutex_);
    const Message* result = FindPtrOrNull(type_map_, type);
    if (result != NULL) return result;
  }

  // If the type is not in the generated pool, then we can't possibly handle
  // it.
  if (type->file()->pool() != DescriptorPool::generated_pool()) return NULL;

  // Apparently the file hasn't been registered yet.  Let's do that now.
  const internal::DescriptorTable* registration_data =
      FindPtrOrNull(file_map_, type->file()->name().c_str());
  if (registration_data == NULL) {
    GOOGLE_LOG(DFATAL) << "File appears to be in generated pool but wasn't "
                   "registered: "
                << type->file()->name();
    return NULL;
  }

  WriterMutexLock lock(&mutex_);

  // Check if another thread preempted us.
  const Message* result = FindPtrOrNull(type_map_, type);
  if (result == NULL) {
    // Nope.  OK, register everything.
    internal::RegisterFileLevelMetadata(registration_data);
    // Should be here now.
    result = FindPtrOrNull(type_map_, type);
  }

  if (result == NULL) {
    GOOGLE_LOG(DFATAL) << "Type appears to be in generated pool but wasn't "
                << "registered: " << type->full_name();
  }

  return result;
}

}  // namespace

MessageFactory* MessageFactory::generated_factory() {
  return GeneratedMessageFactory::singleton();
}

void MessageFactory::InternalRegisterGeneratedFile(
    const google::protobuf::internal::DescriptorTable* table) {
  GeneratedMessageFactory::singleton()->RegisterFile(table);
}

void MessageFactory::InternalRegisterGeneratedMessage(
    const Descriptor* descriptor, const Message* prototype) {
  GeneratedMessageFactory::singleton()->RegisterType(descriptor, prototype);
}


namespace {
template <typename T>
T* GetSingleton() {
  static T singleton;
  return &singleton;
}
}  // namespace

const internal::RepeatedFieldAccessor* Reflection::RepeatedFieldAccessor(
    const FieldDescriptor* field) const {
  GOOGLE_CHECK(field->is_repeated());
  switch (field->cpp_type()) {
#define HANDLE_PRIMITIVE_TYPE(TYPE, type) \
  case FieldDescriptor::CPPTYPE_##TYPE:   \
    return GetSingleton<internal::RepeatedFieldPrimitiveAccessor<type> >();
    HANDLE_PRIMITIVE_TYPE(INT32, int32)
    HANDLE_PRIMITIVE_TYPE(UINT32, uint32)
    HANDLE_PRIMITIVE_TYPE(INT64, int64)
    HANDLE_PRIMITIVE_TYPE(UINT64, uint64)
    HANDLE_PRIMITIVE_TYPE(FLOAT, float)
    HANDLE_PRIMITIVE_TYPE(DOUBLE, double)
    HANDLE_PRIMITIVE_TYPE(BOOL, bool)
    HANDLE_PRIMITIVE_TYPE(ENUM, int32)
#undef HANDLE_PRIMITIVE_TYPE
    case FieldDescriptor::CPPTYPE_STRING:
      switch (field->options().ctype()) {
        default:
        case FieldOptions::STRING:
          return GetSingleton<internal::RepeatedPtrFieldStringAccessor>();
      }
      break;
    case FieldDescriptor::CPPTYPE_MESSAGE:
      if (field->is_map()) {
        return GetSingleton<internal::MapFieldAccessor>();
      } else {
        return GetSingleton<internal::RepeatedPtrFieldMessageAccessor>();
      }
  }
  GOOGLE_LOG(FATAL) << "Should not reach here.";
  return NULL;
}

namespace internal {
template <>
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
// Note: force noinline to workaround MSVC compiler bug with /Zc:inline, issue
// #240
PROTOBUF_NOINLINE
#endif
    Message*
    GenericTypeHandler<Message>::NewFromPrototype(const Message* prototype,
                                                  Arena* arena) {
  return prototype->New(arena);
}
template <>
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
// Note: force noinline to workaround MSVC compiler bug with /Zc:inline, issue
// #240
PROTOBUF_NOINLINE
#endif
    Arena*
    GenericTypeHandler<Message>::GetArena(Message* value) {
  return value->GetArena();
}
template <>
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
// Note: force noinline to workaround MSVC compiler bug with /Zc:inline, issue
// #240
PROTOBUF_NOINLINE
#endif
    void*
    GenericTypeHandler<Message>::GetMaybeArenaPointer(Message* value) {
  return value->GetMaybeArenaPointer();
}
}  // namespace internal

}  // namespace protobuf
}  // namespace google
