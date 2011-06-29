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

#ifndef V8_VERSION_H_
#define V8_VERSION_H_

namespace v8 {
namespace internal {

class Version {
 public:
  // Return the various version components.
  static int GetMajor() { return major_; }
  static int GetMinor() { return minor_; }
  static int GetBuild() { return build_; }
  static int GetPatch() { return patch_; }
  static bool IsCandidate() { return candidate_; }

  // Calculate the V8 version string.
  static void GetString(Vector<char> str);

  // Calculate the SONAME for the V8 shared library.
  static void GetSONAME(Vector<char> str);

  static const char* GetVersion() { return version_string_; }

 private:
  // NOTE: can't make these really const because of test-version.cc.
  static int major_;
  static int minor_;
  static int build_;
  static int patch_;
  static bool candidate_;
  static const char* soname_;
  static const char* version_string_;

  // In test-version.cc.
  friend void SetVersion(int major, int minor, int build, int patch,
                         bool candidate, const char* soname);
};

} }  // namespace v8::internal

#endif  // V8_VERSION_H_
