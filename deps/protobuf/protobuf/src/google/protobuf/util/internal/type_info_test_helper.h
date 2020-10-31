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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_TEST_HELPER_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_TEST_HELPER_H__

#include <memory>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/internal/default_value_objectwriter.h>
#include <google/protobuf/util/internal/type_info.h>
#include <google/protobuf/util/internal/protostream_objectsource.h>
#include <google/protobuf/util/internal/protostream_objectwriter.h>
#include <google/protobuf/util/type_resolver.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace testing {

enum TypeInfoSource {
  USE_TYPE_RESOLVER,
};

// In the unit-tests we want to test two scenarios: one with type info from
// ServiceTypeInfo, the other with type info from TypeResolver. This class
// wraps the detail of where the type info is from and provides the same
// interface so the same unit-test code can test both scenarios.
class TypeInfoTestHelper {
 public:
  explicit TypeInfoTestHelper(TypeInfoSource type) : type_(type) {}

  // Creates a TypeInfo object for the given set of descriptors.
  void ResetTypeInfo(const std::vector<const Descriptor*>& descriptors);

  // Convinent overloads.
  void ResetTypeInfo(const Descriptor* descriptor);
  void ResetTypeInfo(const Descriptor* descriptor1,
                     const Descriptor* descriptor2);

  // Returns the TypeInfo created after ResetTypeInfo.
  TypeInfo* GetTypeInfo();

  ProtoStreamObjectSource* NewProtoSource(io::CodedInputStream* coded_input,
                                          const std::string& type_url);

  ProtoStreamObjectWriter* NewProtoWriter(
      const std::string& type_url, strings::ByteSink* output,
      ErrorListener* listener, const ProtoStreamObjectWriter::Options& options);

  DefaultValueObjectWriter* NewDefaultValueWriter(const std::string& type_url,
                                                  ObjectWriter* writer);

 private:
  TypeInfoSource type_;
  std::unique_ptr<TypeInfo> typeinfo_;
  std::unique_ptr<TypeResolver> type_resolver_;
};
}  // namespace testing
}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_TEST_HELPER_H__
