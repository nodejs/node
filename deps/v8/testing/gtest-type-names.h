// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TESTING_GTEST_TYPE_NAMES_H_
#define V8_TESTING_GTEST_TYPE_NAMES_H_

#include "include/v8stdint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace testing {
namespace internal {

#define GET_TYPE_NAME(type)         \
  template <>                       \
  std::string GetTypeName<type>() { \
    return #type;                   \
  }
GET_TYPE_NAME(int8_t)
GET_TYPE_NAME(uint8_t)
GET_TYPE_NAME(int16_t)
GET_TYPE_NAME(uint16_t)
GET_TYPE_NAME(int32_t)
GET_TYPE_NAME(uint32_t)
GET_TYPE_NAME(int64_t)
GET_TYPE_NAME(uint64_t)
GET_TYPE_NAME(float)
GET_TYPE_NAME(double)
#undef GET_TYPE_NAME

}  // namespace internal
}  // namespace testing

#endif  // V8_TESTING_GTEST_TYPE_NAMES_H_
