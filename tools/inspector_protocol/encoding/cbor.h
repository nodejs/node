// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
#define INSPECTOR_PROTOCOL_ENCODING_CBOR_H_

#include <cstdint>
#include <memory>
#include <vector>
#include "cbor_internals.h"
#include "json_parser_handler.h"
#include "span.h"
#include "status.h"

namespace inspector_protocol {

namespace cbor {

// The major types from RFC 7049 Section 2.1.
enum class MajorType {
  UNSIGNED = 0,
  NEGATIVE = 1,
  BYTE_STRING = 2,
  STRING = 3,
  ARRAY = 4,
  MAP = 5,
  TAG = 6,
  SIMPLE_VALUE = 7
};

// Indicates the number of bits the "initial byte" needs to be shifted to the
// right after applying |kMajorTypeMask| to produce the major type in the
// lowermost bits.
static constexpr uint8_t kMajorTypeBitShift = 5u;
// Mask selecting the low-order 5 bits of the "initial byte", which is where
// the additional information is encoded.
static constexpr uint8_t kAdditionalInformationMask = 0x1f;
// Mask selecting the high-order 3 bits of the "initial byte", which indicates
// the major type of the encoded value.
static constexpr uint8_t kMajorTypeMask = 0xe0;
// Indicates the integer is in the following byte.
static constexpr uint8_t kAdditionalInformation1Byte = 24u;
// Indicates the integer is in the next 2 bytes.
static constexpr uint8_t kAdditionalInformation2Bytes = 25u;
// Indicates the integer is in the next 4 bytes.
static constexpr uint8_t kAdditionalInformation4Bytes = 26u;
// Indicates the integer is in the next 8 bytes.
static constexpr uint8_t kAdditionalInformation8Bytes = 27u;

// Encodes the initial byte, consisting of the |type| in the first 3 bits
// followed by 5 bits of |additional_info|.
constexpr uint8_t EncodeInitialByte(MajorType type, uint8_t additional_info) {
  return (static_cast<uint8_t>(type) << kMajorTypeBitShift) |
         (additional_info & kAdditionalInformationMask);
}

// TAG 24 indicates that what follows is a byte string which is
// encoded in CBOR format. We use this as a wrapper for
// maps and arrays, allowing us to skip them, because the
// byte string carries its size (byte length).
// https://tools.ietf.org/html/rfc7049#section-2.4.4.1
static constexpr uint8_t kInitialByteForEnvelope =
    EncodeInitialByte(MajorType::TAG, 24);
// The initial byte for a byte string with at most 2^32 bytes
// of payload. This is used for envelope encoding, even if
// the byte string is shorter.
static constexpr uint8_t kInitialByteFor32BitLengthByteString =
    EncodeInitialByte(MajorType::BYTE_STRING, 26);

// See RFC 7049 Section 2.2.1, indefinite length arrays / maps have additional
// info = 31.
static constexpr uint8_t kInitialByteIndefiniteLengthArray =
    EncodeInitialByte(MajorType::ARRAY, 31);
static constexpr uint8_t kInitialByteIndefiniteLengthMap =
    EncodeInitialByte(MajorType::MAP, 31);
// See RFC 7049 Section 2.3, Table 1; this is used for finishing indefinite
// length maps / arrays.
static constexpr uint8_t kStopByte =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 31);

}  // namespace cbor

// The binary encoding for the inspector protocol follows the CBOR specification
// (RFC 7049). Additional constraints:
// - Only indefinite length maps and arrays are supported.
// - Maps and arrays are wrapped with an envelope, that is, a
//   CBOR tag with value 24 followed by a byte string specifying
//   the byte length of the enclosed map / array. The byte string
//   must use a 32 bit wide length.
// - At the top level, a message must be an indefinite length map
//   wrapped by an envelope.
// - Maximal size for messages is 2^32 (4 GB).
// - For scalars, we support only the int32_t range, encoded as
//   UNSIGNED/NEGATIVE (major types 0 / 1).
// - UTF16 strings, including with unbalanced surrogate pairs, are encoded
//   as CBOR BYTE_STRING (major type 2). For such strings, the number of
//   bytes encoded must be even.
// - UTF8 strings (major type 3) may only have ASCII characters
//   (7 bit US-ASCII).
// - Arbitrary byte arrays, in the inspector protocol called 'binary',
//   are encoded as BYTE_STRING (major type 2), prefixed with a byte
//   indicating base64 when rendered as JSON.

// Encodes |value| as |UNSIGNED| (major type 0) iff >= 0, or |NEGATIVE|
// (major type 1) iff < 0.
void EncodeInt32(int32_t value, std::vector<uint8_t>* out);

// Encodes a UTF16 string as a BYTE_STRING (major type 2). Each utf16
// character in |in| is emitted with most significant byte first,
// appending to |out|.
void EncodeString16(span<uint16_t> in, std::vector<uint8_t>* out);

// Encodes a UTF8 string |in| as STRING (major type 3).
void EncodeString8(span<uint8_t> in, std::vector<uint8_t>* out);

// Encodes arbitrary binary data in |in| as a BYTE_STRING (major type 2) with
// definitive length, prefixed with tag 22 indicating expected conversion to
// base64 (see RFC 7049, Table 3 and Section 2.4.4.2).
void EncodeBinary(span<uint8_t> in, std::vector<uint8_t>* out);

// Encodes / decodes a double as Major type 7 (SIMPLE_VALUE),
// with additional info = 27, followed by 8 bytes in big endian.
void EncodeDouble(double value, std::vector<uint8_t>* out);

// Some constants for CBOR tokens that only take a single byte on the wire.
uint8_t EncodeTrue();
uint8_t EncodeFalse();
uint8_t EncodeNull();
uint8_t EncodeIndefiniteLengthArrayStart();
uint8_t EncodeIndefiniteLengthMapStart();
uint8_t EncodeStop();

// An envelope indicates the byte length of a wrapped item.
// We use this for maps and array, which allows the decoder
// to skip such (nested) values whole sale.
// It's implemented as a CBOR tag (major type 6) with additional
// info = 24, followed by a byte string with a 32 bit length value;
// so the maximal structure that we can wrap is 2^32 bits long.
// See also: https://tools.ietf.org/html/rfc7049#section-2.4.4.1
class EnvelopeEncoder {
 public:
  // Emits the envelope start bytes and records the position for the
  // byte size in |byte_size_pos_|. Also emits empty bytes for the
  // byte sisze so that encoding can continue.
  void EncodeStart(std::vector<uint8_t>* out);
  // This records the current size in |out| at position byte_size_pos_.
  // Returns true iff successful.
  bool EncodeStop(std::vector<uint8_t>* out);

 private:
  std::size_t byte_size_pos_ = 0;
};

// This can be used to convert from JSON to CBOR, by passing the
// return value to the routines in json_parser.h.  The handler will encode into
// |out|, and iff an error occurs it will set |status| to an error and clear
// |out|. Otherwise, |status.ok()| will be |true|.
std::unique_ptr<JSONParserHandler> NewJSONToCBOREncoder(
    std::vector<uint8_t>* out, Status* status);

// Parses a CBOR encoded message from |bytes|, sending JSON events to
// |json_out|. If an error occurs, sends |out->HandleError|, and parsing stops.
// The client is responsible for discarding the already received information in
// that case.
void ParseCBOR(span<uint8_t> bytes, JSONParserHandler* json_out);

// Tags for the tokens within a CBOR message that CBORStream understands.
// Note that this is not the same terminology as the CBOR spec (RFC 7049),
// but rather, our adaptation. For instance, we lump unsigned and signed
// major type into INT32 here (and disallow values outside the int32_t range).
enum class CBORTokenTag {
  // Encountered an error in the structure of the message. Consult
  // status() for details.
  ERROR_VALUE,
  // Booleans and NULL.
  TRUE_VALUE,
  FALSE_VALUE,
  NULL_VALUE,
  // An int32_t (signed 32 bit integer).
  INT32,
  // A double (64 bit floating point).
  DOUBLE,
  // A UTF8 string.
  STRING8,
  // A UTF16 string.
  STRING16,
  // A binary string.
  BINARY,
  // Starts an indefinite length map; after the map start we expect
  // alternating keys and values, followed by STOP.
  MAP_START,
  // Starts an indefinite length array; after the array start we
  // expect values, followed by STOP.
  ARRAY_START,
  // Ends a map or an array.
  STOP,
  // An envelope indicator, wrapping a map or array.
  // Internally this carries the byte length of the wrapped
  // map or array. While CBORTokenizer::Next() will read / skip the entire
  // envelope, CBORTokenizer::EnterEnvelope() reads the tokens
  // inside of it.
  ENVELOPE,
  // We've reached the end there is nothing else to read.
  DONE,
};

// CBORTokenizer segments a CBOR message, presenting the tokens therein as
// numbers, strings, etc. This is not a complete CBOR parser, but makes it much
// easier to implement one (e.g. ParseCBOR, above). It can also be used to parse
// messages partially.
class CBORTokenizer {
 public:
  explicit CBORTokenizer(span<uint8_t> bytes);
  ~CBORTokenizer();

  // Identifies the current token that we're looking at,
  // or ERROR_VALUE (in which ase ::Status() has details)
  // or DONE (if we're past the last token).
  CBORTokenTag TokenTag() const;

  // Advances to the next token.
  void Next();
  // Can only be called if TokenTag() == CBORTokenTag::ENVELOPE.
  // While Next() would skip past the entire envelope / what it's
  // wrapping, EnterEnvelope positions the cursor inside of the envelope,
  // letting the client explore the nested structure.
  void EnterEnvelope();

  // If TokenTag() is CBORTokenTag::ERROR_VALUE, then Status().error describes
  // the error more precisely; otherwise it'll be set to Error::OK.
  // In either case, Status().pos is the current position.
  struct Status Status() const;

  // The following methods retrieve the token values. They can only
  // be called if TokenTag() matches.

  // To be called only if ::TokenTag() == CBORTokenTag::INT32.
  int32_t GetInt32() const;

  // To be called only if ::TokenTag() == CBORTokenTag::DOUBLE.
  double GetDouble() const;

  // To be called only if ::TokenTag() == CBORTokenTag::STRING8.
  span<uint8_t> GetString8() const;

  // Wire representation for STRING16 is low byte first (little endian).
  // To be called only if ::TokenTag() == CBORTokenTag::STRING16.
  span<uint8_t> GetString16WireRep() const;

  // To be called only if ::TokenTag() == CBORTokenTag::BINARY.
  span<uint8_t> GetBinary() const;

 private:
  void ReadNextToken(bool enter_envelope);
  void SetToken(CBORTokenTag token, std::ptrdiff_t token_byte_length);
  void SetError(Error error);

  span<uint8_t> bytes_;
  CBORTokenTag token_tag_;
  struct Status status_;
  std::ptrdiff_t token_byte_length_;
  cbor::MajorType token_start_type_;
  uint64_t token_start_internal_value_;
};

void DumpCBOR(span<uint8_t> cbor);

}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
