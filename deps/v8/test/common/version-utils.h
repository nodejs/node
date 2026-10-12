// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_VERSION_UTILS_H_
#define V8_TEST_COMMON_VERSION_UTILS_H_

#include "src/utils/version.h"

namespace v8::internal {

class ScopedVersionEmbedderString final {
 public:
  explicit ScopedVersionEmbedderString(const char* embedder)
      : previous_(Version::embedder_) {
    Version::embedder_ = embedder;
  }

  ~ScopedVersionEmbedderString() { Version::embedder_ = previous_; }

  ScopedVersionEmbedderString(const ScopedVersionEmbedderString&) = delete;
  ScopedVersionEmbedderString& operator=(const ScopedVersionEmbedderString&) =
      delete;

 private:
  const char* const previous_;
};

}  // namespace v8::internal

#endif  // V8_TEST_COMMON_VERSION_UTILS_H_
