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

#include "src/traced/probes/filesystem/fs_mount.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

using testing::Contains;
using testing::Pair;

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
TEST(FsMountTest, ParseRealMounts) {
  std::multimap<BlockDeviceID, std::string> mounts = ParseMounts();
  struct stat buf = {};
  ASSERT_NE(stat("/proc", &buf), -1);
  EXPECT_THAT(mounts, Contains(Pair(buf.st_dev, "/proc")));
}
#endif

TEST(FsMountTest, ParseSyntheticMounts) {
  const char kMounts[] = R"(
sysfs / sysfs rw,nosuid,nodev,noexec,relatime 0 0
#INVALIDLINE
devfs /dev devfs,local,nobrowse
)";

  base::TempFile tmp_file = base::TempFile::Create();
  ASSERT_EQ(base::WriteAll(tmp_file.fd(), kMounts, sizeof(kMounts)),
            static_cast<ssize_t>(sizeof(kMounts)));
  std::multimap<BlockDeviceID, std::string> mounts =
      ParseMounts(tmp_file.path().c_str());
  struct stat dev_stat = {}, root_stat = {};
  ASSERT_NE(stat("/dev", &dev_stat), -1);
  ASSERT_NE(stat("/", &root_stat), -1);
  EXPECT_THAT(mounts, Contains(Pair(dev_stat.st_dev, "/dev")));
  EXPECT_THAT(mounts, Contains(Pair(root_stat.st_dev, "/")));
}  // namespace

}  // namespace
}  // namespace perfetto
