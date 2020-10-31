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

#ifndef TEXT_FORMAT_CONFORMANCE_SUITE_H_
#define TEXT_FORMAT_CONFORMANCE_SUITE_H_

#include "conformance_test.h"

namespace google {
namespace protobuf {

class TextFormatConformanceTestSuite : public ConformanceTestSuite {
 public:
  TextFormatConformanceTestSuite();

 private:
  void RunSuiteImpl();
  void RunValidTextFormatTest(const string& test_name, ConformanceLevel level,
                              const string& input);
  void RunValidTextFormatTestProto2(const string& test_name,
                                    ConformanceLevel level,
                                    const string& input);
  void RunValidTextFormatTestWithMessage(const string& test_name,
                                         ConformanceLevel level,
                                         const string& input_text,
                                         const Message& prototype);
  void RunValidUnknownTextFormatTest(const string& test_name,
                                     const Message& message);
  void ExpectParseFailure(const string& test_name, ConformanceLevel level,
                          const string& input);
  bool ParseTextFormatResponse(const conformance::ConformanceResponse& response,
                               const ConformanceRequestSetting& setting,
                               Message* test_message);
  bool ParseResponse(const conformance::ConformanceResponse& response,
                     const ConformanceRequestSetting& setting,
                     Message* test_message) override;
};

}  // namespace protobuf
}  // namespace google

#endif  // TEXT_FORMAT_CONFORMANCE_SUITE_H_
