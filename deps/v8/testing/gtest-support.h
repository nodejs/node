// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TESTING_GTEST_SUPPORT_H_
#define V8_TESTING_GTEST_SUPPORT_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace testing {
namespace internal {

#define GET_TYPE_NAME(type)                \
  template <>                              \
  inline std::string GetTypeName<type>() { \
    return #type;                          \
  }
GET_TYPE_NAME(bool)
GET_TYPE_NAME(signed char)
GET_TYPE_NAME(unsigned char)
GET_TYPE_NAME(short)
GET_TYPE_NAME(unsigned short)
GET_TYPE_NAME(int)
GET_TYPE_NAME(unsigned int)
GET_TYPE_NAME(long)
GET_TYPE_NAME(unsigned long)
GET_TYPE_NAME(long long)
GET_TYPE_NAME(unsigned long long)
GET_TYPE_NAME(float)
GET_TYPE_NAME(double)
#undef GET_TYPE_NAME


// TRACED_FOREACH(type, var, container) expands to a loop that assigns |var|
// every item in the |container| and adds a SCOPED_TRACE() message for the
// |var| while inside the loop body.
#define TRACED_FOREACH(_type, _var, _container)                          \
  for (_type const _var : _container)                                    \
    for (bool _var##_done = false; !_var##_done;)                        \
      for (SCOPED_TRACE(::testing::Message() << #_var << " = " << _var); \
           !_var##_done; _var##_done = true)

// TRACED_FORRANGE(type, var, low, high) expands to a loop that assigns |var|
// every value in the range |low| to (including) |high| and adds a
// SCOPED_TRACE() message for the |var| while inside the loop body.
// TODO(bmeurer): Migrate to C++11 once we're ready.
#define TRACED_FORRANGE(_type, _var, _low, _high)                          \
  for (_type _var##_i = _low; _var##_i <= _high; ++_var##_i)               \
    for (bool _var##_done = false; !_var##_done;)                          \
      for (_type const _var = _var##_i; !_var##_done;)                     \
        for (SCOPED_TRACE(::testing::Message() << #_var << " = " << _var); \
             !_var##_done; _var##_done = true)

}  // namespace internal
}  // namespace testing

#endif  // V8_TESTING_GTEST_SUPPORT_H_
