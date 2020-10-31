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
//         atenasio@google.com (Chris Atenasio) (ZigZag transform)
//         wink@google.com (Wink Saville) (refactored from wire_format.h)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef GOOGLE_PROTOBUF_WIRE_FORMAT_LITE_H__
#define GOOGLE_PROTOBUF_WIRE_FORMAT_LITE_H__

#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/port.h>
#include <google/protobuf/repeated_field.h>

// Do UTF-8 validation on string type in Debug build only
#ifndef NDEBUG
#define GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
#endif

// Avoid conflict with iOS where <ConditionalMacros.h> #defines TYPE_BOOL.
//
// If some one needs the macro TYPE_BOOL in a file that includes this header,
// it's possible to bring it back using push/pop_macro as follows.
//
// #pragma push_macro("TYPE_BOOL")
// #include this header and/or all headers that need the macro to be undefined.
// #pragma pop_macro("TYPE_BOOL")
#undef TYPE_BOOL


namespace google {
namespace protobuf {
namespace internal {

#include <google/protobuf/port_def.inc>

// This class is for internal use by the protocol buffer library and by
// protocol-complier-generated message classes.  It must not be called
// directly by clients.
//
// This class contains helpers for implementing the binary protocol buffer
// wire format without the need for reflection. Use WireFormat when using
// reflection.
//
// This class is really a namespace that contains only static methods.
class PROTOBUF_EXPORT WireFormatLite {
 public:
  // -----------------------------------------------------------------
  // Helper constants and functions related to the format.  These are
  // mostly meant for internal and generated code to use.

  // The wire format is composed of a sequence of tag/value pairs, each
  // of which contains the value of one field (or one element of a repeated
  // field).  Each tag is encoded as a varint.  The lower bits of the tag
  // identify its wire type, which specifies the format of the data to follow.
  // The rest of the bits contain the field number.  Each type of field (as
  // declared by FieldDescriptor::Type, in descriptor.h) maps to one of
  // these wire types.  Immediately following each tag is the field's value,
  // encoded in the format specified by the wire type.  Because the tag
  // identifies the encoding of this data, it is possible to skip
  // unrecognized fields for forwards compatibility.

  enum WireType {
    WIRETYPE_VARINT = 0,
    WIRETYPE_FIXED64 = 1,
    WIRETYPE_LENGTH_DELIMITED = 2,
    WIRETYPE_START_GROUP = 3,
    WIRETYPE_END_GROUP = 4,
    WIRETYPE_FIXED32 = 5,
  };

  // Lite alternative to FieldDescriptor::Type.  Must be kept in sync.
  enum FieldType {
    TYPE_DOUBLE = 1,
    TYPE_FLOAT = 2,
    TYPE_INT64 = 3,
    TYPE_UINT64 = 4,
    TYPE_INT32 = 5,
    TYPE_FIXED64 = 6,
    TYPE_FIXED32 = 7,
    TYPE_BOOL = 8,
    TYPE_STRING = 9,
    TYPE_GROUP = 10,
    TYPE_MESSAGE = 11,
    TYPE_BYTES = 12,
    TYPE_UINT32 = 13,
    TYPE_ENUM = 14,
    TYPE_SFIXED32 = 15,
    TYPE_SFIXED64 = 16,
    TYPE_SINT32 = 17,
    TYPE_SINT64 = 18,
    MAX_FIELD_TYPE = 18,
  };

  // Lite alternative to FieldDescriptor::CppType.  Must be kept in sync.
  enum CppType {
    CPPTYPE_INT32 = 1,
    CPPTYPE_INT64 = 2,
    CPPTYPE_UINT32 = 3,
    CPPTYPE_UINT64 = 4,
    CPPTYPE_DOUBLE = 5,
    CPPTYPE_FLOAT = 6,
    CPPTYPE_BOOL = 7,
    CPPTYPE_ENUM = 8,
    CPPTYPE_STRING = 9,
    CPPTYPE_MESSAGE = 10,
    MAX_CPPTYPE = 10,
  };

  // Helper method to get the CppType for a particular Type.
  static CppType FieldTypeToCppType(FieldType type);

  // Given a FieldDescriptor::Type return its WireType
  static inline WireFormatLite::WireType WireTypeForFieldType(
      WireFormatLite::FieldType type) {
    return kWireTypeForFieldType[type];
  }

  // Number of bits in a tag which identify the wire type.
  static const int kTagTypeBits = 3;
  // Mask for those bits.
  static const uint32 kTagTypeMask = (1 << kTagTypeBits) - 1;

  // Helper functions for encoding and decoding tags.  (Inlined below and in
  // _inl.h)
  //
  // This is different from MakeTag(field->number(), field->type()) in the
  // case of packed repeated fields.
  constexpr static uint32 MakeTag(int field_number, WireType type);
  static WireType GetTagWireType(uint32 tag);
  static int GetTagFieldNumber(uint32 tag);

  // Compute the byte size of a tag.  For groups, this includes both the start
  // and end tags.
  static inline size_t TagSize(int field_number,
                               WireFormatLite::FieldType type);

  // Skips a field value with the given tag.  The input should start
  // positioned immediately after the tag.  Skipped values are simply
  // discarded, not recorded anywhere.  See WireFormat::SkipField() for a
  // version that records to an UnknownFieldSet.
  static bool SkipField(io::CodedInputStream* input, uint32 tag);

  // Skips a field value with the given tag.  The input should start
  // positioned immediately after the tag. Skipped values are recorded to a
  // CodedOutputStream.
  static bool SkipField(io::CodedInputStream* input, uint32 tag,
                        io::CodedOutputStream* output);

  // Reads and ignores a message from the input.  Skipped values are simply
  // discarded, not recorded anywhere.  See WireFormat::SkipMessage() for a
  // version that records to an UnknownFieldSet.
  static bool SkipMessage(io::CodedInputStream* input);

  // Reads and ignores a message from the input.  Skipped values are recorded
  // to a CodedOutputStream.
  static bool SkipMessage(io::CodedInputStream* input,
                          io::CodedOutputStream* output);

  // This macro does the same thing as WireFormatLite::MakeTag(), but the
  // result is usable as a compile-time constant, which makes it usable
  // as a switch case or a template input.  WireFormatLite::MakeTag() is more
  // type-safe, though, so prefer it if possible.
#define GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(FIELD_NUMBER, TYPE) \
  static_cast<uint32>((static_cast<uint32>(FIELD_NUMBER) << 3) | (TYPE))

  // These are the tags for the old MessageSet format, which was defined as:
  //   message MessageSet {
  //     repeated group Item = 1 {
  //       required int32 type_id = 2;
  //       required string message = 3;
  //     }
  //   }
  static const int kMessageSetItemNumber = 1;
  static const int kMessageSetTypeIdNumber = 2;
  static const int kMessageSetMessageNumber = 3;
  static const int kMessageSetItemStartTag = GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(
      kMessageSetItemNumber, WireFormatLite::WIRETYPE_START_GROUP);
  static const int kMessageSetItemEndTag = GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(
      kMessageSetItemNumber, WireFormatLite::WIRETYPE_END_GROUP);
  static const int kMessageSetTypeIdTag = GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(
      kMessageSetTypeIdNumber, WireFormatLite::WIRETYPE_VARINT);
  static const int kMessageSetMessageTag = GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(
      kMessageSetMessageNumber, WireFormatLite::WIRETYPE_LENGTH_DELIMITED);

  // Byte size of all tags of a MessageSet::Item combined.
  static const size_t kMessageSetItemTagsSize;

  // Helper functions for converting between floats/doubles and IEEE-754
  // uint32s/uint64s so that they can be written.  (Assumes your platform
  // uses IEEE-754 floats.)
  static uint32 EncodeFloat(float value);
  static float DecodeFloat(uint32 value);
  static uint64 EncodeDouble(double value);
  static double DecodeDouble(uint64 value);

  // Helper functions for mapping signed integers to unsigned integers in
  // such a way that numbers with small magnitudes will encode to smaller
  // varints.  If you simply static_cast a negative number to an unsigned
  // number and varint-encode it, it will always take 10 bytes, defeating
  // the purpose of varint.  So, for the "sint32" and "sint64" field types,
  // we ZigZag-encode the values.
  static uint32 ZigZagEncode32(int32 n);
  static int32 ZigZagDecode32(uint32 n);
  static uint64 ZigZagEncode64(int64 n);
  static int64 ZigZagDecode64(uint64 n);

  // =================================================================
  // Methods for reading/writing individual field.

  // Read fields, not including tags.  The assumption is that you already
  // read the tag to determine what field to read.

  // For primitive fields, we just use a templatized routine parameterized by
  // the represented type and the FieldType. These are specialized with the
  // appropriate definition for each declared type.
  template <typename CType, enum FieldType DeclaredType>
  PROTOBUF_ALWAYS_INLINE static bool ReadPrimitive(io::CodedInputStream* input,
                                                   CType* value);

  // Reads repeated primitive values, with optimizations for repeats.
  // tag_size and tag should both be compile-time constants provided by the
  // protocol compiler.
  template <typename CType, enum FieldType DeclaredType>
  PROTOBUF_ALWAYS_INLINE static bool ReadRepeatedPrimitive(
      int tag_size, uint32 tag, io::CodedInputStream* input,
      RepeatedField<CType>* value);

  // Identical to ReadRepeatedPrimitive, except will not inline the
  // implementation.
  template <typename CType, enum FieldType DeclaredType>
  static bool ReadRepeatedPrimitiveNoInline(int tag_size, uint32 tag,
                                            io::CodedInputStream* input,
                                            RepeatedField<CType>* value);

  // Reads a primitive value directly from the provided buffer. It returns a
  // pointer past the segment of data that was read.
  //
  // This is only implemented for the types with fixed wire size, e.g.
  // float, double, and the (s)fixed* types.
  template <typename CType, enum FieldType DeclaredType>
  PROTOBUF_ALWAYS_INLINE static const uint8* ReadPrimitiveFromArray(
      const uint8* buffer, CType* value);

  // Reads a primitive packed field.
  //
  // This is only implemented for packable types.
  template <typename CType, enum FieldType DeclaredType>
  PROTOBUF_ALWAYS_INLINE static bool ReadPackedPrimitive(
      io::CodedInputStream* input, RepeatedField<CType>* value);

  // Identical to ReadPackedPrimitive, except will not inline the
  // implementation.
  template <typename CType, enum FieldType DeclaredType>
  static bool ReadPackedPrimitiveNoInline(io::CodedInputStream* input,
                                          RepeatedField<CType>* value);

  // Read a packed enum field. If the is_valid function is not NULL, values for
  // which is_valid(value) returns false are silently dropped.
  static bool ReadPackedEnumNoInline(io::CodedInputStream* input,
                                     bool (*is_valid)(int),
                                     RepeatedField<int>* values);

  // Read a packed enum field. If the is_valid function is not NULL, values for
  // which is_valid(value) returns false are appended to unknown_fields_stream.
  static bool ReadPackedEnumPreserveUnknowns(
      io::CodedInputStream* input, int field_number, bool (*is_valid)(int),
      io::CodedOutputStream* unknown_fields_stream, RepeatedField<int>* values);

  // Read a string.  ReadString(..., std::string* value) requires an
  // existing std::string.
  static inline bool ReadString(io::CodedInputStream* input,
                                std::string* value);
  // ReadString(..., std::string** p) is internal-only, and should only be
  // called from generated code. It starts by setting *p to "new std::string" if
  // *p == &GetEmptyStringAlreadyInited().  It then invokes
  // ReadString(io::CodedInputStream* input, *p).  This is useful for reducing
  // code size.
  static inline bool ReadString(io::CodedInputStream* input, std::string** p);
  // Analogous to ReadString().
  static bool ReadBytes(io::CodedInputStream* input, std::string* value);
  static bool ReadBytes(io::CodedInputStream* input, std::string** p);

  enum Operation {
    PARSE = 0,
    SERIALIZE = 1,
  };

  // Returns true if the data is valid UTF-8.
  static bool VerifyUtf8String(const char* data, int size, Operation op,
                               const char* field_name);

  template <typename MessageType>
  static inline bool ReadGroup(int field_number, io::CodedInputStream* input,
                               MessageType* value);

  template <typename MessageType>
  static inline bool ReadMessage(io::CodedInputStream* input,
                                 MessageType* value);

  template <typename MessageType>
  static inline bool ReadMessageNoVirtual(io::CodedInputStream* input,
                                          MessageType* value) {
    return ReadMessage(input, value);
  }

  // Write a tag.  The Write*() functions typically include the tag, so
  // normally there's no need to call this unless using the Write*NoTag()
  // variants.
  PROTOBUF_ALWAYS_INLINE static void WriteTag(int field_number, WireType type,
                                              io::CodedOutputStream* output);

  // Write fields, without tags.
  PROTOBUF_ALWAYS_INLINE static void WriteInt32NoTag(
      int32 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteInt64NoTag(
      int64 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteUInt32NoTag(
      uint32 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteUInt64NoTag(
      uint64 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteSInt32NoTag(
      int32 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteSInt64NoTag(
      int64 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteFixed32NoTag(
      uint32 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteFixed64NoTag(
      uint64 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteSFixed32NoTag(
      int32 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteSFixed64NoTag(
      int64 value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteFloatNoTag(
      float value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteDoubleNoTag(
      double value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteBoolNoTag(
      bool value, io::CodedOutputStream* output);
  PROTOBUF_ALWAYS_INLINE static void WriteEnumNoTag(
      int value, io::CodedOutputStream* output);

  // Write array of primitive fields, without tags
  static void WriteFloatArray(const float* a, int n,
                              io::CodedOutputStream* output);
  static void WriteDoubleArray(const double* a, int n,
                               io::CodedOutputStream* output);
  static void WriteFixed32Array(const uint32* a, int n,
                                io::CodedOutputStream* output);
  static void WriteFixed64Array(const uint64* a, int n,
                                io::CodedOutputStream* output);
  static void WriteSFixed32Array(const int32* a, int n,
                                 io::CodedOutputStream* output);
  static void WriteSFixed64Array(const int64* a, int n,
                                 io::CodedOutputStream* output);
  static void WriteBoolArray(const bool* a, int n,
                             io::CodedOutputStream* output);

  // Write fields, including tags.
  static void WriteInt32(int field_number, int32 value,
                         io::CodedOutputStream* output);
  static void WriteInt64(int field_number, int64 value,
                         io::CodedOutputStream* output);
  static void WriteUInt32(int field_number, uint32 value,
                          io::CodedOutputStream* output);
  static void WriteUInt64(int field_number, uint64 value,
                          io::CodedOutputStream* output);
  static void WriteSInt32(int field_number, int32 value,
                          io::CodedOutputStream* output);
  static void WriteSInt64(int field_number, int64 value,
                          io::CodedOutputStream* output);
  static void WriteFixed32(int field_number, uint32 value,
                           io::CodedOutputStream* output);
  static void WriteFixed64(int field_number, uint64 value,
                           io::CodedOutputStream* output);
  static void WriteSFixed32(int field_number, int32 value,
                            io::CodedOutputStream* output);
  static void WriteSFixed64(int field_number, int64 value,
                            io::CodedOutputStream* output);
  static void WriteFloat(int field_number, float value,
                         io::CodedOutputStream* output);
  static void WriteDouble(int field_number, double value,
                          io::CodedOutputStream* output);
  static void WriteBool(int field_number, bool value,
                        io::CodedOutputStream* output);
  static void WriteEnum(int field_number, int value,
                        io::CodedOutputStream* output);

  static void WriteString(int field_number, const std::string& value,
                          io::CodedOutputStream* output);
  static void WriteBytes(int field_number, const std::string& value,
                         io::CodedOutputStream* output);
  static void WriteStringMaybeAliased(int field_number,
                                      const std::string& value,
                                      io::CodedOutputStream* output);
  static void WriteBytesMaybeAliased(int field_number, const std::string& value,
                                     io::CodedOutputStream* output);

  static void WriteGroup(int field_number, const MessageLite& value,
                         io::CodedOutputStream* output);
  static void WriteMessage(int field_number, const MessageLite& value,
                           io::CodedOutputStream* output);
  // Like above, but these will check if the output stream has enough
  // space to write directly to a flat array.
  static void WriteGroupMaybeToArray(int field_number, const MessageLite& value,
                                     io::CodedOutputStream* output);
  static void WriteMessageMaybeToArray(int field_number,
                                       const MessageLite& value,
                                       io::CodedOutputStream* output);

  // Like above, but de-virtualize the call to SerializeWithCachedSizes().  The
  // pointer must point at an instance of MessageType, *not* a subclass (or
  // the subclass must not override SerializeWithCachedSizes()).
  template <typename MessageType>
  static inline void WriteGroupNoVirtual(int field_number,
                                         const MessageType& value,
                                         io::CodedOutputStream* output);
  template <typename MessageType>
  static inline void WriteMessageNoVirtual(int field_number,
                                           const MessageType& value,
                                           io::CodedOutputStream* output);

  // Like above, but use only *ToArray methods of CodedOutputStream.
  PROTOBUF_ALWAYS_INLINE static uint8* WriteTagToArray(int field_number,
                                                       WireType type,
                                                       uint8* target);

  // Write fields, without tags.
  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt32NoTagToArray(int32 value,
                                                              uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt64NoTagToArray(int64 value,
                                                              uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt32NoTagToArray(uint32 value,
                                                               uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt64NoTagToArray(uint64 value,
                                                               uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt32NoTagToArray(int32 value,
                                                               uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt64NoTagToArray(int64 value,
                                                               uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed32NoTagToArray(uint32 value,
                                                                uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed64NoTagToArray(uint64 value,
                                                                uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed32NoTagToArray(int32 value,
                                                                 uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed64NoTagToArray(int64 value,
                                                                 uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFloatNoTagToArray(float value,
                                                              uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteDoubleNoTagToArray(double value,
                                                               uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteBoolNoTagToArray(bool value,
                                                             uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteEnumNoTagToArray(int value,
                                                             uint8* target);

  // Write fields, without tags.  These require that value.size() > 0.
  template <typename T>
  PROTOBUF_ALWAYS_INLINE static uint8* WritePrimitiveNoTagToArray(
      const RepeatedField<T>& value, uint8* (*Writer)(T, uint8*),
      uint8* target);
  template <typename T>
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixedNoTagToArray(
      const RepeatedField<T>& value, uint8* (*Writer)(T, uint8*),
      uint8* target);

  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt32NoTagToArray(
      const RepeatedField<int32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt64NoTagToArray(
      const RepeatedField<int64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt32NoTagToArray(
      const RepeatedField<uint32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt64NoTagToArray(
      const RepeatedField<uint64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt32NoTagToArray(
      const RepeatedField<int32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt64NoTagToArray(
      const RepeatedField<int64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed32NoTagToArray(
      const RepeatedField<uint32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed64NoTagToArray(
      const RepeatedField<uint64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed32NoTagToArray(
      const RepeatedField<int32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed64NoTagToArray(
      const RepeatedField<int64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFloatNoTagToArray(
      const RepeatedField<float>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteDoubleNoTagToArray(
      const RepeatedField<double>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteBoolNoTagToArray(
      const RepeatedField<bool>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteEnumNoTagToArray(
      const RepeatedField<int>& value, uint8* output);

  // Write fields, including tags.
  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt32ToArray(int field_number,
                                                         int32 value,
                                                         uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt64ToArray(int field_number,
                                                         int64 value,
                                                         uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt32ToArray(int field_number,
                                                          uint32 value,
                                                          uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt64ToArray(int field_number,
                                                          uint64 value,
                                                          uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt32ToArray(int field_number,
                                                          int32 value,
                                                          uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt64ToArray(int field_number,
                                                          int64 value,
                                                          uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed32ToArray(int field_number,
                                                           uint32 value,
                                                           uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed64ToArray(int field_number,
                                                           uint64 value,
                                                           uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed32ToArray(int field_number,
                                                            int32 value,
                                                            uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed64ToArray(int field_number,
                                                            int64 value,
                                                            uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFloatToArray(int field_number,
                                                         float value,
                                                         uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteDoubleToArray(int field_number,
                                                          double value,
                                                          uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteBoolToArray(int field_number,
                                                        bool value,
                                                        uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteEnumToArray(int field_number,
                                                        int value,
                                                        uint8* target);

  template <typename T>
  PROTOBUF_ALWAYS_INLINE static uint8* WritePrimitiveToArray(
      int field_number, const RepeatedField<T>& value,
      uint8* (*Writer)(int, T, uint8*), uint8* target);

  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt32ToArray(
      int field_number, const RepeatedField<int32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteInt64ToArray(
      int field_number, const RepeatedField<int64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt32ToArray(
      int field_number, const RepeatedField<uint32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteUInt64ToArray(
      int field_number, const RepeatedField<uint64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt32ToArray(
      int field_number, const RepeatedField<int32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSInt64ToArray(
      int field_number, const RepeatedField<int64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed32ToArray(
      int field_number, const RepeatedField<uint32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFixed64ToArray(
      int field_number, const RepeatedField<uint64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed32ToArray(
      int field_number, const RepeatedField<int32>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteSFixed64ToArray(
      int field_number, const RepeatedField<int64>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteFloatToArray(
      int field_number, const RepeatedField<float>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteDoubleToArray(
      int field_number, const RepeatedField<double>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteBoolToArray(
      int field_number, const RepeatedField<bool>& value, uint8* output);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteEnumToArray(
      int field_number, const RepeatedField<int>& value, uint8* output);

  PROTOBUF_ALWAYS_INLINE static uint8* WriteStringToArray(
      int field_number, const std::string& value, uint8* target);
  PROTOBUF_ALWAYS_INLINE static uint8* WriteBytesToArray(
      int field_number, const std::string& value, uint8* target);

  // Whether to serialize deterministically (e.g., map keys are
  // sorted) is a property of a CodedOutputStream, and in the process
  // of serialization, the "ToArray" variants may be invoked.  But they don't
  // have a CodedOutputStream available, so they get an additional parameter
  // telling them whether to serialize deterministically.
  template <typename MessageType>
  PROTOBUF_ALWAYS_INLINE static uint8* InternalWriteGroupToArray(
      int field_number, const MessageType& value, uint8* target);
  template <typename MessageType>
  PROTOBUF_ALWAYS_INLINE static uint8* InternalWriteMessageToArray(
      int field_number, const MessageType& value, uint8* target);

  // Like above, but de-virtualize the call to SerializeWithCachedSizes().  The
  // pointer must point at an instance of MessageType, *not* a subclass (or
  // the subclass must not override SerializeWithCachedSizes()).
  template <typename MessageType>
  PROTOBUF_ALWAYS_INLINE static uint8* InternalWriteGroupNoVirtualToArray(
      int field_number, const MessageType& value, uint8* target);
  template <typename MessageType>
  PROTOBUF_ALWAYS_INLINE static uint8* InternalWriteMessageNoVirtualToArray(
      int field_number, const MessageType& value, uint8* target);

  // For backward-compatibility, the last four methods also have versions
  // that are non-deterministic always.
  PROTOBUF_ALWAYS_INLINE static uint8* WriteGroupToArray(
      int field_number, const MessageLite& value, uint8* target) {
    return InternalWriteGroupToArray(field_number, value, target);
  }
  PROTOBUF_ALWAYS_INLINE static uint8* WriteMessageToArray(
      int field_number, const MessageLite& value, uint8* target) {
    return InternalWriteMessageToArray(field_number, value, target);
  }

  // Compute the byte size of a field.  The XxSize() functions do NOT include
  // the tag, so you must also call TagSize().  (This is because, for repeated
  // fields, you should only call TagSize() once and multiply it by the element
  // count, but you may have to call XxSize() for each individual element.)
  static inline size_t Int32Size(int32 value);
  static inline size_t Int64Size(int64 value);
  static inline size_t UInt32Size(uint32 value);
  static inline size_t UInt64Size(uint64 value);
  static inline size_t SInt32Size(int32 value);
  static inline size_t SInt64Size(int64 value);
  static inline size_t EnumSize(int value);

  static size_t Int32Size(const RepeatedField<int32>& value);
  static size_t Int64Size(const RepeatedField<int64>& value);
  static size_t UInt32Size(const RepeatedField<uint32>& value);
  static size_t UInt64Size(const RepeatedField<uint64>& value);
  static size_t SInt32Size(const RepeatedField<int32>& value);
  static size_t SInt64Size(const RepeatedField<int64>& value);
  static size_t EnumSize(const RepeatedField<int>& value);

  // These types always have the same size.
  static const size_t kFixed32Size = 4;
  static const size_t kFixed64Size = 8;
  static const size_t kSFixed32Size = 4;
  static const size_t kSFixed64Size = 8;
  static const size_t kFloatSize = 4;
  static const size_t kDoubleSize = 8;
  static const size_t kBoolSize = 1;

  static inline size_t StringSize(const std::string& value);
  static inline size_t BytesSize(const std::string& value);

  template <typename MessageType>
  static inline size_t GroupSize(const MessageType& value);
  template <typename MessageType>
  static inline size_t MessageSize(const MessageType& value);

  // Like above, but de-virtualize the call to ByteSize().  The
  // pointer must point at an instance of MessageType, *not* a subclass (or
  // the subclass must not override ByteSize()).
  template <typename MessageType>
  static inline size_t GroupSizeNoVirtual(const MessageType& value);
  template <typename MessageType>
  static inline size_t MessageSizeNoVirtual(const MessageType& value);

  // Given the length of data, calculate the byte size of the data on the
  // wire if we encode the data as a length delimited field.
  static inline size_t LengthDelimitedSize(size_t length);

 private:
  // A helper method for the repeated primitive reader. This method has
  // optimizations for primitive types that have fixed size on the wire, and
  // can be read using potentially faster paths.
  template <typename CType, enum FieldType DeclaredType>
  PROTOBUF_ALWAYS_INLINE static bool ReadRepeatedFixedSizePrimitive(
      int tag_size, uint32 tag, io::CodedInputStream* input,
      RepeatedField<CType>* value);

  // Like ReadRepeatedFixedSizePrimitive but for packed primitive fields.
  template <typename CType, enum FieldType DeclaredType>
  PROTOBUF_ALWAYS_INLINE static bool ReadPackedFixedSizePrimitive(
      io::CodedInputStream* input, RepeatedField<CType>* value);

  static const CppType kFieldTypeToCppTypeMap[];
  static const WireFormatLite::WireType kWireTypeForFieldType[];
  static void WriteSubMessageMaybeToArray(int size, const MessageLite& value,
                                          io::CodedOutputStream* output);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(WireFormatLite);
};

// A class which deals with unknown values.  The default implementation just
// discards them.  WireFormat defines a subclass which writes to an
// UnknownFieldSet.  This class is used by ExtensionSet::ParseField(), since
// ExtensionSet is part of the lite library but UnknownFieldSet is not.
class PROTOBUF_EXPORT FieldSkipper {
 public:
  FieldSkipper() {}
  virtual ~FieldSkipper() {}

  // Skip a field whose tag has already been consumed.
  virtual bool SkipField(io::CodedInputStream* input, uint32 tag);

  // Skip an entire message or group, up to an end-group tag (which is consumed)
  // or end-of-stream.
  virtual bool SkipMessage(io::CodedInputStream* input);

  // Deal with an already-parsed unrecognized enum value.  The default
  // implementation does nothing, but the UnknownFieldSet-based implementation
  // saves it as an unknown varint.
  virtual void SkipUnknownEnum(int field_number, int value);
};

// Subclass of FieldSkipper which saves skipped fields to a CodedOutputStream.

class PROTOBUF_EXPORT CodedOutputStreamFieldSkipper : public FieldSkipper {
 public:
  explicit CodedOutputStreamFieldSkipper(io::CodedOutputStream* unknown_fields)
      : unknown_fields_(unknown_fields) {}
  ~CodedOutputStreamFieldSkipper() override {}

  // implements FieldSkipper -----------------------------------------
  bool SkipField(io::CodedInputStream* input, uint32 tag) override;
  bool SkipMessage(io::CodedInputStream* input) override;
  void SkipUnknownEnum(int field_number, int value) override;

 protected:
  io::CodedOutputStream* unknown_fields_;
};

// inline methods ====================================================

inline WireFormatLite::CppType WireFormatLite::FieldTypeToCppType(
    FieldType type) {
  return kFieldTypeToCppTypeMap[type];
}

constexpr inline uint32 WireFormatLite::MakeTag(int field_number,
                                                WireType type) {
  return GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(field_number, type);
}

inline WireFormatLite::WireType WireFormatLite::GetTagWireType(uint32 tag) {
  return static_cast<WireType>(tag & kTagTypeMask);
}

inline int WireFormatLite::GetTagFieldNumber(uint32 tag) {
  return static_cast<int>(tag >> kTagTypeBits);
}

inline size_t WireFormatLite::TagSize(int field_number,
                                      WireFormatLite::FieldType type) {
  size_t result = io::CodedOutputStream::VarintSize32(
      static_cast<uint32>(field_number << kTagTypeBits));
  if (type == TYPE_GROUP) {
    // Groups have both a start and an end tag.
    return result * 2;
  } else {
    return result;
  }
}

inline uint32 WireFormatLite::EncodeFloat(float value) {
  union {
    float f;
    uint32 i;
  };
  f = value;
  return i;
}

inline float WireFormatLite::DecodeFloat(uint32 value) {
  union {
    float f;
    uint32 i;
  };
  i = value;
  return f;
}

inline uint64 WireFormatLite::EncodeDouble(double value) {
  union {
    double f;
    uint64 i;
  };
  f = value;
  return i;
}

inline double WireFormatLite::DecodeDouble(uint64 value) {
  union {
    double f;
    uint64 i;
  };
  i = value;
  return f;
}

// ZigZag Transform:  Encodes signed integers so that they can be
// effectively used with varint encoding.
//
// varint operates on unsigned integers, encoding smaller numbers into
// fewer bytes.  If you try to use it on a signed integer, it will treat
// this number as a very large unsigned integer, which means that even
// small signed numbers like -1 will take the maximum number of bytes
// (10) to encode.  ZigZagEncode() maps signed integers to unsigned
// in such a way that those with a small absolute value will have smaller
// encoded values, making them appropriate for encoding using varint.
//
//       int32 ->     uint32
// -------------------------
//           0 ->          0
//          -1 ->          1
//           1 ->          2
//          -2 ->          3
//         ... ->        ...
//  2147483647 -> 4294967294
// -2147483648 -> 4294967295
//
//        >> encode >>
//        << decode <<

inline uint32 WireFormatLite::ZigZagEncode32(int32 n) {
  // Note:  the right-shift must be arithmetic
  // Note:  left shift must be unsigned because of overflow
  return (static_cast<uint32>(n) << 1) ^ static_cast<uint32>(n >> 31);
}

inline int32 WireFormatLite::ZigZagDecode32(uint32 n) {
  // Note:  Using unsigned types prevent undefined behavior
  return static_cast<int32>((n >> 1) ^ (~(n & 1) + 1));
}

inline uint64 WireFormatLite::ZigZagEncode64(int64 n) {
  // Note:  the right-shift must be arithmetic
  // Note:  left shift must be unsigned because of overflow
  return (static_cast<uint64>(n) << 1) ^ static_cast<uint64>(n >> 63);
}

inline int64 WireFormatLite::ZigZagDecode64(uint64 n) {
  // Note:  Using unsigned types prevent undefined behavior
  return static_cast<int64>((n >> 1) ^ (~(n & 1) + 1));
}

// String is for UTF-8 text only, but, even so, ReadString() can simply
// call ReadBytes().

inline bool WireFormatLite::ReadString(io::CodedInputStream* input,
                                       std::string* value) {
  return ReadBytes(input, value);
}

inline bool WireFormatLite::ReadString(io::CodedInputStream* input,
                                       std::string** p) {
  return ReadBytes(input, p);
}

inline void SerializeUnknownMessageSetItems(const std::string& unknown_fields,
                                            io::CodedOutputStream* output) {
  output->WriteString(unknown_fields);
}

inline size_t ComputeUnknownMessageSetItemsSize(
    const std::string& unknown_fields) {
  return unknown_fields.size();
}

// Implementation details of ReadPrimitive.

template <>
inline bool WireFormatLite::ReadPrimitive<int32, WireFormatLite::TYPE_INT32>(
    io::CodedInputStream* input, int32* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = static_cast<int32>(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<int64, WireFormatLite::TYPE_INT64>(
    io::CodedInputStream* input, int64* value) {
  uint64 temp;
  if (!input->ReadVarint64(&temp)) return false;
  *value = static_cast<int64>(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<uint32, WireFormatLite::TYPE_UINT32>(
    io::CodedInputStream* input, uint32* value) {
  return input->ReadVarint32(value);
}
template <>
inline bool WireFormatLite::ReadPrimitive<uint64, WireFormatLite::TYPE_UINT64>(
    io::CodedInputStream* input, uint64* value) {
  return input->ReadVarint64(value);
}
template <>
inline bool WireFormatLite::ReadPrimitive<int32, WireFormatLite::TYPE_SINT32>(
    io::CodedInputStream* input, int32* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = ZigZagDecode32(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<int64, WireFormatLite::TYPE_SINT64>(
    io::CodedInputStream* input, int64* value) {
  uint64 temp;
  if (!input->ReadVarint64(&temp)) return false;
  *value = ZigZagDecode64(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<uint32, WireFormatLite::TYPE_FIXED32>(
    io::CodedInputStream* input, uint32* value) {
  return input->ReadLittleEndian32(value);
}
template <>
inline bool WireFormatLite::ReadPrimitive<uint64, WireFormatLite::TYPE_FIXED64>(
    io::CodedInputStream* input, uint64* value) {
  return input->ReadLittleEndian64(value);
}
template <>
inline bool WireFormatLite::ReadPrimitive<int32, WireFormatLite::TYPE_SFIXED32>(
    io::CodedInputStream* input, int32* value) {
  uint32 temp;
  if (!input->ReadLittleEndian32(&temp)) return false;
  *value = static_cast<int32>(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<int64, WireFormatLite::TYPE_SFIXED64>(
    io::CodedInputStream* input, int64* value) {
  uint64 temp;
  if (!input->ReadLittleEndian64(&temp)) return false;
  *value = static_cast<int64>(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<float, WireFormatLite::TYPE_FLOAT>(
    io::CodedInputStream* input, float* value) {
  uint32 temp;
  if (!input->ReadLittleEndian32(&temp)) return false;
  *value = DecodeFloat(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<double, WireFormatLite::TYPE_DOUBLE>(
    io::CodedInputStream* input, double* value) {
  uint64 temp;
  if (!input->ReadLittleEndian64(&temp)) return false;
  *value = DecodeDouble(temp);
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<bool, WireFormatLite::TYPE_BOOL>(
    io::CodedInputStream* input, bool* value) {
  uint64 temp;
  if (!input->ReadVarint64(&temp)) return false;
  *value = temp != 0;
  return true;
}
template <>
inline bool WireFormatLite::ReadPrimitive<int, WireFormatLite::TYPE_ENUM>(
    io::CodedInputStream* input, int* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = static_cast<int>(temp);
  return true;
}

template <>
inline const uint8*
WireFormatLite::ReadPrimitiveFromArray<uint32, WireFormatLite::TYPE_FIXED32>(
    const uint8* buffer, uint32* value) {
  return io::CodedInputStream::ReadLittleEndian32FromArray(buffer, value);
}
template <>
inline const uint8*
WireFormatLite::ReadPrimitiveFromArray<uint64, WireFormatLite::TYPE_FIXED64>(
    const uint8* buffer, uint64* value) {
  return io::CodedInputStream::ReadLittleEndian64FromArray(buffer, value);
}
template <>
inline const uint8*
WireFormatLite::ReadPrimitiveFromArray<int32, WireFormatLite::TYPE_SFIXED32>(
    const uint8* buffer, int32* value) {
  uint32 temp;
  buffer = io::CodedInputStream::ReadLittleEndian32FromArray(buffer, &temp);
  *value = static_cast<int32>(temp);
  return buffer;
}
template <>
inline const uint8*
WireFormatLite::ReadPrimitiveFromArray<int64, WireFormatLite::TYPE_SFIXED64>(
    const uint8* buffer, int64* value) {
  uint64 temp;
  buffer = io::CodedInputStream::ReadLittleEndian64FromArray(buffer, &temp);
  *value = static_cast<int64>(temp);
  return buffer;
}
template <>
inline const uint8*
WireFormatLite::ReadPrimitiveFromArray<float, WireFormatLite::TYPE_FLOAT>(
    const uint8* buffer, float* value) {
  uint32 temp;
  buffer = io::CodedInputStream::ReadLittleEndian32FromArray(buffer, &temp);
  *value = DecodeFloat(temp);
  return buffer;
}
template <>
inline const uint8*
WireFormatLite::ReadPrimitiveFromArray<double, WireFormatLite::TYPE_DOUBLE>(
    const uint8* buffer, double* value) {
  uint64 temp;
  buffer = io::CodedInputStream::ReadLittleEndian64FromArray(buffer, &temp);
  *value = DecodeDouble(temp);
  return buffer;
}

template <typename CType, enum WireFormatLite::FieldType DeclaredType>
inline bool WireFormatLite::ReadRepeatedPrimitive(
    int,  // tag_size, unused.
    uint32 tag, io::CodedInputStream* input, RepeatedField<CType>* values) {
  CType value;
  if (!ReadPrimitive<CType, DeclaredType>(input, &value)) return false;
  values->Add(value);
  int elements_already_reserved = values->Capacity() - values->size();
  while (elements_already_reserved > 0 && input->ExpectTag(tag)) {
    if (!ReadPrimitive<CType, DeclaredType>(input, &value)) return false;
    values->AddAlreadyReserved(value);
    elements_already_reserved--;
  }
  return true;
}

template <typename CType, enum WireFormatLite::FieldType DeclaredType>
inline bool WireFormatLite::ReadRepeatedFixedSizePrimitive(
    int tag_size, uint32 tag, io::CodedInputStream* input,
    RepeatedField<CType>* values) {
  GOOGLE_DCHECK_EQ(UInt32Size(tag), static_cast<size_t>(tag_size));
  CType value;
  if (!ReadPrimitive<CType, DeclaredType>(input, &value)) return false;
  values->Add(value);

  // For fixed size values, repeated values can be read more quickly by
  // reading directly from a raw array.
  //
  // We can get a tight loop by only reading as many elements as can be
  // added to the RepeatedField without having to do any resizing. Additionally,
  // we only try to read as many elements as are available from the current
  // buffer space. Doing so avoids having to perform boundary checks when
  // reading the value: the maximum number of elements that can be read is
  // known outside of the loop.
  const void* void_pointer;
  int size;
  input->GetDirectBufferPointerInline(&void_pointer, &size);
  if (size > 0) {
    const uint8* buffer = reinterpret_cast<const uint8*>(void_pointer);
    // The number of bytes each type occupies on the wire.
    const int per_value_size = tag_size + static_cast<int>(sizeof(value));

    // parentheses around (std::min) prevents macro expansion of min(...)
    int elements_available =
        (std::min)(values->Capacity() - values->size(), size / per_value_size);
    int num_read = 0;
    while (num_read < elements_available &&
           (buffer = io::CodedInputStream::ExpectTagFromArray(buffer, tag)) !=
               NULL) {
      buffer = ReadPrimitiveFromArray<CType, DeclaredType>(buffer, &value);
      values->AddAlreadyReserved(value);
      ++num_read;
    }
    const int read_bytes = num_read * per_value_size;
    if (read_bytes > 0) {
      input->Skip(read_bytes);
    }
  }
  return true;
}

// Specializations of ReadRepeatedPrimitive for the fixed size types, which use
// the optimized code path.
#define READ_REPEATED_FIXED_SIZE_PRIMITIVE(CPPTYPE, DECLARED_TYPE)        \
  template <>                                                             \
  inline bool WireFormatLite::ReadRepeatedPrimitive<                      \
      CPPTYPE, WireFormatLite::DECLARED_TYPE>(                            \
      int tag_size, uint32 tag, io::CodedInputStream* input,              \
      RepeatedField<CPPTYPE>* values) {                                   \
    return ReadRepeatedFixedSizePrimitive<CPPTYPE,                        \
                                          WireFormatLite::DECLARED_TYPE>( \
        tag_size, tag, input, values);                                    \
  }

READ_REPEATED_FIXED_SIZE_PRIMITIVE(uint32, TYPE_FIXED32)
READ_REPEATED_FIXED_SIZE_PRIMITIVE(uint64, TYPE_FIXED64)
READ_REPEATED_FIXED_SIZE_PRIMITIVE(int32, TYPE_SFIXED32)
READ_REPEATED_FIXED_SIZE_PRIMITIVE(int64, TYPE_SFIXED64)
READ_REPEATED_FIXED_SIZE_PRIMITIVE(float, TYPE_FLOAT)
READ_REPEATED_FIXED_SIZE_PRIMITIVE(double, TYPE_DOUBLE)

#undef READ_REPEATED_FIXED_SIZE_PRIMITIVE

template <typename CType, enum WireFormatLite::FieldType DeclaredType>
bool WireFormatLite::ReadRepeatedPrimitiveNoInline(
    int tag_size, uint32 tag, io::CodedInputStream* input,
    RepeatedField<CType>* value) {
  return ReadRepeatedPrimitive<CType, DeclaredType>(tag_size, tag, input,
                                                    value);
}

template <typename CType, enum WireFormatLite::FieldType DeclaredType>
inline bool WireFormatLite::ReadPackedPrimitive(io::CodedInputStream* input,
                                                RepeatedField<CType>* values) {
  int length;
  if (!input->ReadVarintSizeAsInt(&length)) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  while (input->BytesUntilLimit() > 0) {
    CType value;
    if (!ReadPrimitive<CType, DeclaredType>(input, &value)) return false;
    values->Add(value);
  }
  input->PopLimit(limit);
  return true;
}

template <typename CType, enum WireFormatLite::FieldType DeclaredType>
inline bool WireFormatLite::ReadPackedFixedSizePrimitive(
    io::CodedInputStream* input, RepeatedField<CType>* values) {
  int length;
  if (!input->ReadVarintSizeAsInt(&length)) return false;
  const int old_entries = values->size();
  const int new_entries = length / static_cast<int>(sizeof(CType));
  const int new_bytes = new_entries * static_cast<int>(sizeof(CType));
  if (new_bytes != length) return false;
  // We would *like* to pre-allocate the buffer to write into (for
  // speed), but *must* avoid performing a very large allocation due
  // to a malicious user-supplied "length" above.  So we have a fast
  // path that pre-allocates when the "length" is less than a bound.
  // We determine the bound by calling BytesUntilTotalBytesLimit() and
  // BytesUntilLimit().  These return -1 to mean "no limit set".
  // There are four cases:
  // TotalBytesLimit  Limit
  // -1               -1     Use slow path.
  // -1               >= 0   Use fast path if length <= Limit.
  // >= 0             -1     Use slow path.
  // >= 0             >= 0   Use fast path if length <= min(both limits).
  int64 bytes_limit = input->BytesUntilTotalBytesLimit();
  if (bytes_limit == -1) {
    bytes_limit = input->BytesUntilLimit();
  } else {
    // parentheses around (std::min) prevents macro expansion of min(...)
    bytes_limit =
        (std::min)(bytes_limit, static_cast<int64>(input->BytesUntilLimit()));
  }
  if (bytes_limit >= new_bytes) {
    // Fast-path that pre-allocates *values to the final size.
#if defined(PROTOBUF_LITTLE_ENDIAN)
    values->Resize(old_entries + new_entries, 0);
    // values->mutable_data() may change after Resize(), so do this after:
    void* dest = reinterpret_cast<void*>(values->mutable_data() + old_entries);
    if (!input->ReadRaw(dest, new_bytes)) {
      values->Truncate(old_entries);
      return false;
    }
#else
    values->Reserve(old_entries + new_entries);
    CType value;
    for (int i = 0; i < new_entries; ++i) {
      if (!ReadPrimitive<CType, DeclaredType>(input, &value)) return false;
      values->AddAlreadyReserved(value);
    }
#endif
  } else {
    // This is the slow-path case where "length" may be too large to
    // safely allocate.  We read as much as we can into *values
    // without pre-allocating "length" bytes.
    CType value;
    for (int i = 0; i < new_entries; ++i) {
      if (!ReadPrimitive<CType, DeclaredType>(input, &value)) return false;
      values->Add(value);
    }
  }
  return true;
}

// Specializations of ReadPackedPrimitive for the fixed size types, which use
// an optimized code path.
#define READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(CPPTYPE, DECLARED_TYPE)      \
  template <>                                                                  \
  inline bool                                                                  \
  WireFormatLite::ReadPackedPrimitive<CPPTYPE, WireFormatLite::DECLARED_TYPE>( \
      io::CodedInputStream * input, RepeatedField<CPPTYPE> * values) {         \
    return ReadPackedFixedSizePrimitive<CPPTYPE,                               \
                                        WireFormatLite::DECLARED_TYPE>(        \
        input, values);                                                        \
  }

READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(uint32, TYPE_FIXED32)
READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(uint64, TYPE_FIXED64)
READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(int32, TYPE_SFIXED32)
READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(int64, TYPE_SFIXED64)
READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(float, TYPE_FLOAT)
READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE(double, TYPE_DOUBLE)

#undef READ_REPEATED_PACKED_FIXED_SIZE_PRIMITIVE

template <typename CType, enum WireFormatLite::FieldType DeclaredType>
bool WireFormatLite::ReadPackedPrimitiveNoInline(io::CodedInputStream* input,
                                                 RepeatedField<CType>* values) {
  return ReadPackedPrimitive<CType, DeclaredType>(input, values);
}


template <typename MessageType>
inline bool WireFormatLite::ReadGroup(int field_number,
                                      io::CodedInputStream* input,
                                      MessageType* value) {
  if (!input->IncrementRecursionDepth()) return false;
  if (!value->MergePartialFromCodedStream(input)) return false;
  input->UnsafeDecrementRecursionDepth();
  // Make sure the last thing read was an end tag for this group.
  if (!input->LastTagWas(MakeTag(field_number, WIRETYPE_END_GROUP))) {
    return false;
  }
  return true;
}
template <typename MessageType>
inline bool WireFormatLite::ReadMessage(io::CodedInputStream* input,
                                        MessageType* value) {
  int length;
  if (!input->ReadVarintSizeAsInt(&length)) return false;
  std::pair<io::CodedInputStream::Limit, int> p =
      input->IncrementRecursionDepthAndPushLimit(length);
  if (p.second < 0 || !value->MergePartialFromCodedStream(input)) return false;
  // Make sure that parsing stopped when the limit was hit, not at an endgroup
  // tag.
  return input->DecrementRecursionDepthAndPopLimit(p.first);
}

// ===================================================================

inline void WireFormatLite::WriteTag(int field_number, WireType type,
                                     io::CodedOutputStream* output) {
  output->WriteTag(MakeTag(field_number, type));
}

inline void WireFormatLite::WriteInt32NoTag(int32 value,
                                            io::CodedOutputStream* output) {
  output->WriteVarint32SignExtended(value);
}
inline void WireFormatLite::WriteInt64NoTag(int64 value,
                                            io::CodedOutputStream* output) {
  output->WriteVarint64(static_cast<uint64>(value));
}
inline void WireFormatLite::WriteUInt32NoTag(uint32 value,
                                             io::CodedOutputStream* output) {
  output->WriteVarint32(value);
}
inline void WireFormatLite::WriteUInt64NoTag(uint64 value,
                                             io::CodedOutputStream* output) {
  output->WriteVarint64(value);
}
inline void WireFormatLite::WriteSInt32NoTag(int32 value,
                                             io::CodedOutputStream* output) {
  output->WriteVarint32(ZigZagEncode32(value));
}
inline void WireFormatLite::WriteSInt64NoTag(int64 value,
                                             io::CodedOutputStream* output) {
  output->WriteVarint64(ZigZagEncode64(value));
}
inline void WireFormatLite::WriteFixed32NoTag(uint32 value,
                                              io::CodedOutputStream* output) {
  output->WriteLittleEndian32(value);
}
inline void WireFormatLite::WriteFixed64NoTag(uint64 value,
                                              io::CodedOutputStream* output) {
  output->WriteLittleEndian64(value);
}
inline void WireFormatLite::WriteSFixed32NoTag(int32 value,
                                               io::CodedOutputStream* output) {
  output->WriteLittleEndian32(static_cast<uint32>(value));
}
inline void WireFormatLite::WriteSFixed64NoTag(int64 value,
                                               io::CodedOutputStream* output) {
  output->WriteLittleEndian64(static_cast<uint64>(value));
}
inline void WireFormatLite::WriteFloatNoTag(float value,
                                            io::CodedOutputStream* output) {
  output->WriteLittleEndian32(EncodeFloat(value));
}
inline void WireFormatLite::WriteDoubleNoTag(double value,
                                             io::CodedOutputStream* output) {
  output->WriteLittleEndian64(EncodeDouble(value));
}
inline void WireFormatLite::WriteBoolNoTag(bool value,
                                           io::CodedOutputStream* output) {
  output->WriteVarint32(value ? 1 : 0);
}
inline void WireFormatLite::WriteEnumNoTag(int value,
                                           io::CodedOutputStream* output) {
  output->WriteVarint32SignExtended(value);
}

// See comment on ReadGroupNoVirtual to understand the need for this template
// parameter name.
template <typename MessageType_WorkAroundCppLookupDefect>
inline void WireFormatLite::WriteGroupNoVirtual(
    int field_number, const MessageType_WorkAroundCppLookupDefect& value,
    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  value.MessageType_WorkAroundCppLookupDefect::SerializeWithCachedSizes(output);
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}
template <typename MessageType_WorkAroundCppLookupDefect>
inline void WireFormatLite::WriteMessageNoVirtual(
    int field_number, const MessageType_WorkAroundCppLookupDefect& value,
    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  output->WriteVarint32(
      value.MessageType_WorkAroundCppLookupDefect::GetCachedSize());
  value.MessageType_WorkAroundCppLookupDefect::SerializeWithCachedSizes(output);
}

// ===================================================================

inline uint8* WireFormatLite::WriteTagToArray(int field_number, WireType type,
                                              uint8* target) {
  return io::CodedOutputStream::WriteTagToArray(MakeTag(field_number, type),
                                                target);
}

inline uint8* WireFormatLite::WriteInt32NoTagToArray(int32 value,
                                                     uint8* target) {
  return io::CodedOutputStream::WriteVarint32SignExtendedToArray(value, target);
}
inline uint8* WireFormatLite::WriteInt64NoTagToArray(int64 value,
                                                     uint8* target) {
  return io::CodedOutputStream::WriteVarint64ToArray(static_cast<uint64>(value),
                                                     target);
}
inline uint8* WireFormatLite::WriteUInt32NoTagToArray(uint32 value,
                                                      uint8* target) {
  return io::CodedOutputStream::WriteVarint32ToArray(value, target);
}
inline uint8* WireFormatLite::WriteUInt64NoTagToArray(uint64 value,
                                                      uint8* target) {
  return io::CodedOutputStream::WriteVarint64ToArray(value, target);
}
inline uint8* WireFormatLite::WriteSInt32NoTagToArray(int32 value,
                                                      uint8* target) {
  return io::CodedOutputStream::WriteVarint32ToArray(ZigZagEncode32(value),
                                                     target);
}
inline uint8* WireFormatLite::WriteSInt64NoTagToArray(int64 value,
                                                      uint8* target) {
  return io::CodedOutputStream::WriteVarint64ToArray(ZigZagEncode64(value),
                                                     target);
}
inline uint8* WireFormatLite::WriteFixed32NoTagToArray(uint32 value,
                                                       uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian32ToArray(value, target);
}
inline uint8* WireFormatLite::WriteFixed64NoTagToArray(uint64 value,
                                                       uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian64ToArray(value, target);
}
inline uint8* WireFormatLite::WriteSFixed32NoTagToArray(int32 value,
                                                        uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian32ToArray(
      static_cast<uint32>(value), target);
}
inline uint8* WireFormatLite::WriteSFixed64NoTagToArray(int64 value,
                                                        uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian64ToArray(
      static_cast<uint64>(value), target);
}
inline uint8* WireFormatLite::WriteFloatNoTagToArray(float value,
                                                     uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian32ToArray(EncodeFloat(value),
                                                           target);
}
inline uint8* WireFormatLite::WriteDoubleNoTagToArray(double value,
                                                      uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian64ToArray(EncodeDouble(value),
                                                           target);
}
inline uint8* WireFormatLite::WriteBoolNoTagToArray(bool value, uint8* target) {
  return io::CodedOutputStream::WriteVarint32ToArray(value ? 1 : 0, target);
}
inline uint8* WireFormatLite::WriteEnumNoTagToArray(int value, uint8* target) {
  return io::CodedOutputStream::WriteVarint32SignExtendedToArray(value, target);
}

template <typename T>
inline uint8* WireFormatLite::WritePrimitiveNoTagToArray(
    const RepeatedField<T>& value, uint8* (*Writer)(T, uint8*), uint8* target) {
  const int n = value.size();
  GOOGLE_DCHECK_GT(n, 0);

  const T* ii = value.data();
  int i = 0;
  do {
    target = Writer(ii[i], target);
  } while (++i < n);

  return target;
}

template <typename T>
inline uint8* WireFormatLite::WriteFixedNoTagToArray(
    const RepeatedField<T>& value, uint8* (*Writer)(T, uint8*), uint8* target) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  (void)Writer;

  const int n = value.size();
  GOOGLE_DCHECK_GT(n, 0);

  const T* ii = value.data();
  const int bytes = n * static_cast<int>(sizeof(ii[0]));
  memcpy(target, ii, static_cast<size_t>(bytes));
  return target + bytes;
#else
  return WritePrimitiveNoTagToArray(value, Writer, target);
#endif
}

inline uint8* WireFormatLite::WriteInt32NoTagToArray(
    const RepeatedField<int32>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteInt32NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteInt64NoTagToArray(
    const RepeatedField<int64>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteInt64NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteUInt32NoTagToArray(
    const RepeatedField<uint32>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteUInt32NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteUInt64NoTagToArray(
    const RepeatedField<uint64>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteUInt64NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteSInt32NoTagToArray(
    const RepeatedField<int32>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteSInt32NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteSInt64NoTagToArray(
    const RepeatedField<int64>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteSInt64NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteFixed32NoTagToArray(
    const RepeatedField<uint32>& value, uint8* target) {
  return WriteFixedNoTagToArray(value, WriteFixed32NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteFixed64NoTagToArray(
    const RepeatedField<uint64>& value, uint8* target) {
  return WriteFixedNoTagToArray(value, WriteFixed64NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteSFixed32NoTagToArray(
    const RepeatedField<int32>& value, uint8* target) {
  return WriteFixedNoTagToArray(value, WriteSFixed32NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteSFixed64NoTagToArray(
    const RepeatedField<int64>& value, uint8* target) {
  return WriteFixedNoTagToArray(value, WriteSFixed64NoTagToArray, target);
}
inline uint8* WireFormatLite::WriteFloatNoTagToArray(
    const RepeatedField<float>& value, uint8* target) {
  return WriteFixedNoTagToArray(value, WriteFloatNoTagToArray, target);
}
inline uint8* WireFormatLite::WriteDoubleNoTagToArray(
    const RepeatedField<double>& value, uint8* target) {
  return WriteFixedNoTagToArray(value, WriteDoubleNoTagToArray, target);
}
inline uint8* WireFormatLite::WriteBoolNoTagToArray(
    const RepeatedField<bool>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteBoolNoTagToArray, target);
}
inline uint8* WireFormatLite::WriteEnumNoTagToArray(
    const RepeatedField<int>& value, uint8* target) {
  return WritePrimitiveNoTagToArray(value, WriteEnumNoTagToArray, target);
}

inline uint8* WireFormatLite::WriteInt32ToArray(int field_number, int32 value,
                                                uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteInt32NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteInt64ToArray(int field_number, int64 value,
                                                uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteInt64NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteUInt32ToArray(int field_number, uint32 value,
                                                 uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteUInt32NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteUInt64ToArray(int field_number, uint64 value,
                                                 uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteUInt64NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteSInt32ToArray(int field_number, int32 value,
                                                 uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteSInt32NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteSInt64ToArray(int field_number, int64 value,
                                                 uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteSInt64NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteFixed32ToArray(int field_number,
                                                  uint32 value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED32, target);
  return WriteFixed32NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteFixed64ToArray(int field_number,
                                                  uint64 value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED64, target);
  return WriteFixed64NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteSFixed32ToArray(int field_number,
                                                   int32 value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED32, target);
  return WriteSFixed32NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteSFixed64ToArray(int field_number,
                                                   int64 value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED64, target);
  return WriteSFixed64NoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteFloatToArray(int field_number, float value,
                                                uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED32, target);
  return WriteFloatNoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteDoubleToArray(int field_number, double value,
                                                 uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED64, target);
  return WriteDoubleNoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteBoolToArray(int field_number, bool value,
                                               uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteBoolNoTagToArray(value, target);
}
inline uint8* WireFormatLite::WriteEnumToArray(int field_number, int value,
                                               uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteEnumNoTagToArray(value, target);
}

template <typename T>
inline uint8* WireFormatLite::WritePrimitiveToArray(
    int field_number, const RepeatedField<T>& value,
    uint8* (*Writer)(int, T, uint8*), uint8* target) {
  const int n = value.size();
  if (n == 0) {
    return target;
  }

  const T* ii = value.data();
  int i = 0;
  do {
    target = Writer(field_number, ii[i], target);
  } while (++i < n);

  return target;
}

inline uint8* WireFormatLite::WriteInt32ToArray(
    int field_number, const RepeatedField<int32>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteInt32ToArray, target);
}
inline uint8* WireFormatLite::WriteInt64ToArray(
    int field_number, const RepeatedField<int64>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteInt64ToArray, target);
}
inline uint8* WireFormatLite::WriteUInt32ToArray(
    int field_number, const RepeatedField<uint32>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteUInt32ToArray, target);
}
inline uint8* WireFormatLite::WriteUInt64ToArray(
    int field_number, const RepeatedField<uint64>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteUInt64ToArray, target);
}
inline uint8* WireFormatLite::WriteSInt32ToArray(
    int field_number, const RepeatedField<int32>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteSInt32ToArray, target);
}
inline uint8* WireFormatLite::WriteSInt64ToArray(
    int field_number, const RepeatedField<int64>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteSInt64ToArray, target);
}
inline uint8* WireFormatLite::WriteFixed32ToArray(
    int field_number, const RepeatedField<uint32>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteFixed32ToArray,
                               target);
}
inline uint8* WireFormatLite::WriteFixed64ToArray(
    int field_number, const RepeatedField<uint64>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteFixed64ToArray,
                               target);
}
inline uint8* WireFormatLite::WriteSFixed32ToArray(
    int field_number, const RepeatedField<int32>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteSFixed32ToArray,
                               target);
}
inline uint8* WireFormatLite::WriteSFixed64ToArray(
    int field_number, const RepeatedField<int64>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteSFixed64ToArray,
                               target);
}
inline uint8* WireFormatLite::WriteFloatToArray(
    int field_number, const RepeatedField<float>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteFloatToArray, target);
}
inline uint8* WireFormatLite::WriteDoubleToArray(
    int field_number, const RepeatedField<double>& value, uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteDoubleToArray, target);
}
inline uint8* WireFormatLite::WriteBoolToArray(int field_number,
                                               const RepeatedField<bool>& value,
                                               uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteBoolToArray, target);
}
inline uint8* WireFormatLite::WriteEnumToArray(int field_number,
                                               const RepeatedField<int>& value,
                                               uint8* target) {
  return WritePrimitiveToArray(field_number, value, WriteEnumToArray, target);
}
inline uint8* WireFormatLite::WriteStringToArray(int field_number,
                                                 const std::string& value,
                                                 uint8* target) {
  // String is for UTF-8 text only
  // WARNING:  In wire_format.cc, both strings and bytes are handled by
  //   WriteString() to avoid code duplication.  If the implementations become
  //   different, you will need to update that usage.
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  return io::CodedOutputStream::WriteStringWithSizeToArray(value, target);
}
inline uint8* WireFormatLite::WriteBytesToArray(int field_number,
                                                const std::string& value,
                                                uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  return io::CodedOutputStream::WriteStringWithSizeToArray(value, target);
}


template <typename MessageType>
inline uint8* WireFormatLite::InternalWriteGroupToArray(
    int field_number, const MessageType& value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_START_GROUP, target);
  target = value.InternalSerializeWithCachedSizesToArray(target);
  return WriteTagToArray(field_number, WIRETYPE_END_GROUP, target);
}
template <typename MessageType>
inline uint8* WireFormatLite::InternalWriteMessageToArray(
    int field_number, const MessageType& value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  target = io::CodedOutputStream::WriteVarint32ToArray(
      static_cast<uint32>(value.GetCachedSize()), target);
  return value.InternalSerializeWithCachedSizesToArray(target);
}

// See comment on ReadGroupNoVirtual to understand the need for this template
// parameter name.
template <typename MessageType_WorkAroundCppLookupDefect>
inline uint8* WireFormatLite::InternalWriteGroupNoVirtualToArray(
    int field_number, const MessageType_WorkAroundCppLookupDefect& value,
    uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_START_GROUP, target);
  target = value.MessageType_WorkAroundCppLookupDefect::
               InternalSerializeWithCachedSizesToArray(target);
  return WriteTagToArray(field_number, WIRETYPE_END_GROUP, target);
}
template <typename MessageType_WorkAroundCppLookupDefect>
inline uint8* WireFormatLite::InternalWriteMessageNoVirtualToArray(
    int field_number, const MessageType_WorkAroundCppLookupDefect& value,
    uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  target = io::CodedOutputStream::WriteVarint32ToArray(
      static_cast<uint32>(
          value.MessageType_WorkAroundCppLookupDefect::GetCachedSize()),
      target);
  return value.MessageType_WorkAroundCppLookupDefect::
      InternalSerializeWithCachedSizesToArray(target);
}

// ===================================================================

inline size_t WireFormatLite::Int32Size(int32 value) {
  return io::CodedOutputStream::VarintSize32SignExtended(value);
}
inline size_t WireFormatLite::Int64Size(int64 value) {
  return io::CodedOutputStream::VarintSize64(static_cast<uint64>(value));
}
inline size_t WireFormatLite::UInt32Size(uint32 value) {
  return io::CodedOutputStream::VarintSize32(value);
}
inline size_t WireFormatLite::UInt64Size(uint64 value) {
  return io::CodedOutputStream::VarintSize64(value);
}
inline size_t WireFormatLite::SInt32Size(int32 value) {
  return io::CodedOutputStream::VarintSize32(ZigZagEncode32(value));
}
inline size_t WireFormatLite::SInt64Size(int64 value) {
  return io::CodedOutputStream::VarintSize64(ZigZagEncode64(value));
}
inline size_t WireFormatLite::EnumSize(int value) {
  return io::CodedOutputStream::VarintSize32SignExtended(value);
}

inline size_t WireFormatLite::StringSize(const std::string& value) {
  return LengthDelimitedSize(value.size());
}
inline size_t WireFormatLite::BytesSize(const std::string& value) {
  return LengthDelimitedSize(value.size());
}


template <typename MessageType>
inline size_t WireFormatLite::GroupSize(const MessageType& value) {
  return value.ByteSizeLong();
}
template <typename MessageType>
inline size_t WireFormatLite::MessageSize(const MessageType& value) {
  return LengthDelimitedSize(value.ByteSizeLong());
}

// See comment on ReadGroupNoVirtual to understand the need for this template
// parameter name.
template <typename MessageType_WorkAroundCppLookupDefect>
inline size_t WireFormatLite::GroupSizeNoVirtual(
    const MessageType_WorkAroundCppLookupDefect& value) {
  return value.MessageType_WorkAroundCppLookupDefect::ByteSizeLong();
}
template <typename MessageType_WorkAroundCppLookupDefect>
inline size_t WireFormatLite::MessageSizeNoVirtual(
    const MessageType_WorkAroundCppLookupDefect& value) {
  return LengthDelimitedSize(
      value.MessageType_WorkAroundCppLookupDefect::ByteSizeLong());
}

inline size_t WireFormatLite::LengthDelimitedSize(size_t length) {
  // The static_cast here prevents an error in certain compiler configurations
  // but is not technically correct--if length is too large to fit in a uint32
  // then it will be silently truncated. We will need to fix this if we ever
  // decide to start supporting serialized messages greater than 2 GiB in size.
  return length +
         io::CodedOutputStream::VarintSize32(static_cast<uint32>(length));
}

template <typename MS>
bool ParseMessageSetItemImpl(io::CodedInputStream* input, MS ms) {
  // This method parses a group which should contain two fields:
  //   required int32 type_id = 2;
  //   required data message = 3;

  uint32 last_type_id = 0;

  // If we see message data before the type_id, we'll append it to this so
  // we can parse it later.
  std::string message_data;

  while (true) {
    const uint32 tag = input->ReadTagNoLastTag();
    if (tag == 0) return false;

    switch (tag) {
      case WireFormatLite::kMessageSetTypeIdTag: {
        uint32 type_id;
        if (!input->ReadVarint32(&type_id)) return false;
        last_type_id = type_id;

        if (!message_data.empty()) {
          // We saw some message data before the type_id.  Have to parse it
          // now.
          io::CodedInputStream sub_input(
              reinterpret_cast<const uint8*>(message_data.data()),
              static_cast<int>(message_data.size()));
          sub_input.SetRecursionLimit(input->RecursionBudget());
          if (!ms.ParseField(last_type_id, &sub_input)) {
            return false;
          }
          message_data.clear();
        }

        break;
      }

      case WireFormatLite::kMessageSetMessageTag: {
        if (last_type_id == 0) {
          // We haven't seen a type_id yet.  Append this data to message_data.
          uint32 length;
          if (!input->ReadVarint32(&length)) return false;
          if (static_cast<int32>(length) < 0) return false;
          uint32 size = static_cast<uint32>(
              length + io::CodedOutputStream::VarintSize32(length));
          message_data.resize(size);
          auto ptr = reinterpret_cast<uint8*>(&message_data[0]);
          ptr = io::CodedOutputStream::WriteVarint32ToArray(length, ptr);
          if (!input->ReadRaw(ptr, length)) return false;
        } else {
          // Already saw type_id, so we can parse this directly.
          if (!ms.ParseField(last_type_id, input)) {
            return false;
          }
        }

        break;
      }

      case WireFormatLite::kMessageSetItemEndTag: {
        return true;
      }

      default: {
        if (!ms.SkipField(tag, input)) return false;
      }
    }
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_WIRE_FORMAT_LITE_H__
