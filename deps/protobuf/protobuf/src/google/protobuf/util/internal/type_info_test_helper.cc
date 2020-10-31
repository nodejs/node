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

#include <google/protobuf/util/internal/type_info_test_helper.h>

#include <memory>
#include <vector>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/internal/default_value_objectwriter.h>
#include <google/protobuf/util/internal/type_info.h>
#include <google/protobuf/util/internal/constants.h>
#include <google/protobuf/util/internal/protostream_objectsource.h>
#include <google/protobuf/util/internal/protostream_objectwriter.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/util/type_resolver_util.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace testing {


void TypeInfoTestHelper::ResetTypeInfo(
    const std::vector<const Descriptor*>& descriptors) {
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      const DescriptorPool* pool = descriptors[0]->file()->pool();
      for (int i = 1; i < descriptors.size(); ++i) {
        GOOGLE_CHECK(pool == descriptors[i]->file()->pool())
            << "Descriptors from different pools are not supported.";
      }
      type_resolver_.reset(
          NewTypeResolverForDescriptorPool(kTypeServiceBaseUrl, pool));
      typeinfo_.reset(TypeInfo::NewTypeInfo(type_resolver_.get()));
      return;
    }
  }
  GOOGLE_LOG(FATAL) << "Can not reach here.";
}

void TypeInfoTestHelper::ResetTypeInfo(const Descriptor* descriptor) {
  std::vector<const Descriptor*> descriptors;
  descriptors.push_back(descriptor);
  ResetTypeInfo(descriptors);
}

void TypeInfoTestHelper::ResetTypeInfo(const Descriptor* descriptor1,
                                       const Descriptor* descriptor2) {
  std::vector<const Descriptor*> descriptors;
  descriptors.push_back(descriptor1);
  descriptors.push_back(descriptor2);
  ResetTypeInfo(descriptors);
}

TypeInfo* TypeInfoTestHelper::GetTypeInfo() { return typeinfo_.get(); }

ProtoStreamObjectSource* TypeInfoTestHelper::NewProtoSource(
    io::CodedInputStream* coded_input, const std::string& type_url) {
  const google::protobuf::Type* type = typeinfo_->GetTypeByTypeUrl(type_url);
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      return new ProtoStreamObjectSource(coded_input, type_resolver_.get(),
                                         *type);
    }
  }
  GOOGLE_LOG(FATAL) << "Can not reach here.";
  return NULL;
}

ProtoStreamObjectWriter* TypeInfoTestHelper::NewProtoWriter(
    const std::string& type_url, strings::ByteSink* output,
    ErrorListener* listener, const ProtoStreamObjectWriter::Options& options) {
  const google::protobuf::Type* type = typeinfo_->GetTypeByTypeUrl(type_url);
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      return new ProtoStreamObjectWriter(type_resolver_.get(), *type, output,
                                         listener, options);
    }
  }
  GOOGLE_LOG(FATAL) << "Can not reach here.";
  return NULL;
}

DefaultValueObjectWriter* TypeInfoTestHelper::NewDefaultValueWriter(
    const std::string& type_url, ObjectWriter* writer) {
  const google::protobuf::Type* type = typeinfo_->GetTypeByTypeUrl(type_url);
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      return new DefaultValueObjectWriter(type_resolver_.get(), *type, writer);
    }
  }
  GOOGLE_LOG(FATAL) << "Can not reach here.";
  return NULL;
}

}  // namespace testing
}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
