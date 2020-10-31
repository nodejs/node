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


// This file defines a protocol for running the conformance test suite
// in-process.  In other words, the suite itself will run in the same process as
// the code under test.
//
// For pros and cons of this approach, please see conformance.proto.

#ifndef CONFORMANCE_CONFORMANCE_TEST_H
#define CONFORMANCE_CONFORMANCE_TEST_H

#include <functional>
#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/wire_format_lite.h>

#include "conformance.pb.h"

namespace conformance {
class ConformanceRequest;
class ConformanceResponse;
}  // namespace conformance

namespace protobuf_test_messages {
namespace proto3 {
class TestAllTypesProto3;
}  // namespace proto3
}  // namespace protobuf_test_messages

namespace google {
namespace protobuf {

class ConformanceTestSuite;

class ConformanceTestRunner {
 public:
  virtual ~ConformanceTestRunner() {}

  // Call to run a single conformance test.
  //
  // "input" is a serialized conformance.ConformanceRequest.
  // "output" should be set to a serialized conformance.ConformanceResponse.
  //
  // If there is any error in running the test itself, set "runtime_error" in
  // the response.
  virtual void RunTest(const std::string& test_name,
                       const std::string& input,
                       std::string* output) = 0;
};

// Test runner that spawns the process being tested and communicates with it
// over a pipe.
class ForkPipeRunner : public ConformanceTestRunner {
 public:
  // Note: Run() doesn't take ownership of the pointers inside suites.
  static int Run(int argc, char *argv[],
                 const std::vector<ConformanceTestSuite*>& suites);

  ForkPipeRunner(const std::string &executable)
      : child_pid_(-1), executable_(executable) {}

  virtual ~ForkPipeRunner() {}

  void RunTest(const std::string& test_name,
               const std::string& request,
               std::string* response);

 private:
  void SpawnTestProgram();

  void CheckedWrite(int fd, const void *buf, size_t len);
  bool TryRead(int fd, void *buf, size_t len);
  void CheckedRead(int fd, void *buf, size_t len);

  int write_fd_;
  int read_fd_;
  pid_t child_pid_;
  std::string executable_;
  std::string current_test_name_;
};

// Class representing the test suite itself.  To run it, implement your own
// class derived from ConformanceTestRunner, class derived from
// ConformanceTestSuite and then write code like:
//
//    class MyConformanceTestSuite : public ConformanceTestSuite {
//     public:
//      void RunSuiteImpl() {
//        // INSERT ACTURAL TESTS.
//      }
//    };
//
//    class MyConformanceTestRunner : public ConformanceTestRunner {
//     public:
//      static int Run(int argc, char *argv[],
//                 ConformanceTestSuite* suite);
//
//     private:
//      virtual void RunTest(...) {
//        // INSERT YOUR FRAMEWORK-SPECIFIC CODE HERE.
//      }
//    };
//
//    int main() {
//      MyConformanceTestSuite suite;
//      MyConformanceTestRunner::Run(argc, argv, &suite);
//    }
//
class ConformanceTestSuite {
 public:
  ConformanceTestSuite()
      : verbose_(false),
        enforce_recommended_(false),
        failure_list_flag_name_("--failure_list") {}
  virtual ~ConformanceTestSuite() {}

  void SetVerbose(bool verbose) { verbose_ = verbose; }

  // Whether to require the testee to pass RECOMMENDED tests. By default failing
  // a RECOMMENDED test case will not fail the entire suite but will only
  // generated a warning. If this flag is set to true, RECOMMENDED tests will
  // be treated the same way as REQUIRED tests and failing a RECOMMENDED test
  // case will cause the entire test suite to fail as well. An implementation
  // can enable this if it wants to be strictly conforming to protobuf spec.
  // See the comments about ConformanceLevel below to learn more about the
  // difference between REQUIRED and RECOMMENDED test cases.
  void SetEnforceRecommended(bool value) {
    enforce_recommended_ = value;
  }

  // Gets the flag name to the failure list file.
  // By default, this would return --failure_list
  string GetFailureListFlagName() {
    return failure_list_flag_name_;
  }

  void SetFailureListFlagName(const std::string& failure_list_flag_name) {
    failure_list_flag_name_ = failure_list_flag_name;
  }

  // Run all the conformance tests against the given test runner.
  // Test output will be stored in "output".
  //
  // Returns true if the set of failing tests was exactly the same as the
  // failure list.
  // The filename here is *only* used to create/format useful error messages for
  // how to update the failure list.  We do NOT read this file at all.
  bool RunSuite(ConformanceTestRunner* runner, std::string* output,
                const std::string& filename,
                conformance::FailureSet* failure_list);

 protected:
  // Test cases are classified into a few categories:
  //   REQUIRED: the test case must be passed for an implementation to be
  //             interoperable with other implementations. For example, a
  //             parser implementaiton must accept both packed and unpacked
  //             form of repeated primitive fields.
  //   RECOMMENDED: the test case is not required for the implementation to
  //                be interoperable with other implementations, but is
  //                recommended for best performance and compatibility. For
  //                example, a proto3 serializer should serialize repeated
  //                primitive fields in packed form, but an implementation
  //                failing to do so will still be able to communicate with
  //                other implementations.
  enum ConformanceLevel {
    REQUIRED = 0,
    RECOMMENDED = 1,
  };

  class ConformanceRequestSetting {
   public:
    ConformanceRequestSetting(
        ConformanceLevel level,
        conformance::WireFormat input_format,
        conformance::WireFormat output_format,
        conformance::TestCategory test_category,
        const Message& prototype_message,
        const string& test_name, const string& input);
    virtual ~ConformanceRequestSetting() {}

    Message* GetTestMessage() const;

    string GetTestName() const;

    const conformance::ConformanceRequest& GetRequest() const {
      return request_;
    }

    const ConformanceLevel GetLevel() const {
      return level_;
    }

    string ConformanceLevelToString(ConformanceLevel level) const;

    void SetPrintUnknownFields(bool print_unknown_fields) {
      request_.set_print_unknown_fields(true);
    }

    void SetPrototypeMessageForCompare(const Message& message) {
      prototype_message_for_compare_.reset(message.New());
    }

   protected:
    virtual string InputFormatString(conformance::WireFormat format) const;
    virtual string OutputFormatString(conformance::WireFormat format) const;
    conformance::ConformanceRequest request_;

   private:
    ConformanceLevel level_;
    ::conformance::WireFormat input_format_;
    ::conformance::WireFormat output_format_;
    const Message& prototype_message_;
    std::unique_ptr<Message> prototype_message_for_compare_;
    string test_name_;
  };

  bool CheckSetEmpty(const std::set<string>& set_to_check,
                     const std::string& write_to_file, const std::string& msg);
  string WireFormatToString(conformance::WireFormat wire_format);

  // Parse payload in the response to the given message. Returns true on
  // success.
  virtual bool ParseResponse(
      const conformance::ConformanceResponse& response,
      const ConformanceRequestSetting& setting,
      Message* test_message) = 0;

  void VerifyResponse(
      const ConformanceRequestSetting& setting,
      const string& equivalent_wire_format,
      const conformance::ConformanceResponse& response,
      bool need_report_success);

  void ReportSuccess(const std::string& test_name);
  void ReportFailure(const string& test_name,
                     ConformanceLevel level,
                     const conformance::ConformanceRequest& request,
                     const conformance::ConformanceResponse& response,
                     const char* fmt, ...);
  void ReportSkip(const string& test_name,
                  const conformance::ConformanceRequest& request,
                  const conformance::ConformanceResponse& response);

  void RunValidInputTest(const ConformanceRequestSetting& setting,
                         const string& equivalent_text_format);
  void RunValidBinaryInputTest(const ConformanceRequestSetting& setting,
                               const string& equivalent_wire_format);

  void RunTest(const std::string& test_name,
               const conformance::ConformanceRequest& request,
               conformance::ConformanceResponse* response);

  void AddExpectedFailedTest(const std::string& test_name);

  virtual void RunSuiteImpl() = 0;

  ConformanceTestRunner* runner_;
  int successes_;
  int expected_failures_;
  bool verbose_;
  bool enforce_recommended_;
  std::string output_;
  std::string failure_list_flag_name_;
  std::string failure_list_filename_;

  // The set of test names that are expected to fail in this run, but haven't
  // failed yet.
  std::set<std::string> expected_to_fail_;

  // The set of test names that have been run.  Used to ensure that there are no
  // duplicate names in the suite.
  std::set<std::string> test_names_;

  // The set of tests that failed, but weren't expected to.
  std::set<std::string> unexpected_failing_tests_;

  // The set of tests that succeeded, but weren't expected to.
  std::set<std::string> unexpected_succeeding_tests_;

  // The set of tests that the testee opted out of;
  std::set<std::string> skipped_;
};

}  // namespace protobuf
}  // namespace google

#endif  // CONFORMANCE_CONFORMANCE_TEST_H
