// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TESTING_GTEST_SUPPORT_H_
#define V8_TESTING_GTEST_SUPPORT_H_

#include "include/v8stdint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace testing {
namespace internal {

#define GET_TYPE_NAME(type)                \
  template <>                              \
  inline std::string GetTypeName<type>() { \
    return #type;                          \
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


// TRACED_FOREACH(type, var, array) expands to a loop that assigns |var| every
// item in the |array| and adds a SCOPED_TRACE() message for the |var| while
// inside the loop body.
// TODO(bmeurer): Migrate to C++11 once we're ready.
#define TRACED_FOREACH(_type, _var, _array)                                \
  for (size_t _i = 0; _i < arraysize(_array); ++_i)                        \
    for (bool _done = false; !_done;)                                      \
      for (_type const _var = _array[_i]; !_done;)                         \
        for (SCOPED_TRACE(::testing::Message() << #_var << " = " << _var); \
             !_done; _done = true)


// TRACED_FORRANGE(type, var, low, high) expands to a loop that assigns |var|
// every value in the range |low| to (including) |high| and adds a
// SCOPED_TRACE() message for the |var| while inside the loop body.
// TODO(bmeurer): Migrate to C++11 once we're ready.
#define TRACED_FORRANGE(_type, _var, _low, _high)                          \
  for (_type _i = _low; _i <= _high; ++_i)                                 \
    for (bool _done = false; !_done;)                                      \
      for (_type const _var = _i; !_done;)                                 \
        for (SCOPED_TRACE(::testing::Message() << #_var << " = " << _var); \
             !_done; _done = true)

}  // namespace internal
}  // namespace testing

#endif  // V8_TESTING_GTEST_SUPPORT_H_
