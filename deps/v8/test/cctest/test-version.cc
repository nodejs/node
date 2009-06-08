// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include "v8.h"

#include "version.h"
#include "cctest.h"

using namespace v8::internal;


namespace v8 {
namespace internal {

void SetVersion(int major, int minor, int build, int patch,
                bool candidate, const char* soname) {
  Version::major_ = major;
  Version::minor_ = minor;
  Version::build_ = build;
  Version::patch_ = patch;
  Version::candidate_ = candidate;
  Version::soname_ = soname;
}

} }  // namespace v8::internal


static void CheckVersion(int major, int minor, int build,
                         int patch, bool candidate,
                         const char* expected_version_string,
                         const char* expected_generic_soname) {
  static v8::internal::EmbeddedVector<char, 128> version_str;
  static v8::internal::EmbeddedVector<char, 128> soname_str;

  // Test version without specific SONAME.
  SetVersion(major, minor, build, patch, candidate, "");
  Version::GetString(version_str);
  CHECK_EQ(expected_version_string, version_str.start());
  Version::GetSONAME(soname_str);
  CHECK_EQ(expected_generic_soname, soname_str.start());

  // Test version with specific SONAME.
  const char* soname = "libv8.so.1";
  SetVersion(major, minor, build, patch, candidate, soname);
  Version::GetString(version_str);
  CHECK_EQ(expected_version_string, version_str.start());
  Version::GetSONAME(soname_str);
  CHECK_EQ(soname, soname_str.start());
}


TEST(VersionString) {
  CheckVersion(0, 0, 0, 0, false, "0.0.0", "libv8-0.0.0.so");
  CheckVersion(0, 0, 0, 0, true,
               "0.0.0 (candidate)", "libv8-0.0.0-candidate.so");
  CheckVersion(1, 0, 0, 0, false, "1.0.0", "libv8-1.0.0.so");
  CheckVersion(1, 0, 0, 0, true,
               "1.0.0 (candidate)", "libv8-1.0.0-candidate.so");
  CheckVersion(1, 0, 0, 1, false, "1.0.0.1", "libv8-1.0.0.1.so");
  CheckVersion(1, 0, 0, 1, true,
               "1.0.0.1 (candidate)", "libv8-1.0.0.1-candidate.so");
  CheckVersion(2, 5, 10, 7, false, "2.5.10.7", "libv8-2.5.10.7.so");
  CheckVersion(2, 5, 10, 7, true,
               "2.5.10.7 (candidate)", "libv8-2.5.10.7-candidate.so");
}
