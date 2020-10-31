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

#include <set>
#include <stdarg.h>
#include <string>
#include <fstream>

#include "conformance.pb.h"
#include "conformance_test.h"

#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/field_comparator.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>

using conformance::ConformanceRequest;
using conformance::ConformanceResponse;
using conformance::WireFormat;
using google::protobuf::TextFormat;
using google::protobuf::util::DefaultFieldComparator;
using google::protobuf::util::JsonToBinaryString;
using google::protobuf::util::MessageDifferencer;
using google::protobuf::util::Status;
using std::string;

namespace google {
namespace protobuf {

ConformanceTestSuite::ConformanceRequestSetting::ConformanceRequestSetting(
    ConformanceLevel level,
    conformance::WireFormat input_format,
    conformance::WireFormat output_format,
    conformance::TestCategory test_category,
    const Message& prototype_message,
    const string& test_name, const string& input)
    : level_(level),
      input_format_(input_format),
      output_format_(output_format),
      prototype_message_(prototype_message),
      prototype_message_for_compare_(prototype_message.New()),
      test_name_(test_name) {
  switch (input_format) {
    case conformance::PROTOBUF: {
      request_.set_protobuf_payload(input);
      break;
    }

    case conformance::JSON: {
      request_.set_json_payload(input);
      break;
    }

    case conformance::JSPB: {
      request_.set_jspb_payload(input);
      break;
    }

    case conformance::TEXT_FORMAT: {
      request_.set_text_payload(input);
      break;
    }

    default:
      GOOGLE_LOG(FATAL) << "Unspecified input format";
  }

  request_.set_test_category(test_category);

  request_.set_message_type(prototype_message.GetDescriptor()->full_name());
  request_.set_requested_output_format(output_format);
}

Message* ConformanceTestSuite::ConformanceRequestSetting::
    GetTestMessage() const {
  return prototype_message_for_compare_->New();
}

string ConformanceTestSuite::ConformanceRequestSetting::
    GetTestName() const {
  string rname =
      prototype_message_.GetDescriptor()->file()->syntax() ==
        FileDescriptor::SYNTAX_PROTO3 ? "Proto3" : "Proto2";

  return StrCat(ConformanceLevelToString(level_), ".",
                rname, ".",
                InputFormatString(input_format_),
                ".", test_name_, ".",
                OutputFormatString(output_format_));
}

string ConformanceTestSuite::ConformanceRequestSetting::
    ConformanceLevelToString(
        ConformanceLevel level) const {
  switch (level) {
    case REQUIRED: return "Required";
    case RECOMMENDED: return "Recommended";
  }
  GOOGLE_LOG(FATAL) << "Unknown value: " << level;
  return "";
}

string ConformanceTestSuite::ConformanceRequestSetting::
    InputFormatString(conformance::WireFormat format) const {
  switch (format) {
    case conformance::PROTOBUF:
      return "ProtobufInput";
    case conformance::JSON:
      return "JsonInput";
    case conformance::TEXT_FORMAT:
      return "TextFormatInput";
    default:
      GOOGLE_LOG(FATAL) << "Unspecified output format";
  }
  return "";
}

string ConformanceTestSuite::ConformanceRequestSetting::
    OutputFormatString(conformance::WireFormat format) const {
  switch (format) {
    case conformance::PROTOBUF:
      return "ProtobufOutput";
    case conformance::JSON:
      return "JsonOutput";
    case conformance::TEXT_FORMAT:
      return "TextFormatOutput";
    default:
      GOOGLE_LOG(FATAL) << "Unspecified output format";
  }
  return "";
}

void ConformanceTestSuite::ReportSuccess(const string& test_name) {
  if (expected_to_fail_.erase(test_name) != 0) {
    StringAppendF(&output_,
                  "ERROR: test %s is in the failure list, but test succeeded.  "
                  "Remove it from the failure list.\n",
                  test_name.c_str());
    unexpected_succeeding_tests_.insert(test_name);
  }
  successes_++;
}

void ConformanceTestSuite::ReportFailure(const string& test_name,
                                         ConformanceLevel level,
                                         const ConformanceRequest& request,
                                         const ConformanceResponse& response,
                                         const char* fmt, ...) {
  if (expected_to_fail_.erase(test_name) == 1) {
    expected_failures_++;
    if (!verbose_)
      return;
  } else if (level == RECOMMENDED && !enforce_recommended_) {
    StringAppendF(&output_, "WARNING, test=%s: ", test_name.c_str());
  } else {
    StringAppendF(&output_, "ERROR, test=%s: ", test_name.c_str());
    unexpected_failing_tests_.insert(test_name);
  }
  va_list args;
  va_start(args, fmt);
  StringAppendV(&output_, fmt, args);
  va_end(args);
  StringAppendF(&output_, " request=%s, response=%s\n",
                request.ShortDebugString().c_str(),
                response.ShortDebugString().c_str());
}

void ConformanceTestSuite::ReportSkip(const string& test_name,
                                      const ConformanceRequest& request,
                                      const ConformanceResponse& response) {
  if (verbose_) {
    StringAppendF(&output_, "SKIPPED, test=%s request=%s, response=%s\n",
                  test_name.c_str(), request.ShortDebugString().c_str(),
                  response.ShortDebugString().c_str());
  }
  skipped_.insert(test_name);
}

void ConformanceTestSuite::RunValidInputTest(
    const ConformanceRequestSetting& setting,
    const string& equivalent_text_format) {
  Message* reference_message = setting.GetTestMessage();
  GOOGLE_CHECK(
      TextFormat::ParseFromString(equivalent_text_format, reference_message))
          << "Failed to parse data for test case: " << setting.GetTestName()
          << ", data: " << equivalent_text_format;
  const string equivalent_wire_format = reference_message->SerializeAsString();
  RunValidBinaryInputTest(setting, equivalent_wire_format);
}

void ConformanceTestSuite::RunValidBinaryInputTest(
    const ConformanceRequestSetting& setting,
    const string& equivalent_wire_format) {
  const ConformanceRequest& request = setting.GetRequest();
  ConformanceResponse response;
  RunTest(setting.GetTestName(), request, &response);
  VerifyResponse(setting, equivalent_wire_format, response, true);
}

void ConformanceTestSuite::VerifyResponse(
    const ConformanceRequestSetting& setting,
    const string& equivalent_wire_format,
    const ConformanceResponse& response,
    bool need_report_success) {
  Message* test_message = setting.GetTestMessage();
  const ConformanceRequest& request = setting.GetRequest();
  const string& test_name = setting.GetTestName();
  ConformanceLevel level = setting.GetLevel();
  Message* reference_message = setting.GetTestMessage();

  GOOGLE_CHECK(
      reference_message->ParseFromString(equivalent_wire_format))
          << "Failed to parse wire data for test case: " << test_name;

  switch (response.result_case()) {
    case ConformanceResponse::RESULT_NOT_SET:
      ReportFailure(test_name, level, request, response,
                    "Response didn't have any field in the Response.");
      return;

    case ConformanceResponse::kParseError:
    case ConformanceResponse::kRuntimeError:
    case ConformanceResponse::kSerializeError:
      ReportFailure(test_name, level, request, response,
                    "Failed to parse input or produce output.");
      return;

    case ConformanceResponse::kSkipped:
      ReportSkip(test_name, request, response);
      return;

    default:
      if (!ParseResponse(response, setting, test_message)) return;
  }

  MessageDifferencer differencer;
  DefaultFieldComparator field_comparator;
  field_comparator.set_treat_nan_as_equal(true);
  differencer.set_field_comparator(&field_comparator);
  string differences;
  differencer.ReportDifferencesToString(&differences);

  bool check;
  check = differencer.Compare(*reference_message, *test_message);
  if (check) {
    if (need_report_success) {
      ReportSuccess(test_name);
    }
  } else {
    ReportFailure(test_name, level, request, response,
                  "Output was not equivalent to reference message: %s.",
                  differences.c_str());
  }
}

void ConformanceTestSuite::RunTest(const string& test_name,
                                   const ConformanceRequest& request,
                                   ConformanceResponse* response) {
  if (test_names_.insert(test_name).second == false) {
    GOOGLE_LOG(FATAL) << "Duplicated test name: " << test_name;
  }

  string serialized_request;
  string serialized_response;
  request.SerializeToString(&serialized_request);

  runner_->RunTest(test_name, serialized_request, &serialized_response);

  if (!response->ParseFromString(serialized_response)) {
    response->Clear();
    response->set_runtime_error("response proto could not be parsed.");
  }

  if (verbose_) {
    StringAppendF(&output_,
                  "conformance test: name=%s, request=%s, response=%s\n",
                  test_name.c_str(),
                  request.ShortDebugString().c_str(),
                  response->ShortDebugString().c_str());
  }
}

bool ConformanceTestSuite::CheckSetEmpty(
    const std::set<string>& set_to_check,
    const std::string& write_to_file,
    const std::string& msg) {
  if (set_to_check.empty()) {
    return true;
  } else {
    StringAppendF(&output_, "\n");
    StringAppendF(&output_, "%s\n\n", msg.c_str());
    for (std::set<string>::const_iterator iter = set_to_check.begin();
         iter != set_to_check.end(); ++iter) {
      StringAppendF(&output_, "  %s\n", iter->c_str());
    }
    StringAppendF(&output_, "\n");

    if (!write_to_file.empty()) {
      std::ofstream os(write_to_file);
      if (os) {
        for (std::set<string>::const_iterator iter = set_to_check.begin();
             iter != set_to_check.end(); ++iter) {
          os << *iter << "\n";
        }
      } else {
        StringAppendF(&output_, "Failed to open file: %s\n",
                      write_to_file.c_str());
      }
    }

    return false;
  }
}

string ConformanceTestSuite::WireFormatToString(
    WireFormat wire_format) {
  switch (wire_format) {
    case conformance::PROTOBUF:
      return "PROTOBUF";
    case conformance::JSON:
      return "JSON";
    case conformance::JSPB:
      return "JSPB";
    case conformance::TEXT_FORMAT:
      return "TEXT_FORMAT";
    case conformance::UNSPECIFIED:
      return "UNSPECIFIED";
    default:
      GOOGLE_LOG(FATAL) << "unknown wire type: "
                        << wire_format;
  }
  return "";
}

void ConformanceTestSuite::AddExpectedFailedTest(const std::string& test_name) {
  expected_to_fail_.insert(test_name);
}

bool ConformanceTestSuite::RunSuite(ConformanceTestRunner* runner,
                                    std::string* output, const string& filename,
                                    conformance::FailureSet* failure_list) {
  runner_ = runner;
  successes_ = 0;
  expected_failures_ = 0;
  skipped_.clear();
  test_names_.clear();
  unexpected_failing_tests_.clear();
  unexpected_succeeding_tests_.clear();

  output_ = "\nCONFORMANCE TEST BEGIN ====================================\n\n";

  failure_list_filename_ = filename;
  expected_to_fail_.clear();
  for (const string& failure : failure_list->failure()) {
    AddExpectedFailedTest(failure);
  }
  RunSuiteImpl();

  bool ok = true;
  if (!CheckSetEmpty(expected_to_fail_, "nonexistent_tests.txt",
                     "These tests were listed in the failure list, but they "
                     "don't exist.  Remove them from the failure list by "
                     "running:\n"
                     "  ./update_failure_list.py " + failure_list_filename_ +
                     " --remove nonexistent_tests.txt")) {
    ok = false;
  }
  if (!CheckSetEmpty(unexpected_failing_tests_, "failing_tests.txt",
                     "These tests failed.  If they can't be fixed right now, "
                     "you can add them to the failure list so the overall "
                     "suite can succeed.  Add them to the failure list by "
                     "running:\n"
                     "  ./update_failure_list.py " + failure_list_filename_ +
                     " --add failing_tests.txt")) {
    ok = false;
  }
  if (!CheckSetEmpty(unexpected_succeeding_tests_, "succeeding_tests.txt",
                     "These tests succeeded, even though they were listed in "
                     "the failure list.  Remove them from the failure list "
                     "by running:\n"
                     "  ./update_failure_list.py " + failure_list_filename_ +
                     " --remove succeeding_tests.txt")) {
    ok = false;
  }

  if (verbose_) {
    CheckSetEmpty(skipped_, "",
                  "These tests were skipped (probably because support for some "
                  "features is not implemented)");
  }

  StringAppendF(&output_,
                "CONFORMANCE SUITE %s: %d successes, %d skipped, "
                "%d expected failures, %d unexpected failures.\n",
                ok ? "PASSED" : "FAILED", successes_, skipped_.size(),
                expected_failures_, unexpected_failing_tests_.size());
  StringAppendF(&output_, "\n");

  output->assign(output_);

  return ok;
}

}  // namespace protobuf
}  // namespace google
