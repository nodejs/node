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

// Authors: wink@google.com (Wink Saville),
//          kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/message_lite.h>

#include <climits>
#include <cstdint>
#include <string>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/parse_context.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/generated_message_table_driven.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/repeated_field.h>

#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/stl_util.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

std::string MessageLite::InitializationErrorString() const {
  return "(cannot determine missing fields for lite message)";
}

std::string MessageLite::DebugString() const {
  std::uintptr_t address = reinterpret_cast<std::uintptr_t>(this);
  return StrCat("MessageLite at 0x", strings::Hex(address));
}

namespace {

// When serializing, we first compute the byte size, then serialize the message.
// If serialization produces a different number of bytes than expected, we
// call this function, which crashes.  The problem could be due to a bug in the
// protobuf implementation but is more likely caused by concurrent modification
// of the message.  This function attempts to distinguish between the two and
// provide a useful error message.
void ByteSizeConsistencyError(size_t byte_size_before_serialization,
                              size_t byte_size_after_serialization,
                              size_t bytes_produced_by_serialization,
                              const MessageLite& message) {
  GOOGLE_CHECK_EQ(byte_size_before_serialization, byte_size_after_serialization)
      << message.GetTypeName()
      << " was modified concurrently during serialization.";
  GOOGLE_CHECK_EQ(bytes_produced_by_serialization, byte_size_before_serialization)
      << "Byte size calculation and serialization were inconsistent.  This "
         "may indicate a bug in protocol buffers or it may be caused by "
         "concurrent modification of "
      << message.GetTypeName() << ".";
  GOOGLE_LOG(FATAL) << "This shouldn't be called if all the sizes are equal.";
}

std::string InitializationErrorMessage(const char* action,
                                       const MessageLite& message) {
  // Note:  We want to avoid depending on strutil in the lite library, otherwise
  //   we'd use:
  //
  // return strings::Substitute(
  //   "Can't $0 message of type \"$1\" because it is missing required "
  //   "fields: $2",
  //   action, message.GetTypeName(),
  //   message.InitializationErrorString());

  std::string result;
  result += "Can't ";
  result += action;
  result += " message of type \"";
  result += message.GetTypeName();
  result += "\" because it is missing required fields: ";
  result += message.InitializationErrorString();
  return result;
}

inline StringPiece as_string_view(const void* data, int size) {
  return StringPiece(static_cast<const char*>(data), size);
}

}  // namespace

void MessageLite::LogInitializationErrorMessage() const {
  GOOGLE_LOG(ERROR) << InitializationErrorMessage("parse", *this);
}

namespace internal {

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

template <bool aliasing>
bool MergePartialFromImpl(StringPiece input, MessageLite* msg) {
  const char* ptr;
  internal::ParseContext ctx(io::CodedInputStream::GetDefaultRecursionLimit(),
                             aliasing, &ptr, input);
  ptr = msg->_InternalParse(ptr, &ctx);
  // ctx has an explicit limit set (length of string_view).
  return ptr && ctx.EndedAtLimit();
}

template <bool aliasing>
bool MergePartialFromImpl(io::ZeroCopyInputStream* input, MessageLite* msg) {
  const char* ptr;
  internal::ParseContext ctx(io::CodedInputStream::GetDefaultRecursionLimit(),
                             aliasing, &ptr, input);
  ptr = msg->_InternalParse(ptr, &ctx);
  // ctx has no explicit limit (hence we end on end of stream)
  return ptr && ctx.EndedAtEndOfStream();
}

template <bool aliasing>
bool MergePartialFromImpl(BoundedZCIS input, MessageLite* msg) {
  const char* ptr;
  internal::ParseContext ctx(io::CodedInputStream::GetDefaultRecursionLimit(),
                             aliasing, &ptr, input.zcis, input.limit);
  ptr = msg->_InternalParse(ptr, &ctx);
  if (PROTOBUF_PREDICT_FALSE(!ptr)) return false;
  ctx.BackUp(ptr);
  return ctx.EndedAtLimit();
}

#else  // !GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

inline bool InlineMergePartialEntireStream(io::CodedInputStream* cis,
                                           MessageLite* message,
                                           bool aliasing) {
  return message->MergePartialFromCodedStream(cis) &&
         cis->ConsumedEntireMessage();
}

template <bool aliasing>
bool MergePartialFromImpl(StringPiece input, MessageLite* msg) {
  io::CodedInputStream decoder(reinterpret_cast<const uint8*>(input.data()),
                               input.size());
  return InlineMergePartialEntireStream(&decoder, msg, aliasing);
}

template <bool aliasing>
bool MergePartialFromImpl(BoundedZCIS input, MessageLite* msg) {
  io::CodedInputStream decoder(input.zcis);
  decoder.PushLimit(input.limit);
  return InlineMergePartialEntireStream(&decoder, msg, aliasing) &&
         decoder.BytesUntilLimit() == 0;
}

template <bool aliasing>
bool MergePartialFromImpl(io::ZeroCopyInputStream* input, MessageLite* msg) {
  io::CodedInputStream decoder(input);
  return InlineMergePartialEntireStream(&decoder, msg, aliasing);
}

#endif  // !GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

template bool MergePartialFromImpl<false>(StringPiece input,
                                          MessageLite* msg);
template bool MergePartialFromImpl<true>(StringPiece input,
                                         MessageLite* msg);
template bool MergePartialFromImpl<false>(io::ZeroCopyInputStream* input,
                                          MessageLite* msg);
template bool MergePartialFromImpl<true>(io::ZeroCopyInputStream* input,
                                         MessageLite* msg);
template bool MergePartialFromImpl<false>(BoundedZCIS input, MessageLite* msg);
template bool MergePartialFromImpl<true>(BoundedZCIS input, MessageLite* msg);

}  // namespace internal

MessageLite* MessageLite::New(Arena* arena) const {
  MessageLite* message = New();
  if (arena != NULL) {
    arena->Own(message);
  }
  return message;
}

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
class ZeroCopyCodedInputStream : public io::ZeroCopyInputStream {
 public:
  ZeroCopyCodedInputStream(io::CodedInputStream* cis) : cis_(cis) {}
  bool Next(const void** data, int* size) final {
    if (!cis_->GetDirectBufferPointer(data, size)) return false;
    cis_->Skip(*size);
    return true;
  }
  void BackUp(int count) final { cis_->Advance(-count); }
  bool Skip(int count) final { return cis_->Skip(count); }
  int64 ByteCount() const final { return 0; }

  bool aliasing_enabled() { return cis_->aliasing_enabled_; }

 private:
  io::CodedInputStream* cis_;
};

bool MessageLite::MergePartialFromCodedStream(io::CodedInputStream* input) {
  ZeroCopyCodedInputStream zcis(input);
  const char* ptr;
  internal::ParseContext ctx(input->RecursionBudget(), zcis.aliasing_enabled(),
                             &ptr, &zcis);
  // MergePartialFromCodedStream allows terminating the wireformat by 0 or
  // end-group tag. Leaving it up to the caller to verify correct ending by
  // calling LastTagWas on input. We need to maintain this behavior.
  ctx.TrackCorrectEnding();
  ctx.data().pool = input->GetExtensionPool();
  ctx.data().factory = input->GetExtensionFactory();
  ptr = _InternalParse(ptr, &ctx);
  if (PROTOBUF_PREDICT_FALSE(!ptr)) return false;
  ctx.BackUp(ptr);
  if (!ctx.EndedAtEndOfStream()) {
    GOOGLE_DCHECK(ctx.LastTag() != 1);  // We can't end on a pushed limit.
    if (ctx.IsExceedingLimit(ptr)) return false;
    input->SetLastTag(ctx.LastTag());
    return true;
  }
  input->SetConsumed();
  return true;
}
#endif  // GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

bool MessageLite::MergeFromCodedStream(io::CodedInputStream* input) {
  return MergePartialFromCodedStream(input) && IsInitializedWithErrors();
}

bool MessageLite::ParseFromCodedStream(io::CodedInputStream* input) {
  Clear();
  return MergeFromCodedStream(input);
}

bool MessageLite::ParsePartialFromCodedStream(io::CodedInputStream* input) {
  Clear();
  return MergePartialFromCodedStream(input);
}

bool MessageLite::ParseFromZeroCopyStream(io::ZeroCopyInputStream* input) {
  return ParseFrom<kParse>(input);
}

bool MessageLite::ParsePartialFromZeroCopyStream(
    io::ZeroCopyInputStream* input) {
  return ParseFrom<kParsePartial>(input);
}

bool MessageLite::ParseFromFileDescriptor(int file_descriptor) {
  io::FileInputStream input(file_descriptor);
  return ParseFromZeroCopyStream(&input) && input.GetErrno() == 0;
}

bool MessageLite::ParsePartialFromFileDescriptor(int file_descriptor) {
  io::FileInputStream input(file_descriptor);
  return ParsePartialFromZeroCopyStream(&input) && input.GetErrno() == 0;
}

bool MessageLite::ParseFromIstream(std::istream* input) {
  io::IstreamInputStream zero_copy_input(input);
  return ParseFromZeroCopyStream(&zero_copy_input) && input->eof();
}

bool MessageLite::ParsePartialFromIstream(std::istream* input) {
  io::IstreamInputStream zero_copy_input(input);
  return ParsePartialFromZeroCopyStream(&zero_copy_input) && input->eof();
}

bool MessageLite::MergePartialFromBoundedZeroCopyStream(
    io::ZeroCopyInputStream* input, int size) {
  return ParseFrom<kMergePartial>(internal::BoundedZCIS{input, size});
}

bool MessageLite::MergeFromBoundedZeroCopyStream(io::ZeroCopyInputStream* input,
                                                 int size) {
  return ParseFrom<kMerge>(internal::BoundedZCIS{input, size});
}

bool MessageLite::ParseFromBoundedZeroCopyStream(io::ZeroCopyInputStream* input,
                                                 int size) {
  return ParseFrom<kParse>(internal::BoundedZCIS{input, size});
}

bool MessageLite::ParsePartialFromBoundedZeroCopyStream(
    io::ZeroCopyInputStream* input, int size) {
  return ParseFrom<kParsePartial>(internal::BoundedZCIS{input, size});
}

bool MessageLite::ParseFromString(const std::string& data) {
  return ParseFrom<kParse>(data);
}

bool MessageLite::ParsePartialFromString(const std::string& data) {
  return ParseFrom<kParsePartial>(data);
}

bool MessageLite::ParseFromArray(const void* data, int size) {
  return ParseFrom<kParse>(as_string_view(data, size));
}

bool MessageLite::ParsePartialFromArray(const void* data, int size) {
  return ParseFrom<kParsePartial>(as_string_view(data, size));
}

bool MessageLite::MergeFromString(const std::string& data) {
  return ParseFrom<kMerge>(data);
}


// ===================================================================

uint8* MessageLite::SerializeWithCachedSizesToArray(uint8* target) const {
  const internal::SerializationTable* table =
      static_cast<const internal::SerializationTable*>(InternalGetTable());
  auto deterministic =
      io::CodedOutputStream::IsDefaultSerializationDeterministic();
  if (table) {
    return internal::TableSerializeToArray(*this, table, deterministic, target);
  } else {
    if (deterministic) {
      // We only optimize this when using optimize_for = SPEED.  In other cases
      // we just use the CodedOutputStream path.
      int size = GetCachedSize();
      io::ArrayOutputStream out(target, size);
      io::CodedOutputStream coded_out(&out);
      coded_out.SetSerializationDeterministic(true);
      SerializeWithCachedSizes(&coded_out);
      GOOGLE_CHECK(!coded_out.HadError());
      return target + size;
    } else {
      return InternalSerializeWithCachedSizesToArray(target);
    }
  }
}

bool MessageLite::SerializeToCodedStream(io::CodedOutputStream* output) const {
  GOOGLE_DCHECK(IsInitialized()) << InitializationErrorMessage("serialize", *this);
  return SerializePartialToCodedStream(output);
}

bool MessageLite::SerializePartialToCodedStream(
    io::CodedOutputStream* output) const {
  const size_t size = ByteSizeLong();  // Force size to be cached.
  if (size > INT_MAX) {
    GOOGLE_LOG(ERROR) << GetTypeName()
               << " exceeded maximum protobuf size of 2GB: " << size;
    return false;
  }

  if (!output->IsSerializationDeterministic()) {
    uint8* buffer = output->GetDirectBufferForNBytesAndAdvance(size);
    if (buffer != nullptr) {
      uint8* end = InternalSerializeWithCachedSizesToArray(buffer);
      if (end - buffer != size) {
        ByteSizeConsistencyError(size, ByteSizeLong(), end - buffer, *this);
      }
      return true;
    }
  }
  int original_byte_count = output->ByteCount();
  SerializeWithCachedSizes(output);
  if (output->HadError()) {
    return false;
  }
  int final_byte_count = output->ByteCount();

  if (final_byte_count - original_byte_count != size) {
    ByteSizeConsistencyError(size, ByteSizeLong(),
                             final_byte_count - original_byte_count, *this);
  }

  return true;
}

bool MessageLite::SerializeToZeroCopyStream(
    io::ZeroCopyOutputStream* output) const {
  io::CodedOutputStream encoder(output);
  return SerializeToCodedStream(&encoder);
}

bool MessageLite::SerializePartialToZeroCopyStream(
    io::ZeroCopyOutputStream* output) const {
  io::CodedOutputStream encoder(output);
  return SerializePartialToCodedStream(&encoder);
}

bool MessageLite::SerializeToFileDescriptor(int file_descriptor) const {
  io::FileOutputStream output(file_descriptor);
  return SerializeToZeroCopyStream(&output) && output.Flush();
}

bool MessageLite::SerializePartialToFileDescriptor(int file_descriptor) const {
  io::FileOutputStream output(file_descriptor);
  return SerializePartialToZeroCopyStream(&output) && output.Flush();
}

bool MessageLite::SerializeToOstream(std::ostream* output) const {
  {
    io::OstreamOutputStream zero_copy_output(output);
    if (!SerializeToZeroCopyStream(&zero_copy_output)) return false;
  }
  return output->good();
}

bool MessageLite::SerializePartialToOstream(std::ostream* output) const {
  io::OstreamOutputStream zero_copy_output(output);
  return SerializePartialToZeroCopyStream(&zero_copy_output);
}

bool MessageLite::AppendToString(std::string* output) const {
  GOOGLE_DCHECK(IsInitialized()) << InitializationErrorMessage("serialize", *this);
  return AppendPartialToString(output);
}

bool MessageLite::AppendPartialToString(std::string* output) const {
  size_t old_size = output->size();
  size_t byte_size = ByteSizeLong();
  if (byte_size > INT_MAX) {
    GOOGLE_LOG(ERROR) << GetTypeName()
               << " exceeded maximum protobuf size of 2GB: " << byte_size;
    return false;
  }

  STLStringResizeUninitialized(output, old_size + byte_size);
  uint8* start =
      reinterpret_cast<uint8*>(io::mutable_string_data(output) + old_size);
  uint8* end = SerializeWithCachedSizesToArray(start);
  if (end - start != byte_size) {
    ByteSizeConsistencyError(byte_size, ByteSizeLong(), end - start, *this);
  }
  return true;
}

bool MessageLite::SerializeToString(std::string* output) const {
  output->clear();
  return AppendToString(output);
}

bool MessageLite::SerializePartialToString(std::string* output) const {
  output->clear();
  return AppendPartialToString(output);
}

bool MessageLite::SerializeToArray(void* data, int size) const {
  GOOGLE_DCHECK(IsInitialized()) << InitializationErrorMessage("serialize", *this);
  return SerializePartialToArray(data, size);
}

bool MessageLite::SerializePartialToArray(void* data, int size) const {
  const size_t byte_size = ByteSizeLong();
  if (byte_size > INT_MAX) {
    GOOGLE_LOG(ERROR) << GetTypeName()
               << " exceeded maximum protobuf size of 2GB: " << byte_size;
    return false;
  }
  if (size < byte_size) return false;
  uint8* start = reinterpret_cast<uint8*>(data);
  uint8* end = SerializeWithCachedSizesToArray(start);
  if (end - start != byte_size) {
    ByteSizeConsistencyError(byte_size, ByteSizeLong(), end - start, *this);
  }
  return true;
}

std::string MessageLite::SerializeAsString() const {
  // If the compiler implements the (Named) Return Value Optimization,
  // the local variable 'output' will not actually reside on the stack
  // of this function, but will be overlaid with the object that the
  // caller supplied for the return value to be constructed in.
  std::string output;
  if (!AppendToString(&output)) output.clear();
  return output;
}

std::string MessageLite::SerializePartialAsString() const {
  std::string output;
  if (!AppendPartialToString(&output)) output.clear();
  return output;
}

void MessageLite::SerializeWithCachedSizes(
    io::CodedOutputStream* output) const {
  GOOGLE_DCHECK(InternalGetTable());
  internal::TableSerialize(
      *this,
      static_cast<const internal::SerializationTable*>(InternalGetTable()),
      output);
}


// The table driven code optimizes the case that the CodedOutputStream buffer
// is large enough to serialize into it directly.
// If the proto is optimized for speed, this method will be overridden by
// generated code for maximum speed. If the proto is optimized for size or
// is lite, then we need to specialize this to avoid infinite recursion.
uint8* MessageLite::InternalSerializeWithCachedSizesToArray(
    uint8* target) const {
  const internal::SerializationTable* table =
      static_cast<const internal::SerializationTable*>(InternalGetTable());
  if (table == NULL) {
    // We only optimize this when using optimize_for = SPEED.  In other cases
    // we just use the CodedOutputStream path.
    int size = GetCachedSize();
    io::ArrayOutputStream out(target, size);
    io::CodedOutputStream coded_out(&out);
    SerializeWithCachedSizes(&coded_out);
    GOOGLE_CHECK(!coded_out.HadError());
    return target + size;
  } else {
    return internal::TableSerializeToArray(*this, table, false, target);
  }
}

namespace internal {

template <>
MessageLite* GenericTypeHandler<MessageLite>::NewFromPrototype(
    const MessageLite* prototype, Arena* arena) {
  return prototype->New(arena);
}
template <>
void GenericTypeHandler<MessageLite>::Merge(const MessageLite& from,
                                            MessageLite* to) {
  to->CheckTypeAndMergeFrom(from);
}
template <>
void GenericTypeHandler<std::string>::Merge(const std::string& from,
                                            std::string* to) {
  *to = from;
}

}  // namespace internal

}  // namespace protobuf
}  // namespace google
