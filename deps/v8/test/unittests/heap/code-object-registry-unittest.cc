// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(CodeObjectRegistry, RegisterAlreadyExistingObjectsAndContains) {
  CodeObjectRegistry registry;
  const int elements = 10;
  const int offset = 100;
  for (int i = 0; i < elements; i++) {
    registry.RegisterAlreadyExistingCodeObject(i * offset);
  }

  for (int i = 0; i < elements; i++) {
    CHECK(registry.Contains(i * offset));
  }
}

TEST(CodeObjectRegistry, RegisterNewlyAllocatedObjectsAndContains) {
  CodeObjectRegistry registry;
  const int elements = 10;
  const int offset = 100;
  for (int i = 0; i < elements; i++) {
    registry.RegisterNewlyAllocatedCodeObject(i * offset);
  }

  for (int i = 0; i < elements; i++) {
    CHECK(registry.Contains(i * offset));
  }
}

TEST(CodeObjectRegistry, FindAlreadyExistingObjects) {
  CodeObjectRegistry registry;
  const int elements = 10;
  const int offset = 100;
  const int inner = 2;
  for (int i = 1; i <= elements; i++) {
    registry.RegisterAlreadyExistingCodeObject(i * offset);
  }

  for (int i = 1; i <= elements; i++) {
    for (int j = 0; j < inner; j++) {
      CHECK_EQ(registry.GetCodeObjectStartFromInnerAddress(i * offset + j),
               i * offset);
    }
  }
}

TEST(CodeObjectRegistry, FindNewlyAllocatedObjects) {
  CodeObjectRegistry registry;
  const int elements = 10;
  const int offset = 100;
  const int inner = 2;
  for (int i = 1; i <= elements; i++) {
    registry.RegisterNewlyAllocatedCodeObject(i * offset);
  }

  for (int i = 1; i <= elements; i++) {
    for (int j = 0; j < inner; j++) {
      CHECK_EQ(registry.GetCodeObjectStartFromInnerAddress(i * offset + j),
               i * offset);
    }
  }
}

TEST(CodeObjectRegistry, FindAlternatingObjects) {
  CodeObjectRegistry registry;
  const int elements = 10;
  const int offset = 100;
  const int inner = 2;
  for (int i = 1; i <= elements; i++) {
    if (i % 2 == 0) {
      registry.RegisterAlreadyExistingCodeObject(i * offset);
    } else {
      registry.RegisterNewlyAllocatedCodeObject(i * offset);
    }
  }

  for (int i = 1; i <= elements; i++) {
    for (int j = 0; j < inner; j++) {
      CHECK_EQ(registry.GetCodeObjectStartFromInnerAddress(i * offset + j),
               i * offset);
    }
  }
}
}  // namespace internal
}  // namespace v8
