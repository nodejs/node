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

#include "text_format_conformance_suite.h"

#include "conformance_test.h"

#include <google/protobuf/any.pb.h>
#include <google/protobuf/test_messages_proto2.pb.h>
#include <google/protobuf/test_messages_proto3.pb.h>
#include <google/protobuf/text_format.h>

using conformance::ConformanceRequest;
using conformance::ConformanceResponse;
using conformance::WireFormat;
using google::protobuf::Message;
using google::protobuf::TextFormat;
using protobuf_test_messages::proto2::TestAllTypesProto2;
using protobuf_test_messages::proto2::UnknownToTestAllTypes;
using protobuf_test_messages::proto3::TestAllTypesProto3;
using std::string;

namespace google {
namespace protobuf {

TextFormatConformanceTestSuite::TextFormatConformanceTestSuite() {
  SetFailureListFlagName("--text_format_failure_list");
}

bool TextFormatConformanceTestSuite::ParseTextFormatResponse(
    const ConformanceResponse& response,
    const ConformanceRequestSetting& setting, Message* test_message) {
  TextFormat::Parser parser;
  const ConformanceRequest& request = setting.GetRequest();
  if (request.print_unknown_fields()) {
    parser.AllowFieldNumber(true);
  }
  if (!parser.ParseFromString(response.text_payload(), test_message)) {
    GOOGLE_LOG(ERROR) << "INTERNAL ERROR: internal text->protobuf transcode "
                      << "yielded unparseable proto. Text payload: "
                      << response.text_payload();
    return false;
  }

  return true;
}

bool TextFormatConformanceTestSuite::ParseResponse(
    const ConformanceResponse& response,
    const ConformanceRequestSetting& setting, Message* test_message) {
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
                   " output but provided PROTOBUF instead.")
                .c_str());
        return false;
      }

      if (!test_message->ParseFromString(response.protobuf_payload())) {
        ReportFailure(test_name, level, request, response,
                      "Protobuf output we received from test was unparseable.");
        return false;
      }

      break;
    }

    case ConformanceResponse::kTextPayload: {
      if (requested_output != conformance::TEXT_FORMAT) {
        ReportFailure(
            test_name, level, request, response,
            StrCat("Test was asked for ", WireFormatToString(requested_output),
                   " output but provided TEXT_FORMAT instead.")
                .c_str());
        return false;
      }

      if (!ParseTextFormatResponse(response, setting, test_message)) {
        ReportFailure(
            test_name, level, request, response,
            "TEXT_FORMAT output we received from test was unparseable.");
        return false;
      }

      break;
    }

    default:
      GOOGLE_LOG(FATAL) << test_name
                        << ": unknown payload type: " << response.result_case();
  }

  return true;
}

void TextFormatConformanceTestSuite::ExpectParseFailure(const string& test_name,
                                                        ConformanceLevel level,
                                                        const string& input) {
  TestAllTypesProto3 prototype;
  // We don't expect output, but if the program erroneously accepts the protobuf
  // we let it send its response as this.  We must not leave it unspecified.
  ConformanceRequestSetting setting(
      level, conformance::TEXT_FORMAT, conformance::TEXT_FORMAT,
      conformance::TEXT_FORMAT_TEST, prototype, test_name, input);
  const ConformanceRequest& request = setting.GetRequest();
  ConformanceResponse response;
  string effective_test_name = StrCat(setting.ConformanceLevelToString(level),
                                      ".Proto3.TextFormatInput.", test_name);

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

void TextFormatConformanceTestSuite::RunValidTextFormatTest(
    const string& test_name, ConformanceLevel level, const string& input_text) {
  TestAllTypesProto3 prototype;
  RunValidTextFormatTestWithMessage(test_name, level, input_text, prototype);
}

void TextFormatConformanceTestSuite::RunValidTextFormatTestProto2(
    const string& test_name, ConformanceLevel level, const string& input_text) {
  TestAllTypesProto2 prototype;
  RunValidTextFormatTestWithMessage(test_name, level, input_text, prototype);
}

void TextFormatConformanceTestSuite::RunValidTextFormatTestWithMessage(
    const string& test_name, ConformanceLevel level, const string& input_text,
    const Message& prototype) {
  ConformanceRequestSetting setting1(
      level, conformance::TEXT_FORMAT, conformance::PROTOBUF,
      conformance::TEXT_FORMAT_TEST, prototype, test_name, input_text);
  RunValidInputTest(setting1, input_text);
  ConformanceRequestSetting setting2(
      level, conformance::TEXT_FORMAT, conformance::TEXT_FORMAT,
      conformance::TEXT_FORMAT_TEST, prototype, test_name, input_text);
  RunValidInputTest(setting2, input_text);
}

void TextFormatConformanceTestSuite::RunValidUnknownTextFormatTest(
    const string& test_name, const Message& message) {
  string serialized_input;
  message.SerializeToString(&serialized_input);
  TestAllTypesProto3 prototype;
  ConformanceRequestSetting setting1(
      RECOMMENDED, conformance::PROTOBUF, conformance::TEXT_FORMAT,
      conformance::TEXT_FORMAT_TEST, prototype, test_name + "_Drop",
      serialized_input);
  setting1.SetPrototypeMessageForCompare(message);
  RunValidBinaryInputTest(setting1, "");

  ConformanceRequestSetting setting2(
      RECOMMENDED, conformance::PROTOBUF, conformance::TEXT_FORMAT,
      conformance::TEXT_FORMAT_TEST, prototype, test_name + "_Print",
      serialized_input);
  setting2.SetPrototypeMessageForCompare(message);
  setting2.SetPrintUnknownFields(true);
  RunValidBinaryInputTest(setting2, serialized_input);
}

void TextFormatConformanceTestSuite::RunSuiteImpl() {
  RunValidTextFormatTest("HelloWorld", REQUIRED,
                         "optional_string: 'Hello, World!'");
  // Integer fields.
  RunValidTextFormatTest("Int32FieldMaxValue", REQUIRED,
                         "optional_int32: 2147483647");
  RunValidTextFormatTest("Int32FieldMinValue", REQUIRED,
                         "optional_int32: -2147483648");
  RunValidTextFormatTest("Uint32FieldMaxValue", REQUIRED,
                         "optional_uint32: 4294967295");
  RunValidTextFormatTest("Int64FieldMaxValue", REQUIRED,
                         "optional_int64: 9223372036854775807");
  RunValidTextFormatTest("Int64FieldMinValue", REQUIRED,
                         "optional_int64: -9223372036854775808");
  RunValidTextFormatTest("Uint64FieldMaxValue", REQUIRED,
                         "optional_uint64: 18446744073709551615");

  // Parsers reject out-of-bound integer values.
  ExpectParseFailure("Int32FieldTooLarge", REQUIRED,
                     "optional_int32: 2147483648");
  ExpectParseFailure("Int32FieldTooSmall", REQUIRED,
                     "optional_int32: -2147483649");
  ExpectParseFailure("Uint32FieldTooLarge", REQUIRED,
                     "optional_uint32: 4294967296");
  ExpectParseFailure("Int64FieldTooLarge", REQUIRED,
                     "optional_int64: 9223372036854775808");
  ExpectParseFailure("Int64FieldTooSmall", REQUIRED,
                     "optional_int64: -9223372036854775809");
  ExpectParseFailure("Uint64FieldTooLarge", REQUIRED,
                     "optional_uint64: 18446744073709551616");

  // Floating point fields
  RunValidTextFormatTest("FloatField", REQUIRED,
                         "optional_float: 3.192837");
  RunValidTextFormatTest("FloatFieldWithVeryPreciseNumber", REQUIRED,
                         "optional_float: 3.123456789123456789");
  RunValidTextFormatTest("FloatFieldMaxValue", REQUIRED,
                         "optional_float: 3.4028235e+38");
  RunValidTextFormatTest("FloatFieldMinValue", REQUIRED,
                         "optional_float: 1.17549e-38");
  RunValidTextFormatTest("FloatFieldNaNValue", REQUIRED,
                         "optional_float: NaN");
  RunValidTextFormatTest("FloatFieldPosInfValue", REQUIRED,
                         "optional_float: inf");
  RunValidTextFormatTest("FloatFieldNegInfValue", REQUIRED,
                         "optional_float: -inf");
  RunValidTextFormatTest("FloatFieldWithInt32Max", REQUIRED,
                         "optional_float: 4294967296");
  RunValidTextFormatTest("FloatFieldLargerThanInt64", REQUIRED,
                         "optional_float: 9223372036854775808");
  RunValidTextFormatTest("FloatFieldTooLarge", REQUIRED,
                         "optional_float: 3.4028235e+39");
  RunValidTextFormatTest("FloatFieldTooSmall", REQUIRED,
                         "optional_float: 1.17549e-39");
  RunValidTextFormatTest("FloatFieldLargerThanUint64", REQUIRED,
                         "optional_float: 18446744073709551616");

  // Group fields
  RunValidTextFormatTestProto2("GroupFieldNoColon", REQUIRED,
                               "Data { group_int32: 1 }");
  RunValidTextFormatTestProto2("GroupFieldWithColon", REQUIRED,
                               "Data: { group_int32: 1 }");
  RunValidTextFormatTestProto2("GroupFieldEmpty", REQUIRED,
                               "Data {}");


  // Unknown Fields
  UnknownToTestAllTypes message;
  // Unable to print unknown Fixed32/Fixed64 fields as if they are known.
  // Fixed32/Fixed64 fields are not added in the tests.
  message.set_optional_int32(123);
  message.set_optional_string("hello");
  message.set_optional_bool(true);
  RunValidUnknownTextFormatTest("ScalarUnknownFields", message);

  message.Clear();
  message.mutable_nested_message()->set_c(111);
  RunValidUnknownTextFormatTest("MessageUnknownFields", message);

  message.Clear();
  message.mutable_optionalgroup()->set_a(321);
  RunValidUnknownTextFormatTest("GroupUnknownFields", message);

  message.add_repeated_int32(1);
  message.add_repeated_int32(2);
  message.add_repeated_int32(3);
  RunValidUnknownTextFormatTest("RepeatedUnknownFields", message);

  // Any fields
  RunValidTextFormatTest("AnyField", REQUIRED,
                         R"(
      optional_any: {
        [type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3] {
          optional_int32: 12345
        }
      }
      )");
  RunValidTextFormatTest("AnyFieldWithRawBytes", REQUIRED,
                         R"(
      optional_any: {
        type_url: "type.googleapis.com/protobuf_test_messages.proto3.TestAllTypesProto3"
        value: "\b\271`"
      }
      )");
  ExpectParseFailure("AnyFieldWithInvalidType", REQUIRED,
                     R"(
      optional_any: {
        [type.googleapis.com/unknown] {
          optional_int32: 12345
        }
      }
      )");
}

}  // namespace protobuf
}  // namespace google
