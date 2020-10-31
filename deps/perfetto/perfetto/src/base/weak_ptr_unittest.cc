/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "perfetto/ext/base/weak_ptr.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

class WeakClass {
 public:
  WeakClass() : weak_factory(this) {}
  WeakPtrFactory<WeakClass> weak_factory;
};

TEST(WeakPtrTest, AllCases) {
  std::unique_ptr<WeakClass> owned_instance(new WeakClass());
  WeakPtr<WeakClass> weak_ptr1 = owned_instance->weak_factory.GetWeakPtr();
  WeakPtr<WeakClass> weak_ptr2;
  weak_ptr2 = owned_instance->weak_factory.GetWeakPtr();
  WeakPtr<WeakClass> weak_ptr_copied(weak_ptr2);
  WeakPtr<WeakClass> weak_ptr_copied2 = weak_ptr_copied;  // NOLINT

  ASSERT_TRUE(weak_ptr1);
  ASSERT_TRUE(weak_ptr2);
  ASSERT_TRUE(weak_ptr_copied);
  ASSERT_TRUE(weak_ptr_copied2);

  ASSERT_EQ(owned_instance.get(), weak_ptr1.get());
  ASSERT_EQ(owned_instance.get(), weak_ptr2.get());
  ASSERT_EQ(owned_instance.get(), weak_ptr_copied.get());
  ASSERT_EQ(owned_instance.get(), weak_ptr_copied2.get());

  WeakPtr<WeakClass> weak_ptr_moved1(std::move(weak_ptr1));
  WeakPtr<WeakClass> weak_ptr_moved2(weak_ptr_copied2);
  weak_ptr_moved2 = std::move(weak_ptr2);
  ASSERT_FALSE(weak_ptr1);
  ASSERT_FALSE(weak_ptr2);
  ASSERT_TRUE(weak_ptr_moved1);
  ASSERT_TRUE(weak_ptr_moved2);
  ASSERT_TRUE(weak_ptr_copied2);
  ASSERT_EQ(owned_instance.get(), weak_ptr_moved1.get());
  ASSERT_EQ(owned_instance.get(), weak_ptr_moved2.get());

  owned_instance.reset();

  ASSERT_FALSE(weak_ptr1);
  ASSERT_FALSE(weak_ptr2);
  ASSERT_FALSE(weak_ptr_copied);
  ASSERT_FALSE(weak_ptr_copied2);
  ASSERT_FALSE(weak_ptr_moved1);
  ASSERT_FALSE(weak_ptr_moved2);
}

}  // namespace
}  // namespace base
}  // namespace perfetto
