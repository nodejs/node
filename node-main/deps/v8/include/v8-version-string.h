// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VERSION_STRING_H_
#define V8_VERSION_STRING_H_

#include "v8-version.h"  // NOLINT(build/include_directory)

// This is here rather than v8-version.h to keep that file simple and
// machine-processable.

#if V8_IS_CANDIDATE_VERSION
#define V8_CANDIDATE_STRING " (candidate)"
#else
#define V8_CANDIDATE_STRING ""
#endif

#ifndef V8_EMBEDDER_STRING
#define V8_EMBEDDER_STRING ""
#endif

#define V8_SX(x) #x
#define V8_S(x) V8_SX(x)

#if V8_PATCH_LEVEL > 0
#define V8_VERSION_STRING                                        \
  V8_S(V8_MAJOR_VERSION)                                         \
  "." V8_S(V8_MINOR_VERSION) "." V8_S(V8_BUILD_NUMBER) "." V8_S( \
      V8_PATCH_LEVEL) V8_EMBEDDER_STRING V8_CANDIDATE_STRING
#else
#define V8_VERSION_STRING                              \
  V8_S(V8_MAJOR_VERSION)                               \
  "." V8_S(V8_MINOR_VERSION) "." V8_S(V8_BUILD_NUMBER) \
      V8_EMBEDDER_STRING V8_CANDIDATE_STRING
#endif

#endif  // V8_VERSION_STRING_H_
