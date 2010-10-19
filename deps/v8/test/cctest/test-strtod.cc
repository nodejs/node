// Copyright 2006-2008 the V8 project authors. All rights reserved.

#include <stdlib.h>

#include "v8.h"

#include "cctest.h"
#include "strtod.h"

using namespace v8::internal;

static Vector<const char> StringToVector(const char* str) {
  return Vector<const char>(str, StrLength(str));
}


static double StrtodChar(const char* str, int exponent) {
  return Strtod(StringToVector(str), exponent);
}


TEST(Strtod) {
  Vector<const char> vector;

  vector = StringToVector("0");
  CHECK_EQ(0.0, Strtod(vector, 1));
  CHECK_EQ(0.0, Strtod(vector, 2));
  CHECK_EQ(0.0, Strtod(vector, -2));
  CHECK_EQ(0.0, Strtod(vector, -999));
  CHECK_EQ(0.0, Strtod(vector, +999));

  vector = StringToVector("1");
  CHECK_EQ(1.0, Strtod(vector, 0));
  CHECK_EQ(10.0, Strtod(vector, 1));
  CHECK_EQ(100.0, Strtod(vector, 2));
  CHECK_EQ(1e20, Strtod(vector, 20));
  CHECK_EQ(1e22, Strtod(vector, 22));
  CHECK_EQ(1e23, Strtod(vector, 23));
  CHECK_EQ(1e35, Strtod(vector, 35));
  CHECK_EQ(1e36, Strtod(vector, 36));
  CHECK_EQ(1e37, Strtod(vector, 37));
  CHECK_EQ(1e-1, Strtod(vector, -1));
  CHECK_EQ(1e-2, Strtod(vector, -2));
  CHECK_EQ(1e-5, Strtod(vector, -5));
  CHECK_EQ(1e-20, Strtod(vector, -20));
  CHECK_EQ(1e-22, Strtod(vector, -22));
  CHECK_EQ(1e-23, Strtod(vector, -23));
  CHECK_EQ(1e-25, Strtod(vector, -25));
  CHECK_EQ(1e-39, Strtod(vector, -39));

  vector = StringToVector("2");
  CHECK_EQ(2.0, Strtod(vector, 0));
  CHECK_EQ(20.0, Strtod(vector, 1));
  CHECK_EQ(200.0, Strtod(vector, 2));
  CHECK_EQ(2e20, Strtod(vector, 20));
  CHECK_EQ(2e22, Strtod(vector, 22));
  CHECK_EQ(2e23, Strtod(vector, 23));
  CHECK_EQ(2e35, Strtod(vector, 35));
  CHECK_EQ(2e36, Strtod(vector, 36));
  CHECK_EQ(2e37, Strtod(vector, 37));
  CHECK_EQ(2e-1, Strtod(vector, -1));
  CHECK_EQ(2e-2, Strtod(vector, -2));
  CHECK_EQ(2e-5, Strtod(vector, -5));
  CHECK_EQ(2e-20, Strtod(vector, -20));
  CHECK_EQ(2e-22, Strtod(vector, -22));
  CHECK_EQ(2e-23, Strtod(vector, -23));
  CHECK_EQ(2e-25, Strtod(vector, -25));
  CHECK_EQ(2e-39, Strtod(vector, -39));

  vector = StringToVector("9");
  CHECK_EQ(9.0, Strtod(vector, 0));
  CHECK_EQ(90.0, Strtod(vector, 1));
  CHECK_EQ(900.0, Strtod(vector, 2));
  CHECK_EQ(9e20, Strtod(vector, 20));
  CHECK_EQ(9e22, Strtod(vector, 22));
  CHECK_EQ(9e23, Strtod(vector, 23));
  CHECK_EQ(9e35, Strtod(vector, 35));
  CHECK_EQ(9e36, Strtod(vector, 36));
  CHECK_EQ(9e37, Strtod(vector, 37));
  CHECK_EQ(9e-1, Strtod(vector, -1));
  CHECK_EQ(9e-2, Strtod(vector, -2));
  CHECK_EQ(9e-5, Strtod(vector, -5));
  CHECK_EQ(9e-20, Strtod(vector, -20));
  CHECK_EQ(9e-22, Strtod(vector, -22));
  CHECK_EQ(9e-23, Strtod(vector, -23));
  CHECK_EQ(9e-25, Strtod(vector, -25));
  CHECK_EQ(9e-39, Strtod(vector, -39));

  vector = StringToVector("12345");
  CHECK_EQ(12345.0, Strtod(vector, 0));
  CHECK_EQ(123450.0, Strtod(vector, 1));
  CHECK_EQ(1234500.0, Strtod(vector, 2));
  CHECK_EQ(12345e20, Strtod(vector, 20));
  CHECK_EQ(12345e22, Strtod(vector, 22));
  CHECK_EQ(12345e23, Strtod(vector, 23));
  CHECK_EQ(12345e30, Strtod(vector, 30));
  CHECK_EQ(12345e31, Strtod(vector, 31));
  CHECK_EQ(12345e32, Strtod(vector, 32));
  CHECK_EQ(12345e35, Strtod(vector, 35));
  CHECK_EQ(12345e36, Strtod(vector, 36));
  CHECK_EQ(12345e37, Strtod(vector, 37));
  CHECK_EQ(12345e-1, Strtod(vector, -1));
  CHECK_EQ(12345e-2, Strtod(vector, -2));
  CHECK_EQ(12345e-5, Strtod(vector, -5));
  CHECK_EQ(12345e-20, Strtod(vector, -20));
  CHECK_EQ(12345e-22, Strtod(vector, -22));
  CHECK_EQ(12345e-23, Strtod(vector, -23));
  CHECK_EQ(12345e-25, Strtod(vector, -25));
  CHECK_EQ(12345e-39, Strtod(vector, -39));

  vector = StringToVector("12345678901234");
  CHECK_EQ(12345678901234.0, Strtod(vector, 0));
  CHECK_EQ(123456789012340.0, Strtod(vector, 1));
  CHECK_EQ(1234567890123400.0, Strtod(vector, 2));
  CHECK_EQ(12345678901234e20, Strtod(vector, 20));
  CHECK_EQ(12345678901234e22, Strtod(vector, 22));
  CHECK_EQ(12345678901234e23, Strtod(vector, 23));
  CHECK_EQ(12345678901234e30, Strtod(vector, 30));
  CHECK_EQ(12345678901234e31, Strtod(vector, 31));
  CHECK_EQ(12345678901234e32, Strtod(vector, 32));
  CHECK_EQ(12345678901234e35, Strtod(vector, 35));
  CHECK_EQ(12345678901234e36, Strtod(vector, 36));
  CHECK_EQ(12345678901234e37, Strtod(vector, 37));
  CHECK_EQ(12345678901234e-1, Strtod(vector, -1));
  CHECK_EQ(12345678901234e-2, Strtod(vector, -2));
  CHECK_EQ(12345678901234e-5, Strtod(vector, -5));
  CHECK_EQ(12345678901234e-20, Strtod(vector, -20));
  CHECK_EQ(12345678901234e-22, Strtod(vector, -22));
  CHECK_EQ(12345678901234e-23, Strtod(vector, -23));
  CHECK_EQ(12345678901234e-25, Strtod(vector, -25));
  CHECK_EQ(12345678901234e-39, Strtod(vector, -39));

  vector = StringToVector("123456789012345");
  CHECK_EQ(123456789012345.0, Strtod(vector, 0));
  CHECK_EQ(1234567890123450.0, Strtod(vector, 1));
  CHECK_EQ(12345678901234500.0, Strtod(vector, 2));
  CHECK_EQ(123456789012345e20, Strtod(vector, 20));
  CHECK_EQ(123456789012345e22, Strtod(vector, 22));
  CHECK_EQ(123456789012345e23, Strtod(vector, 23));
  CHECK_EQ(123456789012345e35, Strtod(vector, 35));
  CHECK_EQ(123456789012345e36, Strtod(vector, 36));
  CHECK_EQ(123456789012345e37, Strtod(vector, 37));
  CHECK_EQ(123456789012345e39, Strtod(vector, 39));
  CHECK_EQ(123456789012345e-1, Strtod(vector, -1));
  CHECK_EQ(123456789012345e-2, Strtod(vector, -2));
  CHECK_EQ(123456789012345e-5, Strtod(vector, -5));
  CHECK_EQ(123456789012345e-20, Strtod(vector, -20));
  CHECK_EQ(123456789012345e-22, Strtod(vector, -22));
  CHECK_EQ(123456789012345e-23, Strtod(vector, -23));
  CHECK_EQ(123456789012345e-25, Strtod(vector, -25));
  CHECK_EQ(123456789012345e-39, Strtod(vector, -39));

  CHECK_EQ(0.0, StrtodChar("0", 12345));
  CHECK_EQ(0.0, StrtodChar("", 1324));
  CHECK_EQ(0.0, StrtodChar("000000000", 123));
  CHECK_EQ(0.0, StrtodChar("2", -324));
  CHECK_EQ(4e-324, StrtodChar("3", -324));
  // It would be more readable to put non-zero literals on the left side (i.e.
  //   CHECK_EQ(1e-325, StrtodChar("1", -325))), but then Gcc complains that
  // they are truncated to zero.
  CHECK_EQ(0.0, StrtodChar("1", -325));
  CHECK_EQ(0.0, StrtodChar("1", -325));
  CHECK_EQ(0.0, StrtodChar("20000", -328));
  CHECK_EQ(40000e-328, StrtodChar("30000", -328));
  CHECK_EQ(0.0, StrtodChar("10000", -329));
  CHECK_EQ(0.0, StrtodChar("90000", -329));
  CHECK_EQ(0.0, StrtodChar("000000001", -325));
  CHECK_EQ(0.0, StrtodChar("000000001", -325));
  CHECK_EQ(0.0, StrtodChar("0000000020000", -328));
  CHECK_EQ(40000e-328, StrtodChar("00000030000", -328));
  CHECK_EQ(0.0, StrtodChar("0000000010000", -329));
  CHECK_EQ(0.0, StrtodChar("0000000090000", -329));

  // It would be more readable to put the literals (and not V8_INFINITY) on the
  // left side (i.e. CHECK_EQ(1e309, StrtodChar("1", 309))), but then Gcc
  // complains that the floating constant exceeds range of 'double'.
  CHECK_EQ(V8_INFINITY, StrtodChar("1", 309));
  CHECK_EQ(1e308, StrtodChar("1", 308));
  CHECK_EQ(1234e305, StrtodChar("1234", 305));
  CHECK_EQ(1234e304, StrtodChar("1234", 304));
  CHECK_EQ(V8_INFINITY, StrtodChar("18", 307));
  CHECK_EQ(17e307, StrtodChar("17", 307));
  CHECK_EQ(V8_INFINITY, StrtodChar("0000001", 309));
  CHECK_EQ(1e308, StrtodChar("00000001", 308));
  CHECK_EQ(1234e305, StrtodChar("00000001234", 305));
  CHECK_EQ(1234e304, StrtodChar("000000001234", 304));
  CHECK_EQ(V8_INFINITY, StrtodChar("0000000018", 307));
  CHECK_EQ(17e307, StrtodChar("0000000017", 307));
  CHECK_EQ(V8_INFINITY, StrtodChar("1000000", 303));
  CHECK_EQ(1e308, StrtodChar("100000", 303));
  CHECK_EQ(1234e305, StrtodChar("123400000", 300));
  CHECK_EQ(1234e304, StrtodChar("123400000", 299));
  CHECK_EQ(V8_INFINITY, StrtodChar("180000000", 300));
  CHECK_EQ(17e307, StrtodChar("170000000", 300));
  CHECK_EQ(V8_INFINITY, StrtodChar("00000001000000", 303));
  CHECK_EQ(1e308, StrtodChar("000000000000100000", 303));
  CHECK_EQ(1234e305, StrtodChar("00000000123400000", 300));
  CHECK_EQ(1234e304, StrtodChar("0000000123400000", 299));
  CHECK_EQ(V8_INFINITY, StrtodChar("00000000180000000", 300));
  CHECK_EQ(17e307, StrtodChar("00000000170000000", 300));
}
