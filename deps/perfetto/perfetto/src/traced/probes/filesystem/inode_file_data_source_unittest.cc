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

#include "src/traced/probes/filesystem/inode_file_data_source.h"

#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/base/test/test_task_runner.h"
#include "src/base/test/utils.h"
#include "src/traced/probes/filesystem/lru_inode_cache.h"
#include "src/tracing/core/null_trace_writer.h"

#include "test/gtest_and_gmock.h"

#include "protos/perfetto/config/inode_file/inode_file_config.pbzero.h"
#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"

namespace perfetto {
namespace {

using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::_;

class TestInodeFileDataSource : public InodeFileDataSource {
 public:
  TestInodeFileDataSource(
      DataSourceConfig cfg,
      base::TaskRunner* task_runner,
      TracingSessionID tsid,
      std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>*
          static_file_map,
      LRUInodeCache* cache,
      std::unique_ptr<TraceWriter> writer)
      : InodeFileDataSource(std::move(cfg),
                            task_runner,
                            tsid,
                            static_file_map,
                            cache,
                            std::move(writer)) {
    struct stat buf;
    PERFETTO_CHECK(
        lstat(base::GetTestDataPath("src/traced/probes/filesystem/testdata")
                  .c_str(),
              &buf) != -1);
    mount_points_.emplace(
        buf.st_dev,
        base::GetTestDataPath("src/traced/probes/filesystem/testdata"));
  }

  MOCK_METHOD3(FillInodeEntry,
               void(InodeFileMap* destination,
                    Inode inode_number,
                    const InodeMapValue& inode_map_value));
};

class InodeFileDataSourceTest : public ::testing::Test {
 protected:
  InodeFileDataSourceTest() {}

  std::unique_ptr<TestInodeFileDataSource> GetInodeFileDataSource(
      DataSourceConfig cfg) {
    return std::unique_ptr<TestInodeFileDataSource>(new TestInodeFileDataSource(
        cfg, &task_runner_, 0, &static_file_map_, &cache_,
        std::unique_ptr<NullTraceWriter>(new NullTraceWriter)));
  }

  LRUInodeCache cache_{100};
  std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>
      static_file_map_;
  base::TestTaskRunner task_runner_;
};

TEST_F(InodeFileDataSourceTest, TestFileSystemScan) {
  DataSourceConfig ds_config;
  protozero::HeapBuffered<protos::pbzero::InodeFileConfig> inode_cfg;
  inode_cfg->set_scan_interval_ms(1);
  inode_cfg->set_scan_delay_ms(1);
  ds_config.set_inode_file_config_raw(inode_cfg.SerializeAsString());
  auto data_source = GetInodeFileDataSource(ds_config);

  struct stat buf;
  PERFETTO_CHECK(
      lstat(base::GetTestDataPath("src/traced/probes/filesystem/testdata/file2")
                .c_str(),
            &buf) != -1);

  auto done = task_runner_.CreateCheckpoint("done");
  InodeMapValue value(
      protos::pbzero::InodeFileMap_Entry_Type_FILE,
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata/file2")});
  EXPECT_CALL(*data_source, FillInodeEntry(_, buf.st_ino, Eq(value)))
      .WillOnce(InvokeWithoutArgs(done));

  data_source->OnInodes({{buf.st_ino, buf.st_dev}});
  task_runner_.RunUntilCheckpoint("done");

  // Expect that the found inode is added in the LRU cache.
  EXPECT_THAT(cache_.Get(std::make_pair(buf.st_dev, buf.st_ino)),
              Pointee(Eq(value)));
}

TEST_F(InodeFileDataSourceTest, TestStaticMap) {
  DataSourceConfig config;
  auto data_source = GetInodeFileDataSource(config);
  CreateStaticDeviceToInodeMap(
      base::GetTestDataPath("src/traced/probes/filesystem/testdata"),
      &static_file_map_);

  struct stat buf;
  PERFETTO_CHECK(
      lstat(base::GetTestDataPath("src/traced/probes/filesystem/testdata/file2")
                .c_str(),
            &buf) != -1);

  InodeMapValue value(
      protos::pbzero::InodeFileMap_Entry_Type_FILE,
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata/file2")});
  EXPECT_CALL(*data_source, FillInodeEntry(_, buf.st_ino, Eq(value)));

  data_source->OnInodes({{buf.st_ino, buf.st_dev}});
  // Expect that the found inode is not added the the LRU cache.
  EXPECT_THAT(cache_.Get(std::make_pair(buf.st_dev, buf.st_ino)), IsNull());
}

TEST_F(InodeFileDataSourceTest, TestCache) {
  DataSourceConfig config;
  auto data_source = GetInodeFileDataSource(config);
  CreateStaticDeviceToInodeMap(
      base::GetTestDataPath("src/traced/probes/filesystem/testdata"),
      &static_file_map_);

  struct stat buf;
  PERFETTO_CHECK(
      lstat(base::GetTestDataPath("src/traced/probes/filesystem/testdata/file2")
                .c_str(),
            &buf) != -1);

  InodeMapValue value(
      protos::pbzero::InodeFileMap_Entry_Type_FILE,
      {base::GetTestDataPath("src/traced/probes/filesystem/testdata/file2")});
  cache_.Insert(std::make_pair(buf.st_dev, buf.st_ino), value);

  EXPECT_CALL(*data_source, FillInodeEntry(_, buf.st_ino, Eq(value)));

  data_source->OnInodes({{buf.st_ino, buf.st_dev}});
}

}  // namespace
}  // namespace perfetto
