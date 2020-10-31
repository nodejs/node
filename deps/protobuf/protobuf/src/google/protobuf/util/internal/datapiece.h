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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_DATAPIECE_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_DATAPIECE_H__

#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/type.pb.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/stubs/statusor.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {
class ProtoWriter;

// Container for a single piece of data together with its data type.
//
// For primitive types (int32, int64, uint32, uint64, double, float, bool),
// the data is stored by value.
//
// For string, a StringPiece is stored. For Cord, a pointer to Cord is stored.
// Just like StringPiece, the DataPiece class does not own the storage for
// the actual string or Cord, so it is the user's responsiblity to guarantee
// that the underlying storage is still valid when the DataPiece is accessed.
class PROTOBUF_EXPORT DataPiece {
 public:
  // Identifies data type of the value.
  // These are the types supported by DataPiece.
  enum Type {
    TYPE_INT32 = 1,
    TYPE_INT64 = 2,
    TYPE_UINT32 = 3,
    TYPE_UINT64 = 4,
    TYPE_DOUBLE = 5,
    TYPE_FLOAT = 6,
    TYPE_BOOL = 7,
    TYPE_ENUM = 8,
    TYPE_STRING = 9,
    TYPE_BYTES = 10,
    TYPE_NULL = 11,  // explicit NULL type
  };

  // Constructors and Destructor
  explicit DataPiece(const int32 value)
      : type_(TYPE_INT32), i32_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const int64 value)
      : type_(TYPE_INT64), i64_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const uint32 value)
      : type_(TYPE_UINT32), u32_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const uint64 value)
      : type_(TYPE_UINT64), u64_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const double value)
      : type_(TYPE_DOUBLE),
        double_(value),
        use_strict_base64_decoding_(false) {}
  explicit DataPiece(const float value)
      : type_(TYPE_FLOAT), float_(value), use_strict_base64_decoding_(false) {}
  explicit DataPiece(const bool value)
      : type_(TYPE_BOOL), bool_(value), use_strict_base64_decoding_(false) {}
  DataPiece(StringPiece value, bool use_strict_base64_decoding)
      : type_(TYPE_STRING),
        str_(StringPiecePod::CreateFromStringPiece(value)),
        use_strict_base64_decoding_(use_strict_base64_decoding) {}
  // Constructor for bytes. The second parameter is not used.
  DataPiece(StringPiece value, bool dummy, bool use_strict_base64_decoding)
      : type_(TYPE_BYTES),
        str_(StringPiecePod::CreateFromStringPiece(value)),
        use_strict_base64_decoding_(use_strict_base64_decoding) {}

  DataPiece(const DataPiece& r) : type_(r.type_) { InternalCopy(r); }

  DataPiece& operator=(const DataPiece& x) {
    InternalCopy(x);
    return *this;
  }

  static DataPiece NullData() { return DataPiece(TYPE_NULL, 0); }

  virtual ~DataPiece() {
  }

  // Accessors
  Type type() const { return type_; }

  bool use_strict_base64_decoding() { return use_strict_base64_decoding_; }

  StringPiece str() const {
    GOOGLE_LOG_IF(DFATAL, type_ != TYPE_STRING) << "Not a string type.";
    return str_;
  }


  // Parses, casts or converts the value stored in the DataPiece into an int32.
  util::StatusOr<int32> ToInt32() const;

  // Parses, casts or converts the value stored in the DataPiece into a uint32.
  util::StatusOr<uint32> ToUint32() const;

  // Parses, casts or converts the value stored in the DataPiece into an int64.
  util::StatusOr<int64> ToInt64() const;

  // Parses, casts or converts the value stored in the DataPiece into a uint64.
  util::StatusOr<uint64> ToUint64() const;

  // Parses, casts or converts the value stored in the DataPiece into a double.
  util::StatusOr<double> ToDouble() const;

  // Parses, casts or converts the value stored in the DataPiece into a float.
  util::StatusOr<float> ToFloat() const;

  // Parses, casts or converts the value stored in the DataPiece into a bool.
  util::StatusOr<bool> ToBool() const;

  // Parses, casts or converts the value stored in the DataPiece into a string.
  util::StatusOr<std::string> ToString() const;

  // Tries to convert the value contained in this datapiece to string. If the
  // conversion fails, it returns the default_string.
  std::string ValueAsStringOrDefault(StringPiece default_string) const;

  util::StatusOr<std::string> ToBytes() const;

 private:
  friend class ProtoWriter;

  // Disallow implicit constructor.
  DataPiece();

  // Helper to create NULL or ENUM types.
  DataPiece(Type type, int32 val)
      : type_(type), i32_(val), use_strict_base64_decoding_(false) {}

  // Same as the ToEnum() method above but with additional flag to ignore
  // unknown enum values.
  util::StatusOr<int> ToEnum(const google::protobuf::Enum* enum_type,
                               bool use_lower_camel_for_enums,
                               bool case_insensitive_enum_parsing,
                               bool ignore_unknown_enum_values,
                               bool* is_unknown_enum_value) const;

  // For numeric conversion between
  //     int32, int64, uint32, uint64, double, float and bool
  template <typename To>
  util::StatusOr<To> GenericConvert() const;

  // For conversion from string to
  //     int32, int64, uint32, uint64, double, float and bool
  template <typename To>
  util::StatusOr<To> StringToNumber(bool (*func)(StringPiece,
                                                   To*)) const;

  // Decodes a base64 string. Returns true on success.
  bool DecodeBase64(StringPiece src, std::string* dest) const;

  // Helper function to initialize this DataPiece with 'other'.
  void InternalCopy(const DataPiece& other);

  // Data type for this piece of data.
  Type type_;

  typedef ::google::protobuf::internal::StringPiecePod StringPiecePod;

  // Stored piece of data.
  union {
    int32 i32_;
    int64 i64_;
    uint32 u32_;
    uint64 u64_;
    double double_;
    float float_;
    bool bool_;
    StringPiecePod str_;
  };

  // Uses a stricter version of base64 decoding for byte fields.
  bool use_strict_base64_decoding_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_DATAPIECE_H__
