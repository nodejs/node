// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_STARTUP_DATA_UTIL_H_
#define V8_INIT_STARTUP_DATA_UTIL_H_

#include "include/v8.h"

namespace v8 {
namespace internal {

// Helper functions to load external startup data.
//
// This is meant as a convenience for stand-alone binaries like d8, cctest,
// unittest. A V8 embedder would likely either handle startup data on their
// own or just disable the feature if they don't want to handle it at all,
// while tools like cctest need to work in either configuration.

void InitializeExternalStartupData(const char* directory_path);
void InitializeExternalStartupDataFromFile(const char* snapshot_blob);

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_STARTUP_DATA_UTIL_H_
