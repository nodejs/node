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

#include "src/traced/probes/filesystem/file_scanner.h"

#include <sys/stat.h>
#include <memory>
#include <string>

#include "perfetto/base/logging.h"
#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"
#include "src/base/test/test_task_runner.h"
#include "src/base/test/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

using ::testing::Eq;
using ::testing::Contains;
using ::testing::UnorderedElementsAre;

class TestDelegate : public FileScanner::Delegate {
 public:
  TestDelegate(std::function<bool(BlockDeviceID,
                                  Inode,
                                  const std::string&,
                                  InodeFileMap_Entry_Type)> callback,
               std::function<void()> done_callback)
      : callback_(std::move(callback)),
        done_callback_(std::move(done_callback)) {}
  bool OnInodeFound(BlockDeviceID block_device_id,
                    Inode inode,
                    const std::string& path,
                    InodeFileMap_Entry_Type type) override {
    return callback_(block_device_id, inode, path, type);
  }

  void OnInodeScanDone() { return done_callback_(); }

 private:
  std::function<
      bool(BlockDeviceID, Inode, const std::string&, InodeFileMap_Entry_Type)>
      callback_;
  std::function<void()> done_callback_;
};

struct FileEntry {
  FileEntry(BlockDeviceID block_device_id,
            Inode inode,
            std::string path,
            InodeFileMap_Entry_Type type)
      : block_device_id_(block_device_id),
        inode_(inode),
        path_(std::move(path)),
        type_(type) {}

  bool operator==(const FileEntry& other) const {
    return block_device_id_ == other.block_device_id_ &&
           inode_ == other.inode_ && path_ == other.path_ &&
           type_ == other.type_;
  }

  BlockDeviceID block_device_id_;
  Inode inode_;
  std::string path_;
  InodeFileMap_Entry_Type type_;
};

struct stat CheckStat(const std::string& path) {
  struct stat buf;
  PERFETTO_CHECK(lstat(path.c_str(), &buf) != -1);
  return buf;
}

FileEntry StatFileEntry(const std::string& path, InodeFileMap_Entry_Type type) {
  struct stat buf = CheckStat(path);
  return FileEntry(buf.st_dev, buf.st_ino, path, type);
}

TEST(FileScannerTest, TestSynchronousStop) {
  uint64_t seen = 0;
  bool done = false;
  TestDelegate delegate(
      [&seen](BlockDeviceID, Inode, const std::string&,
              InodeFileMap_Entry_Type) {
        ++seen;
        return false;
      },
      [&done] { done = true; });

  FileScanner fs(
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata")},
      &delegate);
  fs.Scan();

  EXPECT_EQ(seen, 1u);
  EXPECT_TRUE(done);
}

TEST(FileScannerTest, TestAsynchronousStop) {
  uint64_t seen = 0;
  base::TestTaskRunner task_runner;
  TestDelegate delegate(
      [&seen](BlockDeviceID, Inode, const std::string&,
              InodeFileMap_Entry_Type) {
        ++seen;
        return false;
      },
      task_runner.CreateCheckpoint("done"));

  FileScanner fs(
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata")},
      &delegate, 1, 1);
  fs.Scan(&task_runner);

  task_runner.RunUntilCheckpoint("done");

  EXPECT_EQ(seen, 1u);
}

TEST(FileScannerTest, TestSynchronousFindFiles) {
  std::vector<FileEntry> file_entries;
  TestDelegate delegate(
      [&file_entries](BlockDeviceID block_device_id, Inode inode,
                      const std::string& path, InodeFileMap_Entry_Type type) {
        file_entries.emplace_back(block_device_id, inode, path, type);
        return true;
      },
      [] {});

  FileScanner fs(
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata")},
      &delegate);
  fs.Scan();

  EXPECT_THAT(
      file_entries,
      UnorderedElementsAre(
          Eq(StatFileEntry(
              base::GetTestDataPath(
                  "src/traced/probes/filesystem/testdata/dir1/file1"),
              protos::pbzero::InodeFileMap_Entry_Type_FILE)),
          Eq(StatFileEntry(base::GetTestDataPath(
                               "src/traced/probes/filesystem/testdata/file2"),
                           protos::pbzero::InodeFileMap_Entry_Type_FILE)),
          Eq(StatFileEntry(
              base::GetTestDataPath(
                  "src/traced/probes/filesystem/testdata/dir1"),
              protos::pbzero::InodeFileMap_Entry_Type_DIRECTORY))));
}

TEST(FileScannerTest, TestAsynchronousFindFiles) {
  base::TestTaskRunner task_runner;
  std::vector<FileEntry> file_entries;
  TestDelegate delegate(
      [&file_entries](BlockDeviceID block_device_id, Inode inode,
                      const std::string& path, InodeFileMap_Entry_Type type) {
        file_entries.emplace_back(block_device_id, inode, path, type);
        return true;
      },
      task_runner.CreateCheckpoint("done"));

  FileScanner fs(
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata")},
      &delegate, 1, 1);
  fs.Scan(&task_runner);

  task_runner.RunUntilCheckpoint("done");

  EXPECT_THAT(
      file_entries,
      UnorderedElementsAre(
          Eq(StatFileEntry(
              base::GetTestDataPath(
                  "src/traced/probes/filesystem/testdata/dir1/file1"),
              protos::pbzero::InodeFileMap_Entry_Type_FILE)),
          Eq(StatFileEntry(base::GetTestDataPath(
                               "src/traced/probes/filesystem/testdata/file2"),
                           protos::pbzero::InodeFileMap_Entry_Type_FILE)),
          Eq(StatFileEntry(
              base::GetTestDataPath(
                  "src/traced/probes/filesystem/testdata/dir1"),
              protos::pbzero::InodeFileMap_Entry_Type_DIRECTORY))));
}

}  // namespace
}  // namespace perfetto
