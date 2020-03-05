// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>

#include "serializable.h"
#include "test_platform.h"

namespace v8_crdtp {
// =============================================================================
// Serializable - An object to be emitted as a sequence of bytes.
// =============================================================================

namespace {
// Tests the default behavior for ::TakeSerialized (to invoke
// ::AppendSerialized).
class SimpleExample : public Serializable {
 public:
  explicit SimpleExample(const std::vector<uint8_t>& contents)
      : contents_(contents) {}

  void AppendSerialized(std::vector<uint8_t>* out) const override {
    out->insert(out->end(), contents_.begin(), contents_.end());
  }

 private:
  std::vector<uint8_t> contents_;
};
}  // namespace

TEST(SerializableTest, YieldsContents) {
  std::vector<uint8_t> contents = {1, 2, 3};
  SimpleExample foo(contents);
  foo.AppendSerialized(&contents);  // Yields contents by appending.
  EXPECT_THAT(contents, testing::ElementsAre(1, 2, 3, 1, 2, 3));
  // Yields contents by returning.
  EXPECT_THAT(std::move(foo).TakeSerialized(), testing::ElementsAre(1, 2, 3));
}
}  // namespace v8_crdtp
