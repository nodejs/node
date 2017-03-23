// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
#include "src/ostreams.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/feedback-vector.h ->
// src/feedback-vector-inl.h
#include "src/feedback-vector-inl.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

#define FLOAT_TEST(type, lane_count)                     \
  {                                                      \
    float nan = std::numeric_limits<float>::quiet_NaN(); \
    float lanes[lane_count] = {0};                       \
    Handle<type> a = factory->New##type(lanes);          \
    Handle<type> b = factory->New##type(lanes);          \
    CHECK(a->BitwiseEquals(*b));                         \
    CHECK(a->SameValue(*b));                             \
    CHECK(a->SameValueZero(*b));                         \
    CHECK_EQ(a->Hash(), b->Hash());                      \
    for (int i = 0; i < lane_count; i++) {               \
      a->set_lane(i, -0.0);                              \
      CHECK(!a->BitwiseEquals(*b));                      \
      CHECK_NE(a->Hash(), b->Hash());                    \
      CHECK(!a->SameValue(*b));                          \
      CHECK(a->SameValueZero(*b));                       \
      b->set_lane(i, -0.0);                              \
      CHECK(a->BitwiseEquals(*b));                       \
      CHECK_EQ(a->Hash(), b->Hash());                    \
      CHECK(a->SameValue(*b));                           \
      CHECK(a->SameValueZero(*b));                       \
      a->set_lane(i, nan);                               \
      CHECK(!a->BitwiseEquals(*b));                      \
      CHECK(!a->SameValue(*b));                          \
      CHECK(!a->SameValueZero(*b));                      \
      CHECK_NE(a->Hash(), b->Hash());                    \
      b->set_lane(i, nan);                               \
      CHECK(a->BitwiseEquals(*b));                       \
      CHECK_EQ(a->Hash(), b->Hash());                    \
      CHECK(a->SameValue(*b));                           \
      CHECK(a->SameValueZero(*b));                       \
    }                                                    \
  }

#define INT_TEST(type, lane_count, lane_type)   \
  {                                             \
    lane_type lanes[lane_count] = {0};          \
    Handle<type> a = factory->New##type(lanes); \
    Handle<type> b = factory->New##type(lanes); \
    CHECK(a->BitwiseEquals(*b));                \
    CHECK(a->SameValue(*b));                    \
    CHECK(a->SameValueZero(*b));                \
    CHECK_EQ(a->Hash(), b->Hash());             \
    for (int i = 0; i < lane_count; i++) {      \
      a->set_lane(i, i + 1);                    \
      CHECK(!a->BitwiseEquals(*b));             \
      CHECK_NE(a->Hash(), b->Hash());           \
      CHECK(!a->SameValue(*b));                 \
      CHECK(!a->SameValueZero(*b));             \
      b->set_lane(i, i + 1);                    \
      CHECK(a->BitwiseEquals(*b));              \
      CHECK_EQ(a->Hash(), b->Hash());           \
      CHECK(a->SameValue(*b));                  \
      CHECK(a->SameValueZero(*b));              \
      a->set_lane(i, -(i + 1));                 \
      CHECK(!a->BitwiseEquals(*b));             \
      CHECK_NE(a->Hash(), b->Hash());           \
      CHECK(!a->SameValue(*b));                 \
      CHECK(!a->SameValueZero(*b));             \
      b->set_lane(i, -(i + 1));                 \
      CHECK(a->BitwiseEquals(*b));              \
      CHECK_EQ(a->Hash(), b->Hash());           \
      CHECK(a->SameValue(*b));                  \
      CHECK(a->SameValueZero(*b));              \
    }                                           \
  }

#define BOOL_TEST(type, lane_count)             \
  {                                             \
    bool lanes[lane_count] = {false};           \
    Handle<type> a = factory->New##type(lanes); \
    Handle<type> b = factory->New##type(lanes); \
    CHECK(a->BitwiseEquals(*b));                \
    CHECK(a->SameValue(*b));                    \
    CHECK(a->SameValueZero(*b));                \
    CHECK_EQ(a->Hash(), b->Hash());             \
    for (int i = 0; i < lane_count; i++) {      \
      a->set_lane(i, true);                     \
      CHECK(!a->BitwiseEquals(*b));             \
      CHECK_NE(a->Hash(), b->Hash());           \
      CHECK(!a->SameValue(*b));                 \
      CHECK(!a->SameValueZero(*b));             \
      b->set_lane(i, true);                     \
      CHECK(a->BitwiseEquals(*b));              \
      CHECK_EQ(a->Hash(), b->Hash());           \
      CHECK(a->SameValue(*b));                  \
      CHECK(a->SameValueZero(*b));              \
    }                                           \
  }

TEST(SimdTypes) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);

  FLOAT_TEST(Float32x4, 4)
  INT_TEST(Int32x4, 4, int32_t)
  INT_TEST(Uint32x4, 4, uint32_t)
  BOOL_TEST(Bool32x4, 4)
  INT_TEST(Int16x8, 8, int16_t)
  INT_TEST(Uint16x8, 8, uint16_t)
  BOOL_TEST(Bool16x8, 8)
  INT_TEST(Int8x16, 16, int8_t)
  INT_TEST(Uint8x16, 16, uint8_t)
  BOOL_TEST(Bool8x16, 16)
}
