// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(AlignedAddressTest, AlignedAddress) {
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFF0), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFF2), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFF2), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFFF), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0x0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFFF), 0x100000));
}

struct TriviallyCopyable {
  const int i;
};
ASSERT_TRIVIALLY_COPYABLE(TriviallyCopyable);

struct StillTriviallyCopyable {
  const int i;
  StillTriviallyCopyable(const StillTriviallyCopyable&) = delete;
};
ASSERT_TRIVIALLY_COPYABLE(StillTriviallyCopyable);

struct NonTrivialDestructor {
  ~NonTrivialDestructor() {}
};
ASSERT_NOT_TRIVIALLY_COPYABLE(NonTrivialDestructor);

struct NonTrivialCopyConstructor {
  NonTrivialCopyConstructor(const NonTrivialCopyConstructor&) {}
};
ASSERT_NOT_TRIVIALLY_COPYABLE(NonTrivialCopyConstructor);

struct NonTrivialMoveConstructor {
  NonTrivialMoveConstructor(const NonTrivialMoveConstructor&) {}
};
ASSERT_NOT_TRIVIALLY_COPYABLE(NonTrivialMoveConstructor);

struct NonTrivialCopyAssignment {
  NonTrivialCopyAssignment(const NonTrivialCopyAssignment&) {}
};
ASSERT_NOT_TRIVIALLY_COPYABLE(NonTrivialCopyAssignment);

struct NonTrivialMoveAssignment {
  NonTrivialMoveAssignment(const NonTrivialMoveAssignment&) {}
};
ASSERT_NOT_TRIVIALLY_COPYABLE(NonTrivialMoveAssignment);

}  // namespace base
}  // namespace v8
