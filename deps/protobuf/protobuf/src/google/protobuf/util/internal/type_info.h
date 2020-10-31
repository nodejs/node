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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_H__

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/type.pb.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/stubs/stringpiece.h>
#include <google/protobuf/stubs/status.h>
#include <google/protobuf/stubs/statusor.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {
// Internal helper class for type resolving. Note that this class is not
// thread-safe and should only be accessed in one thread.
class PROTOBUF_EXPORT TypeInfo {
 public:
  TypeInfo() {}
  virtual ~TypeInfo() {}

  // Resolves a type url into a Type. If the type url is invalid, returns
  // INVALID_ARGUMENT error status. If the type url is valid but the
  // corresponding type cannot be found, returns a NOT_FOUND error status.
  //
  // This TypeInfo class retains the ownership of the returned pointer.
  virtual util::StatusOr<const google::protobuf::Type*> ResolveTypeUrl(
      StringPiece type_url) const = 0;

  // Resolves a type url into a Type. Like ResolveTypeUrl() but returns
  // NULL if the type url is invalid or the type cannot be found.
  //
  // This TypeInfo class retains the ownership of the returned pointer.
  virtual const google::protobuf::Type* GetTypeByTypeUrl(
      StringPiece type_url) const = 0;

  // Resolves a type url for an enum. Returns NULL if the type url is
  // invalid or the type cannot be found.
  //
  // This TypeInfo class retains the ownership of the returned pointer.
  virtual const google::protobuf::Enum* GetEnumByTypeUrl(
      StringPiece type_url) const = 0;

  // Looks up a field in the specified type given a CamelCase name.
  virtual const google::protobuf::Field* FindField(
      const google::protobuf::Type* type,
      StringPiece camel_case_name) const = 0;

  // Creates a TypeInfo object that looks up type information from a
  // TypeResolver. Caller takes ownership of the returned pointer.
  static TypeInfo* NewTypeInfo(TypeResolver* type_resolver);

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TypeInfo);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_H__
