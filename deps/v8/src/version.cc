// Copyright 2008 the V8 project authors. All rights reserved.
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

// These macros define the version number for the current version.
// NOTE these macros are used by the SCons build script so their names
// cannot be changed without changing the SCons build script.
#define MAJOR_VERSION     2
#define MINOR_VERSION     2
#define BUILD_NUMBER      6
#define PATCH_LEVEL       0
#define CANDIDATE_VERSION false

// Define SONAME to have the SCons build the put a specific SONAME into the
// shared library instead the generic SONAME generated from the V8 version
// number. This define is mainly used by the SCons build script.
#define SONAME            ""

namespace v8 {
namespace internal {

int Version::major_ = MAJOR_VERSION;
int Version::minor_ = MINOR_VERSION;
int Version::build_ = BUILD_NUMBER;
int Version::patch_ = PATCH_LEVEL;
bool Version::candidate_ = CANDIDATE_VERSION;
const char* Version::soname_ = SONAME;


// Calculate the V8 version string.
void Version::GetString(Vector<char> str) {
  const char* candidate = IsCandidate() ? " (candidate)" : "";
  if (GetPatch() > 0) {
    OS::SNPrintF(str, "%d.%d.%d.%d%s",
                 GetMajor(), GetMinor(), GetBuild(), GetPatch(), candidate);
  } else {
    OS::SNPrintF(str, "%d.%d.%d%s",
                 GetMajor(), GetMinor(), GetBuild(), candidate);
  }
}


// Calculate the SONAME for the V8 shared library.
void Version::GetSONAME(Vector<char> str) {
  if (soname_ == NULL || *soname_ == '\0') {
    // Generate generic SONAME if no specific SONAME is defined.
    const char* candidate = IsCandidate() ? "-candidate" : "";
    if (GetPatch() > 0) {
      OS::SNPrintF(str, "libv8-%d.%d.%d.%d%s.so",
                   GetMajor(), GetMinor(), GetBuild(), GetPatch(), candidate);
    } else {
      OS::SNPrintF(str, "libv8-%d.%d.%d%s.so",
                   GetMajor(), GetMinor(), GetBuild(), candidate);
    }
  } else {
    // Use specific SONAME.
    OS::SNPrintF(str, "%s", soname_);
  }
}

} }  // namespace v8::internal
