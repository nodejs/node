// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/version.h"

#include "include/v8-version.h"
#include "src/utils.h"

// Define SONAME to have the build system put a specific SONAME into the
// shared library instead the generic SONAME generated from the V8 version
// number. This define is mainly used by the build system script.
#define SONAME            ""

#if V8_IS_CANDIDATE_VERSION
#define CANDIDATE_STRING " (candidate)"
#else
#define CANDIDATE_STRING ""
#endif

#define SX(x) #x
#define S(x) SX(x)

#if V8_PATCH_LEVEL > 0
#define VERSION_STRING                                                      \
  S(V8_MAJOR_VERSION) "." S(V8_MINOR_VERSION) "." S(V8_BUILD_NUMBER) "." S( \
      V8_PATCH_LEVEL) CANDIDATE_STRING
#else
#define VERSION_STRING                                               \
  S(V8_MAJOR_VERSION) "." S(V8_MINOR_VERSION) "." S(V8_BUILD_NUMBER) \
      CANDIDATE_STRING
#endif

namespace v8 {
namespace internal {

int Version::major_ = V8_MAJOR_VERSION;
int Version::minor_ = V8_MINOR_VERSION;
int Version::build_ = V8_BUILD_NUMBER;
int Version::patch_ = V8_PATCH_LEVEL;
bool Version::candidate_ = (V8_IS_CANDIDATE_VERSION != 0);
const char* Version::soname_ = SONAME;
const char* Version::version_string_ = VERSION_STRING;

// Calculate the V8 version string.
void Version::GetString(Vector<char> str) {
  const char* candidate = IsCandidate() ? " (candidate)" : "";
#ifdef USE_SIMULATOR
  const char* is_simulator = " SIMULATOR";
#else
  const char* is_simulator = "";
#endif  // USE_SIMULATOR
  if (GetPatch() > 0) {
    SNPrintF(str, "%d.%d.%d.%d%s%s",
             GetMajor(), GetMinor(), GetBuild(), GetPatch(), candidate,
             is_simulator);
  } else {
    SNPrintF(str, "%d.%d.%d%s%s",
             GetMajor(), GetMinor(), GetBuild(), candidate,
             is_simulator);
  }
}


// Calculate the SONAME for the V8 shared library.
void Version::GetSONAME(Vector<char> str) {
  if (soname_ == NULL || *soname_ == '\0') {
    // Generate generic SONAME if no specific SONAME is defined.
    const char* candidate = IsCandidate() ? "-candidate" : "";
    if (GetPatch() > 0) {
      SNPrintF(str, "libv8-%d.%d.%d.%d%s.so",
               GetMajor(), GetMinor(), GetBuild(), GetPatch(), candidate);
    } else {
      SNPrintF(str, "libv8-%d.%d.%d%s.so",
               GetMajor(), GetMinor(), GetBuild(), candidate);
    }
  } else {
    // Use specific SONAME.
    SNPrintF(str, "%s", soname_);
  }
}

}  // namespace internal
}  // namespace v8
