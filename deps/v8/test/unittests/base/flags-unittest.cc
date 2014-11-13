// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include "src/base/flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

namespace {

enum Flag1 {
  kFlag1None = 0,
  kFlag1First = 1u << 1,
  kFlag1Second = 1u << 2,
  kFlag1All = kFlag1None | kFlag1First | kFlag1Second
};
typedef Flags<Flag1> Flags1;


DEFINE_OPERATORS_FOR_FLAGS(Flags1)


Flags1 bar(Flags1 flags1) { return flags1; }

}  // namespace


TEST(FlagsTest, BasicOperations) {
  Flags1 a;
  EXPECT_EQ(kFlag1None, static_cast<int>(a));
  a |= kFlag1First;
  EXPECT_EQ(kFlag1First, static_cast<int>(a));
  a = a | kFlag1Second;
  EXPECT_EQ(kFlag1All, static_cast<int>(a));
  a &= kFlag1Second;
  EXPECT_EQ(kFlag1Second, static_cast<int>(a));
  a = kFlag1None & a;
  EXPECT_EQ(kFlag1None, static_cast<int>(a));
  a ^= (kFlag1All | kFlag1None);
  EXPECT_EQ(kFlag1All, static_cast<int>(a));
  Flags1 b = ~a;
  EXPECT_EQ(kFlag1All, static_cast<int>(a));
  EXPECT_EQ(~static_cast<int>(a), static_cast<int>(b));
  Flags1 c = a;
  EXPECT_EQ(a, c);
  EXPECT_NE(a, b);
  EXPECT_EQ(a, bar(a));
  EXPECT_EQ(a, bar(kFlag1All));
}


namespace {
namespace foo {

enum Option {
  kNoOptions = 0,
  kOption1 = 1,
  kOption2 = 2,
  kAllOptions = kNoOptions | kOption1 | kOption2
};
typedef Flags<Option> Options;

}  // namespace foo


DEFINE_OPERATORS_FOR_FLAGS(foo::Options)

}  // namespace


TEST(FlagsTest, NamespaceScope) {
  foo::Options options;
  options ^= foo::kNoOptions;
  options |= foo::kOption1 | foo::kOption2;
  EXPECT_EQ(foo::kAllOptions, static_cast<int>(options));
}


namespace {

struct Foo {
  enum Enum { kEnum1 = 1, kEnum2 = 2 };
  typedef Flags<Enum, uint32_t> Enums;
};


DEFINE_OPERATORS_FOR_FLAGS(Foo::Enums)

}  // namespace


TEST(FlagsTest, ClassScope) {
  Foo::Enums enums;
  enums |= Foo::kEnum1;
  enums |= Foo::kEnum2;
  EXPECT_TRUE(enums & Foo::kEnum1);
  EXPECT_TRUE(enums & Foo::kEnum2);
}

}  // namespace base
}  // namespace v8
