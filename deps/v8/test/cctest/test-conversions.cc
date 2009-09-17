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
}


TEST(NonStrDecimalLiteral) {
  CHECK(isnan(StringToDouble(" ", NO_FLAGS, OS::nan_value())));
  CHECK(isnan(StringToDouble("", NO_FLAGS, OS::nan_value())));
  CHECK(isnan(StringToDouble(" ", NO_FLAGS, OS::nan_value())));
  CHECK_EQ(0.0, StringToDouble("", NO_FLAGS));
  CHECK_EQ(0.0, StringToDouble(" ", NO_FLAGS));
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
