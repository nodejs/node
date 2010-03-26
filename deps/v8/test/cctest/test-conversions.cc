// Copyright 2006-2008 the V8 project authors. All rights reserved.

#include <stdlib.h>

#include "v8.h"

#include "platform.h"
#include "cctest.h"

using namespace v8::internal;


TEST(Hex) {
  CHECK_EQ(0.0, StringToDouble("0x0", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("0X0", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(1.0, StringToDouble("0x1", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(16.0, StringToDouble("0x10", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(255.0, StringToDouble("0xff", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(175.0, StringToDouble("0xAF", ALLOW_HEX | ALLOW_OCTALS));

  CHECK_EQ(0.0, StringToDouble("0x0", ALLOW_HEX));
  CHECK_EQ(0.0, StringToDouble("0X0", ALLOW_HEX));
  CHECK_EQ(1.0, StringToDouble("0x1", ALLOW_HEX));
  CHECK_EQ(16.0, StringToDouble("0x10", ALLOW_HEX));
  CHECK_EQ(255.0, StringToDouble("0xff", ALLOW_HEX));
  CHECK_EQ(175.0, StringToDouble("0xAF", ALLOW_HEX));
}


TEST(Octal) {
  CHECK_EQ(0.0, StringToDouble("0", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("00", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(1.0, StringToDouble("01", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(7.0, StringToDouble("07", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(8.0, StringToDouble("010", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(63.0, StringToDouble("077", ALLOW_HEX | ALLOW_OCTALS));

  CHECK_EQ(0.0, StringToDouble("0", ALLOW_HEX));
  CHECK_EQ(0.0, StringToDouble("00", ALLOW_HEX));
  CHECK_EQ(1.0, StringToDouble("01", ALLOW_HEX));
  CHECK_EQ(7.0, StringToDouble("07", ALLOW_HEX));
  CHECK_EQ(10.0, StringToDouble("010", ALLOW_HEX));
  CHECK_EQ(77.0, StringToDouble("077", ALLOW_HEX));

  const double x = 010000000000;  // Power of 2, no rounding errors.
  CHECK_EQ(x * x * x * x * x, StringToDouble("01" "0000000000" "0000000000"
      "0000000000" "0000000000" "0000000000", ALLOW_OCTALS));
}


TEST(MalformedOctal) {
  CHECK_EQ(8.0, StringToDouble("08", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(81.0, StringToDouble("081", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(78.0, StringToDouble("078", ALLOW_HEX | ALLOW_OCTALS));

  CHECK(isnan(StringToDouble("07.7", ALLOW_HEX | ALLOW_OCTALS)));
  CHECK(isnan(StringToDouble("07.8", ALLOW_HEX | ALLOW_OCTALS)));
  CHECK(isnan(StringToDouble("07e8", ALLOW_HEX | ALLOW_OCTALS)));
  CHECK(isnan(StringToDouble("07e7", ALLOW_HEX | ALLOW_OCTALS)));

  CHECK_EQ(8.7, StringToDouble("08.7", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(8e7, StringToDouble("08e7", ALLOW_HEX | ALLOW_OCTALS));

  CHECK_EQ(0.001, StringToDouble("0.001", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.713, StringToDouble("0.713", ALLOW_HEX | ALLOW_OCTALS));

  CHECK_EQ(8.0, StringToDouble("08", ALLOW_HEX));
  CHECK_EQ(81.0, StringToDouble("081", ALLOW_HEX));
  CHECK_EQ(78.0, StringToDouble("078", ALLOW_HEX));

  CHECK_EQ(7.7, StringToDouble("07.7", ALLOW_HEX));
  CHECK_EQ(7.8, StringToDouble("07.8", ALLOW_HEX));
  CHECK_EQ(7e8, StringToDouble("07e8", ALLOW_HEX));
  CHECK_EQ(7e7, StringToDouble("07e7", ALLOW_HEX));

  CHECK_EQ(8.7, StringToDouble("08.7", ALLOW_HEX));
  CHECK_EQ(8e7, StringToDouble("08e7", ALLOW_HEX));

  CHECK_EQ(0.001, StringToDouble("0.001", ALLOW_HEX));
  CHECK_EQ(0.713, StringToDouble("0.713", ALLOW_HEX));
}


TEST(TrailingJunk) {
  CHECK_EQ(8.0, StringToDouble("8q", ALLOW_TRAILING_JUNK));
  CHECK_EQ(63.0, StringToDouble("077qqq", ALLOW_OCTALS | ALLOW_TRAILING_JUNK));
  CHECK_EQ(10.0, StringToDouble("10e", ALLOW_OCTALS | ALLOW_TRAILING_JUNK));
  CHECK_EQ(10.0, StringToDouble("10e-", ALLOW_OCTALS | ALLOW_TRAILING_JUNK));
}


TEST(NonStrDecimalLiteral) {
  CHECK(isnan(StringToDouble(" ", NO_FLAGS, OS::nan_value())));
  CHECK(isnan(StringToDouble("", NO_FLAGS, OS::nan_value())));
  CHECK(isnan(StringToDouble(" ", NO_FLAGS, OS::nan_value())));
  CHECK_EQ(0.0, StringToDouble("", NO_FLAGS));
  CHECK_EQ(0.0, StringToDouble(" ", NO_FLAGS));
}

TEST(IntegerStrLiteral) {
  CHECK_EQ(0.0, StringToDouble("0.0", NO_FLAGS));
  CHECK_EQ(0.0, StringToDouble("0", NO_FLAGS));
  CHECK_EQ(0.0, StringToDouble("00", NO_FLAGS));
  CHECK_EQ(0.0, StringToDouble("000", NO_FLAGS));
  CHECK_EQ(1.0, StringToDouble("1", NO_FLAGS));
  CHECK_EQ(-1.0, StringToDouble("-1", NO_FLAGS));
  CHECK_EQ(-1.0, StringToDouble("  -  1  ", NO_FLAGS));
  CHECK_EQ(1.0, StringToDouble("  +  1  ", NO_FLAGS));

  CHECK_EQ(0.0, StringToDouble("0e0", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("0e1", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("0e-1", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("0e-100000", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("0e+100000", ALLOW_HEX | ALLOW_OCTALS));
  CHECK_EQ(0.0, StringToDouble("0.", ALLOW_HEX | ALLOW_OCTALS));
}

TEST(LongNumberStr) {
  CHECK_EQ(1e10, StringToDouble("1" "0000000000", NO_FLAGS));
  CHECK_EQ(1e20, StringToDouble("1" "0000000000" "0000000000", NO_FLAGS));

  CHECK_EQ(1e60, StringToDouble("1" "0000000000" "0000000000" "0000000000"
      "0000000000" "0000000000" "0000000000", NO_FLAGS));

  CHECK_EQ(1e-2, StringToDouble("." "0" "1", NO_FLAGS));
  CHECK_EQ(1e-11, StringToDouble("." "0000000000" "1", NO_FLAGS));
  CHECK_EQ(1e-21, StringToDouble("." "0000000000" "0000000000" "1", NO_FLAGS));

  CHECK_EQ(1e-61, StringToDouble("." "0000000000" "0000000000" "0000000000"
      "0000000000" "0000000000" "0000000000" "1", NO_FLAGS));


  // x = 24414062505131248.0 and y = 24414062505131252.0 are representable in
  // double. Check chat z = (x + y) / 2 is rounded to x...
  CHECK_EQ(24414062505131248.0,
           StringToDouble("24414062505131250.0", NO_FLAGS));

  // ... and z = (x + y) / 2 + delta is rounded to y.
  CHECK_EQ(24414062505131252.0,
           StringToDouble("24414062505131250.000000001", NO_FLAGS));
}


extern "C" double gay_strtod(const char* s00, const char** se);


TEST(MaximumSignificantDigits) {
  char num[] =
      "4.4501477170144020250819966727949918635852426585926051135169509"
      "122872622312493126406953054127118942431783801370080830523154578"
      "251545303238277269592368457430440993619708911874715081505094180"
      "604803751173783204118519353387964161152051487413083163272520124"
      "606023105869053620631175265621765214646643181420505164043632222"
      "668006474326056011713528291579642227455489682133472873831754840"
      "341397809846934151055619529382191981473003234105366170879223151"
      "087335413188049110555339027884856781219017754500629806224571029"
      "581637117459456877330110324211689177656713705497387108207822477"
      "584250967061891687062782163335299376138075114200886249979505279"
      "101870966346394401564490729731565935244123171539810221213221201"
      "847003580761626016356864581135848683152156368691976240370422601"
      "6998291015625000000000000000000000000000000000e-308";

  CHECK_EQ(gay_strtod(num, NULL), StringToDouble(num, NO_FLAGS));

  // Changes the result of strtod (at least in glibc implementation).
  num[sizeof(num) - 8] = '1';

  CHECK_EQ(gay_strtod(num, NULL), StringToDouble(num, NO_FLAGS));
}


TEST(ExponentNumberStr) {
  CHECK_EQ(1e1, StringToDouble("1e1", NO_FLAGS));
  CHECK_EQ(1e1, StringToDouble("1e+1", NO_FLAGS));
  CHECK_EQ(1e-1, StringToDouble("1e-1", NO_FLAGS));
  CHECK_EQ(1e100, StringToDouble("1e+100", NO_FLAGS));
  CHECK_EQ(1e-100, StringToDouble("1e-100", NO_FLAGS));
  CHECK_EQ(1e-106, StringToDouble(".000001e-100", NO_FLAGS));
}

class OneBit1: public BitField<uint32_t, 0, 1> {};
class OneBit2: public BitField<uint32_t, 7, 1> {};
class EightBit1: public BitField<uint32_t, 0, 8> {};
class EightBit2: public BitField<uint32_t, 13, 8> {};

TEST(BitField) {
  uint32_t x;

  // One bit bit field can hold values 0 and 1.
  CHECK(!OneBit1::is_valid(static_cast<uint32_t>(-1)));
  CHECK(!OneBit2::is_valid(static_cast<uint32_t>(-1)));
  for (int i = 0; i < 2; i++) {
    CHECK(OneBit1::is_valid(i));
    x = OneBit1::encode(i);
    CHECK_EQ(i, OneBit1::decode(x));

    CHECK(OneBit2::is_valid(i));
    x = OneBit2::encode(i);
    CHECK_EQ(i, OneBit2::decode(x));
  }
  CHECK(!OneBit1::is_valid(2));
  CHECK(!OneBit2::is_valid(2));

  // Eight bit bit field can hold values from 0 tp 255.
  CHECK(!EightBit1::is_valid(static_cast<uint32_t>(-1)));
  CHECK(!EightBit2::is_valid(static_cast<uint32_t>(-1)));
  for (int i = 0; i < 256; i++) {
    CHECK(EightBit1::is_valid(i));
    x = EightBit1::encode(i);
    CHECK_EQ(i, EightBit1::decode(x));
    CHECK(EightBit2::is_valid(i));
    x = EightBit2::encode(i);
    CHECK_EQ(i, EightBit2::decode(x));
  }
  CHECK(!EightBit1::is_valid(256));
  CHECK(!EightBit2::is_valid(256));
}
