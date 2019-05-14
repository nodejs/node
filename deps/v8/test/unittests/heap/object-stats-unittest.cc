// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_set>

#include "src/heap/object-stats.h"
#include "src/objects-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace heap {

namespace {

template <typename T>
bool Contains(const std::unordered_set<T>& set, T needle) {
  return set.find(needle) != set.end();
}

}  // namespace

TEST(ObjectStats, NoClashWithInstanceTypes) {
  std::unordered_set<const char*> virtual_types;
#define ADD_VIRTUAL_INSTANCE_TYPE(type) virtual_types.insert(#type);
  VIRTUAL_INSTANCE_TYPE_LIST(ADD_VIRTUAL_INSTANCE_TYPE)
#undef ADD_VIRTUAL_INSTANCE_TYPE
#define CHECK_REGULARINSTANCE_TYPE(type) \
  EXPECT_FALSE(Contains(virtual_types, #type));
  INSTANCE_TYPE_LIST(CHECK_REGULARINSTANCE_TYPE)
#undef CHECK_REGULARINSTANCE_TYPE
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
