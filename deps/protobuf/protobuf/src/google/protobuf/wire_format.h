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
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef GOOGLE_PROTOBUF_WIRE_FORMAT_H__
#define GOOGLE_PROTOBUF_WIRE_FORMAT_H__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/stubs/casts.h>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace io {
class CodedInputStream;   // coded_stream.h
class CodedOutputStream;  // coded_stream.h
}  // namespace io
class UnknownFieldSet;  // unknown_field_set.h
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace internal {

// This class is for internal use by the protocol buffer library and by
// protocol-complier-generated message classes.  It must not be called
// directly by clients.
//
// This class contains code for implementing the binary protocol buffer
// wire format via reflection.  The WireFormatLite class implements the
// non-reflection based routines.
//
// This class is really a namespace that contains only static methods
class PROTOBUF_EXPORT WireFormat {
 public:
  // Given a field return its WireType
  static inline WireFormatLite::WireType WireTypeForField(
      const FieldDescriptor* field);

  // Given a FieldDescriptor::Type return its WireType
  static inline WireFormatLite::WireType WireTypeForFieldType(
      FieldDescriptor::Type type);

  // Compute the byte size of a tag.  For groups, this includes both the start
  // and end tags.
  static inline size_t TagSize(int field_number, FieldDescriptor::Type type);

  // These procedures can be used to implement the methods of Message which
  // handle parsing and serialization of the protocol buffer wire format
  // using only the Reflection interface.  When you ask the protocol
  // compiler to optimize for code size rather than speed, it will implement
  // those methods in terms of these procedures.  Of course, these are much
  // slower than the specialized implementations which the protocol compiler
  // generates when told to optimize for speed.

  // Read a message in protocol buffer wire format.
  //
  // This procedure reads either to the end of the input stream or through
  // a WIRETYPE_END_GROUP tag ending the message, whichever comes first.
  // It returns false if the input is invalid.
  //
  // Required fields are NOT checked by this method.  You must call
  // IsInitialized() on the resulting message yourself.
  static bool ParseAndMergePartial(io::CodedInputStream* input,
                                   Message* message);

  // Serialize a message in protocol buffer wire format.
  //
  // Any embedded messages within the message must have their correct sizes
  // cached.  However, the top-level message need not; its size is passed as
  // a parameter to this procedure.
  //
  // These return false iff the underlying stream returns a write error.
  static void SerializeWithCachedSizes(const Message& message, int size,
                                       io::CodedOutputStream* output);

  // Implements Message::ByteSize() via reflection.  WARNING:  The result
  // of this method is *not* cached anywhere.  However, all embedded messages
  // will have their ByteSize() methods called, so their sizes will be cached.
  // Therefore, calling this method is sufficient to allow you to call
  // WireFormat::SerializeWithCachedSizes() on the same object.
  static size_t ByteSize(const Message& message);

  // -----------------------------------------------------------------
  // Helpers for dealing with unknown fields

  // Skips a field value of the given WireType.  The input should start
  // positioned immediately after the tag.  If unknown_fields is non-NULL,
  // the contents of the field will be added to it.
  static bool SkipField(io::CodedInputStream* input, uint32 tag,
                        UnknownFieldSet* unknown_fields);

  // Reads and ignores a message from the input.  If unknown_fields is
  // non-NULL, the contents will be added to it.
  static bool SkipMessage(io::CodedInputStream* input,
                          UnknownFieldSet* unknown_fields);

  // Read a packed enum field. If the is_valid function is not NULL, values
  // for which is_valid(value) returns false are appended to
  // unknown_fields_stream.
  static bool ReadPackedEnumPreserveUnknowns(io::CodedInputStream* input,
                                             uint32 field_number,
                                             bool (*is_valid)(int),
                                             UnknownFieldSet* unknown_fields,
                                             RepeatedField<int>* values);

  // Write the contents of an UnknownFieldSet to the output.
  static void SerializeUnknownFields(const UnknownFieldSet& unknown_fields,
                                     io::CodedOutputStream* output);
  // Same as above, except writing directly to the provided buffer.
  // Requires that the buffer have sufficient capacity for
  // ComputeUnknownFieldsSize(unknown_fields).
  //
  // Returns a pointer past the last written byte.
  static uint8* SerializeUnknownFieldsToArray(
      const UnknownFieldSet& unknown_fields, uint8* target);

  // Same thing except for messages that have the message_set_wire_format
  // option.
  static void SerializeUnknownMessageSetItems(
      const UnknownFieldSet& unknown_fields, io::CodedOutputStream* output);
  // Same as above, except writing directly to the provided buffer.
  // Requires that the buffer have sufficient capacity for
  // ComputeUnknownMessageSetItemsSize(unknown_fields).
  //
  // Returns a pointer past the last written byte.
  static uint8* SerializeUnknownMessageSetItemsToArray(
      const UnknownFieldSet& unknown_fields, uint8* target);

  // Compute the size of the UnknownFieldSet on the wire.
  static size_t ComputeUnknownFieldsSize(const UnknownFieldSet& unknown_fields);

  // Same thing except for messages that have the message_set_wire_format
  // option.
  static size_t ComputeUnknownMessageSetItemsSize(
      const UnknownFieldSet& unknown_fields);

  // Helper functions for encoding and decoding tags.  (Inlined below and in
  // _inl.h)
  //
  // This is different from MakeTag(field->number(), field->type()) in the
  // case of packed repeated fields.
  static uint32 MakeTag(const FieldDescriptor* field);

  // Parse a single field.  The input should start out positioned immediately
  // after the tag.
  static bool ParseAndMergeField(
      uint32 tag,
      const FieldDescriptor* field,  // May be NULL for unknown
      Message* message, io::CodedInputStream* input);

  // Serialize a single field.
  static void SerializeFieldWithCachedSizes(
      const FieldDescriptor* field,  // Cannot be NULL
      const Message& message, io::CodedOutputStream* output);

  // Compute size of a single field.  If the field is a message type, this
  // will call ByteSize() for the embedded message, insuring that it caches
  // its size.
  static size_t FieldByteSize(const FieldDescriptor* field,  // Cannot be NULL
                              const Message& message);

  // Parse/serialize a MessageSet::Item group.  Used with messages that use
  // opion message_set_wire_format = true.
  static bool ParseAndMergeMessageSetItem(io::CodedInputStream* input,
                                          Message* message);
  static void SerializeMessageSetItemWithCachedSizes(
      const FieldDescriptor* field, const Message& message,
      io::CodedOutputStream* output);
  static size_t MessageSetItemByteSize(const FieldDescriptor* field,
                                       const Message& message);

  // Computes the byte size of a field, excluding tags. For packed fields, it
  // only includes the size of the raw data, and not the size of the total
  // length, but for other length-delimited types, the size of the length is
  // included.
  static size_t FieldDataOnlyByteSize(
      const FieldDescriptor* field,  // Cannot be NULL
      const Message& message);

  enum Operation {
    PARSE = 0,
    SERIALIZE = 1,
  };

  // Verifies that a string field is valid UTF8, logging an error if not.
  // This function will not be called by newly generated protobuf code
  // but remains present to support existing code.
  static void VerifyUTF8String(const char* data, int size, Operation op);
  // The NamedField variant takes a field name in order to produce an
  // informative error message if verification fails.
  static void VerifyUTF8StringNamedField(const char* data, int size,
                                         Operation op, const char* field_name);

 private:
  // Skip a MessageSet field.
  static bool SkipMessageSetField(io::CodedInputStream* input,
                                  uint32 field_number,
                                  UnknownFieldSet* unknown_fields);

  // Parse a MessageSet field.
  static bool ParseAndMergeMessageSetField(uint32 field_number,
                                           const FieldDescriptor* field,
                                           Message* message,
                                           io::CodedInputStream* input);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(WireFormat);
};

// Subclass of FieldSkipper which saves skipped fields to an UnknownFieldSet.
class PROTOBUF_EXPORT UnknownFieldSetFieldSkipper : public FieldSkipper {
 public:
  UnknownFieldSetFieldSkipper(UnknownFieldSet* unknown_fields)
      : unknown_fields_(unknown_fields) {}
  ~UnknownFieldSetFieldSkipper() override {}

  // implements FieldSkipper -----------------------------------------
  bool SkipField(io::CodedInputStream* input, uint32 tag) override;
  bool SkipMessage(io::CodedInputStream* input) override;
  void SkipUnknownEnum(int field_number, int value) override;

 protected:
  UnknownFieldSet* unknown_fields_;
};

// inline methods ====================================================

inline WireFormatLite::WireType WireFormat::WireTypeForField(
    const FieldDescriptor* field) {
  if (field->is_packed()) {
    return WireFormatLite::WIRETYPE_LENGTH_DELIMITED;
  } else {
    return WireTypeForFieldType(field->type());
  }
}

inline WireFormatLite::WireType WireFormat::WireTypeForFieldType(
    FieldDescriptor::Type type) {
  // Some compilers don't like enum -> enum casts, so we implicit_cast to
  // int first.
  return WireFormatLite::WireTypeForFieldType(
      static_cast<WireFormatLite::FieldType>(implicit_cast<int>(type)));
}

inline uint32 WireFormat::MakeTag(const FieldDescriptor* field) {
  return WireFormatLite::MakeTag(field->number(), WireTypeForField(field));
}

inline size_t WireFormat::TagSize(int field_number,
                                  FieldDescriptor::Type type) {
  // Some compilers don't like enum -> enum casts, so we implicit_cast to
  // int first.
  return WireFormatLite::TagSize(
      field_number,
      static_cast<WireFormatLite::FieldType>(implicit_cast<int>(type)));
}

inline void WireFormat::VerifyUTF8String(const char* data, int size,
                                         WireFormat::Operation op) {
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  WireFormatLite::VerifyUtf8String(
      data, size, static_cast<WireFormatLite::Operation>(op), NULL);
#else
  // Avoid the compiler warning about unused variables.
  (void)data;
  (void)size;
  (void)op;
#endif
}

inline void WireFormat::VerifyUTF8StringNamedField(const char* data, int size,
                                                   WireFormat::Operation op,
                                                   const char* field_name) {
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  WireFormatLite::VerifyUtf8String(
      data, size, static_cast<WireFormatLite::Operation>(op), field_name);
#else
  // Avoid the compiler warning about unused variables.
  (void)data;
  (void)size;
  (void)op;
  (void)field_name;
#endif
}


inline void SerializeUnknownMessageSetItems(
    const UnknownFieldSet& unknown_fields, io::CodedOutputStream* output) {
  WireFormat::SerializeUnknownMessageSetItems(unknown_fields, output);
}

inline size_t ComputeUnknownMessageSetItemsSize(
    const UnknownFieldSet& unknown_fields) {
  return WireFormat::ComputeUnknownMessageSetItemsSize(unknown_fields);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_WIRE_FORMAT_H__
