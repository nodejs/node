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

#ifndef GOOGLE_PROTOBUF_MAP_LITE_TEST_UTIL_H__
#define GOOGLE_PROTOBUF_MAP_LITE_TEST_UTIL_H__

#include <google/protobuf/map_lite_unittest.pb.h>

namespace google {
namespace protobuf {

class MapLiteTestUtil {
 public:
  // Set every field in the TestMapLite message to a unique value.
  static void SetMapFields(protobuf_unittest::TestMapLite* message);

  // Set every field in the TestArenaMapLite message to a unique value.
  static void SetArenaMapFields(protobuf_unittest::TestArenaMapLite* message);

  // Set every field in the message to a default value.
  static void SetMapFieldsInitialized(protobuf_unittest::TestMapLite* message);

  // Modify all the map fields of the message (which should already have been
  // initialized with SetMapFields()).
  static void ModifyMapFields(protobuf_unittest::TestMapLite* message);

  // Check that all fields have the values that they should have after
  // SetMapFields() is called.
  static void ExpectMapFieldsSet(const protobuf_unittest::TestMapLite& message);

  // Check that all fields have the values that they should have after
  // SetMapFields() is called for TestArenaMapLite.
  static void ExpectArenaMapFieldsSet(
      const protobuf_unittest::TestArenaMapLite& message);

  // Check that all fields have the values that they should have after
  // SetMapFieldsInitialized() is called.
  static void ExpectMapFieldsSetInitialized(
      const protobuf_unittest::TestMapLite& message);

  // Expect that the message is modified as would be expected from
  // ModifyMapFields().
  static void ExpectMapFieldsModified(
      const protobuf_unittest::TestMapLite& message);

  // Check that all fields are empty.
  static void ExpectClear(const protobuf_unittest::TestMapLite& message);
};

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_MAP_LITE_TEST_UTIL_H__
