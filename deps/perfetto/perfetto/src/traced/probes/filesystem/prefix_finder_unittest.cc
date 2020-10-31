/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/traced/probes/filesystem/prefix_finder.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

TEST(PrefixFinderTest, Basic) {
  PrefixFinder pr(4);
  pr.AddPath("/a/1");
  pr.AddPath("/a/2");
  pr.AddPath("/a/3");
  pr.AddPath("/b/4");
  pr.AddPath("/b/5");

  pr.Finalize();

  ASSERT_EQ(pr.GetPrefix("/a/1")->ToString(), "");
  ASSERT_EQ(pr.GetPrefix("/a/2")->ToString(), "");
  ASSERT_EQ(pr.GetPrefix("/a/3")->ToString(), "");
  ASSERT_EQ(pr.GetPrefix("/b/4")->ToString(), "/b");
  ASSERT_EQ(pr.GetPrefix("/b/5")->ToString(), "/b");
}

}  // namespace
}  // namespace perfetto
