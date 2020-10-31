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

#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include "conformance.pb.h"
#include <google/protobuf/test_messages_proto3.pb.h>
#include <google/protobuf/test_messages_proto2.pb.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>

using conformance::ConformanceRequest;
using conformance::ConformanceResponse;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::Message;
using google::protobuf::MessageFactory;
using google::protobuf::TextFormat;
using google::protobuf::util::BinaryToJsonString;
using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::JsonToBinaryString;
using google::protobuf::util::NewTypeResolverForDescriptorPool;
using google::protobuf::util::Status;
using google::protobuf::util::TypeResolver;
using protobuf_test_messages::proto2::TestAllTypesProto2;
using protobuf_test_messages::proto3::TestAllTypesProto3;
using std::string;

static const char kTypeUrlPrefix[] = "type.googleapis.com";

const char* kFailures[] = {
#if !GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
    "Required.Proto2.ProtobufInput."
    "PrematureEofInDelimitedDataForKnownNonRepeatedValue.MESSAGE",
    "Required.Proto2.ProtobufInput."
    "PrematureEofInDelimitedDataForKnownRepeatedValue.MESSAGE",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.BOOL",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.ENUM",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.INT32",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.INT64",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.SINT32",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.SINT64",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.UINT32",
    "Required.Proto2.ProtobufInput.PrematureEofInPackedField.UINT64",
    "Required.Proto3.ProtobufInput."
    "PrematureEofInDelimitedDataForKnownNonRepeatedValue.MESSAGE",
    "Required.Proto3.ProtobufInput."
    "PrematureEofInDelimitedDataForKnownRepeatedValue.MESSAGE",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.BOOL",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.ENUM",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.INT32",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.INT64",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.SINT32",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.SINT64",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.UINT32",
    "Required.Proto3.ProtobufInput.PrematureEofInPackedField.UINT64",
#endif
};

static string GetTypeUrl(const Descriptor* message) {
  return string(kTypeUrlPrefix) + "/" + message->full_name();
}

int test_count = 0;
bool verbose = false;
TypeResolver* type_resolver;
string* type_url;


bool CheckedRead(int fd, void *buf, size_t len) {
  size_t ofs = 0;
  while (len > 0) {
    ssize_t bytes_read = read(fd, (char*)buf + ofs, len);

    if (bytes_read == 0) return false;

    if (bytes_read < 0) {
      GOOGLE_LOG(FATAL) << "Error reading from test runner: " <<  strerror(errno);
    }

    len -= bytes_read;
    ofs += bytes_read;
  }

  return true;
}

void CheckedWrite(int fd, const void *buf, size_t len) {
  if (write(fd, buf, len) != len) {
    GOOGLE_LOG(FATAL) << "Error writing to test runner: " << strerror(errno);
  }
}

void DoTest(const ConformanceRequest& request, ConformanceResponse* response) {
  Message *test_message;
  const Descriptor *descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(
      request.message_type());
  if (!descriptor) {
    GOOGLE_LOG(FATAL) << "No such message type: " << request.message_type();
  }
  test_message = MessageFactory::generated_factory()->GetPrototype(descriptor)->New();

  switch (request.payload_case()) {
    case ConformanceRequest::kProtobufPayload: {
      if (!test_message->ParseFromString(request.protobuf_payload())) {
        // Getting parse details would involve something like:
        //   http://stackoverflow.com/questions/22121922/how-can-i-get-more-details-about-errors-generated-during-protobuf-parsing-c
        response->set_parse_error("Parse error (no more details available).");
        return;
      }
      break;
    }

    case ConformanceRequest::kJsonPayload: {
      string proto_binary;
      JsonParseOptions options;
      options.ignore_unknown_fields =
          (request.test_category() ==
              conformance::JSON_IGNORE_UNKNOWN_PARSING_TEST);
      Status status = JsonToBinaryString(type_resolver, *type_url,
                                         request.json_payload(), &proto_binary,
                                         options);
      if (!status.ok()) {
        response->set_parse_error(string("Parse error: ") +
                                  status.error_message().as_string());
        return;
      }

      if (!test_message->ParseFromString(proto_binary)) {
        response->set_runtime_error(
            "Parsing JSON generates invalid proto output.");
        return;
      }
      break;
    }

    case ConformanceRequest::kTextPayload: {
      if (!TextFormat::ParseFromString(request.text_payload(), test_message)) {
        response->set_parse_error("Parse error");
        return;
      }
      break;
    }

    case ConformanceRequest::PAYLOAD_NOT_SET:
      GOOGLE_LOG(FATAL) << "Request didn't have payload.";
      break;

    default:
      GOOGLE_LOG(FATAL) << "unknown payload type: "
                        << request.payload_case();
      break;
  }

  conformance::FailureSet failures;
  if (descriptor == failures.GetDescriptor()) {
    for (const char* s : kFailures) failures.add_failure(s);
    test_message = &failures;
  }

  switch (request.requested_output_format()) {
    case conformance::UNSPECIFIED:
      GOOGLE_LOG(FATAL) << "Unspecified output format";
      break;

    case conformance::PROTOBUF: {
      GOOGLE_CHECK(test_message->SerializeToString(response->mutable_protobuf_payload()));
      break;
    }

    case conformance::JSON: {
      string proto_binary;
      GOOGLE_CHECK(test_message->SerializeToString(&proto_binary));
      Status status = BinaryToJsonString(type_resolver, *type_url, proto_binary,
                                         response->mutable_json_payload());
      if (!status.ok()) {
        response->set_serialize_error(
            string("Failed to serialize JSON output: ") +
            status.error_message().as_string());
        return;
      }
      break;
    }

    case conformance::TEXT_FORMAT: {
      TextFormat::Printer printer;
      printer.SetHideUnknownFields(!request.print_unknown_fields());
      GOOGLE_CHECK(printer.PrintToString(*test_message,
                                         response->mutable_text_payload()));
      break;
    }

    default:
      GOOGLE_LOG(FATAL) << "Unknown output format: "
                        << request.requested_output_format();
  }
}

bool DoTestIo() {
  string serialized_input;
  string serialized_output;
  ConformanceRequest request;
  ConformanceResponse response;
  uint32_t bytes;

  if (!CheckedRead(STDIN_FILENO, &bytes, sizeof(uint32_t))) {
    // EOF.
    return false;
  }

  serialized_input.resize(bytes);

  if (!CheckedRead(STDIN_FILENO, (char*)serialized_input.c_str(), bytes)) {
    GOOGLE_LOG(ERROR) << "Unexpected EOF on stdin. " << strerror(errno);
  }

  if (!request.ParseFromString(serialized_input)) {
    GOOGLE_LOG(FATAL) << "Parse of ConformanceRequest proto failed.";
    return false;
  }

  DoTest(request, &response);

  response.SerializeToString(&serialized_output);

  bytes = serialized_output.size();
  CheckedWrite(STDOUT_FILENO, &bytes, sizeof(uint32_t));
  CheckedWrite(STDOUT_FILENO, serialized_output.c_str(), bytes);

  if (verbose) {
    fprintf(stderr, "conformance-cpp: request=%s, response=%s\n",
            request.ShortDebugString().c_str(),
            response.ShortDebugString().c_str());
  }

  test_count++;

  return true;
}

int main() {
  type_resolver = NewTypeResolverForDescriptorPool(
      kTypeUrlPrefix, DescriptorPool::generated_pool());
  type_url = new string(GetTypeUrl(TestAllTypesProto3::descriptor()));
  while (1) {
    if (!DoTestIo()) {
      fprintf(stderr, "conformance-cpp: received EOF from test runner "
                      "after %d tests, exiting\n", test_count);
      return 0;
    }
  }
}
