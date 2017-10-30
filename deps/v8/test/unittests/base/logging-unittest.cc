// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "src/base/logging.h"
#include "src/objects.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace base {

namespace {

#define CHECK_SUCCEED(NAME, lhs, rhs)                                      \
  {                                                                        \
    std::string* error_message =                                           \
        Check##NAME##Impl<decltype(lhs), decltype(rhs)>((lhs), (rhs), ""); \
    EXPECT_EQ(nullptr, error_message);                                     \
  }

#define CHECK_FAIL(NAME, lhs, rhs)                                         \
  {                                                                        \
    std::string* error_message =                                           \
        Check##NAME##Impl<decltype(lhs), decltype(rhs)>((lhs), (rhs), ""); \
    EXPECT_NE(nullptr, error_message);                                     \
    delete error_message;                                                  \
  }

}  // namespace

TEST(LoggingTest, CheckEQImpl) {
  CHECK_SUCCEED(EQ, 0.0, 0.0)
  CHECK_SUCCEED(EQ, 0.0, -0.0)
  CHECK_SUCCEED(EQ, -0.0, 0.0)
  CHECK_SUCCEED(EQ, -0.0, -0.0)
}

TEST(LoggingTest, CompareSignedMismatch) {
  CHECK_SUCCEED(EQ, static_cast<int32_t>(14), static_cast<uint32_t>(14))
  CHECK_FAIL(EQ, static_cast<int32_t>(14), static_cast<uint32_t>(15))
  CHECK_FAIL(EQ, static_cast<int32_t>(-1), static_cast<uint32_t>(-1))
  CHECK_SUCCEED(LT, static_cast<int32_t>(-1), static_cast<uint32_t>(0))
  CHECK_SUCCEED(LT, static_cast<int32_t>(-1), static_cast<uint32_t>(-1))
  CHECK_SUCCEED(LE, static_cast<int32_t>(-1), static_cast<uint32_t>(0))
  CHECK_SUCCEED(LE, static_cast<int32_t>(55), static_cast<uint32_t>(55))
  CHECK_SUCCEED(LT, static_cast<int32_t>(55), static_cast<uint32_t>(0x7fffff00))
  CHECK_SUCCEED(LE, static_cast<int32_t>(55), static_cast<uint32_t>(0x7fffff00))
  CHECK_SUCCEED(GE, static_cast<uint32_t>(0x7fffff00), static_cast<int32_t>(55))
  CHECK_SUCCEED(GT, static_cast<uint32_t>(0x7fffff00), static_cast<int32_t>(55))
  CHECK_SUCCEED(GT, static_cast<uint32_t>(-1), static_cast<int32_t>(-1))
  CHECK_SUCCEED(GE, static_cast<uint32_t>(0), static_cast<int32_t>(-1))
  CHECK_SUCCEED(LT, static_cast<int8_t>(-1), static_cast<uint32_t>(0))
  CHECK_SUCCEED(GT, static_cast<uint64_t>(0x7f01010101010101), 0)
  CHECK_SUCCEED(LE, static_cast<int64_t>(0xff01010101010101),
                static_cast<uint8_t>(13))
}

TEST(LoggingTest, CompareAgainstStaticConstPointer) {
  // These used to produce link errors before http://crrev.com/2524093002.
  CHECK_FAIL(EQ, v8::internal::Smi::kZero, v8::internal::Smi::FromInt(17));
  CHECK_SUCCEED(GT, 0, v8::internal::Smi::kMinValue);
}

#define CHECK_BOTH(name, lhs, rhs) \
  CHECK_##name(lhs, rhs);          \
  DCHECK_##name(lhs, rhs)

TEST(LoggingTest, CompareWithDifferentSignedness) {
  int32_t i32 = 10;
  uint32_t u32 = 20;
  int64_t i64 = 30;
  uint64_t u64 = 40;

  // All these checks should compile (!) and succeed.
  CHECK_BOTH(EQ, i32 + 10, u32);
  CHECK_BOTH(LT, i32, u64);
  CHECK_BOTH(LE, u32, i64);
  CHECK_BOTH(IMPLIES, i32, i64);
  CHECK_BOTH(IMPLIES, u32, i64);
  CHECK_BOTH(IMPLIES, !u32, !i64);
}

TEST(LoggingTest, CompareWithReferenceType) {
  int32_t i32 = 10;
  uint32_t u32 = 20;
  int64_t i64 = 30;
  uint64_t u64 = 40;

  // All these checks should compile (!) and succeed.
  CHECK_BOTH(EQ, i32 + 10, *&u32);
  CHECK_BOTH(LT, *&i32, u64);
  CHECK_BOTH(IMPLIES, *&i32, i64);
  CHECK_BOTH(IMPLIES, *&i32, u64);
}

}  // namespace base
}  // namespace v8
