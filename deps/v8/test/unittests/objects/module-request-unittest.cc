// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects-inl.h"
#include "src/objects/source-text-module.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using ModuleRequestTest = TestWithIsolate;

TEST_F(ModuleRequestTest, LargePosition) {
  HandleScope scope(isolate());
  Handle<String> specifier = factory()->NewStringFromAsciiChecked("foo.js");
  Handle<FixedArray> import_attributes = factory()->empty_fixed_array();

  // 270MB is 283115520.
  // 283115520 << 2 = 1132462080.
  // kSmiMaxValue is 1073741823.
  int large_position = 283115520;

  Handle<ModuleRequest> request =
      ModuleRequest::New(isolate(), specifier, ModuleImportPhase::kEvaluation,
                         import_attributes, large_position);

  EXPECT_EQ(request->position(), static_cast<unsigned>(large_position));
}

}  // namespace internal
}  // namespace v8
