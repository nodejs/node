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

#include <google/protobuf/wire_format_lite.h>

#include <stack>
#include <string>
#include <vector>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/io/coded_stream_inl.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/port_def.inc>


namespace google {
namespace protobuf {
namespace internal {

#if !defined(_MSC_VER) || _MSC_VER >= 1900
// Old version of MSVC doesn't like definitions of inline constants, GCC
// requires them.
const int WireFormatLite::kMessageSetItemStartTag;
const int WireFormatLite::kMessageSetItemEndTag;
const int WireFormatLite::kMessageSetTypeIdTag;
const int WireFormatLite::kMessageSetMessageTag;

#endif

// IBM xlC requires prefixing constants with WireFormatLite::
const size_t WireFormatLite::kMessageSetItemTagsSize =
    io::CodedOutputStream::StaticVarintSize32<
        WireFormatLite::kMessageSetItemStartTag>::value +
    io::CodedOutputStream::StaticVarintSize32<
        WireFormatLite::kMessageSetItemEndTag>::value +
    io::CodedOutputStream::StaticVarintSize32<
        WireFormatLite::kMessageSetTypeIdTag>::value +
    io::CodedOutputStream::StaticVarintSize32<
        WireFormatLite::kMessageSetMessageTag>::value;

const WireFormatLite::CppType
    WireFormatLite::kFieldTypeToCppTypeMap[MAX_FIELD_TYPE + 1] = {
        static_cast<CppType>(0),  // 0 is reserved for errors

        CPPTYPE_DOUBLE,   // TYPE_DOUBLE
        CPPTYPE_FLOAT,    // TYPE_FLOAT
        CPPTYPE_INT64,    // TYPE_INT64
        CPPTYPE_UINT64,   // TYPE_UINT64
        CPPTYPE_INT32,    // TYPE_INT32
        CPPTYPE_UINT64,   // TYPE_FIXED64
        CPPTYPE_UINT32,   // TYPE_FIXED32
        CPPTYPE_BOOL,     // TYPE_BOOL
        CPPTYPE_STRING,   // TYPE_STRING
        CPPTYPE_MESSAGE,  // TYPE_GROUP
        CPPTYPE_MESSAGE,  // TYPE_MESSAGE
        CPPTYPE_STRING,   // TYPE_BYTES
        CPPTYPE_UINT32,   // TYPE_UINT32
        CPPTYPE_ENUM,     // TYPE_ENUM
        CPPTYPE_INT32,    // TYPE_SFIXED32
        CPPTYPE_INT64,    // TYPE_SFIXED64
        CPPTYPE_INT32,    // TYPE_SINT32
        CPPTYPE_INT64,    // TYPE_SINT64
};

const WireFormatLite::WireType
    WireFormatLite::kWireTypeForFieldType[MAX_FIELD_TYPE + 1] = {
        static_cast<WireFormatLite::WireType>(-1),  // invalid
        WireFormatLite::WIRETYPE_FIXED64,           // TYPE_DOUBLE
        WireFormatLite::WIRETYPE_FIXED32,           // TYPE_FLOAT
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_INT64
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_UINT64
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_INT32
        WireFormatLite::WIRETYPE_FIXED64,           // TYPE_FIXED64
        WireFormatLite::WIRETYPE_FIXED32,           // TYPE_FIXED32
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_BOOL
        WireFormatLite::WIRETYPE_LENGTH_DELIMITED,  // TYPE_STRING
        WireFormatLite::WIRETYPE_START_GROUP,       // TYPE_GROUP
        WireFormatLite::WIRETYPE_LENGTH_DELIMITED,  // TYPE_MESSAGE
        WireFormatLite::WIRETYPE_LENGTH_DELIMITED,  // TYPE_BYTES
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_UINT32
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_ENUM
        WireFormatLite::WIRETYPE_FIXED32,           // TYPE_SFIXED32
        WireFormatLite::WIRETYPE_FIXED64,           // TYPE_SFIXED64
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_SINT32
        WireFormatLite::WIRETYPE_VARINT,            // TYPE_SINT64
};

bool WireFormatLite::SkipField(io::CodedInputStream* input, uint32 tag) {
  // Field number 0 is illegal.
  if (WireFormatLite::GetTagFieldNumber(tag) == 0) return false;
  switch (WireFormatLite::GetTagWireType(tag)) {
    case WireFormatLite::WIRETYPE_VARINT: {
      uint64 value;
      if (!input->ReadVarint64(&value)) return false;
      return true;
    }
    case WireFormatLite::WIRETYPE_FIXED64: {
      uint64 value;
      if (!input->ReadLittleEndian64(&value)) return false;
      return true;
    }
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
      uint32 length;
      if (!input->ReadVarint32(&length)) return false;
      if (!input->Skip(length)) return false;
      return true;
    }
    case WireFormatLite::WIRETYPE_START_GROUP: {
      if (!input->IncrementRecursionDepth()) return false;
      if (!SkipMessage(input)) return false;
      input->DecrementRecursionDepth();
      // Check that the ending tag matched the starting tag.
      if (!input->LastTagWas(
              WireFormatLite::MakeTag(WireFormatLite::GetTagFieldNumber(tag),
                                      WireFormatLite::WIRETYPE_END_GROUP))) {
        return false;
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_END_GROUP: {
      return false;
    }
    case WireFormatLite::WIRETYPE_FIXED32: {
      uint32 value;
      if (!input->ReadLittleEndian32(&value)) return false;
      return true;
    }
    default: {
      return false;
    }
  }
}

bool WireFormatLite::SkipField(io::CodedInputStream* input, uint32 tag,
                               io::CodedOutputStream* output) {
  // Field number 0 is illegal.
  if (WireFormatLite::GetTagFieldNumber(tag) == 0) return false;
  switch (WireFormatLite::GetTagWireType(tag)) {
    case WireFormatLite::WIRETYPE_VARINT: {
      uint64 value;
      if (!input->ReadVarint64(&value)) return false;
      output->WriteVarint32(tag);
      output->WriteVarint64(value);
      return true;
    }
    case WireFormatLite::WIRETYPE_FIXED64: {
      uint64 value;
      if (!input->ReadLittleEndian64(&value)) return false;
      output->WriteVarint32(tag);
      output->WriteLittleEndian64(value);
      return true;
    }
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
      uint32 length;
      if (!input->ReadVarint32(&length)) return false;
      output->WriteVarint32(tag);
      output->WriteVarint32(length);
      // TODO(mkilavuz): Provide API to prevent extra string copying.
      std::string temp;
      if (!input->ReadString(&temp, length)) return false;
      output->WriteString(temp);
      return true;
    }
    case WireFormatLite::WIRETYPE_START_GROUP: {
      output->WriteVarint32(tag);
      if (!input->IncrementRecursionDepth()) return false;
      if (!SkipMessage(input, output)) return false;
      input->DecrementRecursionDepth();
      // Check that the ending tag matched the starting tag.
      if (!input->LastTagWas(
              WireFormatLite::MakeTag(WireFormatLite::GetTagFieldNumber(tag),
                                      WireFormatLite::WIRETYPE_END_GROUP))) {
        return false;
      }
      return true;
    }
    case WireFormatLite::WIRETYPE_END_GROUP: {
      return false;
    }
    case WireFormatLite::WIRETYPE_FIXED32: {
      uint32 value;
      if (!input->ReadLittleEndian32(&value)) return false;
      output->WriteVarint32(tag);
      output->WriteLittleEndian32(value);
      return true;
    }
    default: {
      return false;
    }
  }
}

bool WireFormatLite::SkipMessage(io::CodedInputStream* input) {
  while (true) {
    uint32 tag = input->ReadTag();
    if (tag == 0) {
      // End of input.  This is a valid place to end, so return true.
      return true;
    }

    WireFormatLite::WireType wire_type = WireFormatLite::GetTagWireType(tag);

    if (wire_type == WireFormatLite::WIRETYPE_END_GROUP) {
      // Must be the end of the message.
      return true;
    }

    if (!SkipField(input, tag)) return false;
  }
}

bool WireFormatLite::SkipMessage(io::CodedInputStream* input,
                                 io::CodedOutputStream* output) {
  while (true) {
    uint32 tag = input->ReadTag();
    if (tag == 0) {
      // End of input.  This is a valid place to end, so return true.
      return true;
    }

    WireFormatLite::WireType wire_type = WireFormatLite::GetTagWireType(tag);

    if (wire_type == WireFormatLite::WIRETYPE_END_GROUP) {
      output->WriteVarint32(tag);
      // Must be the end of the message.
      return true;
    }

    if (!SkipField(input, tag, output)) return false;
  }
}

bool FieldSkipper::SkipField(io::CodedInputStream* input, uint32 tag) {
  return WireFormatLite::SkipField(input, tag);
}

bool FieldSkipper::SkipMessage(io::CodedInputStream* input) {
  return WireFormatLite::SkipMessage(input);
}

void FieldSkipper::SkipUnknownEnum(int /* field_number */, int /* value */) {
  // Nothing.
}

bool CodedOutputStreamFieldSkipper::SkipField(io::CodedInputStream* input,
                                              uint32 tag) {
  return WireFormatLite::SkipField(input, tag, unknown_fields_);
}

bool CodedOutputStreamFieldSkipper::SkipMessage(io::CodedInputStream* input) {
  return WireFormatLite::SkipMessage(input, unknown_fields_);
}

void CodedOutputStreamFieldSkipper::SkipUnknownEnum(int field_number,
                                                    int value) {
  unknown_fields_->WriteVarint32(field_number);
  unknown_fields_->WriteVarint64(value);
}

bool WireFormatLite::ReadPackedEnumPreserveUnknowns(
    io::CodedInputStream* input, int field_number, bool (*is_valid)(int),
    io::CodedOutputStream* unknown_fields_stream, RepeatedField<int>* values) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  while (input->BytesUntilLimit() > 0) {
    int value;
    if (!ReadPrimitive<int, WireFormatLite::TYPE_ENUM>(input, &value)) {
      return false;
    }
    if (is_valid == NULL || is_valid(value)) {
      values->Add(value);
    } else {
      uint32 tag = WireFormatLite::MakeTag(field_number,
                                           WireFormatLite::WIRETYPE_VARINT);
      unknown_fields_stream->WriteVarint32(tag);
      unknown_fields_stream->WriteVarint32(value);
    }
  }
  input->PopLimit(limit);
  return true;
}

#if !defined(PROTOBUF_LITTLE_ENDIAN)

namespace {
void EncodeFixedSizeValue(float v, uint8* dest) {
  WireFormatLite::WriteFloatNoTagToArray(v, dest);
}

void EncodeFixedSizeValue(double v, uint8* dest) {
  WireFormatLite::WriteDoubleNoTagToArray(v, dest);
}

void EncodeFixedSizeValue(uint32 v, uint8* dest) {
  WireFormatLite::WriteFixed32NoTagToArray(v, dest);
}

void EncodeFixedSizeValue(uint64 v, uint8* dest) {
  WireFormatLite::WriteFixed64NoTagToArray(v, dest);
}

void EncodeFixedSizeValue(int32 v, uint8* dest) {
  WireFormatLite::WriteSFixed32NoTagToArray(v, dest);
}

void EncodeFixedSizeValue(int64 v, uint8* dest) {
  WireFormatLite::WriteSFixed64NoTagToArray(v, dest);
}

void EncodeFixedSizeValue(bool v, uint8* dest) {
  WireFormatLite::WriteBoolNoTagToArray(v, dest);
}
}  // anonymous namespace

#endif  // !defined(PROTOBUF_LITTLE_ENDIAN)

template <typename CType>
static void WriteArray(const CType* a, int n, io::CodedOutputStream* output) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  output->WriteRaw(reinterpret_cast<const char*>(a), n * sizeof(a[0]));
#else
  const int kAtATime = 128;
  uint8 buf[sizeof(CType) * kAtATime];
  for (int i = 0; i < n; i += kAtATime) {
    int to_do = std::min(kAtATime, n - i);
    uint8* ptr = buf;
    for (int j = 0; j < to_do; j++) {
      EncodeFixedSizeValue(a[i + j], ptr);
      ptr += sizeof(a[0]);
    }
    output->WriteRaw(buf, to_do * sizeof(a[0]));
  }
#endif
}

void WireFormatLite::WriteFloatArray(const float* a, int n,
                                     io::CodedOutputStream* output) {
  WriteArray<float>(a, n, output);
}

void WireFormatLite::WriteDoubleArray(const double* a, int n,
                                      io::CodedOutputStream* output) {
  WriteArray<double>(a, n, output);
}

void WireFormatLite::WriteFixed32Array(const uint32* a, int n,
                                       io::CodedOutputStream* output) {
  WriteArray<uint32>(a, n, output);
}

void WireFormatLite::WriteFixed64Array(const uint64* a, int n,
                                       io::CodedOutputStream* output) {
  WriteArray<uint64>(a, n, output);
}

void WireFormatLite::WriteSFixed32Array(const int32* a, int n,
                                        io::CodedOutputStream* output) {
  WriteArray<int32>(a, n, output);
}

void WireFormatLite::WriteSFixed64Array(const int64* a, int n,
                                        io::CodedOutputStream* output) {
  WriteArray<int64>(a, n, output);
}

void WireFormatLite::WriteBoolArray(const bool* a, int n,
                                    io::CodedOutputStream* output) {
  WriteArray<bool>(a, n, output);
}

void WireFormatLite::WriteInt32(int field_number, int32 value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteInt32NoTag(value, output);
}
void WireFormatLite::WriteInt64(int field_number, int64 value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteInt64NoTag(value, output);
}
void WireFormatLite::WriteUInt32(int field_number, uint32 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteUInt32NoTag(value, output);
}
void WireFormatLite::WriteUInt64(int field_number, uint64 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteUInt64NoTag(value, output);
}
void WireFormatLite::WriteSInt32(int field_number, int32 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteSInt32NoTag(value, output);
}
void WireFormatLite::WriteSInt64(int field_number, int64 value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteSInt64NoTag(value, output);
}
void WireFormatLite::WriteFixed32(int field_number, uint32 value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteFixed32NoTag(value, output);
}
void WireFormatLite::WriteFixed64(int field_number, uint64 value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteFixed64NoTag(value, output);
}
void WireFormatLite::WriteSFixed32(int field_number, int32 value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteSFixed32NoTag(value, output);
}
void WireFormatLite::WriteSFixed64(int field_number, int64 value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteSFixed64NoTag(value, output);
}
void WireFormatLite::WriteFloat(int field_number, float value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteFloatNoTag(value, output);
}
void WireFormatLite::WriteDouble(int field_number, double value,
                                 io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteDoubleNoTag(value, output);
}
void WireFormatLite::WriteBool(int field_number, bool value,
                               io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteBoolNoTag(value, output);
}
void WireFormatLite::WriteEnum(int field_number, int value,
                               io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteEnumNoTag(value, output);
}

void WireFormatLite::WriteString(int field_number, const std::string& value,
                                 io::CodedOutputStream* output) {
  // String is for UTF-8 text only
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK_LE(value.size(), kint32max);
  output->WriteVarint32(value.size());
  output->WriteString(value);
}
void WireFormatLite::WriteStringMaybeAliased(int field_number,
                                             const std::string& value,
                                             io::CodedOutputStream* output) {
  // String is for UTF-8 text only
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK_LE(value.size(), kint32max);
  output->WriteVarint32(value.size());
  output->WriteRawMaybeAliased(value.data(), value.size());
}
void WireFormatLite::WriteBytes(int field_number, const std::string& value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK_LE(value.size(), kint32max);
  output->WriteVarint32(value.size());
  output->WriteString(value);
}
void WireFormatLite::WriteBytesMaybeAliased(int field_number,
                                            const std::string& value,
                                            io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  GOOGLE_CHECK_LE(value.size(), kint32max);
  output->WriteVarint32(value.size());
  output->WriteRawMaybeAliased(value.data(), value.size());
}


void WireFormatLite::WriteGroup(int field_number, const MessageLite& value,
                                io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  value.SerializeWithCachedSizes(output);
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}

void WireFormatLite::WriteMessage(int field_number, const MessageLite& value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  const int size = value.GetCachedSize();
  output->WriteVarint32(size);
  value.SerializeWithCachedSizes(output);
}

void WireFormatLite::WriteSubMessageMaybeToArray(
    int size, const MessageLite& value, io::CodedOutputStream* output) {
  if (!output->IsSerializationDeterministic()) {
    uint8* target = output->GetDirectBufferForNBytesAndAdvance(size);
    if (target != nullptr) {
      uint8* end = value.InternalSerializeWithCachedSizesToArray(target);
      GOOGLE_DCHECK_EQ(end - target, size);
      return;
    }
  }
  value.SerializeWithCachedSizes(output);
}

void WireFormatLite::WriteGroupMaybeToArray(int field_number,
                                            const MessageLite& value,
                                            io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  const int size = value.GetCachedSize();
  WriteSubMessageMaybeToArray(size, value, output);
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}

void WireFormatLite::WriteMessageMaybeToArray(int field_number,
                                              const MessageLite& value,
                                              io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  const int size = value.GetCachedSize();
  output->WriteVarint32(size);
  WriteSubMessageMaybeToArray(size, value, output);
}

PROTOBUF_ALWAYS_INLINE static bool ReadBytesToString(
    io::CodedInputStream* input, std::string* value);
inline static bool ReadBytesToString(io::CodedInputStream* input,
                                     std::string* value) {
  uint32 length;
  return input->ReadVarint32(&length) &&
         input->InternalReadStringInline(value, length);
}

bool WireFormatLite::ReadBytes(io::CodedInputStream* input,
                               std::string* value) {
  return ReadBytesToString(input, value);
}

bool WireFormatLite::ReadBytes(io::CodedInputStream* input, std::string** p) {
  if (*p == &GetEmptyStringAlreadyInited()) {
    *p = new std::string();
  }
  return ReadBytesToString(input, *p);
}

void PrintUTF8ErrorLog(const char* field_name, const char* operation_str,
                       bool emit_stacktrace) {
  std::string stacktrace;
  std::string quoted_field_name = "";
  if (field_name != nullptr) {
    quoted_field_name = StringPrintf(" '%s'", field_name);
  }
  GOOGLE_LOG(ERROR) << "String field" << quoted_field_name << " contains invalid "
             << "UTF-8 data when " << operation_str << " a protocol "
             << "buffer. Use the 'bytes' type if you intend to send raw "
             << "bytes. " << stacktrace;
}

bool WireFormatLite::VerifyUtf8String(const char* data, int size, Operation op,
                                      const char* field_name) {
  if (!IsStructurallyValidUTF8(data, size)) {
    const char* operation_str = NULL;
    switch (op) {
      case PARSE:
        operation_str = "parsing";
        break;
      case SERIALIZE:
        operation_str = "serializing";
        break;
        // no default case: have the compiler warn if a case is not covered.
    }
    PrintUTF8ErrorLog(field_name, operation_str, false);
    return false;
  }
  return true;
}

// this code is deliberately written such that clang makes it into really
// efficient SSE code.
template <bool ZigZag, bool SignExtended, typename T>
static size_t VarintSize(const T* data, const int n) {
#if __cplusplus >= 201103L
  static_assert(sizeof(T) == 4, "This routine only works for 32 bit integers");
  // is_unsigned<T> => !ZigZag
  static_assert(
      (std::is_unsigned<T>::value ^ ZigZag) || std::is_signed<T>::value,
      "Cannot ZigZag encode unsigned types");
  // is_unsigned<T> => !SignExtended
  static_assert(
      (std::is_unsigned<T>::value ^ SignExtended) || std::is_signed<T>::value,
      "Cannot SignExtended unsigned types");
  static_assert(!(SignExtended && ZigZag),
                "Cannot SignExtended and ZigZag on the same type");
#endif
  uint32 sum = n;
  uint32 msb_sum = 0;
  for (int i = 0; i < n; i++) {
    uint32 x = data[i];
    if (ZigZag) {
      x = WireFormatLite::ZigZagEncode32(x);
    } else if (SignExtended) {
      msb_sum += x >> 31;
    }
    // clang is so smart that it produces optimal SSE sequence unrolling
    // the loop 8 ints at a time. With a sequence of 4
    // cmpres = cmpgt x, sizeclass  ( -1 or 0)
    // sum = sum - cmpres
    if (x > 0x7F) sum++;
    if (x > 0x3FFF) sum++;
    if (x > 0x1FFFFF) sum++;
    if (x > 0xFFFFFFF) sum++;
  }
  if (SignExtended) sum += msb_sum * 5;
  return sum;
}

template <bool ZigZag, typename T>
static size_t VarintSize64(const T* data, const int n) {
#if __cplusplus >= 201103L
  static_assert(sizeof(T) == 8, "This routine only works for 64 bit integers");
  // is_unsigned<T> => !ZigZag
  static_assert(!ZigZag || !std::is_unsigned<T>::value,
                "Cannot ZigZag encode unsigned types");
#endif
  uint64 sum = n;
  for (int i = 0; i < n; i++) {
    uint64 x = data[i];
    if (ZigZag) {
      x = WireFormatLite::ZigZagEncode64(x);
    }
    // First step is a binary search, we can't branch in sse so we use the
    // result of the compare to adjust sum and appropriately. This code is
    // written to make clang recognize the vectorization.
    uint64 tmp = x >= (static_cast<uint64>(1) << 35) ? -1 : 0;
    sum += 5 & tmp;
    x >>= 35 & tmp;
    if (x > 0x7F) sum++;
    if (x > 0x3FFF) sum++;
    if (x > 0x1FFFFF) sum++;
    if (x > 0xFFFFFFF) sum++;
  }
  return sum;
}

// GCC does not recognize the vectorization opportunity
// and other platforms are untested, in those cases using the optimized
// varint size routine for each element is faster.
// Hence we enable it only for clang
#if defined(__SSE__) && defined(__clang__)
size_t WireFormatLite::Int32Size(const RepeatedField<int32>& value) {
  return VarintSize<false, true>(value.data(), value.size());
}

size_t WireFormatLite::UInt32Size(const RepeatedField<uint32>& value) {
  return VarintSize<false, false>(value.data(), value.size());
}

size_t WireFormatLite::SInt32Size(const RepeatedField<int32>& value) {
  return VarintSize<true, false>(value.data(), value.size());
}

size_t WireFormatLite::EnumSize(const RepeatedField<int>& value) {
  // On ILP64, sizeof(int) == 8, which would require a different template.
  return VarintSize<false, true>(value.data(), value.size());
}

#else  // !(defined(__SSE4_1__) && defined(__clang__))

size_t WireFormatLite::Int32Size(const RepeatedField<int32>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += Int32Size(value.Get(i));
  }
  return out;
}

size_t WireFormatLite::UInt32Size(const RepeatedField<uint32>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += UInt32Size(value.Get(i));
  }
  return out;
}

size_t WireFormatLite::SInt32Size(const RepeatedField<int32>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += SInt32Size(value.Get(i));
  }
  return out;
}

size_t WireFormatLite::EnumSize(const RepeatedField<int>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += EnumSize(value.Get(i));
  }
  return out;
}

#endif

// Micro benchmarks show that the SSE improved loop only starts beating
// the normal loop on Haswell platforms and then only for >32 ints. We
// disable this for now. Some specialized users might find it worthwhile to
// enable this.
#define USE_SSE_FOR_64_BIT_INTEGER_ARRAYS 0
#if USE_SSE_FOR_64_BIT_INTEGER_ARRAYS
size_t WireFormatLite::Int64Size(const RepeatedField<int64>& value) {
  return VarintSize64<false>(value.data(), value.size());
}

size_t WireFormatLite::UInt64Size(const RepeatedField<uint64>& value) {
  return VarintSize64<false>(value.data(), value.size());
}

size_t WireFormatLite::SInt64Size(const RepeatedField<int64>& value) {
  return VarintSize64<true>(value.data(), value.size());
}

#else

size_t WireFormatLite::Int64Size(const RepeatedField<int64>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += Int64Size(value.Get(i));
  }
  return out;
}

size_t WireFormatLite::UInt64Size(const RepeatedField<uint64>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += UInt64Size(value.Get(i));
  }
  return out;
}

size_t WireFormatLite::SInt64Size(const RepeatedField<int64>& value) {
  size_t out = 0;
  const int n = value.size();
  for (int i = 0; i < n; i++) {
    out += SInt64Size(value.Get(i));
  }
  return out;
}

#endif

}  // namespace internal
}  // namespace protobuf
}  // namespace google
