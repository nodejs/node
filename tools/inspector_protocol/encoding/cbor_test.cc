// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cbor.h"

#include <array>
#include <cmath>
#include <string>
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json_parser.h"
#include "json_std_string_writer.h"
#include "linux_dev_platform.h"

using testing::ElementsAreArray;

namespace inspector_protocol {

using cbor::MajorType;

//
// EncodeInt32 / CBORTokenTag::INT32
//
TEST(EncodeDecodeInt32Test, Roundtrips23) {
  // This roundtrips the int32_t value 23 through the pair of EncodeInt32 /
  // CBORTokenizer; this is interesting since 23 is encoded as a single byte.
  std::vector<uint8_t> encoded;
  EncodeInt32(23, &encoded);
  // first three bits: major type = 0; remaining five bits: additional info =
  // value 23.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 1>{{23}}));

  // Reverse direction: decode with CBORTokenizer.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::INT32, tokenizer.TokenTag());
  EXPECT_EQ(23, tokenizer.GetInt32());
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeInt32Test, RoundtripsUint8) {
  // This roundtrips the int32_t value 42 through the pair of EncodeInt32 /
  // CBORTokenizer. This is different from Roundtrip23 because 42 is encoded
  // in an extra byte after the initial one.
  std::vector<uint8_t> encoded;
  EncodeInt32(42, &encoded);
  // first three bits: major type = 0;
  // remaining five bits: additional info = 24, indicating payload is uint8.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 2>{{24, 42}}));

  // Reverse direction: decode with CBORTokenizer.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::INT32, tokenizer.TokenTag());
  EXPECT_EQ(42, tokenizer.GetInt32());
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeInt32Test, RoundtripsUint16) {
  // 500 is encoded as a uint16 after the initial byte.
  std::vector<uint8_t> encoded;
  EncodeInt32(500, &encoded);
  // 1 for initial byte, 2 for uint16.
  EXPECT_EQ(static_cast<std::size_t>(3), encoded.size());
  // first three bits: major type = 0;
  // remaining five bits: additional info = 25, indicating payload is uint16.
  EXPECT_EQ(25, encoded[0]);
  EXPECT_EQ(0x01, encoded[1]);
  EXPECT_EQ(0xf4, encoded[2]);

  // Reverse direction: decode with CBORTokenizer.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::INT32, tokenizer.TokenTag());
  EXPECT_EQ(500, tokenizer.GetInt32());
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeInt32Test, RoundtripsInt32Max) {
  // std::numeric_limits<int32_t> is encoded as a uint32 after the initial byte.
  std::vector<uint8_t> encoded;
  EncodeInt32(std::numeric_limits<int32_t>::max(), &encoded);
  // 1 for initial byte, 4 for the uint32.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 26, indicating payload is uint32.
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 5>{{26, 0x7f, 0xff, 0xff, 0xff}}));

  // Reverse direction: decode with CBORTokenizer.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::INT32, tokenizer.TokenTag());
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), tokenizer.GetInt32());
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeInt32Test, CantRoundtripUint32) {
  // 0xdeadbeef is a value which does not fit below
  // std::numerical_limits<int32_t>::max(), so we can't encode
  // it with EncodeInt32. However, CBOR does support this, so we
  // encode it here manually with the internal routine, just to observe
  // that it's considered an invalid int32 by CBORTokenizer.
  std::vector<uint8_t> encoded;
  cbor_internals::WriteTokenStart(MajorType::UNSIGNED, 0xdeadbeef, &encoded);
  // 1 for initial byte, 4 for the uint32.
  // first three bits: major type = 0;
  // remaining five bits: additional info = 26, indicating payload is uint32.
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 5>{{26, 0xde, 0xad, 0xbe, 0xef}}));

  // Now try to decode; we treat this as an invalid INT32.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  // 0xdeadbeef is > std::numerical_limits<int32_t>::max().
  EXPECT_EQ(CBORTokenTag::ERROR_VALUE, tokenizer.TokenTag());
  EXPECT_EQ(Error::CBOR_INVALID_INT32, tokenizer.Status().error);
}

TEST(EncodeDecodeInt32Test, DecodeErrorCases) {
  struct TestCase {
    std::vector<uint8_t> data;
    std::string msg;
  };
  std::vector<TestCase> tests{
      {TestCase{
           {24},
           "additional info = 24 would require 1 byte of payload (but it's 0)"},
       TestCase{{27, 0xaa, 0xbb, 0xcc},
                "additional info = 27 would require 8 bytes of payload (but "
                "it's 3)"},
       TestCase{{29}, "additional info = 29 isn't recognized"}}};

  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.msg);
    span<uint8_t> encoded_bytes(&test.data[0], test.data.size());
    CBORTokenizer tokenizer(
        span<uint8_t>(&encoded_bytes[0], encoded_bytes.size()));
    EXPECT_EQ(CBORTokenTag::ERROR_VALUE, tokenizer.TokenTag());
    EXPECT_EQ(Error::CBOR_INVALID_INT32, tokenizer.Status().error);
  }
}

TEST(EncodeDecodeInt32Test, RoundtripsMinus24) {
  // This roundtrips the int32_t value -24 through the pair of EncodeInt32 /
  // CBORTokenizer; this is interesting since -24 is encoded as
  // a single byte as NEGATIVE, and it tests the specific encoding
  // (note how for unsigned the single byte covers values up to 23).
  // Additional examples are covered in RoundtripsAdditionalExamples.
  std::vector<uint8_t> encoded;
  EncodeInt32(-24, &encoded);
  // first three bits: major type = 1; remaining five bits: additional info =
  // value 23.
  EXPECT_THAT(encoded, ElementsAreArray(std::array<uint8_t, 1>{{1 << 5 | 23}}));

  // Reverse direction: decode with CBORTokenizer.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::INT32, tokenizer.TokenTag());
  EXPECT_EQ(-24, tokenizer.GetInt32());
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeInt32Test, RoundtripsAdditionalNegativeExamples) {
  std::vector<int32_t> examples = {-1,
                                   -10,
                                   -24,
                                   -25,
                                   -300,
                                   -30000,
                                   -300 * 1000,
                                   -1000 * 1000,
                                   -1000 * 1000 * 1000,
                                   std::numeric_limits<int32_t>::min()};
  for (int32_t example : examples) {
    SCOPED_TRACE(base::StringPrintf("example %d", example));
    std::vector<uint8_t> encoded;
    EncodeInt32(example, &encoded);
    CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
    EXPECT_EQ(CBORTokenTag::INT32, tokenizer.TokenTag());
    EXPECT_EQ(example, tokenizer.GetInt32());
    tokenizer.Next();
    EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
  }
}

//
// EncodeString16 / CBORTokenTag::STRING16
//
TEST(EncodeDecodeString16Test, RoundtripsEmpty) {
  // This roundtrips the empty utf16 string through the pair of EncodeString16 /
  // CBORTokenizer.
  std::vector<uint8_t> encoded;
  EncodeString16(span<uint16_t>(), &encoded);
  EXPECT_EQ(static_cast<std::size_t>(1), encoded.size());
  // first three bits: major type = 2; remaining five bits: additional info =
  // size 0.
  EXPECT_EQ(2 << 5, encoded[0]);

  // Reverse direction: decode with CBORTokenizer.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::STRING16, tokenizer.TokenTag());
  span<uint8_t> decoded_string16_wirerep = tokenizer.GetString16WireRep();
  EXPECT_TRUE(decoded_string16_wirerep.empty());
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

// On the wire, we STRING16 is encoded as little endian (least
// significant byte first). The host may or may not be little endian,
// so this routine follows the advice in
// https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html.
std::vector<uint16_t> String16WireRepToHost(span<uint8_t> in) {
  assert((in.size() & 1) == 0);  // must be even number of bytes.
  std::vector<uint16_t> host_out;
  for (std::ptrdiff_t ii = 0; ii < in.size(); ii += 2)
    host_out.push_back(in[ii + 1] << 8 | in[ii]);
  return host_out;
}

TEST(EncodeDecodeString16Test, RoundtripsHelloWorld) {
  // This roundtrips the hello world message which is given here in utf16
  // characters. 0xd83c, 0xdf0e: UTF16 encoding for the "Earth Globe Americas"
  // character, 🌎.
  std::array<uint16_t, 10> msg{
      {'H', 'e', 'l', 'l', 'o', ',', ' ', 0xd83c, 0xdf0e, '.'}};
  std::vector<uint8_t> encoded;
  EncodeString16(span<uint16_t>(msg.data(), msg.size()), &encoded);
  // This will be encoded as BYTE_STRING of length 20, so the 20 is encoded in
  // the additional info part of the initial byte. Payload is two bytes for each
  // UTF16 character.
  uint8_t initial_byte = /*major type=*/2 << 5 | /*additional info=*/20;
  std::array<uint8_t, 21> encoded_expected = {
      {initial_byte, 'H', 0,   'e', 0,    'l',  0,    'l',  0,   'o', 0,
       ',',          0,   ' ', 0,   0x3c, 0xd8, 0x0e, 0xdf, '.', 0}};
  EXPECT_THAT(encoded, ElementsAreArray(encoded_expected));

  // Now decode to complete the roundtrip.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::STRING16, tokenizer.TokenTag());
  std::vector<uint16_t> decoded =
      String16WireRepToHost(tokenizer.GetString16WireRep());
  EXPECT_THAT(decoded, ElementsAreArray(msg));
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());

  // For bonus points, we look at the decoded message in UTF8 as well so we can
  // easily see it on the terminal screen.
  std::string utf8_decoded =
      base::UTF16ToUTF8(base::StringPiece16(decoded.data(), decoded.size()));
  EXPECT_EQ("Hello, 🌎.", utf8_decoded);
}

TEST(EncodeDecodeString16Test, Roundtrips500) {
  // We roundtrip a message that has 250 16 bit values. Each of these are just
  // set to their index. 250 is interesting because the cbor spec uses a
  // BYTE_STRING of length 500 for one of their examples of how to encode the
  // start of it (section 2.1) so it's easy for us to look at the first three
  // bytes closely.
  std::vector<uint16_t> two_fifty;
  for (uint16_t ii = 0; ii < 250; ++ii) two_fifty.push_back(ii);
  std::vector<uint8_t> encoded;
  EncodeString16(span<uint16_t>(two_fifty.data(), two_fifty.size()), &encoded);
  EXPECT_EQ(static_cast<std::size_t>(3 + 250 * 2), encoded.size());
  // Now check the first three bytes:
  // Major type: 2 (BYTE_STRING)
  // Additional information: 25, indicating size is represented by 2 bytes.
  // Bytes 1 and 2 encode 500 (0x01f4).
  EXPECT_EQ(2 << 5 | 25, encoded[0]);
  EXPECT_EQ(0x01, encoded[1]);
  EXPECT_EQ(0xf4, encoded[2]);

  // Now decode to complete the roundtrip.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::STRING16, tokenizer.TokenTag());
  std::vector<uint16_t> decoded =
      String16WireRepToHost(tokenizer.GetString16WireRep());
  EXPECT_THAT(decoded, ElementsAreArray(two_fifty));
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeString16Test, ErrorCases) {
  struct TestCase {
    std::vector<uint8_t> data;
    std::string msg;
  };
  std::vector<TestCase> tests{
      {TestCase{{2 << 5 | 1, 'a'},
                "length must be divisible by 2 (but it's 1)"},
       TestCase{{2 << 5 | 29}, "additional info = 29 isn't recognized"}}};
  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.msg);
    CBORTokenizer tokenizer(span<uint8_t>(&test.data[0], test.data.size()));
    EXPECT_EQ(CBORTokenTag::ERROR_VALUE, tokenizer.TokenTag());
    EXPECT_EQ(Error::CBOR_INVALID_STRING16, tokenizer.Status().error);
  }
}

//
// EncodeString8 / CBORTokenTag::STRING8
//
TEST(EncodeDecodeString8Test, RoundtripsHelloWorld) {
  // This roundtrips the hello world message which is given here in utf8
  // characters. 🌎 is a four byte utf8 character.
  std::string utf8_msg = "Hello, 🌎.";
  std::vector<uint8_t> msg(utf8_msg.begin(), utf8_msg.end());
  std::vector<uint8_t> encoded;
  EncodeString8(span<uint8_t>(msg.data(), msg.size()), &encoded);
  // This will be encoded as STRING of length 12, so the 12 is encoded in
  // the additional info part of the initial byte. Payload is one byte per
  // utf8 byte.
  uint8_t initial_byte = /*major type=*/3 << 5 | /*additional info=*/12;
  std::array<uint8_t, 13> encoded_expected = {{initial_byte, 'H', 'e', 'l', 'l',
                                               'o', ',', ' ', 0xF0, 0x9f, 0x8c,
                                               0x8e, '.'}};
  EXPECT_THAT(encoded, ElementsAreArray(encoded_expected));

  // Now decode to complete the roundtrip.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::STRING8, tokenizer.TokenTag());
  std::vector<uint8_t> decoded(tokenizer.GetString8().begin(),
                               tokenizer.GetString8().end());
  EXPECT_THAT(decoded, ElementsAreArray(msg));
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

//
// EncodeBinary / CBORTokenTag::BINARY
//
TEST(EncodeDecodeBinaryTest, RoundtripsHelloWorld) {
  std::vector<uint8_t> binary = {'H', 'e', 'l', 'l', 'o', ',', ' ',
                                 'w', 'o', 'r', 'l', 'd', '.'};
  std::vector<uint8_t> encoded;
  EncodeBinary(span<uint8_t>(binary.data(), binary.size()), &encoded);
  // So, on the wire we see that the binary blob travels unmodified.
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 15>{
          {(6 << 5 | 22),  // tag 22 indicating base64 interpretation in JSON
           (2 << 5 | 13),  // BYTE_STRING (type 2) of length 13
           'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '.'}}));
  std::vector<uint8_t> decoded;
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::BINARY, tokenizer.TokenTag());
  EXPECT_EQ(0, int(tokenizer.Status().error));
  decoded = std::vector<uint8_t>(tokenizer.GetBinary().begin(),
                                 tokenizer.GetBinary().end());
  EXPECT_THAT(decoded, ElementsAreArray(binary));
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

//
// EncodeDouble / CBORTokenTag::DOUBLE
//
TEST(EncodeDecodeDoubleTest, RoundtripsWikipediaExample) {
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format
  // provides the example of a hex representation 3FD5 5555 5555 5555, which
  // approximates 1/3.

  const double kOriginalValue = 1.0 / 3;
  std::vector<uint8_t> encoded;
  EncodeDouble(kOriginalValue, &encoded);
  // first three bits: major type = 7; remaining five bits: additional info =
  // value 27. This is followed by 8 bytes of payload (which match Wikipedia).
  EXPECT_THAT(
      encoded,
      ElementsAreArray(std::array<uint8_t, 9>{
          {7 << 5 | 27, 0x3f, 0xd5, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}}));

  // Reverse direction: decode and compare with original value.
  CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
  EXPECT_EQ(CBORTokenTag::DOUBLE, tokenizer.TokenTag());
  EXPECT_THAT(tokenizer.GetDouble(), testing::DoubleEq(kOriginalValue));
  tokenizer.Next();
  EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
}

TEST(EncodeDecodeDoubleTest, RoundtripsAdditionalExamples) {
  std::vector<double> examples = {0.0,
                                  1.0,
                                  -1.0,
                                  3.1415,
                                  std::numeric_limits<double>::min(),
                                  std::numeric_limits<double>::max(),
                                  std::numeric_limits<double>::infinity(),
                                  std::numeric_limits<double>::quiet_NaN()};
  for (double example : examples) {
    SCOPED_TRACE(base::StringPrintf("example %lf", example));
    std::vector<uint8_t> encoded;
    EncodeDouble(example, &encoded);
    span<uint8_t> encoded_bytes(&encoded[0], encoded.size());
    CBORTokenizer tokenizer(span<uint8_t>(&encoded[0], encoded.size()));
    EXPECT_EQ(CBORTokenTag::DOUBLE, tokenizer.TokenTag());
    if (std::isnan(example))
      EXPECT_TRUE(std::isnan(tokenizer.GetDouble()));
    else
      EXPECT_THAT(tokenizer.GetDouble(), testing::DoubleEq(example));
    tokenizer.Next();
    EXPECT_EQ(CBORTokenTag::DONE, tokenizer.TokenTag());
  }
}

//
// NewJSONToCBOREncoder
//
void EncodeSevenBitStringForTest(const std::string& key,
                                 std::vector<uint8_t>* out) {
  for (char c : key) assert(c > 0);  // Enforce 7 bits. Sadly, char is signed.
  EncodeString8(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(key.data()), key.size()),
      out);
}

TEST(JSONToCBOREncoderTest, SevenBitStrings) {
  // When a string can be represented as 7 bit ASCII, the encoder will use the
  // STRING (major Type 3) type, so the actual characters end up as bytes on the
  // wire.
  std::vector<uint8_t> encoded;
  Status status;
  std::unique_ptr<JSONParserHandler> encoder =
      NewJSONToCBOREncoder(&encoded, &status);
  std::vector<uint16_t> utf16 = {'f', 'o', 'o'};
  encoder->HandleString16(utf16);
  EXPECT_EQ(Error::OK, status.error);
  // Here we assert that indeed, seven bit strings are represented as
  // bytes on the wire, "foo" is just "foo".
  EXPECT_THAT(encoded,
              ElementsAreArray(std::array<uint8_t, 4>{
                  {/*major type 3*/ 3 << 5 | /*length*/ 3, 'f', 'o', 'o'}}));
}

TEST(JsonCborRoundtrip, EncodingDecoding) {
  // Hits all the cases except binary and error in JSONParserHandler, first
  // parsing a JSON message into CBOR, then parsing it back from CBOR into JSON.
  std::string json =
      "{"
      "\"string\":\"Hello, \\ud83c\\udf0e.\","
      "\"double\":3.1415,"
      "\"int\":1,"
      "\"negative int\":-1,"
      "\"bool\":true,"
      "\"null\":null,"
      "\"array\":[1,2,3]"
      "}";
  std::vector<uint8_t> encoded;
  Status status;
  std::unique_ptr<JSONParserHandler> encoder =
      NewJSONToCBOREncoder(&encoded, &status);
  span<uint8_t> ascii_in(reinterpret_cast<const uint8_t*>(json.data()),
                         json.size());
  ParseJSONChars(GetLinuxDevPlatform(), ascii_in, encoder.get());
  std::vector<uint8_t> expected = {
      0xd8,            // envelope
      0x5a,            // byte string with 32 bit length
      0,    0, 0, 94,  // length is 94 bytes
  };
  expected.push_back(0xbf);  // indef length map start
  EncodeSevenBitStringForTest("string", &expected);
  // This is followed by the encoded string for "Hello, 🌎."
  // So, it's the same bytes that we tested above in
  // EncodeDecodeString16Test.RoundtripsHelloWorld.
  expected.push_back(/*major type=*/2 << 5 | /*additional info=*/20);
  for (uint8_t ch : std::array<uint8_t, 20>{
           {'H', 0, 'e', 0, 'l',  0,    'l',  0,    'o', 0,
            ',', 0, ' ', 0, 0x3c, 0xd8, 0x0e, 0xdf, '.', 0}})
    expected.push_back(ch);
  EncodeSevenBitStringForTest("double", &expected);
  EncodeDouble(3.1415, &expected);
  EncodeSevenBitStringForTest("int", &expected);
  EncodeInt32(1, &expected);
  EncodeSevenBitStringForTest("negative int", &expected);
  EncodeInt32(-1, &expected);
  EncodeSevenBitStringForTest("bool", &expected);
  expected.push_back(7 << 5 | 21);  // RFC 7049 Section 2.3, Table 2: true
  EncodeSevenBitStringForTest("null", &expected);
  expected.push_back(7 << 5 | 22);  // RFC 7049 Section 2.3, Table 2: null
  EncodeSevenBitStringForTest("array", &expected);
  expected.push_back(0xd8);  // envelope
  expected.push_back(0x5a);  // byte string with 32 bit length
  // the length is 5 bytes (that's up to end indef length array below).
  for (uint8_t ch : std::array<uint8_t, 4>{{0, 0, 0, 5}})
    expected.push_back(ch);
  expected.push_back(0x9f);  // RFC 7049 Section 2.2.1, indef length array start
  expected.push_back(1);     // Three UNSIGNED values (easy since Major Type 0)
  expected.push_back(2);
  expected.push_back(3);
  expected.push_back(0xff);  // End indef length array
  expected.push_back(0xff);  // End indef length map
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(encoded, ElementsAreArray(expected));

  // And now we roundtrip, decoding the message we just encoded.
  std::string decoded;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &decoded, &status);
  ParseCBOR(span<uint8_t>(encoded.data(), encoded.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ(json, decoded);
}

TEST(JsonCborRoundtrip, MoreRoundtripExamples) {
  std::vector<std::string> examples = {
      // Tests that after closing a nested objects, additional key/value pairs
      // are considered.
      "{\"foo\":{\"bar\":1},\"baz\":2}", "{\"foo\":[1,2,3],\"baz\":2}"};
  for (const std::string& json : examples) {
    SCOPED_TRACE(std::string("example: ") + json);
    std::vector<uint8_t> encoded;
    Status status;
    std::unique_ptr<JSONParserHandler> encoder =
        NewJSONToCBOREncoder(&encoded, &status);
    span<uint8_t> ascii_in(reinterpret_cast<const uint8_t*>(json.data()),
                           json.size());
    ParseJSONChars(GetLinuxDevPlatform(), ascii_in, encoder.get());
    std::string decoded;
    std::unique_ptr<JSONParserHandler> json_writer =
        NewJSONWriter(GetLinuxDevPlatform(), &decoded, &status);
    ParseCBOR(span<uint8_t>(encoded.data(), encoded.size()), json_writer.get());
    EXPECT_EQ(Error::OK, status.error);
    EXPECT_EQ(json, decoded);
  }
}

TEST(JSONToCBOREncoderTest, HelloWorldBinary_WithTripToJson) {
  // The JSONParserHandler::HandleBinary is a special case: The JSON parser will
  // never call this method, because JSON does not natively support the binary
  // type. So, we can't fully roundtrip. However, the other direction works:
  // binary will be rendered in JSON, as a base64 string. So, we make calls to
  // the encoder directly here, to construct a message, and one of these calls
  // is ::HandleBinary, to which we pass a "binary" string containing "Hello,
  // world.".
  std::vector<uint8_t> encoded;
  Status status;
  std::unique_ptr<JSONParserHandler> encoder =
      NewJSONToCBOREncoder(&encoded, &status);
  encoder->HandleObjectBegin();
  // Emit a key.
  encoder->HandleString16(std::vector<uint16_t>{'f', 'o', 'o'});
  // Emit the binary payload, an arbitrary array of bytes that happens to
  // be the ascii message "Hello, world.".
  encoder->HandleBinary(std::vector<uint8_t>{'H', 'e', 'l', 'l', 'o', ',', ' ',
                                             'w', 'o', 'r', 'l', 'd', '.'});
  encoder->HandleObjectEnd();
  EXPECT_EQ(Error::OK, status.error);

  // Now drive the json writer via the CBOR decoder.
  std::string decoded;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &decoded, &status);
  ParseCBOR(span<uint8_t>(encoded.data(), encoded.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ(Status::npos(), status.pos);
  // "Hello, world." in base64 is "SGVsbG8sIHdvcmxkLg==".
  EXPECT_EQ("{\"foo\":\"SGVsbG8sIHdvcmxkLg==\"}", decoded);
}

//
// ParseCBOR
//
TEST(ParseCBORTest, ParseEmptyCBORMessage) {
  // An envelope starting with 0xd8, 0x5a, with the byte length
  // of 2, containing a map that's empty (0xbf for map
  // start, and 0xff for map end).
  std::vector<uint8_t> in = {0xd8, 0x5a, 0, 0, 0, 2, 0xbf, 0xff};
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(in.data(), in.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ("{}", out);
}

TEST(ParseCBORTest, ParseCBORHelloWorld) {
  const uint8_t kPayloadLen = 27;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen};
  bytes.push_back(0xbf);                       // start indef length map.
  EncodeSevenBitStringForTest("msg", &bytes);  // key: msg
  // Now write the value, the familiar "Hello, 🌎." where the globe is expressed
  // as two utf16 chars.
  bytes.push_back(/*major type=*/2 << 5 | /*additional info=*/20);
  for (uint8_t ch : std::array<uint8_t, 20>{
           {'H', 0, 'e', 0, 'l',  0,    'l',  0,    'o', 0,
            ',', 0, ' ', 0, 0x3c, 0xd8, 0x0e, 0xdf, '.', 0}})
    bytes.push_back(ch);
  bytes.push_back(0xff);  // stop byte
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);

  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::OK, status.error);
  EXPECT_EQ("{\"msg\":\"Hello, \\ud83c\\udf0e.\"}", out);
}

TEST(ParseCBORTest, NoInputError) {
  std::vector<uint8_t> in = {};
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(in.data(), in.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_NO_INPUT, status.error);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidStartByteError) {
  // Here we test that some actual json, which usually starts with {,
  // is not considered CBOR. CBOR messages must start with 0x5a, the
  // envelope start byte.
  std::string json = "{\"msg\": \"Hello, world.\"}";
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_START_BYTE, status.error);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, UnexpectedEofExpectedValueError) {
  constexpr uint8_t kPayloadLen = 5;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);  // A key; so value would be next.
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_UNEXPECTED_EOF_EXPECTED_VALUE, status.error);
  EXPECT_EQ(static_cast<int64_t>(bytes.size()), status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, UnexpectedEofInArrayError) {
  constexpr uint8_t kPayloadLen = 8;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};  // The byte for starting a map.
  EncodeSevenBitStringForTest("array",
                              &bytes);  // A key; so value would be next.
  bytes.push_back(0x9f);  // byte for indefinite length array start.
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_UNEXPECTED_EOF_IN_ARRAY, status.error);
  EXPECT_EQ(static_cast<int64_t>(bytes.size()), status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, UnexpectedEofInMapError) {
  constexpr uint8_t kPayloadLen = 1;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};  // The byte for starting a map.
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_UNEXPECTED_EOF_IN_MAP, status.error);
  EXPECT_EQ(7, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidMapKeyError) {
  constexpr uint8_t kPayloadLen = 2;
  std::vector<uint8_t> bytes = {0xd8,       0x5a, 0,
                                0,          0,    kPayloadLen,  // envelope
                                0xbf,                           // map start
                                7 << 5 | 22};  // null (not a valid map key)
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_MAP_KEY, status.error);
  EXPECT_EQ(7, status.pos);
  EXPECT_EQ("", out);
}

std::vector<uint8_t> MakeNestedCBOR(int depth) {
  std::vector<uint8_t> bytes;
  std::vector<EnvelopeEncoder> envelopes;
  for (int ii = 0; ii < depth; ++ii) {
    envelopes.emplace_back();
    envelopes.back().EncodeStart(&bytes);
    bytes.push_back(0xbf);  // indef length map start
    EncodeSevenBitStringForTest("key", &bytes);
  }
  EncodeSevenBitStringForTest("innermost_value", &bytes);
  for (int ii = 0; ii < depth; ++ii) {
    bytes.push_back(0xff);  // stop byte, finishes map.
    envelopes.back().EncodeStop(&bytes);
    envelopes.pop_back();
  }
  return bytes;
}

TEST(ParseCBORTest, StackLimitExceededError) {
  {  // Depth 3: no stack limit exceeded error and is easy to inspect.
    std::vector<uint8_t> bytes = MakeNestedCBOR(3);
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> json_writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::OK, status.error);
    EXPECT_EQ(Status::npos(), status.pos);
    EXPECT_EQ("{\"key\":{\"key\":{\"key\":\"innermost_value\"}}}", out);
  }
  {  // Depth 1000: no stack limit exceeded.
    std::vector<uint8_t> bytes = MakeNestedCBOR(1000);
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> json_writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::OK, status.error);
    EXPECT_EQ(Status::npos(), status.pos);
  }

  // We just want to know the length of one opening map so we can compute
  // where the error is encountered. So we look at a small example and find
  // the second envelope start.
  std::vector<uint8_t> small_example = MakeNestedCBOR(3);
  int64_t opening_segment_size = 1;  // Start after the first envelope start.
  while (opening_segment_size < static_cast<int64_t>(small_example.size()) &&
         small_example[opening_segment_size] != 0xd8)
    opening_segment_size++;

  {  // Depth 1001: limit exceeded.
    std::vector<uint8_t> bytes = MakeNestedCBOR(1001);
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> json_writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::CBOR_STACK_LIMIT_EXCEEDED, status.error);
    EXPECT_EQ(opening_segment_size * 1001, status.pos);
  }
  {  // Depth 1200: still limit exceeded, and at the same pos as for 1001
    std::vector<uint8_t> bytes = MakeNestedCBOR(1200);
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> json_writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
    EXPECT_EQ(Error::CBOR_STACK_LIMIT_EXCEEDED, status.error);
    EXPECT_EQ(opening_segment_size * 1001, status.pos);
  }
}

TEST(ParseCBORTest, UnsupportedValueError) {
  constexpr uint8_t kPayloadLen = 6;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  bytes.push_back(6 << 5 | 5);  // tags aren't supported yet.
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);

  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_UNSUPPORTED_VALUE, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidString16Error) {
  constexpr uint8_t kPayloadLen = 11;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // a BYTE_STRING of length 5 as value; since we interpret these as string16,
  // it's going to be invalid as each character would need two bytes, but
  // 5 isn't divisible by 2.
  bytes.push_back(2 << 5 | 5);
  for (int ii = 0; ii < 5; ++ii) bytes.push_back(' ');
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_STRING16, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidString8Error) {
  constexpr uint8_t kPayloadLen = 6;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // a STRING of length 5 as value, but we're at the end of the bytes array
  // so it can't be decoded successfully.
  bytes.push_back(3 << 5 | 5);
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_STRING8, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, String8MustBe7BitError) {
  constexpr uint8_t kPayloadLen = 11;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // a STRING of length 5 as value, with a payload that has bytes outside
  // 7 bit (> 0x7f).
  bytes.push_back(3 << 5 | 5);
  for (int ii = 0; ii < 5; ++ii) bytes.push_back(0xf0);
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_STRING8_MUST_BE_7BIT, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidBinaryError) {
  constexpr uint8_t kPayloadLen = 9;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  bytes.push_back(6 << 5 | 22);  // base64 hint for JSON; indicates binary
  bytes.push_back(2 << 5 | 10);  // BYTE_STRING (major type 2) of length 10
  // Just two garbage bytes, not enough for the binary.
  bytes.push_back(0x31);
  bytes.push_back(0x23);
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_BINARY, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidDoubleError) {
  constexpr uint8_t kPayloadLen = 8;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  bytes.push_back(7 << 5 | 27);  // initial byte for double
  // Just two garbage bytes, not enough to represent an actual double.
  bytes.push_back(0x31);
  bytes.push_back(0x23);
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_DOUBLE, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, InvalidSignedError) {
  constexpr uint8_t kPayloadLen = 14;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  int64_t error_pos = bytes.size();
  // uint64_t max is a perfectly fine value to encode as CBOR unsigned,
  // but we don't support this since we only cover the int32_t range.
  cbor_internals::WriteTokenStart(MajorType::UNSIGNED,
                                  std::numeric_limits<uint64_t>::max(), &bytes);
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_INVALID_INT32, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}

TEST(ParseCBORTest, TrailingJunk) {
  constexpr uint8_t kPayloadLen = 35;
  std::vector<uint8_t> bytes = {0xd8, 0x5a, 0, 0, 0, kPayloadLen,  // envelope
                                0xbf};                             // map start
  EncodeSevenBitStringForTest("key", &bytes);
  EncodeSevenBitStringForTest("value", &bytes);
  bytes.push_back(0xff);  // Up to here, it's a perfectly fine msg.
  int64_t error_pos = bytes.size();
  EncodeSevenBitStringForTest("trailing junk", &bytes);

  cbor_internals::WriteTokenStart(MajorType::UNSIGNED,
                                  std::numeric_limits<uint64_t>::max(), &bytes);
  EXPECT_EQ(kPayloadLen, bytes.size() - 6);
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> json_writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  ParseCBOR(span<uint8_t>(bytes.data(), bytes.size()), json_writer.get());
  EXPECT_EQ(Error::CBOR_TRAILING_JUNK, status.error);
  EXPECT_EQ(error_pos, status.pos);
  EXPECT_EQ("", out);
}
}  // namespace inspector_protocol
