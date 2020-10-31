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

#ifndef GOOGLE_PROTOBUF_ANY_H__
#define GOOGLE_PROTOBUF_ANY_H__

#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/message_lite.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

class FieldDescriptor;
class Message;

namespace internal {

extern const char kAnyFullTypeName[];          // "google.protobuf.Any".
extern const char kTypeGoogleApisComPrefix[];  // "type.googleapis.com/".
extern const char kTypeGoogleProdComPrefix[];  // "type.googleprod.com/".

std::string GetTypeUrl(StringPiece message_name,
                       StringPiece type_url_prefix);

// Helper class used to implement google::protobuf::Any.
class PROTOBUF_EXPORT AnyMetadata {
  typedef ArenaStringPtr UrlType;
  typedef ArenaStringPtr ValueType;
 public:
  // AnyMetadata does not take ownership of "type_url" and "value".
  AnyMetadata(UrlType* type_url, ValueType* value);

  // Packs a message using the default type URL prefix: "type.googleapis.com".
  // The resulted type URL will be "type.googleapis.com/<message_full_name>".
  template <typename T>
  void PackFrom(const T& message) {
    InternalPackFrom(message, kTypeGoogleApisComPrefix, T::FullMessageName());
  }

  void PackFrom(const Message& message);

  // Packs a message using the given type URL prefix. The type URL will be
  // constructed by concatenating the message type's full name to the prefix
  // with an optional "/" separator if the prefix doesn't already end up "/".
  // For example, both PackFrom(message, "type.googleapis.com") and
  // PackFrom(message, "type.googleapis.com/") yield the same result type
  // URL: "type.googleapis.com/<message_full_name>".
  template <typename T>
  void PackFrom(const T& message, StringPiece type_url_prefix) {
    InternalPackFrom(message, type_url_prefix, T::FullMessageName());
  }

  void PackFrom(const Message& message, const std::string& type_url_prefix);

  // Unpacks the payload into the given message. Returns false if the message's
  // type doesn't match the type specified in the type URL (i.e., the full
  // name after the last "/" of the type URL doesn't match the message's actual
  // full name) or parsing the payload has failed.
  template <typename T>
  bool UnpackTo(T* message) const {
    return InternalUnpackTo(T::FullMessageName(), message);
  }

  bool UnpackTo(Message* message) const;

  // Checks whether the type specified in the type URL matches the given type.
  // A type is consdiered matching if its full name matches the full name after
  // the last "/" in the type URL.
  template <typename T>
  bool Is() const {
    return InternalIs(T::FullMessageName());
  }

 private:
  void InternalPackFrom(const MessageLite& message,
                        StringPiece type_url_prefix,
                        StringPiece type_name);
  bool InternalUnpackTo(StringPiece type_name,
                        MessageLite* message) const;
  bool InternalIs(StringPiece type_name) const;

  UrlType* type_url_;
  ValueType* value_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(AnyMetadata);
};

// Get the proto type name from Any::type_url value. For example, passing
// "type.googleapis.com/rpc.QueryOrigin" will return "rpc.QueryOrigin" in
// *full_type_name. Returns false if the type_url does not have a "/"
// in the type url separating the full type name.
//
// NOTE: this function is available publicly as:
//   google::protobuf::Any()  // static method on the generated message type.
bool ParseAnyTypeUrl(const std::string& type_url, std::string* full_type_name);

// Get the proto type name and prefix from Any::type_url value. For example,
// passing "type.googleapis.com/rpc.QueryOrigin" will return
// "type.googleapis.com/" in *url_prefix and "rpc.QueryOrigin" in
// *full_type_name. Returns false if the type_url does not have a "/" in the
// type url separating the full type name.
bool ParseAnyTypeUrl(const std::string& type_url, std::string* url_prefix,
                     std::string* full_type_name);

// See if message is of type google.protobuf.Any, if so, return the descriptors
// for "type_url" and "value" fields.
bool GetAnyFieldDescriptors(const Message& message,
                            const FieldDescriptor** type_url_field,
                            const FieldDescriptor** value_field);

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_ANY_H__
