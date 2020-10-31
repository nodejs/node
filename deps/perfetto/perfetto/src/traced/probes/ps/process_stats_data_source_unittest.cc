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

#include "src/traced/probes/ps/process_stats_data_source.h"

#include <dirent.h>

#include "perfetto/ext/base/temp_file.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "src/traced/probes/common/cpu_freq_info_for_testing.h"
#include "src/tracing/core/trace_writer_for_testing.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/config/process_stats/process_stats_config.gen.h"
#include "protos/perfetto/trace/ps/process_stats.gen.h"
#include "protos/perfetto/trace/ps/process_tree.gen.h"

using ::perfetto::protos::gen::ProcessStatsConfig;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Truly;

namespace perfetto {
namespace {

class TestProcessStatsDataSource : public ProcessStatsDataSource {
 public:
  TestProcessStatsDataSource(base::TaskRunner* task_runner,
                             TracingSessionID id,
                             std::unique_ptr<TraceWriter> writer,
                             const DataSourceConfig& config,
                             std::unique_ptr<CpuFreqInfo> cpu_freq_info)
      : ProcessStatsDataSource(task_runner,
                               id,
                               std::move(writer),
                               config,
                               std::move(cpu_freq_info)) {}

  MOCK_METHOD0(OpenProcDir, base::ScopedDir());
  MOCK_METHOD2(ReadProcPidFile, std::string(int32_t pid, const std::string&));
  MOCK_METHOD1(OpenProcTaskDir, base::ScopedDir(int32_t pid));
};

class ProcessStatsDataSourceTest : public ::testing::Test {
 protected:
  ProcessStatsDataSourceTest() {}

  std::unique_ptr<TestProcessStatsDataSource> GetProcessStatsDataSource(
      const DataSourceConfig& cfg) {
    auto writer =
        std::unique_ptr<TraceWriterForTesting>(new TraceWriterForTesting());
    writer_raw_ = writer.get();
    return std::unique_ptr<TestProcessStatsDataSource>(
        new TestProcessStatsDataSource(
            &task_runner_, 0, std::move(writer), cfg,
            cpu_freq_info_for_testing.GetInstance()));
  }

  base::TestTaskRunner task_runner_;
  TraceWriterForTesting* writer_raw_;
  CpuFreqInfoForTesting cpu_freq_info_for_testing;
};

TEST_F(ProcessStatsDataSourceTest, WriteOnceProcess) {
  auto data_source = GetProcessStatsDataSource(DataSourceConfig());
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "status"))
      .WillOnce(Return(
          "Name: foo\nTgid:\t42\nPid:   42\nPPid:  17\nUid:  43 44 45 56\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "cmdline"))
      .WillOnce(Return(std::string("foo\0bar\0baz\0", 12)));

  data_source->OnPids({42});

  auto trace = writer_raw_->GetAllTracePackets();
  ASSERT_EQ(trace.size(), 1u);
  auto ps_tree = trace[0].process_tree();
  ASSERT_EQ(ps_tree.processes_size(), 1);
  auto first_process = ps_tree.processes()[0];
  ASSERT_EQ(first_process.pid(), 42);
  ASSERT_EQ(first_process.ppid(), 17);
  ASSERT_EQ(first_process.uid(), 43);
  ASSERT_THAT(first_process.cmdline(), ElementsAreArray({"foo", "bar", "baz"}));
}

// Regression test for b/147438623.
TEST_F(ProcessStatsDataSourceTest, NonNulTerminatedCmdline) {
  auto data_source = GetProcessStatsDataSource(DataSourceConfig());
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "status"))
      .WillOnce(Return(
          "Name: foo\nTgid:\t42\nPid:   42\nPPid:  17\nUid:  43 44 45 56\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "cmdline"))
      .WillOnce(Return(std::string("surfaceflinger", 14)));

  data_source->OnPids({42});

  auto trace = writer_raw_->GetAllTracePackets();
  ASSERT_EQ(trace.size(), 1u);
  auto ps_tree = trace[0].process_tree();
  ASSERT_EQ(ps_tree.processes_size(), 1);
  auto first_process = ps_tree.processes()[0];
  ASSERT_THAT(first_process.cmdline(), ElementsAreArray({"surfaceflinger"}));
}

TEST_F(ProcessStatsDataSourceTest, DontRescanCachedPIDsAndTIDs) {
  // assertion helpers
  auto expected_process = [](int pid) {
    return [pid](protos::gen::ProcessTree::Process process) {
      return process.pid() == pid && process.cmdline_size() > 0 &&
             process.cmdline()[0] == "proc_" + std::to_string(pid);
    };
  };
  auto expected_thread = [](int tid) {
    return [tid](protos::gen::ProcessTree::Thread thread) {
      return thread.tid() == tid && thread.tgid() == tid / 10 * 10 &&
             thread.name() == "thread_" + std::to_string(tid);
    };
  };

  DataSourceConfig ds_config;
  ProcessStatsConfig cfg;
  cfg.set_record_thread_names(true);
  ds_config.set_process_stats_config_raw(cfg.SerializeAsString());
  auto data_source = GetProcessStatsDataSource(ds_config);
  for (int p : {10, 11, 12, 20, 21, 22, 30, 31, 32}) {
    EXPECT_CALL(*data_source, ReadProcPidFile(p, "status"))
        .WillOnce(Invoke([](int32_t pid, const std::string&) {
          int32_t tgid = (pid / 10) * 10;
          return "Name: \tthread_" + std::to_string(pid) +
                 "\nTgid:  " + std::to_string(tgid) +
                 "\nPid:   " + std::to_string(pid) + "\nPPid:  1\n";
        }));
    if (p % 10 == 0) {
      std::string proc_name = "proc_" + std::to_string(p);
      proc_name.resize(proc_name.size() + 1);  // Add a trailing \0.
      EXPECT_CALL(*data_source, ReadProcPidFile(p, "cmdline"))
          .WillOnce(Return(proc_name));
    }
  }

  data_source->OnPids({10, 11, 12, 20, 21, 22, 10, 20, 11, 21});
  data_source->OnPids({30});
  data_source->OnPids({10, 30, 10, 31, 32});

  // check written contents
  auto trace = writer_raw_->GetAllTracePackets();
  EXPECT_EQ(trace.size(), 3u);

  // first packet - two unique processes, four threads
  auto ps_tree = trace[0].process_tree();
  EXPECT_THAT(ps_tree.processes(),
              UnorderedElementsAre(Truly(expected_process(10)),
                                   Truly(expected_process(20))));
  EXPECT_THAT(ps_tree.threads(),
              UnorderedElementsAre(
                  Truly(expected_thread(11)), Truly(expected_thread(12)),
                  Truly(expected_thread(21)), Truly(expected_thread(22))));

  // second packet - one new process
  ps_tree = trace[1].process_tree();
  EXPECT_THAT(ps_tree.processes(),
              UnorderedElementsAre(Truly(expected_process(30))));
  EXPECT_EQ(ps_tree.threads_size(), 0);

  // third packet - two threads that haven't been seen before
  ps_tree = trace[2].process_tree();
  EXPECT_EQ(ps_tree.processes_size(), 0);
  EXPECT_THAT(ps_tree.threads(),
              UnorderedElementsAre(Truly(expected_thread(31)),
                                   Truly(expected_thread(32))));
}

TEST_F(ProcessStatsDataSourceTest, IncrementalStateClear) {
  auto data_source = GetProcessStatsDataSource(DataSourceConfig());
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "status"))
      .WillOnce(Return("Name: foo\nTgid:\t42\nPid:   42\nPPid:  17\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "cmdline"))
      .WillOnce(Return(std::string("first_cmdline\0", 14)));

  data_source->OnPids({42});

  {
    auto trace = writer_raw_->GetAllTracePackets();
    ASSERT_EQ(trace.size(), 1u);
    auto packet = trace[0];
    // First packet in the trace has no previous state, so the clear marker is
    // emitted.
    ASSERT_TRUE(packet.incremental_state_cleared());

    auto ps_tree = packet.process_tree();
    ASSERT_EQ(ps_tree.processes_size(), 1);
    ASSERT_EQ(ps_tree.processes()[0].pid(), 42);
    ASSERT_EQ(ps_tree.processes()[0].ppid(), 17);
    ASSERT_THAT(ps_tree.processes()[0].cmdline(),
                ElementsAreArray({"first_cmdline"}));
  }

  // Look up the same pid, which shouldn't be re-emitted.
  Mock::VerifyAndClearExpectations(data_source.get());
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "status")).Times(0);
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "cmdline")).Times(0);

  data_source->OnPids({42});

  {
    auto trace = writer_raw_->GetAllTracePackets();
    ASSERT_EQ(trace.size(), 1u);
  }

  // Invalidate incremental state, and look up the same pid again, which should
  // re-emit the proc tree info.
  Mock::VerifyAndClearExpectations(data_source.get());
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "status"))
      .WillOnce(Return("Name: foo\nTgid:\t42\nPid:   42\nPPid:  18\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(42, "cmdline"))
      .WillOnce(Return(std::string("second_cmdline\0", 15)));

  data_source->ClearIncrementalState();
  data_source->OnPids({42});

  {
    // Second packet with new proc information.
    auto trace = writer_raw_->GetAllTracePackets();
    ASSERT_EQ(trace.size(), 2u);
    auto packet = trace[1];
    ASSERT_TRUE(packet.incremental_state_cleared());

    auto ps_tree = packet.process_tree();
    ASSERT_EQ(ps_tree.processes_size(), 1);
    ASSERT_EQ(ps_tree.processes()[0].pid(), 42);
    ASSERT_EQ(ps_tree.processes()[0].ppid(), 18);
    ASSERT_THAT(ps_tree.processes()[0].cmdline(),
                ElementsAreArray({"second_cmdline"}));
  }
}

TEST_F(ProcessStatsDataSourceTest, RenamePids) {
  // assertion helpers
  auto expected_old_process = [](int pid) {
    return [pid](protos::gen::ProcessTree::Process process) {
      return process.pid() == pid && process.cmdline_size() > 0 &&
             process.cmdline()[0] == "proc_" + std::to_string(pid);
    };
  };
  auto expected_new_process = [](int pid) {
    return [pid](protos::gen::ProcessTree::Process process) {
      return process.pid() == pid && process.cmdline_size() > 0 &&
             process.cmdline()[0] == "new_" + std::to_string(pid);
    };
  };

  DataSourceConfig config;
  auto data_source = GetProcessStatsDataSource(config);
  for (int p : {10, 20}) {
    EXPECT_CALL(*data_source, ReadProcPidFile(p, "status"))
        .WillRepeatedly(Invoke([](int32_t pid, const std::string&) {
          return "Name: \tthread_" + std::to_string(pid) +
                 "\nTgid:  " + std::to_string(pid) +
                 "\nPid:   " + std::to_string(pid) + "\nPPid:  1\n";
        }));

    std::string old_proc_name = "proc_" + std::to_string(p);
    old_proc_name.resize(old_proc_name.size() + 1);  // Add a trailing \0.

    std::string new_proc_name = "new_" + std::to_string(p);
    new_proc_name.resize(new_proc_name.size() + 1);  // Add a trailing \0.
    EXPECT_CALL(*data_source, ReadProcPidFile(p, "cmdline"))
        .WillOnce(Return(old_proc_name))
        .WillOnce(Return(new_proc_name));
  }

  data_source->OnPids({10, 20});
  data_source->OnRenamePids({10});
  data_source->OnPids({10, 20});
  data_source->OnRenamePids({20});
  data_source->OnPids({10, 20});

  // check written contents
  auto trace = writer_raw_->GetAllTracePackets();
  EXPECT_EQ(trace.size(), 3u);

  // first packet - two unique processes
  auto ps_tree = trace[0].process_tree();
  EXPECT_THAT(ps_tree.processes(),
              UnorderedElementsAre(Truly(expected_old_process(10)),
                                   Truly(expected_old_process(20))));
  EXPECT_EQ(ps_tree.threads_size(), 0);

  // second packet - one new process
  ps_tree = trace[1].process_tree();
  EXPECT_THAT(ps_tree.processes(),
              UnorderedElementsAre(Truly(expected_new_process(10))));
  EXPECT_EQ(ps_tree.threads_size(), 0);

  // third packet - two threads that haven't been seen before
  ps_tree = trace[2].process_tree();
  EXPECT_THAT(ps_tree.processes(),
              UnorderedElementsAre(Truly(expected_new_process(20))));
  EXPECT_EQ(ps_tree.threads_size(), 0);
}

TEST_F(ProcessStatsDataSourceTest, ProcessStats) {
  DataSourceConfig ds_config;
  ProcessStatsConfig cfg;
  cfg.set_proc_stats_poll_ms(1);
  cfg.add_quirks(ProcessStatsConfig::DISABLE_ON_DEMAND);
  ds_config.set_process_stats_config_raw(cfg.SerializeAsString());
  auto data_source = GetProcessStatsDataSource(ds_config);

  // Populate a fake /proc/ directory.
  auto fake_proc = base::TempDir::Create();
  const int kPids[] = {1, 2};
  std::vector<std::string> dirs_to_delete;
  for (int pid : kPids) {
    char path[256];
    sprintf(path, "%s/%d", fake_proc.path().c_str(), pid);
    dirs_to_delete.push_back(path);
    mkdir(path, 0755);
  }

  auto checkpoint = task_runner_.CreateCheckpoint("all_done");

  EXPECT_CALL(*data_source, OpenProcDir()).WillRepeatedly(Invoke([&fake_proc] {
    return base::ScopedDir(opendir(fake_proc.path().c_str()));
  }));

  const int kNumIters = 4;
  int iter = 0;
  for (int pid : kPids) {
    EXPECT_CALL(*data_source, ReadProcPidFile(pid, "status"))
        .WillRepeatedly(Invoke([checkpoint, &iter](int32_t p,
                                                   const std::string&) {
          char ret[1024];
          sprintf(ret, "Name:	pid_10\nVmSize:	 %d kB\nVmRSS:\t%d  kB\n",
                  p * 100 + iter * 10 + 1, p * 100 + iter * 10 + 2);
          return std::string(ret);
        }));

    EXPECT_CALL(*data_source, ReadProcPidFile(pid, "oom_score_adj"))
        .WillRepeatedly(Invoke(
            [checkpoint, kPids, &iter](int32_t inner_pid, const std::string&) {
              auto oom_score = inner_pid * 100 + iter * 10 + 3;
              if (inner_pid == kPids[base::ArraySize(kPids) - 1]) {
                if (++iter == kNumIters)
                  checkpoint();
              }
              return std::to_string(oom_score);
            }));
  }

  data_source->Start();
  task_runner_.RunUntilCheckpoint("all_done");
  data_source->Flush(1 /* FlushRequestId */, []() {});

  std::vector<protos::gen::ProcessStats::Process> processes;
  auto trace = writer_raw_->GetAllTracePackets();
  for (const auto& packet : trace) {
    for (const auto& process : packet.process_stats().processes()) {
      processes.push_back(process);
    }
  }
  ASSERT_EQ(processes.size(), kNumIters * base::ArraySize(kPids));
  iter = 0;
  for (const auto& proc_counters : processes) {
    int32_t pid = proc_counters.pid();
    ASSERT_EQ(static_cast<int>(proc_counters.vm_size_kb()),
              pid * 100 + iter * 10 + 1);
    ASSERT_EQ(static_cast<int>(proc_counters.vm_rss_kb()),
              pid * 100 + iter * 10 + 2);
    ASSERT_EQ(static_cast<int>(proc_counters.oom_score_adj()),
              pid * 100 + iter * 10 + 3);
    if (pid == kPids[base::ArraySize(kPids) - 1])
      iter++;
  }

  // Cleanup |fake_proc|. TempDir checks that the directory is empty.
  for (std::string& path : dirs_to_delete)
    rmdir(path.c_str());
}

TEST_F(ProcessStatsDataSourceTest, CacheProcessStats) {
  DataSourceConfig ds_config;
  ProcessStatsConfig cfg;
  cfg.set_proc_stats_poll_ms(105);
  cfg.set_proc_stats_cache_ttl_ms(220);
  cfg.add_quirks(ProcessStatsConfig::DISABLE_ON_DEMAND);
  ds_config.set_process_stats_config_raw(cfg.SerializeAsString());
  auto data_source = GetProcessStatsDataSource(ds_config);

  // Populate a fake /proc/ directory.
  auto fake_proc = base::TempDir::Create();
  const int kPid = 1;

  char path[256];
  sprintf(path, "%s/%d", fake_proc.path().c_str(), kPid);
  mkdir(path, 0755);

  auto checkpoint = task_runner_.CreateCheckpoint("all_done");

  EXPECT_CALL(*data_source, OpenProcDir()).WillRepeatedly(Invoke([&fake_proc] {
    return base::ScopedDir(opendir(fake_proc.path().c_str()));
  }));

  const int kNumIters = 4;
  int iter = 0;
  EXPECT_CALL(*data_source, ReadProcPidFile(kPid, "status"))
      .WillRepeatedly(Invoke([checkpoint](int32_t p, const std::string&) {
        char ret[1024];
        sprintf(ret, "Name:	pid_10\nVmSize:	 %d kB\nVmRSS:\t%d  kB\n",
                p * 100 + 1, p * 100 + 2);
        return std::string(ret);
      }));

  EXPECT_CALL(*data_source, ReadProcPidFile(kPid, "oom_score_adj"))
      .WillRepeatedly(
          Invoke([checkpoint, &iter](int32_t inner_pid, const std::string&) {
            if (++iter == kNumIters)
              checkpoint();
            return std::to_string(inner_pid * 100);
          }));

  data_source->Start();
  task_runner_.RunUntilCheckpoint("all_done");
  data_source->Flush(1 /* FlushRequestId */, []() {});

  std::vector<protos::gen::ProcessStats::Process> processes;
  auto trace = writer_raw_->GetAllTracePackets();
  for (const auto& packet : trace) {
    for (const auto& process : packet.process_stats().processes()) {
      processes.push_back(process);
    }
  }
  // We should get two counter events because:
  // a) emissions happen at 0ms, 105ms, 210ms, 315ms
  // b) clear events happen at 220ms, 440ms...
  // Therefore, we should see the emissions at 0ms and 315ms.
  ASSERT_EQ(processes.size(), 2u);
  for (const auto& proc_counters : processes) {
    ASSERT_EQ(proc_counters.pid(), kPid);
    ASSERT_EQ(static_cast<int>(proc_counters.vm_size_kb()), kPid * 100 + 1);
    ASSERT_EQ(static_cast<int>(proc_counters.vm_rss_kb()), kPid * 100 + 2);
    ASSERT_EQ(static_cast<int>(proc_counters.oom_score_adj()), kPid * 100);
  }

  // Cleanup |fake_proc|. TempDir checks that the directory is empty.
  rmdir(path);
}

TEST_F(ProcessStatsDataSourceTest, ThreadTimeInState) {
  DataSourceConfig ds_config;
  ProcessStatsConfig config;
  // Do 2 ticks before cache clear.
  config.set_proc_stats_poll_ms(100);
  config.set_proc_stats_cache_ttl_ms(200);
  config.add_quirks(ProcessStatsConfig::DISABLE_ON_DEMAND);
  config.set_record_thread_time_in_state(true);
  ds_config.set_process_stats_config_raw(config.SerializeAsString());
  auto data_source = GetProcessStatsDataSource(ds_config);

  std::vector<std::string> dirs_to_delete;
  auto make_proc_path = [&dirs_to_delete](base::TempDir& temp_dir, int pid) {
    char path[256];
    sprintf(path, "%s/%d", temp_dir.path().c_str(), pid);
    dirs_to_delete.push_back(path);
    mkdir(path, 0755);
  };
  // Populate a fake /proc/ directory.
  auto fake_proc = base::TempDir::Create();
  const int kPid = 1;
  make_proc_path(fake_proc, kPid);
  const int kIgnoredPid = 5;
  make_proc_path(fake_proc, kIgnoredPid);

  // Populate a fake /proc/1/task directory.
  auto fake_proc_task = base::TempDir::Create();
  const int kTids[] = {1, 2};
  for (int tid : kTids)
    make_proc_path(fake_proc_task, tid);
  // Populate a fake /proc/5/task directory.
  auto fake_ignored_proc_task = base::TempDir::Create();
  const int kIgnoredTid = 5;
  make_proc_path(fake_ignored_proc_task, kIgnoredTid);

  auto checkpoint = task_runner_.CreateCheckpoint("all_done");

  EXPECT_CALL(*data_source, OpenProcDir()).WillRepeatedly(Invoke([&fake_proc] {
    return base::ScopedDir(opendir(fake_proc.path().c_str()));
  }));
  EXPECT_CALL(*data_source, ReadProcPidFile(kPid, "status"))
      .WillRepeatedly(
          Return("Name:	pid_1\nVmSize:	 100 kB\nVmRSS:\t100  kB\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(kPid, "oom_score_adj"))
      .WillRepeatedly(Return("901"));
  EXPECT_CALL(*data_source, ReadProcPidFile(kPid, "stat"))
      .WillOnce(Return("1 (pid_1) S 1 1 0 0 -1 4210944 2197 2451 0 1 54 117 4"))
      // ctime++
      .WillOnce(Return("1 (pid_1) S 1 1 0 0 -1 4210944 2197 2451 0 1 55 117 4"))
      // stime++
      .WillOnce(
          Return("1 (pid_1) S 1 1 0 0 -1 4210944 2197 2451 0 1 55 118 4"));
  EXPECT_CALL(*data_source, OpenProcTaskDir(kPid))
      .WillRepeatedly(Invoke([&fake_proc_task](int32_t) {
        return base::ScopedDir(opendir(fake_proc_task.path().c_str()));
      }));
  EXPECT_CALL(*data_source, ReadProcPidFile(kTids[0], "time_in_state"))
      .Times(3)
      .WillRepeatedly(Return("cpu0\n300000 1\n748800 1\ncpu1\n300000 5\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(kTids[1], "time_in_state"))
      .WillOnce(
          Return("cpu0\n300000 10\n748800 0\ncpu1\n300000 50\n652800 60\n"))
      .WillOnce(Return("cpu0\n300000 20\n748800 0\n1324800 30\ncpu1\n300000 "
                       "100\n652800 60\n"))
      .WillOnce(Invoke([&checkpoint](int32_t, const std::string&) {
        // Call checkpoint here to stop after the third tick.
        checkpoint();
        return "cpu0\n300000 200\n748800 0\n1324800 30\ncpu1\n300000 "
               "100\n652800 60\n";
      }));
  EXPECT_CALL(*data_source, ReadProcPidFile(kIgnoredPid, "status"))
      .WillRepeatedly(
          Return("Name:	pid_5\nVmSize:	 100 kB\nVmRSS:\t100  kB\n"));
  EXPECT_CALL(*data_source, ReadProcPidFile(kIgnoredPid, "oom_score_adj"))
      .WillRepeatedly(Return("905"));
  EXPECT_CALL(*data_source, OpenProcTaskDir(kIgnoredPid))
      .WillRepeatedly(Invoke([&fake_ignored_proc_task](int32_t) {
        return base::ScopedDir(opendir(fake_ignored_proc_task.path().c_str()));
      }));
  EXPECT_CALL(*data_source, ReadProcPidFile(kIgnoredPid, "stat"))
      .WillRepeatedly(
          Return("5 (pid_5) S 1 5 0 0 -1 4210944 2197 2451 0 1 99 99 4"));
  EXPECT_CALL(*data_source, ReadProcPidFile(kIgnoredTid, "time_in_state"))
      .Times(2)
      .WillRepeatedly(
          Return("cpu0\n300000 10\n748800 0\ncpu1\n300000 00\n652800 20\n"));

  data_source->Start();
  task_runner_.RunUntilCheckpoint("all_done");
  data_source->Flush(1 /* FlushRequestId */, []() {});

  // Collect all process packets order by their timestamp and pid.
  using TimestampPid = std::pair</* timestamp */ uint64_t, /* pid */ int32_t>;
  std::map<TimestampPid, protos::gen::ProcessStats::Process> processes_map;
  for (const auto& packet : writer_raw_->GetAllTracePackets())
    for (const auto& process : packet.process_stats().processes())
      processes_map.insert({{packet.timestamp(), process.pid()}, process});
  std::vector<protos::gen::ProcessStats::Process> processes;
  for (auto it : processes_map)
    processes.push_back(it.second);

  // 3 packets for pid=1, 2 packets for pid=5.
  EXPECT_EQ(processes.size(), 5u);

  auto compare_tid = [](protos::gen::ProcessStats_Thread& l,
                        protos::gen::ProcessStats_Thread& r) {
    return l.tid() < r.tid();
  };

  // First pull has all threads.
  // Check pid = 1.
  auto threads = processes[0].threads();
  EXPECT_EQ(threads.size(), 2u);
  std::sort(threads.begin(), threads.end(), compare_tid);
  auto thread = threads[0];
  EXPECT_EQ(thread.tid(), 1);
  EXPECT_THAT(thread.cpu_freq_indices(), ElementsAre(1u, 3u, 10u));
  EXPECT_THAT(thread.cpu_freq_ticks(), ElementsAre(1, 1, 5));
  thread = threads[1];
  EXPECT_EQ(thread.tid(), 2);
  EXPECT_THAT(thread.cpu_freq_indices(), ElementsAre(1u, 10u, 11u));
  EXPECT_THAT(thread.cpu_freq_ticks(), ElementsAre(10, 50, 60));
  // Check pid = 5.
  threads = processes[1].threads();
  EXPECT_EQ(threads.size(), 1u);
  EXPECT_EQ(threads[0].tid(), 5);
  EXPECT_THAT(threads[0].cpu_freq_ticks(), ElementsAre(10, 20));

  // Second pull has only one thread with delta.
  threads = processes[2].threads();
  EXPECT_EQ(threads.size(), 1u);
  thread = threads[0];
  EXPECT_EQ(thread.tid(), 2);
  EXPECT_THAT(thread.cpu_freq_indices(), ElementsAre(1u, 6u, 10u));
  EXPECT_THAT(thread.cpu_freq_ticks(), ElementsAre(20, 30, 100));

  // Third pull has all thread because cache was cleared.
  // Check pid = 1.
  threads = processes[3].threads();
  EXPECT_EQ(threads.size(), 2u);
  std::sort(threads.begin(), threads.end(), compare_tid);
  thread = threads[0];
  EXPECT_EQ(thread.tid(), 1);
  EXPECT_THAT(thread.cpu_freq_indices(), ElementsAre(1u, 3u, 10u));
  EXPECT_THAT(thread.cpu_freq_ticks(), ElementsAre(1, 1, 5));
  thread = threads[1];
  EXPECT_EQ(thread.tid(), 2);
  EXPECT_THAT(thread.cpu_freq_indices(), ElementsAre(1u, 6u, 10u, 11u));
  EXPECT_THAT(thread.cpu_freq_ticks(), ElementsAre(200, 30, 100, 60));
  // Check pid = 5.
  threads = processes[4].threads();
  EXPECT_EQ(threads.size(), 1u);
  EXPECT_EQ(threads[0].tid(), 5);
  EXPECT_THAT(threads[0].cpu_freq_ticks(), ElementsAre(10, 20));

  for (const std::string& path : dirs_to_delete)
    rmdir(path.c_str());
}

}  // namespace
}  // namespace perfetto
