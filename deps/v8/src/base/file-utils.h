// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FILE_UTILS_H_
#define V8_FILE_UTILS_H_

namespace v8 {
namespace internal {

// Helper functions to manipulate file paths.

char* RelativePath(char** buffer, const char* exec_path, const char* name);

}  // namespace internal
}  // namespace v8

#endif  // V8_FILE_UTILS_H_
