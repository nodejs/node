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

#include "binary_json_conformance_suite.h"
#include "conformance_test.h"
#include "third_party/jsoncpp/json.h"

#include <google/protobuf/test_messages_proto3.pb.h>
#include <google/protobuf/test_messages_proto2.pb.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <google/protobuf/wire_format_lite.h>

using conformance::ConformanceRequest;
using conformance::ConformanceResponse;
using conformance::WireFormat;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Message;
using google::protobuf::internal::WireFormatLite;
using google::protobuf::TextFormat;
using google::protobuf::util::NewTypeResolverForDescriptorPool;
using protobuf_test_messages::proto3::TestAllTypesProto3;
using protobuf_test_messages::proto2::TestAllTypesProto2;
using std::string;

namespace {

static const char kTypeUrlPrefix[] = "type.googleapis.com";

static string GetTypeUrl(const Descriptor* message) {
  return string(kTypeUrlPrefix) + "/" + message->full_name();
}

/* Routines for building arbitrary protos *************************************/

// We would use CodedOutputStream except that we want more freedom to build
// arbitrary protos (even invalid ones).

const string empty;

string cat(const string& a, const string& b,
           const string& c = empty,
           const string& d = empty,
           const string& e = empty,
           const string& f = empty,
           const string& g = empty,
           const string& h = empty,
           const string& i = empty,
           const string& j = empty,
           const string& k = empty,
           const string& l = empty) {
  string ret;
  ret.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size() + h.size() + i.size() + j.size() + k.size() + l.size());
  ret.append(a);
  ret.append(b);
  ret.append(c);
  ret.append(d);
  ret.append(e);
  ret.append(f);
  ret.append(g);
  ret.append(h);
  ret.append(i);
  ret.append(j);
  ret.append(k);
  ret.append(l);
  return ret;
}

// The maximum number of bytes that it takes to encode a 64-bit varint.
#define VARINT_MAX_LEN 10

size_t vencode64(uint64_t val, int over_encoded_bytes, char *buf) {
  if (val == 0) { buf[0] = 0; return 1; }
  size_t i = 0;
  while (val) {
    uint8_t byte = val & 0x7fU;
    val >>= 7;
    if (val || over_encoded_bytes) byte |= 0x80U;
    buf[i++] = byte;
  }
  while (over_encoded_bytes--) {
    assert(i < 10);
    uint8_t byte = over_encoded_bytes ? 0x80 : 0;
    buf[i++] = byte;
  }
  return i;
}

string varint(uint64_t x) {
  char buf[VARINT_MAX_LEN];
  size_t len = vencode64(x, 0, buf);
  return string(buf, len);
}

// Encodes a varint that is |extra| bytes longer than it needs to be, but still
// valid.
string longvarint(uint64_t x, int extra) {
  char buf[VARINT_MAX_LEN];
  size_t len = vencode64(x, extra, buf);
  return string(buf, len);
}

// TODO: proper byte-swapping for big-endian machines.
string fixed32(void *data) { return string(static_cast<char*>(data), 4); }
string fixed64(void *data) { return string(static_cast<char*>(data), 8); }

string delim(const string& buf) { return cat(varint(buf.size()), buf); }
string u32(uint32_t u32) { return fixed32(&u32); }
string u64(uint64_t u64) { return fixed64(&u64); }
string flt(float f) { return fixed32(&f); }
string dbl(double d) { return fixed64(&d); }
string zz32(int32_t x) { return varint(WireFormatLite::ZigZagEncode32(x)); }
string zz64(int64_t x) { return varint(WireFormatLite::ZigZagEncode64(x)); }

string tag(uint32_t fieldnum, char wire_type) {
  return varint((fieldnum << 3) | wire_type);
}

string submsg(uint32_t fn, const string& buf) {
  return cat( tag(fn, WireFormatLite::WIRETYPE_LENGTH_DELIMITED), delim(buf) );
}

#define UNKNOWN_FIELD 666

const FieldDescriptor* GetFieldForType(FieldDescriptor::Type type,
                                       bool repeated, bool is_proto3) {

  const Descriptor* d = is_proto3 ?
      TestAllTypesProto3().GetDescriptor() : TestAllTypesProto2().GetDescriptor();
  for (int i = 0; i < d->field_count(); i++) {
    const FieldDescriptor* f = d->field(i);
    if (f->type() == type && f->is_repeated() == repeated) {
      return f;
    }
  }
  GOOGLE_LOG(FATAL) << "Couldn't find field with type " << (int)type;
  return nullptr;
}

string UpperCase(string str) {
  for (int i = 0; i < str.size(); i++) {
    str[i] = toupper(str[i]);
  }
  return str;
}

std::unique_ptr<Message> NewTestMessage(bool is_proto3) {
  std::unique_ptr<Message> prototype;
  if (is_proto3) {
    prototype.reset(new TestAllTypesProto3());
  } else {
    prototype.reset(new TestAllTypesProto2());
  }
  return prototype;
}

}  // anonymous namespace

namespace google {
namespace protobuf {

bool BinaryAndJsonConformanceSuite::ParseJsonResponse(
    const ConformanceResponse& response,
    Message* test_message) {
  string binary_protobuf;
  util::Status status =
      JsonToBinaryString(type_resolver_.get(), type_url_,
                         response.json_payload(), &binary_protobuf);

  if (!status.ok()) {
    return false;
  }

  if (!test_message->ParseFromString(binary_protobuf)) {
    GOOGLE_LOG(FATAL)
        << "INTERNAL ERROR: internal JSON->protobuf transcode "
        << "yielded unparseable proto.";
    return false;
  }

  return true;
}

bool BinaryAndJsonConformanceSuite::ParseResponse(
    const ConformanceResponse& response,
    const ConformanceRequestSetting& setting,
    Message* test_message) {
  const ConformanceRequest& request = setting.GetRequest();
  WireFormat requested_output = request.requested_output_format();
  const string& test_name = setting.GetTestName();
  ConformanceLevel level = setting.GetLevel();

  switch (response.result_case()) {
    case ConformanceResponse::kProtobufPayload: {
      if (requested_output != conformance::PROTOBUF) {
        ReportFailure(
            test_name, level, request, response,
            StrCat("Test was asked for ", WireFormatToString(requested_output),
                   " output but provided PROTOBUF instead.").c_str());
        return false;
      }

      if (!test_message->ParseFromString(response.protobuf_payload())) {
        ReportFailure(test_name, level, request, response,
                   "Protobuf output we received from test was unparseable.");
        return false;
      }

      break;
    }

    case ConformanceResponse::kJsonPayload: {
      if (requested_output != conformance::JSON) {
        ReportFailure(
            test_name, level, request, response,
            StrCat("Test was asked for ", WireFormatToString(requested_output),
                   " output but provided JSON instead.").c_str());
        return false;
      }

      if (!ParseJsonResponse(response, test_message)) {
        ReportFailure(test_name, level, request, response,
                      "JSON output we received from test was unparseable.");
        return false;
      }

      break;
    }

    default:
      GOOGLE_LOG(FATAL) << test_name << ": unknown payload type: "
                        << response.result_case();
  }

  return true;
}

void BinaryAndJsonConformanceSuite::ExpectParseFailureForProtoWithProtoVersion (
    const string& proto, const string& test_name, ConformanceLevel level,
    bool is_proto3) {
  std::unique_ptr<Message> prototype = NewTestMessage(is_proto3);
  // We don't expect output, but if the program erroneously accepts the protobuf
  // we let it send its response as this.  We must not leave it unspecified.
  ConformanceRequestSetting setting(
      level, conformance::PROTOBUF, conformance::PROTOBUF,
      conformance::BINARY_TEST,
      *prototype, test_name, proto);

  const ConformanceRequest& request = setting.GetRequest();
  ConformanceResponse response;
  string effective_test_name =
      StrCat(setting.ConformanceLevelToString(level),
             (is_proto3 ? ".Proto3" : ".Proto2"),
             ".ProtobufInput.", test_name);

  RunTest(effective_test_name, request, &response);
  if (response.result_case() == ConformanceResponse::kParseError) {
    ReportSuccess(effective_test_name);
  } else if (response.result_case() == ConformanceResponse::kSkipped) {
    ReportSkip(effective_test_name, request, response);
  } else {
    ReportFailure(effective_test_name, level, request, response,
                  "Should have failed to parse, but didn't.");
  }
}

// Expect that this precise protobuf will cause a parse error.
void BinaryAndJsonConformanceSuite::ExpectParseFailureForProto(
    const string& proto, const string& test_name, ConformanceLevel level) {
  ExpectParseFailureForProtoWithProtoVersion(proto, test_name, level, true);
  ExpectParseFailureForProtoWithProtoVersion(proto, test_name, level, false);
}

// Expect that this protobuf will cause a parse error, even if it is followed
// by valid protobuf data.  We can try running this twice: once with this
// data verbatim and once with this data followed by some valid data.
//
// TODO(haberman): implement the second of these.
void BinaryAndJsonConformanceSuite::ExpectHardParseFailureForProto(
    const string& proto, const string& test_name, ConformanceLevel level) {
  return ExpectParseFailureForProto(proto, test_name, level);
}

void BinaryAndJsonConformanceSuite::RunValidJsonTest(
    const string& test_name, ConformanceLevel level, const string& input_json,
    const string& equivalent_text_format) {
  TestAllTypesProto3 prototype;
  ConformanceRequestSetting setting1(
      level, conformance::JSON, conformance::PROTOBUF,
      conformance::JSON_TEST,
      prototype, test_name, input_json);
  RunValidInputTest(setting1, equivalent_text_format);
  ConformanceRequestSetting setting2(
      level, conformance::JSON, conformance::JSON,
      conformance::JSON_TEST,
      prototype, test_name, input_json);
  RunValidInputTest(setting2, equivalent_text_format);
}

void BinaryAndJsonConformanceSuite::RunValidJsonTestWithProtobufInput(
    const string& test_name, ConformanceLevel level, const TestAllTypesProto3& input,
    const string& equivalent_text_format) {
  ConformanceRequestSetting setting(
      level, conformance::PROTOBUF, conformance::JSON,
      conformance::JSON_TEST,
      input, test_name, input.SerializeAsString());
  RunValidInputTest(setting, equivalent_text_format);
}

void BinaryAndJsonConformanceSuite::RunValidJsonIgnoreUnknownTest(
    const string& test_name, ConformanceLevel level, const string& input_json,
    const string& equivalent_text_format) {
  TestAllTypesProto3 prototype;
  ConformanceRequestSetting setting(
      level, conformance::JSON, conformance::PROTOBUF,
      conformance::JSON_IGNORE_UNKNOWN_PARSING_TEST,
      prototype, test_name, input_json);
  RunValidInputTest(setting, equivalent_text_format);
}

void BinaryAndJsonConformanceSuite::RunValidProtobufTest(
    const string& test_name, ConformanceLevel level,
    const string& input_protobuf, const string& equivalent_text_format,
    bool is_proto3) {
  std::unique_ptr<Message> prototype = NewTestMessage(is_proto3);

  ConformanceRequestSetting setting1(
      level, conformance::PROTOBUF, conformance::PROTOBUF,
      conformance::BINARY_TEST,
      *prototype, test_name, input_protobuf);
  RunValidInputTest(setting1, equivalent_text_format);

  if (is_proto3) {
    ConformanceRequestSetting setting2(
        level, conformance::PROTOBUF, conformance::JSON,
        conformance::BINARY_TEST,
        *prototype, test_name, input_protobuf);
    RunValidInputTest(setting2, equivalent_text_format);
  }
}

void BinaryAndJsonConformanceSuite::RunValidBinaryProtobufTest(
    const string& test_name, ConformanceLevel level,
    const string& input_protobuf, bool is_proto3) {
  std::unique_ptr<Message> prototype = NewTestMessage(is_proto3);
  ConformanceRequestSetting setting(
      level, conformance::PROTOBUF, conformance::PROTOBUF,
      conformance::BINARY_TEST,
      *prototype, test_name, input_protobuf);
  RunValidBinaryInputTest(setting, input_protobuf);
}

void BinaryAndJsonConformanceSuite::RunValidProtobufTestWithMessage(
    const string& test_name, ConformanceLevel level, const Message *input,
    const string& equivalent_text_format, bool is_proto3) {
  RunValidProtobufTest(test_name, level, input->SerializeAsString(),
                       equivalent_text_format, is_proto3);
}

// According to proto3 JSON specification, JSON serializers follow more strict
// rules than parsers (e.g., a serializer must serialize int32 values as JSON
// numbers while the parser is allowed to accept them as JSON strings). This
// method allows strict checking on a proto3 JSON serializer by inspecting
// the JSON output directly.
void BinaryAndJsonConformanceSuite::RunValidJsonTestWithValidator(
    const string& test_name, ConformanceLevel level, const string& input_json,
    const Validator& validator) {
  TestAllTypesProto3 prototype;
  ConformanceRequestSetting setting(
      level, conformance::JSON, conformance::JSON,
      conformance::JSON_TEST,
      prototype, test_name, input_json);
  const ConformanceRequest& request = setting.GetRequest();
  ConformanceResponse response;
  string effective_test_name =
      StrCat(setting.ConformanceLevelToString(level),
             ".Proto3.JsonInput.",
             test_name, ".Validator");

  RunTest(effective_test_name, request, &response);

  if (response.result_case() == ConformanceResponse::kSkipped) {
    ReportSkip(effective_test_name, request, response);
    return;
  }

  if (response.result_case() != ConformanceResponse::kJsonPayload) {
    ReportFailure(effective_test_name, level, request, response,
                  "Expected JSON payload but got type %d.",
                  response.result_case());
    return;
  }
  Json::Reader reader;
  Json::Value value;
  if (!reader.parse(response.json_payload(), value)) {
    ReportFailure(effective_test_name, level, request, response,
                  "JSON payload cannot be parsed as valid JSON: %s",
                  reader.getFormattedErrorMessages().c_str());
    return;
  }
  if (!validator(value)) {
    ReportFailure(effective_test_name, level, request, response,
                  "JSON payload validation failed.");
    return;
  }
  ReportSuccess(effective_test_name);
}

void BinaryAndJsonConformanceSuite::ExpectParseFailureForJson(
    const string& test_name, ConformanceLevel level, const string& input_json) {
  TestAllTypesProto3 prototype;
  // We don't expect output, but if the program erroneously accepts the protobuf
  // we let it send its response as this.  We must not leave it unspecified.
  ConformanceRequestSetting setting(
      level, conformance::JSON, conformance::JSON,
      conformance::JSON_TEST,
      prototype, test_name, input_json);
  const ConformanceRequest& request = setting.GetRequest();
  ConformanceResponse response;
  string effective_test_name =
      StrCat(setting.ConformanceLevelToString(level),
             ".Proto3.JsonInput.", test_name);

  RunTest(effective_test_name, request, &response);
  if (response.result_case() == ConformanceResponse::kParseError) {
    ReportSuccess(effective_test_name);
  } else if (response.result_case() == ConformanceResponse::kSkipped) {
    ReportSkip(effective_test_name, request, response);
  } else {
    ReportFailure(effective_test_name, level, request, response,
                  "Should have failed to parse, but didn't.");
  }
}

void BinaryAndJsonConformanceSuite::ExpectSerializeFailureForJson(
    const string& test_name, ConformanceLevel level, const string& text_format) {
  TestAllTypesProto3 payload_message;
  GOOGLE_CHECK(
      TextFormat::ParseFromString(text_format, &payload_message))
          << "Failed to parse: " << text_format;

  TestAllTypesProto3 prototype;
  ConformanceRequestSetting setting(
      level, conformance::PROTOBUF, conformance::JSON,
      conformance::JSON_TEST,
      prototype, test_name, payload_message.SerializeAsString());
  const ConformanceRequest& request = setting.GetRequest();
  ConformanceResponse response;
  string effective_test_name =
      StrCat(setting.ConformanceLevelToString(level),
             ".", test_name, ".JsonOutput");

  RunTest(effective_test_name, request, &response);
  if (response.result_case() == ConformanceResponse::kSerializeError) {
    ReportSuccess(effective_test_name);
  } else if (response.result_case() == ConformanceResponse::kSkipped) {
    ReportSkip(effective_test_name, request, response);
  } else {
    ReportFailure(effective_test_name, level, request, response,
                  "Should have failed to serialize, but didn't.");
  }
}

//TODO: proto2?
void BinaryAndJsonConformanceSuite::TestPrematureEOFForType(
    FieldDescriptor::Type type) {
  // Incomplete values for each wire type.
  static const string incompletes[6] = {
    string("\x80"),     // VARINT
    string("abcdefg"),  // 64BIT
    string("\x80"),     // DELIMITED (partial length)
    string(),           // START_GROUP (no value required)
    string(),           // END_GROUP (no value required)
    string("abc")       // 32BIT
  };

  const FieldDescriptor* field = GetFieldForType(type, false, true);
  const FieldDescriptor* rep_field = GetFieldForType(type, true, true);
  WireFormatLite::WireType wire_type = WireFormatLite::WireTypeForFieldType(
      static_cast<WireFormatLite::FieldType>(type));
  const string& incomplete = incompletes[wire_type];
  const string type_name =
      UpperCase(string(".") + FieldDescriptor::TypeName(type));

  ExpectParseFailureForProto(
      tag(field->number(), wire_type),
      "PrematureEofBeforeKnownNonRepeatedValue" + type_name, REQUIRED);

  ExpectParseFailureForProto(
      tag(rep_field->number(), wire_type),
      "PrematureEofBeforeKnownRepeatedValue" + type_name, REQUIRED);

  ExpectParseFailureForProto(
      tag(UNKNOWN_FIELD, wire_type),
      "PrematureEofBeforeUnknownValue" + type_name, REQUIRED);

  ExpectParseFailureForProto(
      cat( tag(field->number(), wire_type), incomplete ),
      "PrematureEofInsideKnownNonRepeatedValue" + type_name, REQUIRED);

  ExpectParseFailureForProto(
      cat( tag(rep_field->number(), wire_type), incomplete ),
      "PrematureEofInsideKnownRepeatedValue" + type_name, REQUIRED);

  ExpectParseFailureForProto(
      cat( tag(UNKNOWN_FIELD, wire_type), incomplete ),
      "PrematureEofInsideUnknownValue" + type_name, REQUIRED);

  if (wire_type == WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
    ExpectParseFailureForProto(
        cat( tag(field->number(), wire_type), varint(1) ),
        "PrematureEofInDelimitedDataForKnownNonRepeatedValue" + type_name,
        REQUIRED);

    ExpectParseFailureForProto(
        cat( tag(rep_field->number(), wire_type), varint(1) ),
        "PrematureEofInDelimitedDataForKnownRepeatedValue" + type_name,
        REQUIRED);

    // EOF in the middle of delimited data for unknown value.
    ExpectParseFailureForProto(
        cat( tag(UNKNOWN_FIELD, wire_type), varint(1) ),
        "PrematureEofInDelimitedDataForUnknownValue" + type_name, REQUIRED);

    if (type == FieldDescriptor::TYPE_MESSAGE) {
      // Submessage ends in the middle of a value.
      string incomplete_submsg =
          cat( tag(WireFormatLite::TYPE_INT32, WireFormatLite::WIRETYPE_VARINT),
                incompletes[WireFormatLite::WIRETYPE_VARINT] );
      ExpectHardParseFailureForProto(
          cat( tag(field->number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED),
               varint(incomplete_submsg.size()),
               incomplete_submsg ),
          "PrematureEofInSubmessageValue" + type_name, REQUIRED);
    }
  } else if (type != FieldDescriptor::TYPE_GROUP) {
    // Non-delimited, non-group: eligible for packing.

    // Packed region ends in the middle of a value.
    ExpectHardParseFailureForProto(
        cat(tag(rep_field->number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED),
            varint(incomplete.size()), incomplete),
        "PrematureEofInPackedFieldValue" + type_name, REQUIRED);

    // EOF in the middle of packed region.
    ExpectParseFailureForProto(
        cat(tag(rep_field->number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED),
            varint(1)),
        "PrematureEofInPackedField" + type_name, REQUIRED);
  }
}

void BinaryAndJsonConformanceSuite::TestValidDataForType(
    FieldDescriptor::Type type,
    std::vector<std::pair<std::string, std::string>> values) {
  for (int is_proto3 = 0; is_proto3 < 2; is_proto3++) {
    const string type_name =
        UpperCase(string(".") + FieldDescriptor::TypeName(type));
    WireFormatLite::WireType wire_type = WireFormatLite::WireTypeForFieldType(
        static_cast<WireFormatLite::FieldType>(type));
    const FieldDescriptor* field = GetFieldForType(type, false, is_proto3);
    const FieldDescriptor* rep_field = GetFieldForType(type, true, is_proto3);

    RunValidProtobufTest("ValidDataScalar" + type_name, REQUIRED,
                         cat(tag(field->number(), wire_type), values[0].first),
                         field->name() + ": " + values[0].second, is_proto3);

    string proto;
    string text = field->name() + ": " + values.back().second;
    for (size_t i = 0; i < values.size(); i++) {
      proto += cat(tag(field->number(), wire_type), values[i].first);
    }
    RunValidProtobufTest("RepeatedScalarSelectsLast" + type_name, REQUIRED,
                         proto, text, is_proto3);

    proto.clear();
    text.clear();

    for (size_t i = 0; i < values.size(); i++) {
      proto += cat(tag(rep_field->number(), wire_type), values[i].first);
      text += rep_field->name() + ": " + values[i].second + " ";
    }
    RunValidProtobufTest("ValidDataRepeated" + type_name, REQUIRED,
                         proto, text, is_proto3);
  }
}

// TODO: proto2?
void BinaryAndJsonConformanceSuite::TestIllegalTags() {
  // field num 0 is illegal
  string nullfield[] = {
    "\1DEADBEEF",
    "\2\1\1",
    "\3\4",
    "\5DEAD"
  };
  for (int i = 0; i < 4; i++) {
    string name = "IllegalZeroFieldNum_Case_0";
    name.back() += i;
    ExpectParseFailureForProto(nullfield[i], name, REQUIRED);
  }
}
template <class MessageType>
void BinaryAndJsonConformanceSuite::TestOneofMessage (
    MessageType &message, bool is_proto3) {
  message.set_oneof_uint32(0);
  RunValidProtobufTestWithMessage(
      "OneofZeroUint32", RECOMMENDED, &message, "oneof_uint32: 0", is_proto3);
  message.mutable_oneof_nested_message()->set_a(0);
  RunValidProtobufTestWithMessage(
      "OneofZeroMessage", RECOMMENDED, &message,
      is_proto3 ? "oneof_nested_message: {}" : "oneof_nested_message: {a: 0}",
      is_proto3);
  message.mutable_oneof_nested_message()->set_a(1);
  RunValidProtobufTestWithMessage(
      "OneofZeroMessageSetTwice", RECOMMENDED, &message,
      "oneof_nested_message: {a: 1}",
      is_proto3);
  message.set_oneof_string("");
  RunValidProtobufTestWithMessage(
      "OneofZeroString", RECOMMENDED, &message, "oneof_string: \"\"", is_proto3);
  message.set_oneof_bytes("");
  RunValidProtobufTestWithMessage(
      "OneofZeroBytes", RECOMMENDED, &message, "oneof_bytes: \"\"", is_proto3);
  message.set_oneof_bool(false);
  RunValidProtobufTestWithMessage(
      "OneofZeroBool", RECOMMENDED, &message, "oneof_bool: false", is_proto3);
  message.set_oneof_uint64(0);
  RunValidProtobufTestWithMessage(
      "OneofZeroUint64", RECOMMENDED, &message, "oneof_uint64: 0", is_proto3);
  message.set_oneof_float(0.0f);
  RunValidProtobufTestWithMessage(
      "OneofZeroFloat", RECOMMENDED, &message, "oneof_float: 0", is_proto3);
  message.set_oneof_double(0.0);
  RunValidProtobufTestWithMessage(
      "OneofZeroDouble", RECOMMENDED, &message, "oneof_double: 0", is_proto3);
  message.set_oneof_enum(MessageType::FOO);
  RunValidProtobufTestWithMessage(
      "OneofZeroEnum", RECOMMENDED, &message, "oneof_enum: FOO", is_proto3);
}

template <class MessageType>
void BinaryAndJsonConformanceSuite::TestUnknownMessage(
    MessageType& message, bool is_proto3) {
  message.ParseFromString("\xA8\x1F\x01");
  RunValidBinaryProtobufTest("UnknownVarint", REQUIRED,
                             message.SerializeAsString(), is_proto3);
}

void BinaryAndJsonConformanceSuite::RunSuiteImpl() {
  // Hack to get the list of test failures based on whether
  // GOOGLE3_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER is enabled or not.
  conformance::FailureSet failure_set;
  ConformanceRequest req;
  ConformanceResponse res;
  req.set_message_type(failure_set.GetTypeName());
  req.set_protobuf_payload("");
  req.set_requested_output_format(conformance::WireFormat::PROTOBUF);
  RunTest("FindFailures", req, &res);
  GOOGLE_CHECK(failure_set.MergeFromString(res.protobuf_payload()));
  for (const string& failure : failure_set.failure()) {
    AddExpectedFailedTest(failure);
  }

  type_resolver_.reset(NewTypeResolverForDescriptorPool(
      kTypeUrlPrefix, DescriptorPool::generated_pool()));
  type_url_ = GetTypeUrl(TestAllTypesProto3::descriptor());

  for (int i = 1; i <= FieldDescriptor::MAX_TYPE; i++) {
    if (i == FieldDescriptor::TYPE_GROUP) continue;
    TestPrematureEOFForType(static_cast<FieldDescriptor::Type>(i));
  }

  TestIllegalTags();

  int64 kInt64Min = -9223372036854775808ULL;
  int64 kInt64Max = 9223372036854775807ULL;
  uint64 kUint64Max = 18446744073709551615ULL;
  int32 kInt32Max = 2147483647;
  int32 kInt32Min = -2147483648;
  uint32 kUint32Max = 4294967295UL;

  TestValidDataForType(FieldDescriptor::TYPE_DOUBLE, {
    {dbl(0.1), "0.1"},
    {dbl(1.7976931348623157e+308), "1.7976931348623157e+308"},
    {dbl(2.22507385850720138309e-308), "2.22507385850720138309e-308"}
  });
  TestValidDataForType(FieldDescriptor::TYPE_FLOAT, {
    {flt(0.1), "0.1"},
    {flt(1.00000075e-36), "1.00000075e-36"},
    {flt(3.402823e+38), "3.402823e+38"},  // 3.40282347e+38
    {flt(1.17549435e-38f), "1.17549435e-38"}
  });
  TestValidDataForType(FieldDescriptor::TYPE_INT64, {
    {varint(12345), "12345"},
    {varint(kInt64Max), std::to_string(kInt64Max)},
    {varint(kInt64Min), std::to_string(kInt64Min)}
  });
  TestValidDataForType(FieldDescriptor::TYPE_UINT64, {
    {varint(12345), "12345"},
    {varint(kUint64Max), std::to_string(kUint64Max)},
    {varint(0), "0"}
  });
  TestValidDataForType(FieldDescriptor::TYPE_INT32, {
    {varint(12345), "12345"},
    {longvarint(12345, 2), "12345"},
    {longvarint(12345, 7), "12345"},
    {varint(kInt32Max), std::to_string(kInt32Max)},
    {varint(kInt32Min), std::to_string(kInt32Min)},
    {varint(1LL << 33), std::to_string(static_cast<int32>(1LL << 33))},
    {varint((1LL << 33) - 1),
     std::to_string(static_cast<int32>((1LL << 33) - 1))},
  });
  TestValidDataForType(FieldDescriptor::TYPE_UINT32, {
    {varint(12345), "12345"},
    {longvarint(12345, 2), "12345"},
    {longvarint(12345, 7), "12345"},
    {varint(kUint32Max), std::to_string(kUint32Max)},  // UINT32_MAX
    {varint(0), "0"},
    {varint(1LL << 33), std::to_string(static_cast<uint32>(1LL << 33))},
    {varint((1LL << 33) - 1),
     std::to_string(static_cast<uint32>((1LL << 33) - 1))},
  });
  TestValidDataForType(FieldDescriptor::TYPE_FIXED64, {
    {u64(12345), "12345"},
    {u64(kUint64Max), std::to_string(kUint64Max)},
    {u64(0), "0"}
  });
  TestValidDataForType(FieldDescriptor::TYPE_FIXED32, {
    {u32(12345), "12345"},
    {u32(kUint32Max), std::to_string(kUint32Max)},  // UINT32_MAX
    {u32(0), "0"}
  });
  TestValidDataForType(FieldDescriptor::TYPE_SFIXED64, {
    {u64(12345), "12345"},
    {u64(kInt64Max), std::to_string(kInt64Max)},
    {u64(kInt64Min), std::to_string(kInt64Min)}
  });
  TestValidDataForType(FieldDescriptor::TYPE_SFIXED32, {
    {u32(12345), "12345"},
    {u32(kInt32Max), std::to_string(kInt32Max)},
    {u32(kInt32Min), std::to_string(kInt32Min)}
  });
  TestValidDataForType(FieldDescriptor::TYPE_BOOL, {
    {varint(1), "true"},
    {varint(0), "false"},
    {varint(12345678), "true"}
  });
  TestValidDataForType(FieldDescriptor::TYPE_SINT32, {
    {zz32(12345), "12345"},
    {zz32(kInt32Max), std::to_string(kInt32Max)},
    {zz32(kInt32Min), std::to_string(kInt32Min)}
  });
  TestValidDataForType(FieldDescriptor::TYPE_SINT64, {
    {zz64(12345), "12345"},
    {zz64(kInt64Max), std::to_string(kInt64Max)},
    {zz64(kInt64Min), std::to_string(kInt64Min)}
  });

  // TODO(haberman):
  // TestValidDataForType(FieldDescriptor::TYPE_STRING
  // TestValidDataForType(FieldDescriptor::TYPE_GROUP
  // TestValidDataForType(FieldDescriptor::TYPE_MESSAGE
  // TestValidDataForType(FieldDescriptor::TYPE_BYTES
  // TestValidDataForType(FieldDescriptor::TYPE_ENUM

  RunValidJsonTest("HelloWorld", REQUIRED,
                   "{\"optionalString\":\"Hello, World!\"}",
                   "optional_string: 'Hello, World!'");

  // NOTE: The spec for JSON support is still being sorted out, these may not
  // all be correct.
  // Test field name conventions.
  RunValidJsonTest(
      "FieldNameInSnakeCase", REQUIRED,
      R"({
        "fieldname1": 1,
        "fieldName2": 2,
        "FieldName3": 3,
        "fieldName4": 4
      })",
      R"(
        fieldname1: 1
        field_name2: 2
        _field_name3: 3
        field__name4_: 4
      )");
  RunValidJsonTest(
      "FieldNameWithNumbers", REQUIRED,
      R"({
        "field0name5": 5,
        "field0Name6": 6
      })",
      R"(
        field0name5: 5
        field_0_name6: 6
      )");
  RunValidJsonTest(
      "FieldNameWithMixedCases", REQUIRED,
      R"({
        "fieldName7": 7,
        "FieldName8": 8,
        "fieldName9": 9,
        "FieldName10": 10,
        "FIELDNAME11": 11,
        "FIELDName12": 12
      })",
      R"(
        fieldName7: 7
        FieldName8: 8
        field_Name9: 9
        Field_Name10: 10
        FIELD_NAME11: 11
        FIELD_name12: 12
      )");
  RunValidJsonTest(
      "FieldNameWithDoubleUnderscores", RECOMMENDED,
      R"({
        "FieldName13": 13,
        "FieldName14": 14,
        "fieldName15": 15,
        "fieldName16": 16,
        "fieldName17": 17,
        "FieldName18": 18
      })",
      R"(
        __field_name13: 13
        __Field_name14: 14
        field__name15: 15
        field__Name16: 16
        field_name17__: 17
        Field_name18__: 18
      )");
  // Using the original proto field name in JSON is also allowed.
  RunValidJsonTest(
      "OriginalProtoFieldName", REQUIRED,
      R"({
        "fieldname1": 1,
        "field_name2": 2,
        "_field_name3": 3,
        "field__name4_": 4,
        "field0name5": 5,
        "field_0_name6": 6,
        "fieldName7": 7,
        "FieldName8": 8,
        "field_Name9": 9,
        "Field_Name10": 10,
        "FIELD_NAME11": 11,
        "FIELD_name12": 12,
        "__field_name13": 13,
        "__Field_name14": 14,
        "field__name15": 15,
        "field__Name16": 16,
        "field_name17__": 17,
        "Field_name18__": 18
      })",
      R"(
        fieldname1: 1
        field_name2: 2
        _field_name3: 3
        field__name4_: 4
        field0name5: 5
        field_0_name6: 6
        fieldName7: 7
        FieldName8: 8
        field_Name9: 9
        Field_Name10: 10
        FIELD_NAME11: 11
        FIELD_name12: 12
        __field_name13: 13
        __Field_name14: 14
        field__name15: 15
        field__Name16: 16
        field_name17__: 17
        Field_name18__: 18
      )");
  // Field names can be escaped.
  RunValidJsonTest(
      "FieldNameEscaped", REQUIRED,
      R"({"fieldn\u0061me1": 1})",
      "fieldname1: 1");
  // String ends with escape character.
  ExpectParseFailureForJson(
      "StringEndsWithEscapeChar", RECOMMENDED,
      "{\"optionalString\": \"abc\\");
  // Field names must be quoted (or it's not valid JSON).
  ExpectParseFailureForJson(
      "FieldNameNotQuoted", RECOMMENDED,
      "{fieldname1: 1}");
  // Trailing comma is not allowed (not valid JSON).
  ExpectParseFailureForJson(
      "TrailingCommaInAnObject", RECOMMENDED,
      R"({"fieldname1":1,})");
  ExpectParseFailureForJson(
      "TrailingCommaInAnObjectWithSpace", RECOMMENDED,
      R"({"fieldname1":1 ,})");
  ExpectParseFailureForJson(
      "TrailingCommaInAnObjectWithSpaceCommaSpace", RECOMMENDED,
      R"({"fieldname1":1 , })");
  ExpectParseFailureForJson(
      "TrailingCommaInAnObjectWithNewlines", RECOMMENDED,
      R"({
        "fieldname1":1,
      })");
  // JSON doesn't support comments.
  ExpectParseFailureForJson(
      "JsonWithComments", RECOMMENDED,
      R"({
        // This is a comment.
        "fieldname1": 1
      })");
  // JSON spec says whitespace doesn't matter, so try a few spacings to be sure.
  RunValidJsonTest(
      "OneLineNoSpaces", RECOMMENDED,
      "{\"optionalInt32\":1,\"optionalInt64\":2}",
      R"(
        optional_int32: 1
        optional_int64: 2
      )");
  RunValidJsonTest(
      "OneLineWithSpaces", RECOMMENDED,
      "{ \"optionalInt32\" : 1 , \"optionalInt64\" : 2 }",
      R"(
        optional_int32: 1
        optional_int64: 2
      )");
  RunValidJsonTest(
      "MultilineNoSpaces", RECOMMENDED,
      "{\n\"optionalInt32\"\n:\n1\n,\n\"optionalInt64\"\n:\n2\n}",
      R"(
        optional_int32: 1
        optional_int64: 2
      )");
  RunValidJsonTest(
      "MultilineWithSpaces", RECOMMENDED,
      "{\n  \"optionalInt32\"  :  1\n  ,\n  \"optionalInt64\"  :  2\n}\n",
      R"(
        optional_int32: 1
        optional_int64: 2
      )");
  // Missing comma between key/value pairs.
  ExpectParseFailureForJson(
      "MissingCommaOneLine", RECOMMENDED,
      "{ \"optionalInt32\": 1 \"optionalInt64\": 2 }");
  ExpectParseFailureForJson(
      "MissingCommaMultiline", RECOMMENDED,
      "{\n  \"optionalInt32\": 1\n  \"optionalInt64\": 2\n}");
  // Duplicated field names are not allowed.
  ExpectParseFailureForJson(
      "FieldNameDuplicate", RECOMMENDED,
      R"({
        "optionalNestedMessage": {a: 1},
        "optionalNestedMessage": {}
      })");
  ExpectParseFailureForJson(
      "FieldNameDuplicateDifferentCasing1", RECOMMENDED,
      R"({
        "optional_nested_message": {a: 1},
        "optionalNestedMessage": {}
      })");
  ExpectParseFailureForJson(
      "FieldNameDuplicateDifferentCasing2", RECOMMENDED,
      R"({
        "optionalNestedMessage": {a: 1},
        "optional_nested_message": {}
      })");
  // Serializers should use lowerCamelCase by default.
  RunValidJsonTestWithValidator(
      "FieldNameInLowerCamelCase", REQUIRED,
      R"({
        "fieldname1": 1,
        "fieldName2": 2,
        "FieldName3": 3,
        "fieldName4": 4
      })",
      [](const Json::Value& value) {
        return value.isMember("fieldname1") &&
            value.isMember("fieldName2") &&
            value.isMember("FieldName3") &&
            value.isMember("fieldName4");
      });
  RunValidJsonTestWithValidator(
      "FieldNameWithNumbers", REQUIRED,
      R"({
        "field0name5": 5,
        "field0Name6": 6
      })",
      [](const Json::Value& value) {
        return value.isMember("field0name5") &&
            value.isMember("field0Name6");
      });
  RunValidJsonTestWithValidator(
      "FieldNameWithMixedCases", REQUIRED,
      R"({
        "fieldName7": 7,
        "FieldName8": 8,
        "fieldName9": 9,
        "FieldName10": 10,
        "FIELDNAME11": 11,
        "FIELDName12": 12
      })",
      [](const Json::Value& value) {
        return value.isMember("fieldName7") &&
            value.isMember("FieldName8") &&
            value.isMember("fieldName9") &&
            value.isMember("FieldName10") &&
            value.isMember("FIELDNAME11") &&
            value.isMember("FIELDName12");
      });
  RunValidJsonTestWithValidator(
      "FieldNameWithDoubleUnderscores", RECOMMENDED,
      R"({
        "FieldName13": 13,
        "FieldName14": 14,
        "fieldName15": 15,
        "fieldName16": 16,
        "fieldName17": 17,
        "FieldName18": 18
      })",
      [](const Json::Value& value) {
        return value.isMember("FieldName13") &&
            value.isMember("FieldName14") &&
            value.isMember("fieldName15") &&
            value.isMember("fieldName16") &&
            value.isMember("fieldName17") &&
            value.isMember("FieldName18");
      });

  // Integer fields.
  RunValidJsonTest(
      "Int32FieldMaxValue", REQUIRED,
      R"({"optionalInt32": 2147483647})",
      "optional_int32: 2147483647");
  RunValidJsonTest(
      "Int32FieldMinValue", REQUIRED,
      R"({"optionalInt32": -2147483648})",
      "optional_int32: -2147483648");
  RunValidJsonTest(
      "Uint32FieldMaxValue", REQUIRED,
      R"({"optionalUint32": 4294967295})",
      "optional_uint32: 4294967295");
  RunValidJsonTest(
      "Int64FieldMaxValue", REQUIRED,
      R"({"optionalInt64": "9223372036854775807"})",
      "optional_int64: 9223372036854775807");
  RunValidJsonTest(
      "Int64FieldMinValue", REQUIRED,
      R"({"optionalInt64": "-9223372036854775808"})",
      "optional_int64: -9223372036854775808");
  RunValidJsonTest(
      "Uint64FieldMaxValue", REQUIRED,
      R"({"optionalUint64": "18446744073709551615"})",
      "optional_uint64: 18446744073709551615");
  // While not the largest Int64, this is the largest
  // Int64 which can be exactly represented within an
  // IEEE-754 64-bit float, which is the expected level
  // of interoperability guarantee. Larger values may
  // work in some implementations, but should not be
  // relied upon.
  RunValidJsonTest(
      "Int64FieldMaxValueNotQuoted", REQUIRED,
      R"({"optionalInt64": 9223372036854774784})",
      "optional_int64: 9223372036854774784");
  RunValidJsonTest(
      "Int64FieldMinValueNotQuoted", REQUIRED,
      R"({"optionalInt64": -9223372036854775808})",
      "optional_int64: -9223372036854775808");
  // Largest interoperable Uint64; see comment above
  // for Int64FieldMaxValueNotQuoted.
  RunValidJsonTest(
      "Uint64FieldMaxValueNotQuoted", REQUIRED,
      R"({"optionalUint64": 18446744073709549568})",
      "optional_uint64: 18446744073709549568");
  // Values can be represented as JSON strings.
  RunValidJsonTest(
      "Int32FieldStringValue", REQUIRED,
      R"({"optionalInt32": "2147483647"})",
      "optional_int32: 2147483647");
  RunValidJsonTest(
      "Int32FieldStringValueEscaped", REQUIRED,
      R"({"optionalInt32": "2\u003147483647"})",
      "optional_int32: 2147483647");

  // Parsers reject out-of-bound integer values.
  ExpectParseFailureForJson(
      "Int32FieldTooLarge", REQUIRED,
      R"({"optionalInt32": 2147483648})");
  ExpectParseFailureForJson(
      "Int32FieldTooSmall", REQUIRED,
      R"({"optionalInt32": -2147483649})");
  ExpectParseFailureForJson(
      "Uint32FieldTooLarge", REQUIRED,
      R"({"optionalUint32": 4294967296})");
  ExpectParseFailureForJson(
      "Int64FieldTooLarge", REQUIRED,
      R"({"optionalInt64": "9223372036854775808"})");
  ExpectParseFailureForJson(
      "Int64FieldTooSmall", REQUIRED,
      R"({"optionalInt64": "-9223372036854775809"})");
  ExpectParseFailureForJson(
      "Uint64FieldTooLarge", REQUIRED,
      R"({"optionalUint64": "18446744073709551616"})");
  // Parser reject non-integer numeric values as well.
  ExpectParseFailureForJson(
      "Int32FieldNotInteger", REQUIRED,
      R"({"optionalInt32": 0.5})");
  ExpectParseFailureForJson(
      "Uint32FieldNotInteger", REQUIRED,
      R"({"optionalUint32": 0.5})");
  ExpectParseFailureForJson(
      "Int64FieldNotInteger", REQUIRED,
      R"({"optionalInt64": "0.5"})");
  ExpectParseFailureForJson(
      "Uint64FieldNotInteger", REQUIRED,
      R"({"optionalUint64": "0.5"})");

  // Integers but represented as float values are accepted.
  RunValidJsonTest(
      "Int32FieldFloatTrailingZero", REQUIRED,
      R"({"optionalInt32": 100000.000})",
      "optional_int32: 100000");
  RunValidJsonTest(
      "Int32FieldExponentialFormat", REQUIRED,
      R"({"optionalInt32": 1e5})",
      "optional_int32: 100000");
  RunValidJsonTest(
      "Int32FieldMaxFloatValue", REQUIRED,
      R"({"optionalInt32": 2.147483647e9})",
      "optional_int32: 2147483647");
  RunValidJsonTest(
      "Int32FieldMinFloatValue", REQUIRED,
      R"({"optionalInt32": -2.147483648e9})",
      "optional_int32: -2147483648");
  RunValidJsonTest(
      "Uint32FieldMaxFloatValue", REQUIRED,
      R"({"optionalUint32": 4.294967295e9})",
      "optional_uint32: 4294967295");

  // Parser reject non-numeric values.
  ExpectParseFailureForJson(
      "Int32FieldNotNumber", REQUIRED,
      R"({"optionalInt32": "3x3"})");
  ExpectParseFailureForJson(
      "Uint32FieldNotNumber", REQUIRED,
      R"({"optionalUint32": "3x3"})");
  ExpectParseFailureForJson(
      "Int64FieldNotNumber", REQUIRED,
      R"({"optionalInt64": "3x3"})");
  ExpectParseFailureForJson(
      "Uint64FieldNotNumber", REQUIRED,
      R"({"optionalUint64": "3x3"})");
  // JSON does not allow "+" on numric values.
  ExpectParseFailureForJson(
      "Int32FieldPlusSign", REQUIRED,
      R"({"optionalInt32": +1})");
  // JSON doesn't allow leading 0s.
  ExpectParseFailureForJson(
      "Int32FieldLeadingZero", REQUIRED,
      R"({"optionalInt32": 01})");
  ExpectParseFailureForJson(
      "Int32FieldNegativeWithLeadingZero", REQUIRED,
      R"({"optionalInt32": -01})");
  // String values must follow the same syntax rule. Specifically leading
  // or trailing spaces are not allowed.
  ExpectParseFailureForJson(
      "Int32FieldLeadingSpace", REQUIRED,
      R"({"optionalInt32": " 1"})");
  ExpectParseFailureForJson(
      "Int32FieldTrailingSpace", REQUIRED,
      R"({"optionalInt32": "1 "})");

  // 64-bit values are serialized as strings.
  RunValidJsonTestWithValidator(
      "Int64FieldBeString", RECOMMENDED,
      R"({"optionalInt64": 1})",
      [](const Json::Value& value) {
        return value["optionalInt64"].type() == Json::stringValue &&
            value["optionalInt64"].asString() == "1";
      });
  RunValidJsonTestWithValidator(
      "Uint64FieldBeString", RECOMMENDED,
      R"({"optionalUint64": 1})",
      [](const Json::Value& value) {
        return value["optionalUint64"].type() == Json::stringValue &&
            value["optionalUint64"].asString() == "1";
      });

  // Bool fields.
  RunValidJsonTest(
      "BoolFieldTrue", REQUIRED,
      R"({"optionalBool":true})",
      "optional_bool: true");
  RunValidJsonTest(
      "BoolFieldFalse", REQUIRED,
      R"({"optionalBool":false})",
      "optional_bool: false");

  // Other forms are not allowed.
  ExpectParseFailureForJson(
      "BoolFieldIntegerZero", RECOMMENDED,
      R"({"optionalBool":0})");
  ExpectParseFailureForJson(
      "BoolFieldIntegerOne", RECOMMENDED,
      R"({"optionalBool":1})");
  ExpectParseFailureForJson(
      "BoolFieldCamelCaseTrue", RECOMMENDED,
      R"({"optionalBool":True})");
  ExpectParseFailureForJson(
      "BoolFieldCamelCaseFalse", RECOMMENDED,
      R"({"optionalBool":False})");
  ExpectParseFailureForJson(
      "BoolFieldAllCapitalTrue", RECOMMENDED,
      R"({"optionalBool":TRUE})");
  ExpectParseFailureForJson(
      "BoolFieldAllCapitalFalse", RECOMMENDED,
      R"({"optionalBool":FALSE})");
  ExpectParseFailureForJson(
      "BoolFieldDoubleQuotedTrue", RECOMMENDED,
      R"({"optionalBool":"true"})");
  ExpectParseFailureForJson(
      "BoolFieldDoubleQuotedFalse", RECOMMENDED,
      R"({"optionalBool":"false"})");

  // Float fields.
  RunValidJsonTest(
      "FloatFieldMinPositiveValue", REQUIRED,
      R"({"optionalFloat": 1.175494e-38})",
      "optional_float: 1.175494e-38");
  RunValidJsonTest(
      "FloatFieldMaxNegativeValue", REQUIRED,
      R"({"optionalFloat": -1.175494e-38})",
      "optional_float: -1.175494e-38");
  RunValidJsonTest(
      "FloatFieldMaxPositiveValue", REQUIRED,
      R"({"optionalFloat": 3.402823e+38})",
      "optional_float: 3.402823e+38");
  RunValidJsonTest(
      "FloatFieldMinNegativeValue", REQUIRED,
      R"({"optionalFloat": 3.402823e+38})",
      "optional_float: 3.402823e+38");
  // Values can be quoted.
  RunValidJsonTest(
      "FloatFieldQuotedValue", REQUIRED,
      R"({"optionalFloat": "1"})",
      "optional_float: 1");
  // Special values.
  RunValidJsonTest(
      "FloatFieldNan", REQUIRED,
      R"({"optionalFloat": "NaN"})",
      "optional_float: nan");
  RunValidJsonTest(
      "FloatFieldInfinity", REQUIRED,
      R"({"optionalFloat": "Infinity"})",
      "optional_float: inf");
  RunValidJsonTest(
      "FloatFieldNegativeInfinity", REQUIRED,
      R"({"optionalFloat": "-Infinity"})",
      "optional_float: -inf");
  // Non-cannonical Nan will be correctly normalized.
  {
    TestAllTypesProto3 message;
    // IEEE floating-point standard 32-bit quiet NaN:
    //   0111 1111 1xxx xxxx xxxx xxxx xxxx xxxx
    message.set_optional_float(
        WireFormatLite::DecodeFloat(0x7FA12345));
    RunValidJsonTestWithProtobufInput(
        "FloatFieldNormalizeQuietNan", REQUIRED, message,
        "optional_float: nan");
    // IEEE floating-point standard 64-bit signaling NaN:
    //   1111 1111 1xxx xxxx xxxx xxxx xxxx xxxx
    message.set_optional_float(
        WireFormatLite::DecodeFloat(0xFFB54321));
    RunValidJsonTestWithProtobufInput(
        "FloatFieldNormalizeSignalingNan", REQUIRED, message,
        "optional_float: nan");
  }

  // Special values must be quoted.
  ExpectParseFailureForJson(
      "FloatFieldNanNotQuoted", RECOMMENDED,
      R"({"optionalFloat": NaN})");
  ExpectParseFailureForJson(
      "FloatFieldInfinityNotQuoted", RECOMMENDED,
      R"({"optionalFloat": Infinity})");
  ExpectParseFailureForJson(
      "FloatFieldNegativeInfinityNotQuoted", RECOMMENDED,
      R"({"optionalFloat": -Infinity})");
  // Parsers should reject out-of-bound values.
  ExpectParseFailureForJson(
      "FloatFieldTooSmall", REQUIRED,
      R"({"optionalFloat": -3.502823e+38})");
  ExpectParseFailureForJson(
      "FloatFieldTooLarge", REQUIRED,
      R"({"optionalFloat": 3.502823e+38})");

  // Double fields.
  RunValidJsonTest(
      "DoubleFieldMinPositiveValue", REQUIRED,
      R"({"optionalDouble": 2.22507e-308})",
      "optional_double: 2.22507e-308");
  RunValidJsonTest(
      "DoubleFieldMaxNegativeValue", REQUIRED,
      R"({"optionalDouble": -2.22507e-308})",
      "optional_double: -2.22507e-308");
  RunValidJsonTest(
      "DoubleFieldMaxPositiveValue", REQUIRED,
      R"({"optionalDouble": 1.79769e+308})",
      "optional_double: 1.79769e+308");
  RunValidJsonTest(
      "DoubleFieldMinNegativeValue", REQUIRED,
      R"({"optionalDouble": -1.79769e+308})",
      "optional_double: -1.79769e+308");
  // Values can be quoted.
  RunValidJsonTest(
      "DoubleFieldQuotedValue", REQUIRED,
      R"({"optionalDouble": "1"})",
      "optional_double: 1");
  // Speical values.
  RunValidJsonTest(
      "DoubleFieldNan", REQUIRED,
      R"({"optionalDouble": "NaN"})",
      "optional_double: nan");
  RunValidJsonTest(
      "DoubleFieldInfinity", REQUIRED,
      R"({"optionalDouble": "Infinity"})",
      "optional_double: inf");
  RunValidJsonTest(
      "DoubleFieldNegativeInfinity", REQUIRED,
      R"({"optionalDouble": "-Infinity"})",
      "optional_double: -inf");
  // Non-cannonical Nan will be correctly normalized.
  {
    TestAllTypesProto3 message;
    message.set_optional_double(
        WireFormatLite::DecodeDouble(0x7FFA123456789ABCLL));
    RunValidJsonTestWithProtobufInput(
        "DoubleFieldNormalizeQuietNan", REQUIRED, message,
        "optional_double: nan");
    message.set_optional_double(
        WireFormatLite::DecodeDouble(0xFFFBCBA987654321LL));
    RunValidJsonTestWithProtobufInput(
        "DoubleFieldNormalizeSignalingNan", REQUIRED, message,
        "optional_double: nan");
  }

  // Special values must be quoted.
  ExpectParseFailureForJson(
      "DoubleFieldNanNotQuoted", RECOMMENDED,
      R"({"optionalDouble": NaN})");
  ExpectParseFailureForJson(
      "DoubleFieldInfinityNotQuoted", RECOMMENDED,
      R"({"optionalDouble": Infinity})");
  ExpectParseFailureForJson(
      "DoubleFieldNegativeInfinityNotQuoted", RECOMMENDED,
      R"({"optionalDouble": -Infinity})");

  // Parsers should reject out-of-bound values.
  ExpectParseFailureForJson(
      "DoubleFieldTooSmall", REQUIRED,
      R"({"optionalDouble": -1.89769e+308})");
  ExpectParseFailureForJson(
      "DoubleFieldTooLarge", REQUIRED,
      R"({"optionalDouble": +1.89769e+308})");

  // Enum fields.
  RunValidJsonTest(
      "EnumField", REQUIRED,
      R"({"optionalNestedEnum": "FOO"})",
      "optional_nested_enum: FOO");
  // Enum fields with alias
  RunValidJsonTest(
      "EnumFieldWithAlias", REQUIRED,
      R"({"optionalAliasedEnum": "ALIAS_BAZ"})",
      "optional_aliased_enum: ALIAS_BAZ");
  RunValidJsonTest(
      "EnumFieldWithAliasUseAlias", REQUIRED,
      R"({"optionalAliasedEnum": "QUX"})",
      "optional_aliased_enum: ALIAS_BAZ");
  RunValidJsonTest(
      "EnumFieldWithAliasLowerCase", REQUIRED,
      R"({"optionalAliasedEnum": "qux"})",
      "optional_aliased_enum: ALIAS_BAZ");
  RunValidJsonTest(
      "EnumFieldWithAliasDifferentCase", REQUIRED,
      R"({"optionalAliasedEnum": "bAz"})",
      "optional_aliased_enum: ALIAS_BAZ");
  // Enum values must be represented as strings.
  ExpectParseFailureForJson(
      "EnumFieldNotQuoted", REQUIRED,
      R"({"optionalNestedEnum": FOO})");
  // Numeric values are allowed.
  RunValidJsonTest(
      "EnumFieldNumericValueZero", REQUIRED,
      R"({"optionalNestedEnum": 0})",
      "optional_nested_enum: FOO");
  RunValidJsonTest(
      "EnumFieldNumericValueNonZero", REQUIRED,
      R"({"optionalNestedEnum": 1})",
      "optional_nested_enum: BAR");
  // Unknown enum values are represented as numeric values.
  RunValidJsonTestWithValidator(
      "EnumFieldUnknownValue", REQUIRED,
      R"({"optionalNestedEnum": 123})",
      [](const Json::Value& value) {
        return value["optionalNestedEnum"].type() == Json::intValue &&
            value["optionalNestedEnum"].asInt() == 123;
      });

  // String fields.
  RunValidJsonTest(
      "StringField", REQUIRED,
      R"({"optionalString": "Hello world!"})",
      "optional_string: \"Hello world!\"");
  RunValidJsonTest(
      "StringFieldUnicode", REQUIRED,
      // Google in Chinese.
      R"({"optionalString": "谷歌"})",
      R"(optional_string: "谷歌")");
  RunValidJsonTest(
      "StringFieldEscape", REQUIRED,
      R"({"optionalString": "\"\\\/\b\f\n\r\t"})",
      R"(optional_string: "\"\\/\b\f\n\r\t")");
  RunValidJsonTest(
      "StringFieldUnicodeEscape", REQUIRED,
      R"({"optionalString": "\u8C37\u6B4C"})",
      R"(optional_string: "谷歌")");
  RunValidJsonTest(
      "StringFieldUnicodeEscapeWithLowercaseHexLetters", REQUIRED,
      R"({"optionalString": "\u8c37\u6b4c"})",
      R"(optional_string: "谷歌")");
  RunValidJsonTest(
      "StringFieldSurrogatePair", REQUIRED,
      // The character is an emoji: grinning face with smiling eyes. 😁
      R"({"optionalString": "\uD83D\uDE01"})",
      R"(optional_string: "\xF0\x9F\x98\x81")");

  // Unicode escapes must start with "\u" (lowercase u).
  ExpectParseFailureForJson(
      "StringFieldUppercaseEscapeLetter", RECOMMENDED,
      R"({"optionalString": "\U8C37\U6b4C"})");
  ExpectParseFailureForJson(
      "StringFieldInvalidEscape", RECOMMENDED,
      R"({"optionalString": "\uXXXX\u6B4C"})");
  ExpectParseFailureForJson(
      "StringFieldUnterminatedEscape", RECOMMENDED,
      R"({"optionalString": "\u8C3"})");
  ExpectParseFailureForJson(
      "StringFieldUnpairedHighSurrogate", RECOMMENDED,
      R"({"optionalString": "\uD800"})");
  ExpectParseFailureForJson(
      "StringFieldUnpairedLowSurrogate", RECOMMENDED,
      R"({"optionalString": "\uDC00"})");
  ExpectParseFailureForJson(
      "StringFieldSurrogateInWrongOrder", RECOMMENDED,
      R"({"optionalString": "\uDE01\uD83D"})");
  ExpectParseFailureForJson(
      "StringFieldNotAString", REQUIRED,
      R"({"optionalString": 12345})");

  // Bytes fields.
  RunValidJsonTest(
      "BytesField", REQUIRED,
      R"({"optionalBytes": "AQI="})",
      R"(optional_bytes: "\x01\x02")");
  RunValidJsonTest(
      "BytesFieldBase64Url", RECOMMENDED,
      R"({"optionalBytes": "-_"})",
      R"(optional_bytes: "\xfb")");

  // Message fields.
  RunValidJsonTest(
      "MessageField", REQUIRED,
      R"({"optionalNestedMessage": {"a": 1234}})",
      "optional_nested_message: {a: 1234}");

  // Oneof fields.
  ExpectParseFailureForJson(
      "OneofFieldDuplicate", REQUIRED,
      R"({"oneofUint32": 1, "oneofString": "test"})");
  // Ensure zero values for oneof make it out/backs.
  TestAllTypesProto3 messageProto3;
  TestAllTypesProto2 messageProto2;
  TestOneofMessage(messageProto3, true);
  TestOneofMessage(messageProto2, false);
  RunValidJsonTest(
      "OneofZeroUint32", RECOMMENDED,
      R"({"oneofUint32": 0})", "oneof_uint32: 0");
  RunValidJsonTest(
      "OneofZeroMessage", RECOMMENDED,
      R"({"oneofNestedMessage": {}})", "oneof_nested_message: {}");
  RunValidJsonTest(
      "OneofZeroString", RECOMMENDED,
      R"({"oneofString": ""})", "oneof_string: \"\"");
  RunValidJsonTest(
      "OneofZeroBytes", RECOMMENDED,
      R"({"oneofBytes": ""})", "oneof_bytes: \"\"");
  RunValidJsonTest(
      "OneofZeroBool", RECOMMENDED,
      R"({"oneofBool": false})", "oneof_bool: false");
  RunValidJsonTest(
      "OneofZeroUint64", RECOMMENDED,
      R"({"oneofUint64": 0})", "oneof_uint64: 0");
  RunValidJsonTest(
      "OneofZeroFloat", RECOMMENDED,
      R"({"oneofFloat": 0.0})", "oneof_float: 0");
  RunValidJsonTest(
      "OneofZeroDouble", RECOMMENDED,
      R"({"oneofDouble": 0.0})", "oneof_double: 0");
  RunValidJsonTest(
      "OneofZeroEnum", RECOMMENDED,
      R"({"oneofEnum":"FOO"})", "oneof_enum: FOO");

  // Repeated fields.
  RunValidJsonTest(
      "PrimitiveRepeatedField", REQUIRED,
      R"({"repeatedInt32": [1, 2, 3, 4]})",
      "repeated_int32: [1, 2, 3, 4]");
  RunValidJsonTest(
      "EnumRepeatedField", REQUIRED,
      R"({"repeatedNestedEnum": ["FOO", "BAR", "BAZ"]})",
      "repeated_nested_enum: [FOO, BAR, BAZ]");
  RunValidJsonTest(
      "StringRepeatedField", REQUIRED,
      R"({"repeatedString": ["Hello", "world"]})",
      R"(repeated_string: ["Hello", "world"])");
  RunValidJsonTest(
      "BytesRepeatedField", REQUIRED,
      R"({"repeatedBytes": ["AAEC", "AQI="]})",
      R"(repeated_bytes: ["\x00\x01\x02", "\x01\x02"])");
  RunValidJsonTest(
      "MessageRepeatedField", REQUIRED,
      R"({"repeatedNestedMessage": [{"a": 1234}, {"a": 5678}]})",
      "repeated_nested_message: {a: 1234}"
      "repeated_nested_message: {a: 5678}");

  // Repeated field elements are of incorrect type.
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingIntegersGotBool", REQUIRED,
      R"({"repeatedInt32": [1, false, 3, 4]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingIntegersGotString", REQUIRED,
      R"({"repeatedInt32": [1, 2, "name", 4]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingIntegersGotMessage", REQUIRED,
      R"({"repeatedInt32": [1, 2, 3, {"a": 4}]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingStringsGotInt", REQUIRED,
      R"({"repeatedString": ["1", 2, "3", "4"]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingStringsGotBool", REQUIRED,
      R"({"repeatedString": ["1", "2", false, "4"]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingStringsGotMessage", REQUIRED,
      R"({"repeatedString": ["1", 2, "3", {"a": 4}]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingMessagesGotInt", REQUIRED,
      R"({"repeatedNestedMessage": [{"a": 1}, 2]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingMessagesGotBool", REQUIRED,
      R"({"repeatedNestedMessage": [{"a": 1}, false]})");
  ExpectParseFailureForJson(
      "RepeatedFieldWrongElementTypeExpectingMessagesGotString", REQUIRED,
      R"({"repeatedNestedMessage": [{"a": 1}, "2"]})");
  // Trailing comma in the repeated field is not allowed.
  ExpectParseFailureForJson(
      "RepeatedFieldTrailingComma", RECOMMENDED,
      R"({"repeatedInt32": [1, 2, 3, 4,]})");
  ExpectParseFailureForJson(
      "RepeatedFieldTrailingCommaWithSpace", RECOMMENDED,
      "{\"repeatedInt32\": [1, 2, 3, 4 ,]}");
  ExpectParseFailureForJson(
      "RepeatedFieldTrailingCommaWithSpaceCommaSpace", RECOMMENDED,
      "{\"repeatedInt32\": [1, 2, 3, 4 , ]}");
  ExpectParseFailureForJson(
      "RepeatedFieldTrailingCommaWithNewlines", RECOMMENDED,
      "{\"repeatedInt32\": [\n  1,\n  2,\n  3,\n  4,\n]}");

  // Map fields.
  RunValidJsonTest(
      "Int32MapField", REQUIRED,
      R"({"mapInt32Int32": {"1": 2, "3": 4}})",
      "map_int32_int32: {key: 1 value: 2}"
      "map_int32_int32: {key: 3 value: 4}");
  ExpectParseFailureForJson(
      "Int32MapFieldKeyNotQuoted", RECOMMENDED,
      R"({"mapInt32Int32": {1: 2, 3: 4}})");
  RunValidJsonTest(
      "Uint32MapField", REQUIRED,
      R"({"mapUint32Uint32": {"1": 2, "3": 4}})",
      "map_uint32_uint32: {key: 1 value: 2}"
      "map_uint32_uint32: {key: 3 value: 4}");
  ExpectParseFailureForJson(
      "Uint32MapFieldKeyNotQuoted", RECOMMENDED,
      R"({"mapUint32Uint32": {1: 2, 3: 4}})");
  RunValidJsonTest(
      "Int64MapField", REQUIRED,
      R"({"mapInt64Int64": {"1": 2, "3": 4}})",
      "map_int64_int64: {key: 1 value: 2}"
      "map_int64_int64: {key: 3 value: 4}");
  ExpectParseFailureForJson(
      "Int64MapFieldKeyNotQuoted", RECOMMENDED,
      R"({"mapInt64Int64": {1: 2, 3: 4}})");
  RunValidJsonTest(
      "Uint64MapField", REQUIRED,
      R"({"mapUint64Uint64": {"1": 2, "3": 4}})",
      "map_uint64_uint64: {key: 1 value: 2}"
      "map_uint64_uint64: {key: 3 value: 4}");
  ExpectParseFailureForJson(
      "Uint64MapFieldKeyNotQuoted", RECOMMENDED,
      R"({"mapUint64Uint64": {1: 2, 3: 4}})");
  RunValidJsonTest(
      "BoolMapField", REQUIRED,
      R"({"mapBoolBool": {"true": true, "false": false}})",
      "map_bool_bool: {key: true value: true}"
      "map_bool_bool: {key: false value: false}");
  ExpectParseFailureForJson(
      "BoolMapFieldKeyNotQuoted", RECOMMENDED,
      R"({"mapBoolBool": {true: true, false: false}})");
  RunValidJsonTest(
      "MessageMapField", REQUIRED,
      R"({
        "mapStringNestedMessage": {
          "hello": {"a": 1234},
          "world": {"a": 5678}
        }
      })",
      R"(
        map_string_nested_message: {
          key: "hello"
          value: {a: 1234}
        }
        map_string_nested_message: {
          key: "world"
          value: {a: 5678}
        }
      )");
  // Since Map keys are represented as JSON strings, escaping should be allowed.
  RunValidJsonTest(
      "Int32MapEscapedKey", REQUIRED,
      R"({"mapInt32Int32": {"\u0031": 2}})",
      "map_int32_int32: {key: 1 value: 2}");
  RunValidJsonTest(
      "Int64MapEscapedKey", REQUIRED,
      R"({"mapInt64Int64": {"\u0031": 2}})",
      "map_int64_int64: {key: 1 value: 2}");
  RunValidJsonTest(
      "BoolMapEscapedKey", REQUIRED,
      R"({"mapBoolBool": {"tr\u0075e": true}})",
      "map_bool_bool: {key: true value: true}");

  // "null" is accepted for all fields types.
  RunValidJsonTest(
      "AllFieldAcceptNull", REQUIRED,
      R"({
        "optionalInt32": null,
        "optionalInt64": null,
        "optionalUint32": null,
        "optionalUint64": null,
        "optionalSint32": null,
        "optionalSint64": null,
        "optionalFixed32": null,
        "optionalFixed64": null,
        "optionalSfixed32": null,
        "optionalSfixed64": null,
        "optionalFloat": null,
        "optionalDouble": null,
        "optionalBool": null,
        "optionalString": null,
        "optionalBytes": null,
        "optionalNestedEnum": null,
        "optionalNestedMessage": null,
        "repeatedInt32": null,
        "repeatedInt64": null,
        "repeatedUint32": null,
        "repeatedUint64": null,
        "repeatedSint32": null,
        "repeatedSint64": null,
        "repeatedFixed32": null,
        "repeatedFixed64": null,
        "repeatedSfixed32": null,
        "repeatedSfixed64": null,
        "repeatedFloat": null,
        "repeatedDouble": null,
        "repeatedBool": null,
        "repeatedString": null,
        "repeatedBytes": null,
        "repeatedNestedEnum": null,
        "repeatedNestedMessage": null,
        "mapInt32Int32": null,
        "mapBoolBool": null,
        "mapStringNestedMessage": null
      })",
      "");

  // Repeated field elements cannot be null.
  ExpectParseFailureForJson(
      "RepeatedFieldPrimitiveElementIsNull", RECOMMENDED,
      R"({"repeatedInt32": [1, null, 2]})");
  ExpectParseFailureForJson(
      "RepeatedFieldMessageElementIsNull", RECOMMENDED,
      R"({"repeatedNestedMessage": [{"a":1}, null, {"a":2}]})");
  // Map field keys cannot be null.
  ExpectParseFailureForJson(
      "MapFieldKeyIsNull", RECOMMENDED,
      R"({"mapInt32Int32": {null: 1}})");
  // Map field values cannot be null.
  ExpectParseFailureForJson(
      "MapFieldValueIsNull", RECOMMENDED,
      R"({"mapInt32Int32": {"0": null}})");

  // http://www.rfc-editor.org/rfc/rfc7159.txt says strings have to use double
  // quotes.
  ExpectParseFailureForJson(
      "StringFieldSingleQuoteKey", RECOMMENDED,
      R"({'optionalString': "Hello world!"})");
  ExpectParseFailureForJson(
      "StringFieldSingleQuoteValue", RECOMMENDED,
      R"({"optionalString": 'Hello world!'})");
  ExpectParseFailureForJson(
      "StringFieldSingleQuoteBoth", RECOMMENDED,
      R"({'optionalString': 'Hello world!'})");

  // Unknown fields.
  {
    TestAllTypesProto3 messageProto3;
    TestAllTypesProto2 messageProto2;
    //TODO(yilunchong): update this behavior when unknown field's behavior
    // changed in open source. Also delete
    // Required.Proto3.ProtobufInput.UnknownVarint.ProtobufOutput
    // from failure list of python_cpp python java
    TestUnknownMessage(messageProto3, true);
    TestUnknownMessage(messageProto2, false);
  }

  // Wrapper types.
  RunValidJsonTest(
      "OptionalBoolWrapper", REQUIRED,
      R"({"optionalBoolWrapper": false})",
      "optional_bool_wrapper: {value: false}");
  RunValidJsonTest(
      "OptionalInt32Wrapper", REQUIRED,
      R"({"optionalInt32Wrapper": 0})",
      "optional_int32_wrapper: {value: 0}");
  RunValidJsonTest(
      "OptionalUint32Wrapper", REQUIRED,
      R"({"optionalUint32Wrapper": 0})",
      "optional_uint32_wrapper: {value: 0}");
  RunValidJsonTest(
      "OptionalInt64Wrapper", REQUIRED,
      R"({"optionalInt64Wrapper": 0})",
      "optional_int64_wrapper: {value: 0}");
  RunValidJsonTest(
      "OptionalUint64Wrapper", REQUIRED,
      R"({"optionalUint64Wrapper": 0})",
      "optional_uint64_wrapper: {value: 0}");
  RunValidJsonTest(
      "OptionalFloatWrapper", REQUIRED,
      R"({"optionalFloatWrapper": 0})",
      "optional_float_wrapper: {value: 0}");
  RunValidJsonTest(
      "OptionalDoubleWrapper", REQUIRED,
      R"({"optionalDoubleWrapper": 0})",
      "optional_double_wrapper: {value: 0}");
  RunValidJsonTest(
      "OptionalStringWrapper", REQUIRED,
      R"({"optionalStringWrapper": ""})",
      R"(optional_string_wrapper: {value: ""})");
  RunValidJsonTest(
      "OptionalBytesWrapper", REQUIRED,
      R"({"optionalBytesWrapper": ""})",
      R"(optional_bytes_wrapper: {value: ""})");
  RunValidJsonTest(
      "OptionalWrapperTypesWithNonDefaultValue", REQUIRED,
      R"({
        "optionalBoolWrapper": true,
        "optionalInt32Wrapper": 1,
        "optionalUint32Wrapper": 1,
        "optionalInt64Wrapper": "1",
        "optionalUint64Wrapper": "1",
        "optionalFloatWrapper": 1,
        "optionalDoubleWrapper": 1,
        "optionalStringWrapper": "1",
        "optionalBytesWrapper": "AQI="
      })",
      R"(
        optional_bool_wrapper: {value: true}
        optional_int32_wrapper: {value: 1}
        optional_uint32_wrapper: {value: 1}
        optional_int64_wrapper: {value: 1}
        optional_uint64_wrapper: {value: 1}
        optional_float_wrapper: {value: 1}
        optional_double_wrapper: {value: 1}
        optional_string_wrapper: {value: "1"}
        optional_bytes_wrapper: {value: "\x01\x02"}
      )");
  RunValidJsonTest(
      "RepeatedBoolWrapper", REQUIRED,
      R"({"repeatedBoolWrapper": [true, false]})",
      "repeated_bool_wrapper: {value: true}"
      "repeated_bool_wrapper: {value: false}");
  RunValidJsonTest(
      "RepeatedInt32Wrapper", REQUIRED,
      R"({"repeatedInt32Wrapper": [0, 1]})",
      "repeated_int32_wrapper: {value: 0}"
      "repeated_int32_wrapper: {value: 1}");
  RunValidJsonTest(
      "RepeatedUint32Wrapper", REQUIRED,
      R"({"repeatedUint32Wrapper": [0, 1]})",
      "repeated_uint32_wrapper: {value: 0}"
      "repeated_uint32_wrapper: {value: 1}");
  RunValidJsonTest(
      "RepeatedInt64Wrapper", REQUIRED,
      R"({"repeatedInt64Wrapper": [0, 1]})",
      "repeated_int64_wrapper: {value: 0}"
      "repeated_int64_wrapper: {value: 1}");
  RunValidJsonTest(
      "RepeatedUint64Wrapper", REQUIRED,
      R"({"repeatedUint64Wrapper": [0, 1]})",
      "repeated_uint64_wrapper: {value: 0}"
      "repeated_uint64_wrapper: {value: 1}");
  RunValidJsonTest(
      "RepeatedFloatWrapper", REQUIRED,
      R"({"repeatedFloatWrapper": [0, 1]})",
      "repeated_float_wrapper: {value: 0}"
      "repeated_float_wrapper: {value: 1}");
  RunValidJsonTest(
      "RepeatedDoubleWrapper", REQUIRED,
      R"({"repeatedDoubleWrapper": [0, 1]})",
      "repeated_double_wrapper: {value: 0}"
      "repeated_double_wrapper: {value: 1}");
  RunValidJsonTest(
      "RepeatedStringWrapper", REQUIRED,
      R"({"repeatedStringWrapper": ["", "AQI="]})",
      R"(
        repeated_string_wrapper: {value: ""}
        repeated_string_wrapper: {value: "AQI="}
      )");
  RunValidJsonTest(
      "RepeatedBytesWrapper", REQUIRED,
      R"({"repeatedBytesWrapper": ["", "AQI="]})",
      R"(
        repeated_bytes_wrapper: {value: ""}
        repeated_bytes_wrapper: {value: "\x01\x02"}
      )");
  RunValidJsonTest(
      "WrapperTypesWithNullValue", REQUIRED,
      R"({
        "optionalBoolWrapper": null,
        "optionalInt32Wrapper": null,
        "optionalUint32Wrapper": null,
        "optionalInt64Wrapper": null,
        "optionalUint64Wrapper": null,
        "optionalFloatWrapper": null,
        "optionalDoubleWrapper": null,
        "optionalStringWrapper": null,
        "optionalBytesWrapper": null,
        "repeatedBoolWrapper": null,
        "repeatedInt32Wrapper": null,
        "repeatedUint32Wrapper": null,
        "repeatedInt64Wrapper": null,
        "repeatedUint64Wrapper": null,
        "repeatedFloatWrapper": null,
        "repeatedDoubleWrapper": null,
        "repeatedStringWrapper": null,
        "repeatedBytesWrapper": null
      })",
      "");

  // Duration
  RunValidJsonTest(
      "DurationMinValue", REQUIRED,
      R"({"optionalDuration": "-315576000000.999999999s"})",
      "optional_duration: {seconds: -315576000000 nanos: -999999999}");
  RunValidJsonTest(
      "DurationMaxValue", REQUIRED,
      R"({"optionalDuration": "315576000000.999999999s"})",
      "optional_duration: {seconds: 315576000000 nanos: 999999999}");
  RunValidJsonTest(
      "DurationRepeatedValue", REQUIRED,
      R"({"repeatedDuration": ["1.5s", "-1.5s"]})",
      "repeated_duration: {seconds: 1 nanos: 500000000}"
      "repeated_duration: {seconds: -1 nanos: -500000000}");
  RunValidJsonTest(
      "DurationNull", REQUIRED,
      R"({"optionalDuration": null})",
      "");

  ExpectParseFailureForJson(
      "DurationMissingS", REQUIRED,
      R"({"optionalDuration": "1"})");
  ExpectParseFailureForJson(
      "DurationJsonInputTooSmall", REQUIRED,
      R"({"optionalDuration": "-315576000001.000000000s"})");
  ExpectParseFailureForJson(
      "DurationJsonInputTooLarge", REQUIRED,
      R"({"optionalDuration": "315576000001.000000000s"})");
  ExpectSerializeFailureForJson(
      "DurationProtoInputTooSmall", REQUIRED,
      "optional_duration: {seconds: -315576000001 nanos: 0}");
  ExpectSerializeFailureForJson(
      "DurationProtoInputTooLarge", REQUIRED,
      "optional_duration: {seconds: 315576000001 nanos: 0}");

  RunValidJsonTestWithValidator(
      "DurationHasZeroFractionalDigit", RECOMMENDED,
      R"({"optionalDuration": "1.000000000s"})",
      [](const Json::Value& value) {
        return value["optionalDuration"].asString() == "1s";
      });
  RunValidJsonTestWithValidator(
      "DurationHas3FractionalDigits", RECOMMENDED,
      R"({"optionalDuration": "1.010000000s"})",
      [](const Json::Value& value) {
        return value["optionalDuration"].asString() == "1.010s";
      });
  RunValidJsonTestWithValidator(
      "DurationHas6FractionalDigits", RECOMMENDED,
      R"({"optionalDuration": "1.000010000s"})",
      [](const Json::Value& value) {
        return value["optionalDuration"].asString() == "1.000010s";
      });
  RunValidJsonTestWithValidator(
      "DurationHas9FractionalDigits", RECOMMENDED,
      R"({"optionalDuration": "1.000000010s"})",
      [](const Json::Value& value) {
        return value["optionalDuration"].asString() == "1.000000010s";
      });

  // Timestamp
  RunValidJsonTest(
      "TimestampMinValue", REQUIRED,
      R"({"optionalTimestamp": "0001-01-01T00:00:00Z"})",
      "optional_timestamp: {seconds: -62135596800}");
  RunValidJsonTest(
      "TimestampMaxValue", REQUIRED,
      R"({"optionalTimestamp": "9999-12-31T23:59:59.999999999Z"})",
      "optional_timestamp: {seconds: 253402300799 nanos: 999999999}");
  RunValidJsonTest(
      "TimestampRepeatedValue", REQUIRED,
      R"({
        "repeatedTimestamp": [
          "0001-01-01T00:00:00Z",
          "9999-12-31T23:59:59.999999999Z"
        ]
      })",
      "repeated_timestamp: {seconds: -62135596800}"
      "repeated_timestamp: {seconds: 253402300799 nanos: 999999999}");
  RunValidJsonTest(
      "TimestampWithPositiveOffset", REQUIRED,
      R"({"optionalTimestamp": "1970-01-01T08:00:01+08:00"})",
      "optional_timestamp: {seconds: 1}");
  RunValidJsonTest(
      "TimestampWithNegativeOffset", REQUIRED,
      R"({"optionalTimestamp": "1969-12-31T16:00:01-08:00"})",
      "optional_timestamp: {seconds: 1}");
  RunValidJsonTest(
      "TimestampNull", REQUIRED,
      R"({"optionalTimestamp": null})",
      "");

  ExpectParseFailureForJson(
      "TimestampJsonInputTooSmall", REQUIRED,
      R"({"optionalTimestamp": "0000-01-01T00:00:00Z"})");
  ExpectParseFailureForJson(
      "TimestampJsonInputTooLarge", REQUIRED,
      R"({"optionalTimestamp": "10000-01-01T00:00:00Z"})");
  ExpectParseFailureForJson(
      "TimestampJsonInputMissingZ", REQUIRED,
      R"({"optionalTimestamp": "0001-01-01T00:00:00"})");
  ExpectParseFailureForJson(
      "TimestampJsonInputMissingT", REQUIRED,
      R"({"optionalTimestamp": "0001-01-01 00:00:00Z"})");
  ExpectParseFailureForJson(
      "TimestampJsonInputLowercaseZ", REQUIRED,
      R"({"optionalTimestamp": "0001-01-01T00:00:00z"})");
  ExpectParseFailureForJson(
      "TimestampJsonInputLowercaseT", REQUIRED,
      R"({"optionalTimestamp": "0001-01-01t00:00:00Z"})");
  ExpectSerializeFailureForJson(
      "TimestampProtoInputTooSmall", REQUIRED,
      "optional_timestamp: {seconds: -62135596801}");
  ExpectSerializeFailureForJson(
      "TimestampProtoInputTooLarge", REQUIRED,
      "optional_timestamp: {seconds: 253402300800}");
  RunValidJsonTestWithValidator(
      "TimestampZeroNormalized", RECOMMENDED,
      R"({"optionalTimestamp": "1969-12-31T16:00:00-08:00"})",
      [](const Json::Value& value) {
        return value["optionalTimestamp"].asString() ==
            "1970-01-01T00:00:00Z";
      });
  RunValidJsonTestWithValidator(
      "TimestampHasZeroFractionalDigit", RECOMMENDED,
      R"({"optionalTimestamp": "1970-01-01T00:00:00.000000000Z"})",
      [](const Json::Value& value) {
        return value["optionalTimestamp"].asString() ==
            "1970-01-01T00:00:00Z";
      });
  RunValidJsonTestWithValidator(
      "TimestampHas3FractionalDigits", RECOMMENDED,
      R"({"optionalTimestamp": "1970-01-01T00:00:00.010000000Z"})",
      [](const Json::Value& value) {
        return value["optionalTimestamp"].asString() ==
            "1970-01-01T00:00:00.010Z";
      });
  RunValidJsonTestWithValidator(
      "TimestampHas6FractionalDigits", RECOMMENDED,
      R"({"optionalTimestamp": "1970-01-01T00:00:00.000010000Z"})",
      [](const Json::Value& value) {
        return value["optionalTimestamp"].asString() ==
            "1970-01-01T00:00:00.000010Z";
      });
  RunValidJsonTestWithValidator(
      "TimestampHas9FractionalDigits", RECOMMENDED,
      R"({"optionalTimestamp": "1970-01-01T00:00:00.000000010Z"})",
      [](const Json::Value& value) {
        return value["optionalTimestamp"].asString() ==
            "1970-01-01T00:00:00.000000010Z";
      });

  // FieldMask
  RunValidJsonTest(
      "FieldMask", REQUIRED,
      R"({"optionalFieldMask": "foo,barBaz"})",
      R"(optional_field_mask: {paths: "foo" paths: "bar_baz"})");
  RunValidJsonTest(
      "EmptyFieldMask", REQUIRED,
      R"({"optionalFieldMask": ""})",
      R"(optional_field_mask: {})");
  ExpectParseFailureForJson(
      "FieldMaskInvalidCharacter", RECOMMENDED,
      R"({"optionalFieldMask": "foo,bar_bar"})");
  ExpectSerializeFailureForJson(
      "FieldMaskPathsDontRoundTrip", RECOMMENDED,
      R"(optional_field_mask: {paths: "fooBar"})");
  ExpectSerializeFailureForJson(
      "FieldMaskNumbersDontRoundTrip", RECOMMENDED,
      R"(optional_field_mask: {paths: "foo_3_bar"})");
  ExpectSerializeFailureForJson(
      "FieldMaskTooManyUnderscore", RECOMMENDED,
      R"(optional_field_mask: {paths: "foo__bar"})");

  // Struct
  RunValidJsonTest(
      "Struct", REQUIRED,
      R"({
        "optionalStruct": {
          "nullValue": null,
          "intValue": 1234,
          "boolValue": true,
          "doubleValue": 1234.5678,
          "stringValue": "Hello world!",
          "listValue": [1234, "5678"],
          "objectValue": {
            "value": 0
          }
        }
      })",
      R"(
        optional_struct: {
          fields: {
            key: "nullValue"
            value: {null_value: NULL_VALUE}
          }
          fields: {
            key: "intValue"
            value: {number_value: 1234}
          }
          fields: {
            key: "boolValue"
            value: {bool_value: true}
          }
          fields: {
            key: "doubleValue"
            value: {number_value: 1234.5678}
          }
          fields: {
            key: "stringValue"
            value: {string_value: "Hello world!"}
          }
          fields: {
            key: "listValue"
            value: {
              list_value: {
                values: {
                  number_value: 1234
                }
                values: {
                  string_value: "5678"
                }
              }
            }
          }
          fields: {
            key: "objectValue"
            value: {
              struct_value: {
                fields: {
                  key: "value"
                  value: {
                    number_value: 0
                  }
                }
              }
            }
          }
        }
      )");
  RunValidJsonTest(
      "StructWithEmptyListValue", REQUIRED,
      R"({
        "optionalStruct": {
          "listValue": []
        }
      })",
      R"(
        optional_struct: {
          fields: {
            key: "listValue"
            value: {
              list_value: {
              }
            }
          }
        }
      )");
  // Value
  RunValidJsonTest(
      "ValueAcceptInteger", REQUIRED,
      R"({"optionalValue": 1})",
      "optional_value: { number_value: 1}");
  RunValidJsonTest(
      "ValueAcceptFloat", REQUIRED,
      R"({"optionalValue": 1.5})",
      "optional_value: { number_value: 1.5}");
  RunValidJsonTest(
      "ValueAcceptBool", REQUIRED,
      R"({"optionalValue": false})",
      "optional_value: { bool_value: false}");
  RunValidJsonTest(
      "ValueAcceptNull", REQUIRED,
      R"({"optionalValue": null})",
      "optional_value: { null_value: NULL_VALUE}");
  RunValidJsonTest(
      "ValueAcceptString", REQUIRED,
      R"({"optionalValue": "hello"})",
      R"(optional_value: { string_value: "hello"})");
  RunValidJsonTest(
      "ValueAcceptList", REQUIRED,
      R"({"optionalValue": [0, "hello"]})",
      R"(
        optional_value: {
          list_value: {
            values: {
              number_value: 0
            }
            values: {
              string_value: "hello"
            }
          }
        }
      )");
  RunValidJsonTest(
      "ValueAcceptObject", REQUIRED,
      R"({"optionalValue": {"value": 1}})",
      R"(
        optional_value: {
          struct_value: {
            fields: {
              key: "value"
              value: {
                number_value: 1
              }
            }
          }
        }
      )");
  RunValidJsonTest(
      "RepeatedValue", REQUIRED,
      R"({
        "repeatedValue": [["a"]]
      })",
      R"(
        repeated_value: [
          {
            list_value: {
              values: [
                { string_value: "a"}
              ]
            }
          }
        ]
      )");
  RunValidJsonTest(
      "RepeatedListValue", REQUIRED,
      R"({
        "repeatedListValue": [["a"]]
      })",
      R"(
        repeated_list_value: [
          {
            values: [
              { string_value: "a"}
            ]
          }
        ]
      )");

  // Any
  RunValidJsonTest(
      "Any", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3",
          "optionalInt32": 12345
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3] {
            optional_int32: 12345
          }
        }
      )");
  RunValidJsonTest(
      "AnyNested", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Any",
          "value": {
            "@type": "type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3",
            "optionalInt32": 12345
          }
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Any] {
            [type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3] {
              optional_int32: 12345
            }
          }
        }
      )");
  // The special "@type" tag is not required to appear first.
  RunValidJsonTest(
      "AnyUnorderedTypeTag", REQUIRED,
      R"({
        "optionalAny": {
          "optionalInt32": 12345,
          "@type": "type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3"
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3] {
            optional_int32: 12345
          }
        }
      )");
  // Well-known types in Any.
  RunValidJsonTest(
      "AnyWithInt32ValueWrapper", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Int32Value",
          "value": 12345
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Int32Value] {
            value: 12345
          }
        }
      )");
  RunValidJsonTest(
      "AnyWithDuration", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Duration",
          "value": "1.5s"
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Duration] {
            seconds: 1
            nanos: 500000000
          }
        }
      )");
  RunValidJsonTest(
      "AnyWithTimestamp", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Timestamp",
          "value": "1970-01-01T00:00:00Z"
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Timestamp] {
            seconds: 0
            nanos: 0
          }
        }
      )");
  RunValidJsonTest(
      "AnyWithFieldMask", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.FieldMask",
          "value": "foo,barBaz"
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.FieldMask] {
            paths: ["foo", "bar_baz"]
          }
        }
      )");
  RunValidJsonTest(
      "AnyWithStruct", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Struct",
          "value": {
            "foo": 1
          }
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Struct] {
            fields: {
              key: "foo"
              value: {
                number_value: 1
              }
            }
          }
        }
      )");
  RunValidJsonTest(
      "AnyWithValueForJsonObject", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Value",
          "value": {
            "foo": 1
          }
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Value] {
            struct_value: {
              fields: {
                key: "foo"
                value: {
                  number_value: 1
                }
              }
            }
          }
        }
      )");
  RunValidJsonTest(
      "AnyWithValueForInteger", REQUIRED,
      R"({
        "optionalAny": {
          "@type": "type.googleapis.com/google.protobuf.Value",
          "value": 1
        }
      })",
      R"(
        optional_any: {
          [type.googleapis.com/google.protobuf.Value] {
            number_value: 1
          }
        }
      )");

  RunValidJsonIgnoreUnknownTest(
      "IgnoreUnknownJsonNumber", REQUIRED,
      R"({
        "unknown": 1
      })",
      "");
  RunValidJsonIgnoreUnknownTest(
      "IgnoreUnknownJsonString", REQUIRED,
      R"({
        "unknown": "a"
      })",
      "");
  RunValidJsonIgnoreUnknownTest(
      "IgnoreUnknownJsonTrue", REQUIRED,
      R"({
        "unknown": true
      })",
      "");
  RunValidJsonIgnoreUnknownTest(
      "IgnoreUnknownJsonFalse", REQUIRED,
      R"({
        "unknown": false
      })",
      "");
  RunValidJsonIgnoreUnknownTest(
      "IgnoreUnknownJsonNull", REQUIRED,
      R"({
        "unknown": null
      })",
      "");
  RunValidJsonIgnoreUnknownTest(
      "IgnoreUnknownJsonObject", REQUIRED,
      R"({
        "unknown": {"a": 1}
      })",
      "");

  ExpectParseFailureForJson("RejectTopLevelNull", REQUIRED, "null");
}

}  // namespace protobuf
}  // namespace google
