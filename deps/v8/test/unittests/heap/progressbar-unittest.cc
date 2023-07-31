// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/progress-bar.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(ProgressBar, DefaultDisabled) {
  ProgressBar progress_bar;
  EXPECT_FALSE(progress_bar.IsEnabled());
}

TEST(ProgressBar, EnabledAfterExplicitEnable) {
  ProgressBar progress_bar;
  progress_bar.Enable();
  EXPECT_TRUE(progress_bar.IsEnabled());
}

TEST(ProgressBar, ZeroValueAfterEnable) {
  ProgressBar progress_bar;
  progress_bar.Enable();
  ASSERT_TRUE(progress_bar.IsEnabled());
  EXPECT_EQ(0u, progress_bar.Value());
}

TEST(ProgressBar, TrySetValue) {
  ProgressBar progress_bar;
  progress_bar.Enable();
  ASSERT_TRUE(progress_bar.IsEnabled());
  EXPECT_TRUE(progress_bar.TrySetNewValue(0, 17));
  EXPECT_EQ(17u, progress_bar.Value());
}

TEST(ProgressBar, MultipleTrySetValue) {
  ProgressBar progress_bar;
  progress_bar.Enable();
  ASSERT_TRUE(progress_bar.IsEnabled());
  EXPECT_TRUE(progress_bar.TrySetNewValue(0, 23));
  EXPECT_EQ(23u, progress_bar.Value());
  EXPECT_TRUE(progress_bar.TrySetNewValue(23, 127));
  EXPECT_EQ(127u, progress_bar.Value());
}

TEST(ProgressBar, ResetIfEnabledOnDisabled) {
  ProgressBar progress_bar;
  progress_bar.ResetIfEnabled();
  EXPECT_FALSE(progress_bar.IsEnabled());
}

TEST(ProgressBar, ResetIfEnabledOnEnabled) {
  ProgressBar progress_bar;
  progress_bar.Enable();
  ASSERT_TRUE(progress_bar.TrySetNewValue(0, 1));
  progress_bar.ResetIfEnabled();
  ASSERT_TRUE(progress_bar.IsEnabled());
  EXPECT_EQ(0u, progress_bar.Value());
}

#ifdef DEBUG

TEST(ProgressBarDeathTest, DiesOnTrySetValueOnDisabled) {
  ProgressBar progress_bar;
  EXPECT_DEATH_IF_SUPPORTED(progress_bar.TrySetNewValue(0, 1), "");
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
