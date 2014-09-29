// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/checks.h"

#include "src/v8.h"

namespace v8 {
namespace internal {

intptr_t HeapObjectTagMask() { return kHeapObjectTagMask; }

} }  // namespace v8::internal


static bool CheckEqualsStrict(volatile double* exp, volatile double* val) {
  v8::internal::DoubleRepresentation exp_rep(*exp);
  v8::internal::DoubleRepresentation val_rep(*val);
  if (std::isnan(exp_rep.value) && std::isnan(val_rep.value)) return true;
  return exp_rep.bits == val_rep.bits;
}


void CheckEqualsHelper(const char* file, int line, const char* expected_source,
                       double expected, const char* value_source,
                       double value) {
  // Force values to 64 bit memory to truncate 80 bit precision on IA32.
  volatile double* exp = new double[1];
  *exp = expected;
  volatile double* val = new double[1];
  *val = value;
  if (!CheckEqualsStrict(exp, val)) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %f\n#   Found: %f",
             expected_source, value_source, *exp, *val);
  }
  delete[] exp;
  delete[] val;
}


void CheckNonEqualsHelper(const char* file, int line,
                          const char* expected_source, double expected,
                          const char* value_source, double value) {
  // Force values to 64 bit memory to truncate 80 bit precision on IA32.
  volatile double* exp = new double[1];
  *exp = expected;
  volatile double* val = new double[1];
  *val = value;
  if (CheckEqualsStrict(exp, val)) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %f\n#   Found: %f",
             expected_source, value_source, *exp, *val);
  }
  delete[] exp;
  delete[] val;
}


void CheckEqualsHelper(const char* file,
                       int line,
                       const char* expected_source,
                       v8::Handle<v8::Value> expected,
                       const char* value_source,
                       v8::Handle<v8::Value> value) {
  if (!expected->Equals(value)) {
    v8::String::Utf8Value value_str(value);
    v8::String::Utf8Value expected_str(expected);
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %s\n#   Found: %s",
             expected_source, value_source, *expected_str, *value_str);
  }
}


void CheckNonEqualsHelper(const char* file,
                          int line,
                          const char* unexpected_source,
                          v8::Handle<v8::Value> unexpected,
                          const char* value_source,
                          v8::Handle<v8::Value> value) {
  if (unexpected->Equals(value)) {
    v8::String::Utf8Value value_str(value);
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %s",
             unexpected_source, value_source, *value_str);
  }
}
