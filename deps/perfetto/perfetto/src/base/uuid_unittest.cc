/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/uuid.h"

#include "test/gtest_and_gmock.h"

#include "perfetto/ext/base/optional.h"

namespace perfetto {
namespace base {
namespace {

TEST(Uuid, DefaultConstructorIsBlank) {
  Uuid a;
  Uuid b;
  EXPECT_EQ(a, b);
  EXPECT_EQ(a.msb(), 0);
  EXPECT_EQ(a.lsb(), 0);
}

TEST(Uuid, TwoUuidsShouldBeDifferent) {
  Uuid a = Uuidv4();
  Uuid b = Uuidv4();
  EXPECT_NE(a, b);
  EXPECT_EQ(a, a);
  EXPECT_EQ(b, b);
}

TEST(Uuid, CanRoundTripUuid) {
  Uuid uuid = Uuidv4();
  EXPECT_EQ(Uuid(uuid.ToString()), uuid);
}

TEST(Uuid, SetGet) {
  Uuid a = Uuidv4();
  Uuid b;
  b.set_lsb_msb(a.lsb(), a.msb());
  EXPECT_EQ(a, b);
}

TEST(Uuid, LsbMsbConstructor) {
  Uuid uuid(-6605018796207623390, 1314564453825188563);
  EXPECT_EQ(uuid.ToPrettyString(), "123e4567-e89b-12d3-a456-426655443322");
}

TEST(Uuid, UuidToPrettyString) {
  Uuid uuid;
  uuid.set_lsb_msb(-6605018796207623390, 1314564453825188563);
  EXPECT_EQ(uuid.ToPrettyString(), "123e4567-e89b-12d3-a456-426655443322");
}

}  // namespace
}  // namespace base
}  // namespace perfetto
