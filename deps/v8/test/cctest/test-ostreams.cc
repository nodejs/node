// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <limits>

#include "include/v8stdint.h"
#include "src/ostreams.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


TEST(OStringStreamConstructor) {
  OStringStream oss;
  const size_t expected_size = 0;
  CHECK(expected_size == oss.size());
  CHECK_GT(oss.capacity(), 0);
  CHECK_NE(NULL, oss.data());
  CHECK_EQ("", oss.c_str());
}


#define TEST_STRING            \
  "Ash nazg durbatuluk, "      \
  "ash nazg gimbatul, "        \
  "ash nazg thrakatuluk, "     \
  "agh burzum-ishi krimpatul."

TEST(OStringStreamGrow) {
  OStringStream oss;
  const int repeat = 30;
  size_t len = strlen(TEST_STRING);
  for (int i = 0; i < repeat; ++i) {
    oss.write(TEST_STRING, len);
  }
  const char* expected =
      TEST_STRING TEST_STRING TEST_STRING TEST_STRING TEST_STRING
      TEST_STRING TEST_STRING TEST_STRING TEST_STRING TEST_STRING
      TEST_STRING TEST_STRING TEST_STRING TEST_STRING TEST_STRING
      TEST_STRING TEST_STRING TEST_STRING TEST_STRING TEST_STRING
      TEST_STRING TEST_STRING TEST_STRING TEST_STRING TEST_STRING
      TEST_STRING TEST_STRING TEST_STRING TEST_STRING TEST_STRING;
  const size_t expected_len = len * repeat;
  CHECK(expected_len == oss.size());
  CHECK_GT(oss.capacity(), 0);
  CHECK_EQ(0, strncmp(expected, oss.data(), expected_len));
  CHECK_EQ(expected, oss.c_str());
}


template <class T>
static void check(const char* expected, T value) {
  OStringStream oss;
  oss << value << " " << hex << value;
  CHECK_EQ(expected, oss.c_str());
}


TEST(NumericFormatting) {
  check<bool>("0 0", false);
  check<bool>("1 1", true);

  check<int16_t>("-12345 cfc7", -12345);
  check<int16_t>("-32768 8000", std::numeric_limits<int16_t>::min());
  check<int16_t>("32767 7fff", std::numeric_limits<int16_t>::max());

  check<uint16_t>("34567 8707", 34567);
  check<uint16_t>("0 0", std::numeric_limits<uint16_t>::min());
  check<uint16_t>("65535 ffff", std::numeric_limits<uint16_t>::max());

  check<int32_t>("-1234567 ffed2979", -1234567);
  check<int32_t>("-2147483648 80000000", std::numeric_limits<int32_t>::min());
  check<int32_t>("2147483647 7fffffff", std::numeric_limits<int32_t>::max());

  check<uint32_t>("3456789 34bf15", 3456789);
  check<uint32_t>("0 0", std::numeric_limits<uint32_t>::min());
  check<uint32_t>("4294967295 ffffffff", std::numeric_limits<uint32_t>::max());

  check<int64_t>("-1234567 ffffffffffed2979", -1234567);
  check<int64_t>("-9223372036854775808 8000000000000000",
                 std::numeric_limits<int64_t>::min());
  check<int64_t>("9223372036854775807 7fffffffffffffff",
                 std::numeric_limits<int64_t>::max());

  check<uint64_t>("3456789 34bf15", 3456789);
  check<uint64_t>("0 0", std::numeric_limits<uint64_t>::min());
  check<uint64_t>("18446744073709551615 ffffffffffffffff",
                  std::numeric_limits<uint64_t>::max());

  check<float>("0 0", 0.0f);
  check<float>("123 123", 123.0f);
  check<float>("-0.5 -0.5", -0.5f);
  check<float>("1.25 1.25", 1.25f);
  check<float>("0.0625 0.0625", 6.25e-2f);

  check<double>("0 0", 0.0);
  check<double>("123 123", 123.0);
  check<double>("-0.5 -0.5", -0.5);
  check<double>("1.25 1.25", 1.25);
  check<double>("0.0625 0.0625", 6.25e-2);
}


TEST(CharacterOutput) {
  check<char>("a a", 'a');
  check<signed char>("B B", 'B');
  check<unsigned char>("9 9", '9');
  check<const char*>("bye bye", "bye");

  OStringStream os;
  os.put('H').write("ello", 4);
  CHECK_EQ("Hello", os.c_str());
}


TEST(Manipulators) {
  OStringStream os;
  os << 123 << hex << 123 << endl << 123 << dec << 123 << 123;
  CHECK_EQ("1237b\n7b123123", os.c_str());
}


class MiscStuff {
 public:
  MiscStuff(int i, double d, const char* s) : i_(i), d_(d), s_(s) { }

 private:
  friend OStream& operator<<(OStream& os, const MiscStuff& m);

  int i_;
  double d_;
  const char* s_;
};


OStream& operator<<(OStream& os, const MiscStuff& m) {
  return os << "{i:" << m.i_ << ", d:" << m.d_ << ", s:'" << m.s_ << "'}";
}


TEST(CustomOutput) {
  OStringStream os;
  MiscStuff m(123, 4.5, "Hurz!");
  os << m;
  CHECK_EQ("{i:123, d:4.5, s:'Hurz!'}", os.c_str());
}
