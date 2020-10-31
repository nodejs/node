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

#ifndef GOOGLE_PROTOBUF_TEST_UTIL2_H__
#define GOOGLE_PROTOBUF_TEST_UTIL2_H__

#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/testing/googletest.h>


namespace google {
namespace protobuf {
namespace TestUtil {

// Translate net/proto2/* -> google/protobuf/*
inline std::string TranslatePathToOpensource(const std::string& google3_path) {
  const std::string prefix = "net/proto2/";
  GOOGLE_CHECK(google3_path.find(prefix) == 0) << google3_path;
  std::string path = google3_path.substr(prefix.size());

  path = StringReplace(path, "internal/", "", false);
  path = StringReplace(path, "proto/", "", false);
  path = StringReplace(path, "public/", "", false);
  return "google/protobuf/" + path;
}

inline std::string MaybeTranslatePath(const std::string& google3_path) {
  std::string path = google3_path;
  path = TranslatePathToOpensource(path);
  return path;
}

inline std::string TestSourceDir() {
  return google::protobuf::TestSourceDir();
}

inline std::string GetTestDataPath(const std::string& google3_path) {
  return TestSourceDir() + "/" + MaybeTranslatePath(google3_path);
}

// Checks the equality of "message" and serialized proto of type "ProtoType".
// Do not directly compare two serialized protos.
template <typename ProtoType>
bool EqualsToSerialized(const ProtoType& message, const std::string& data) {
  ProtoType other;
  other.ParsePartialFromString(data);
  return util::MessageDifferencer::Equals(message, other);
}

}  // namespace TestUtil
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_TEST_UTIL2_H__
