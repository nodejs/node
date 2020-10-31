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

#include "perfetto/ext/base/temp_file.h"

#include <sys/stat.h>
#include <unistd.h>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

bool PathExists(const std::string& path) {
  struct stat stat_buf;
  return stat(path.c_str(), &stat_buf) == 0;
}

TEST(TempFileTest, Create) {
  std::string path;
  int fd;
  {
    TempFile tf = TempFile::Create();
    path = tf.path();
    fd = tf.fd();
    ASSERT_NE("", path);
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(PathExists(path));
    ASSERT_GE(write(fd, "foo", 4), 0);

    TempFile moved_tf(std::move(tf));
    ASSERT_EQ("", tf.path());
    ASSERT_EQ(-1, tf.fd());
    ASSERT_EQ(path, moved_tf.path());
    ASSERT_EQ(fd, moved_tf.fd());
    ASSERT_GE(write(moved_tf.fd(), "foo", 4), 0);

    TempFile moved_tf2 = std::move(moved_tf);
    ASSERT_EQ("", moved_tf.path());
    ASSERT_EQ(-1, moved_tf.fd());
    ASSERT_EQ(path, moved_tf2.path());
    ASSERT_EQ(fd, moved_tf2.fd());
    ASSERT_GE(write(moved_tf2.fd(), "foo", 4), 0);
  }

  // The file should be deleted and closed now.
  ASSERT_FALSE(PathExists(path));

  ASSERT_EQ(-1, write(fd, "foo", 4));
}

TEST(TempFileTest, CreateUnlinked) {
  int fd;
  {
    TempFile tf = TempFile::CreateUnlinked();
    ASSERT_EQ("", tf.path());
    fd = tf.fd();
    ASSERT_GE(fd, 0);
    ASSERT_GE(write(fd, "foo", 4), 0);
  }
  ASSERT_EQ(-1, write(fd, "foo", 4));
}

TEST(TempFileTest, ReleaseUnlinked) {
  ScopedFile fd;
  {
    TempFile tf = TempFile::Create();
    fd = tf.ReleaseFD();
  }
  ASSERT_GE(write(*fd, "foo", 4), 0);
}

TEST(TempFileTest, ReleaseLinked) {
  ScopedFile fd;
  std::string path;
  {
    TempFile tf = TempFile::CreateUnlinked();
    path = tf.path();
    fd = tf.ReleaseFD();
  }

  // The file should be unlinked from the filesystem.
  ASSERT_FALSE(PathExists(path));

  // But still open and writable.
  ASSERT_GE(write(*fd, "foo", 4), 0);
}

TEST(TempFileTest, TempDir) {
  std::string path;
  {
    TempDir td = TempDir::Create();
    ASSERT_NE("", td.path());
    ASSERT_TRUE(PathExists(td.path()));
    path = td.path();
  }
  ASSERT_FALSE(PathExists(path));
}

}  // namespace
}  // namespace base
}  // namespace perfetto
