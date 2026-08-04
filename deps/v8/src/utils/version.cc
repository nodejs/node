// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/version.h"

#include "include/v8-version-string.h"
#include "include/v8-version.h"
#include "src/base/strings.h"

// Define SONAME to have the build system put a specific SONAME into the
// shared library instead the generic SONAME generated from the V8 version
// number. This define is mainly used by the build system script.
#define SONAME ""

namespace v8 {
namespace internal {

int Version::major_ = V8_MAJOR_VERSION;
int Version::minor_ = V8_MINOR_VERSION;
int Version::build_ = V8_BUILD_NUMBER;
int Version::patch_ = V8_PATCH_LEVEL;
const char* Version::embedder_ = V8_EMBEDDER_STRING;
bool Version::candidate_ = (V8_IS_CANDIDATE_VERSION != 0);
const char* Version::soname_ = SONAME;
const char* Version::version_string_ = V8_VERSION_STRING;

// Calculate the V8 version string.
void Version::GetString(base::Vector<char> str) {
  const char* candidate = IsCandidate() ? " (candidate)" : "";
  if (GetPatch() > 0) {
    base::SNPrintF(str, "%d.%d.%d.%d%s%s", GetMajor(), GetMinor(), GetBuild(),
                   GetPatch(), GetEmbedder(), candidate);
  } else {
    base::SNPrintF(str, "%d.%d.%d%s%s", GetMajor(), GetMinor(), GetBuild(),
                   GetEmbedder(), candidate);
  }
}

// Calculate the SONAME for the V8 shared library.
void Version::GetSONAME(base::Vector<char> str) {
  if (soname_ == nullptr || *soname_ == '\0') {
    // Generate generic SONAME if no specific SONAME is defined.
    const char* candidate = IsCandidate() ? "-candidate" : "";
    if (GetPatch() > 0) {
      SNPrintF(str, "libv8-%d.%d.%d.%d%s%s.so", GetMajor(), GetMinor(),
               GetBuild(), GetPatch(), GetEmbedder(), candidate);
    } else {
      SNPrintF(str, "libv8-%d.%d.%d%s%s.so", GetMajor(), GetMinor(), GetBuild(),
               GetEmbedder(), candidate);
    }
  } else {
    // Use specific SONAME.
    SNPrintF(str, "%s", soname_);
  }
}

#undef SONAME

}  // namespace internal
}  // namespace v8
