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

#include <google/protobuf/map_lite_test_util.h>
#include <google/protobuf/map_lite_unittest.pb.h>
#include <google/protobuf/map_test_util_impl.h>

namespace google {
namespace protobuf {

void MapLiteTestUtil::SetMapFields(unittest::TestMapLite* message) {
  MapTestUtilImpl::SetMapFields<unittest::MapEnumLite,
                                unittest::MAP_ENUM_BAR_LITE,
                                unittest::MAP_ENUM_BAZ_LITE>(message);
}

void MapLiteTestUtil::SetArenaMapFields(unittest::TestArenaMapLite* message) {
  MapTestUtilImpl::SetArenaMapFields<unittest::MapEnumLite,
                                     unittest::MAP_ENUM_BAR_LITE,
                                     unittest::MAP_ENUM_BAZ_LITE>(message);
}

void MapLiteTestUtil::SetMapFieldsInitialized(unittest::TestMapLite* message) {
  MapTestUtilImpl::SetMapFieldsInitialized(message);
}

void MapLiteTestUtil::ModifyMapFields(unittest::TestMapLite* message) {
  MapTestUtilImpl::ModifyMapFields<unittest::MapEnumLite,
                                   unittest::MAP_ENUM_FOO_LITE>(message);
}

void MapLiteTestUtil::ExpectClear(const unittest::TestMapLite& message) {
  MapTestUtilImpl::ExpectClear(message);
}

void MapLiteTestUtil::ExpectMapFieldsSet(const unittest::TestMapLite& message) {
  MapTestUtilImpl::ExpectMapFieldsSet<unittest::MapEnumLite,
                                      unittest::MAP_ENUM_BAR_LITE,
                                      unittest::MAP_ENUM_BAZ_LITE>(message);
}

void MapLiteTestUtil::ExpectArenaMapFieldsSet(
    const unittest::TestArenaMapLite& message) {
  MapTestUtilImpl::ExpectArenaMapFieldsSet<unittest::MapEnumLite,
                                           unittest::MAP_ENUM_BAR_LITE,
                                           unittest::MAP_ENUM_BAZ_LITE>(
      message);
}

void MapLiteTestUtil::ExpectMapFieldsSetInitialized(
    const unittest::TestMapLite& message) {
  MapTestUtilImpl::ExpectMapFieldsSetInitialized<unittest::MapEnumLite,
                                                 unittest::MAP_ENUM_FOO_LITE>(
      message);
}

void MapLiteTestUtil::ExpectMapFieldsModified(
    const unittest::TestMapLite& message) {
  MapTestUtilImpl::ExpectMapFieldsModified<unittest::MapEnumLite,
                                           unittest::MAP_ENUM_BAR_LITE,
                                           unittest::MAP_ENUM_FOO_LITE>(
      message);
}

}  // namespace protobuf
}  // namespace google
