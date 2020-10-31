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

#include <unistd.h>

#include <chrono>
#include <functional>
#include <initializer_list>
#include <random>
#include <thread>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/pipe.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/subprocess.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/ext/ipc/basic_types.h"
#include "perfetto/ext/traced/traced.h"
#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/tracing/core/tracing_service_state.h"
#include "src/base/test/test_task_runner.h"
#include "src/base/test/utils.h"
#include "src/traced/probes/ftrace/ftrace_controller.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "test/gtest_and_gmock.h"
#include "test/test_helper.h"

#include "protos/perfetto/config/power/android_power_config.pbzero.h"
#include "protos/perfetto/config/test_config.gen.h"
#include "protos/perfetto/config/trace_config.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.gen.h"
#include "protos/perfetto/trace/power/battery_counters.gen.h"
#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/trigger.gen.h"

namespace perfetto {

namespace {

using ::testing::ContainsRegex;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

constexpr size_t kBuiltinPackets = 8;

std::string RandomTraceFileName() {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  constexpr char kSysTmpPath[] = "/data/misc/perfetto-traces";
#else
  constexpr char kSysTmpPath[] = "/tmp";
#endif
  static int suffix = 0;

  std::string path;
  path.assign(kSysTmpPath);
  path.append("/trace-");
  path.append(std::to_string(base::GetBootTimeNs().count()));
  path.append("-");
  path.append(std::to_string(suffix++));
  return path;
}

// This class is a reference to a child process that has in essence been execv
// to the requested binary. The process will start and then wait for Run()
// before proceeding. We use this to fork new processes before starting any
// additional threads in the parent process (otherwise you would risk
// deadlocks), but pause the forked processes until remaining setup (including
// any necessary threads) in the parent process is complete.
class Exec {
 public:
  // Starts the forked process that was created. If not null then |stderr_out|
  // will contain the stderr of the process.
  int Run(std::string* stderr_out = nullptr) {
    // We can't be the child process.
    PERFETTO_CHECK(getpid() != subprocess_.pid());
    // Will cause the entrypoint to continue.
    PERFETTO_CHECK(write(*sync_pipe_.wr, "1", 1) == 1);
    sync_pipe_.wr.reset();
    subprocess_.Wait();

    if (stderr_out) {
      *stderr_out = std::move(subprocess_.output());
    } else {
      PERFETTO_LOG("Child proc %d exited with stderr: \"%s\"",
                   subprocess_.pid(), subprocess_.output().c_str());
    }
    return subprocess_.returncode();
  }

 private:
  Exec(const std::string& argv0,
       std::initializer_list<std::string> args,
       std::string input = "") {
    subprocess_.args.stderr_mode = base::Subprocess::kBuffer;
    subprocess_.args.stdout_mode = base::Subprocess::kDevNull;
    subprocess_.args.input = input;

#if PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
    constexpr bool kUseSystemBinaries = false;
#else
    constexpr bool kUseSystemBinaries = true;
#endif

    std::vector<std::string>& cmd = subprocess_.args.exec_cmd;
    if (kUseSystemBinaries) {
      cmd.push_back("/system/bin/" + argv0);
      cmd.insert(cmd.end(), args.begin(), args.end());
    } else {
      subprocess_.args.env.push_back(
          std::string("PERFETTO_PRODUCER_SOCK_NAME=") +
          TestHelper::GetProducerSocketName());
      subprocess_.args.env.push_back(
          std::string("PERFETTO_CONSUMER_SOCK_NAME=") +
          TestHelper::GetConsumerSocketName());
      cmd.push_back(base::GetCurExecutableDir() + "/" + argv0);
      cmd.insert(cmd.end(), args.begin(), args.end());
    }

    if (access(cmd[0].c_str(), F_OK)) {
      PERFETTO_FATAL(
          "Cannot find %s. Make sure that the target has been built and, on "
          "Android, pushed to the device.",
          cmd[0].c_str());
    }

    // This pipe blocks the execution of the child process until the main test
    // process calls Run(). There are two conflicting problems here:
    // 1) We can't fork() subprocesses too late, because the test spawns threads
    //    for hosting the service. fork+threads = bad (see aosp/1089744).
    // 2) We can't run the subprocess too early, because we need to wait that
    //    the service threads are ready before trying to connect from the child
    //    process.
    sync_pipe_ = base::Pipe::Create();
    int sync_pipe_rd = *sync_pipe_.rd;
    subprocess_.args.preserve_fds.push_back(sync_pipe_rd);

    // This lambda will be called on the forked child process after having
    // setup pipe redirection and closed all FDs, right before the exec().
    // The Subprocesss harness will take care of closing also |sync_pipe_.wr|.
    subprocess_.args.entrypoint_for_testing = [sync_pipe_rd] {
      // Don't add any logging here, all file descriptors are closed and trying
      // to log will likely cause undefined behaviors.
      char ignored = 0;
      PERFETTO_CHECK(PERFETTO_EINTR(read(sync_pipe_rd, &ignored, 1)) > 0);
      PERFETTO_CHECK(close(sync_pipe_rd) == 0 || errno == EINTR);
    };

    subprocess_.Start();
    sync_pipe_.rd.reset();
  }

  friend class PerfettoCmdlineTest;
  base::Subprocess subprocess_;
  base::Pipe sync_pipe_;
};

class PerfettoTest : public ::testing::Test {
 public:
  void SetUp() override {
    // TODO(primiano): refactor this, it's copy/pasted in three places now.
    size_t index = 0;
    constexpr auto kTracingPaths = FtraceController::kTracingPaths;
    while (!ftrace_procfs_ && kTracingPaths[index]) {
      ftrace_procfs_ = FtraceProcfs::Create(kTracingPaths[index++]);
    }
  }

  std::unique_ptr<FtraceProcfs> ftrace_procfs_;
};

class PerfettoCmdlineTest : public ::testing::Test {
 public:
  void SetUp() override { exec_allowed_ = true; }

  void TearDown() override {}

  void StartServiceIfRequiredNoNewExecsAfterThis() {
    exec_allowed_ = false;
    test_helper_.StartServiceIfRequired();
  }

  FakeProducer* ConnectFakeProducer() {
    return test_helper_.ConnectFakeProducer();
  }

  std::function<void()> WrapTask(const std::function<void()>& function) {
    return test_helper_.WrapTask(function);
  }

  void WaitForProducerSetup() { test_helper_.WaitForProducerSetup(); }

  void WaitForProducerEnabled() { test_helper_.WaitForProducerEnabled(); }

  // Creates a process that represents the perfetto binary that will
  // start when Run() is called. |args| will be passed as part of
  // the command line and |std_in| will be piped into std::cin.
  Exec ExecPerfetto(std::initializer_list<std::string> args,
                    std::string std_in = "") {
    // You can not fork after you've started the service due to risk of
    // deadlocks.
    PERFETTO_CHECK(exec_allowed_);
    return Exec("perfetto", std::move(args), std::move(std_in));
  }

  // Creates a process that represents the trigger_perfetto binary that will
  // start when Run() is called. |args| will be passed as part of
  // the command line and |std_in| will be piped into std::cin.
  Exec ExecTrigger(std::initializer_list<std::string> args,
                   std::string std_in = "") {
    // You can not fork after you've started the service due to risk of
    // deadlocks.
    PERFETTO_CHECK(exec_allowed_);
    return Exec("trigger_perfetto", std::move(args), std::move(std_in));
  }

  // Tests are allowed to freely use these variables.
  std::string stderr_;
  base::TestTaskRunner task_runner_;

 private:
  bool exec_allowed_;
  TestHelper test_helper_{&task_runner_};
};

}  // namespace

// If we're building on Android and starting the daemons ourselves,
// create the sockets in a world-writable location.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) && \
    PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
#define TEST_PRODUCER_SOCK_NAME "/data/local/tmp/traced_producer"
#else
#define TEST_PRODUCER_SOCK_NAME ::perfetto::GetProducerSocket()
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
#define TreeHuggerOnly(x) x
#else
#define TreeHuggerOnly(x) DISABLED_##x
#endif

// TODO(b/73453011): reenable on more platforms (including standalone Android).
TEST_F(PerfettoTest, TreeHuggerOnly(TestFtraceProducer)) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();

#if PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
  ProbesProducerThread probes(TEST_PRODUCER_SOCK_NAME);
  probes.Connect();
#endif

  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.set_duration_ms(3000);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("linux.ftrace");
  ds_config->set_target_buffer(0);

  protos::gen::FtraceConfig ftrace_config;
  ftrace_config.add_ftrace_events("sched_switch");
  ftrace_config.add_ftrace_events("bar");
  ds_config->set_ftrace_config_raw(ftrace_config.SerializeAsString());

  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled();

  helper.ReadData();
  helper.WaitForReadData();

  const auto& packets = helper.trace();
  ASSERT_GT(packets.size(), 0u);

  for (const auto& packet : packets) {
    for (int ev = 0; ev < packet.ftrace_events().event_size(); ev++) {
      ASSERT_TRUE(packet.ftrace_events()
                      .event()[static_cast<size_t>(ev)]
                      .has_sched_switch());
    }
  }
}

// TODO(b/73453011): reenable on more platforms (including standalone Android).
TEST_F(PerfettoTest, TreeHuggerOnly(TestFtraceFlush)) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();

#if PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
  ProbesProducerThread probes(TEST_PRODUCER_SOCK_NAME);
  probes.Connect();
#endif

  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  const uint32_t kTestTimeoutMs = 30000;
  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(32);
  trace_config.set_duration_ms(kTestTimeoutMs);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("linux.ftrace");

  protos::gen::FtraceConfig ftrace_config;
  ftrace_config.add_ftrace_events("print");
  ds_config->set_ftrace_config_raw(ftrace_config.SerializeAsString());

  helper.StartTracing(trace_config);

  // Do a first flush just to synchronize with the producer. The problem here
  // is that, on a Linux workstation, the producer can take several seconds just
  // to get to the point where ftrace is ready. We use the flush ack as a
  // synchronization point.
  helper.FlushAndWait(kTestTimeoutMs);

  EXPECT_TRUE(ftrace_procfs_->IsTracingEnabled());
  const char kMarker[] = "just_one_event";
  EXPECT_TRUE(ftrace_procfs_->WriteTraceMarker(kMarker));

  // This is the real flush we are testing.
  helper.FlushAndWait(kTestTimeoutMs);

  helper.DisableTracing();
  helper.WaitForTracingDisabled(kTestTimeoutMs);

  helper.ReadData();
  helper.WaitForReadData();

  int marker_found = 0;
  for (const auto& packet : helper.trace()) {
    for (int i = 0; i < packet.ftrace_events().event_size(); i++) {
      const auto& ev = packet.ftrace_events().event()[static_cast<size_t>(i)];
      if (ev.has_print() && ev.print().buf().find(kMarker) != std::string::npos)
        marker_found++;
    }
  }
  ASSERT_EQ(marker_found, 1);
}

// TODO(b/73453011): reenable on more platforms (including standalone Android).
TEST_F(PerfettoTest, TreeHuggerOnly(TestBatteryTracing)) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();

#if PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
  ProbesProducerThread probes(TEST_PRODUCER_SOCK_NAME);
  probes.Connect();
#else
  base::ignore_result(TEST_PRODUCER_SOCK_NAME);
#endif

  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.set_duration_ms(3000);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.power");
  ds_config->set_target_buffer(0);

  using protos::pbzero::AndroidPowerConfig;
  protozero::HeapBuffered<AndroidPowerConfig> power_config;
  power_config->set_battery_poll_ms(250);
  power_config->add_battery_counters(
      AndroidPowerConfig::BATTERY_COUNTER_CHARGE);
  power_config->add_battery_counters(
      AndroidPowerConfig::BATTERY_COUNTER_CAPACITY_PERCENT);
  ds_config->set_android_power_config_raw(power_config.SerializeAsString());

  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled();

  helper.ReadData();
  helper.WaitForReadData();

  const auto& packets = helper.trace();
  ASSERT_GT(packets.size(), 0u);

  bool has_battery_packet = false;
  for (const auto& packet : packets) {
    if (!packet.has_battery())
      continue;
    has_battery_packet = true;
    // Unfortunately we cannot make any assertions on the charge counter.
    // On some devices it can reach negative values (b/64685329).
    EXPECT_GE(packet.battery().capacity_percent(), 0);
    EXPECT_LE(packet.battery().capacity_percent(), 100);
  }

  ASSERT_TRUE(has_battery_packet);
}

TEST_F(PerfettoTest, TestFakeProducer) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();
  helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.set_duration_ms(200);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->set_target_buffer(0);

  static constexpr size_t kNumPackets = 11;
  static constexpr uint32_t kRandomSeed = 42;
  static constexpr uint32_t kMsgSize = 1024;
  ds_config->mutable_for_testing()->set_seed(kRandomSeed);
  ds_config->mutable_for_testing()->set_message_count(kNumPackets);
  ds_config->mutable_for_testing()->set_message_size(kMsgSize);
  ds_config->mutable_for_testing()->set_send_batch_on_register(true);

  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled();

  helper.ReadData();
  helper.WaitForReadData();

  const auto& packets = helper.trace();
  ASSERT_EQ(packets.size(), kNumPackets);

  std::minstd_rand0 rnd_engine(kRandomSeed);
  for (const auto& packet : packets) {
    ASSERT_TRUE(packet.has_for_testing());
    ASSERT_EQ(packet.for_testing().seq_value(), rnd_engine());
  }
}

TEST_F(PerfettoTest, VeryLargePackets) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();
  helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096 * 10);
  trace_config.set_duration_ms(500);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->set_target_buffer(0);

  static constexpr size_t kNumPackets = 7;
  static constexpr uint32_t kRandomSeed = 42;
  static constexpr uint32_t kMsgSize = 1024 * 1024 - 42;
  ds_config->mutable_for_testing()->set_seed(kRandomSeed);
  ds_config->mutable_for_testing()->set_message_count(kNumPackets);
  ds_config->mutable_for_testing()->set_message_size(kMsgSize);
  ds_config->mutable_for_testing()->set_send_batch_on_register(true);

  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled();

  helper.ReadData();
  helper.WaitForReadData(/* read_count */ 0, /* timeout_ms */ 10000);

  const auto& packets = helper.trace();
  ASSERT_EQ(packets.size(), kNumPackets);

  std::minstd_rand0 rnd_engine(kRandomSeed);
  for (const auto& packet : packets) {
    ASSERT_TRUE(packet.has_for_testing());
    ASSERT_EQ(packet.for_testing().seq_value(), rnd_engine());
    size_t msg_size = packet.for_testing().str().size();
    ASSERT_EQ(kMsgSize, msg_size);
    for (size_t i = 0; i < msg_size; i++)
      ASSERT_EQ(i < msg_size - 1 ? '.' : 0, packet.for_testing().str()[i]);
  }
}

TEST_F(PerfettoTest, DetachAndReattach) {
  base::TestTaskRunner task_runner;

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.set_duration_ms(10000);  // Max timeout, session is ended before.
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  static constexpr size_t kNumPackets = 11;
  ds_config->mutable_for_testing()->set_message_count(kNumPackets);
  ds_config->mutable_for_testing()->set_message_size(32);

  // Enable tracing and detach as soon as it gets started.
  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();
  auto* fake_producer = helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();
  helper.StartTracing(trace_config);

  // Detach.
  helper.DetachConsumer("key");

  // Write data while detached.
  helper.WaitForProducerEnabled();
  auto on_data_written = task_runner.CreateCheckpoint("data_written");
  fake_producer->ProduceEventBatch(helper.WrapTask(on_data_written));
  task_runner.RunUntilCheckpoint("data_written");

  // Then reattach the consumer.
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();
  helper.AttachConsumer("key");

  helper.DisableTracing();
  helper.WaitForTracingDisabled();

  helper.ReadData();
  helper.WaitForReadData();
  const auto& packets = helper.trace();
  ASSERT_EQ(packets.size(), kNumPackets);
}

// Tests that a detached trace session is automatically cleaned up if the
// consumer doesn't re-attach before its expiration time.
TEST_F(PerfettoTest, ReattachFailsAfterTimeout) {
  base::TestTaskRunner task_runner;

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.set_duration_ms(250);
  trace_config.set_write_into_file(true);
  trace_config.set_file_write_period_ms(100000);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->mutable_for_testing()->set_message_count(1);
  ds_config->mutable_for_testing()->set_message_size(32);
  ds_config->mutable_for_testing()->set_send_batch_on_register(true);

  // Enable tracing and detach as soon as it gets started.
  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();
  helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  auto pipe_pair = base::Pipe::Create();
  helper.StartTracing(trace_config, std::move(pipe_pair.wr));

  // Detach.
  helper.DetachConsumer("key");

  // Use the file EOF (write end closed) as a way to detect when the trace
  // session is ended.
  char buf[1024];
  while (PERFETTO_EINTR(read(*pipe_pair.rd, buf, sizeof(buf))) > 0) {
  }

  // Give some margin for the tracing service to destroy the session.
  usleep(250000);

  // Reconnect and find out that it's too late and the session is gone.
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();
  EXPECT_FALSE(helper.AttachConsumer("key"));
}

TEST_F(PerfettoTest, TestProducerProvidedSMB) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.CreateProducerProvidedSmb();

  protos::gen::TestConfig test_config;
  test_config.set_seed(42);
  test_config.set_message_count(1);
  test_config.set_message_size(1024);
  test_config.set_send_batch_on_register(true);

  // Write a first batch before connection.
  helper.ProduceStartupEventBatch(test_config);

  helper.StartServiceIfRequired();
  helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.set_duration_ms(200);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->set_target_buffer(0);
  *ds_config->mutable_for_testing() = test_config;

  // The data source is configured to emit another batch when it is started via
  // send_batch_on_register in the TestConfig.
  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled();

  EXPECT_TRUE(helper.IsShmemProvidedByProducer());

  helper.ReadData();
  helper.WaitForReadData();

  const auto& packets = helper.trace();
  // We should have produced two batches, one before the producer connected and
  // another one when the data source was started.
  ASSERT_EQ(packets.size(), 2u);
  ASSERT_TRUE(packets[0].has_for_testing());
  ASSERT_TRUE(packets[1].has_for_testing());
}

// Regression test for b/153142114.
TEST_F(PerfettoTest, QueryServiceStateLargeResponse) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  FakeProducer* producer = helper.ConnectFakeProducer();

  // Register 5 data sources with very large descriptors. Each descriptor will
  // max out the IPC message size, so that the service has no other choice
  // than chunking them.
  std::map<std::string, std::string> ds_expected;
  for (int i = 0; i < 5; i++) {
    DataSourceDescriptor dsd;
    std::string name = "big_ds_" + std::to_string(i);
    dsd.set_name(name);
    std::string descriptor(ipc::kIPCBufferSize - 64, (' ' + i) % 64);
    dsd.set_track_event_descriptor_raw(descriptor);
    ds_expected[name] = std::move(descriptor);
    producer->RegisterDataSource(dsd);
  }

  // Linearize the producer with the service. We need to make sure that all the
  // RegisterDataSource() calls above have been seen by the service before
  // continuing.
  helper.SyncAndWaitProducer();

  // Now invoke QueryServiceState() and wait for the reply. The service will
  // send 6 (1 + 5) IPCs which will be merged together in
  // producer_ipc_client_impl.cc.
  auto svc_state = helper.QueryServiceStateAndWait();

  ASSERT_GE(svc_state.producers().size(), 1u);

  std::map<std::string, std::string> ds_found;
  for (const auto& ds : svc_state.data_sources()) {
    if (!base::StartsWith(ds.ds_descriptor().name(), "big_ds_"))
      continue;
    ds_found[ds.ds_descriptor().name()] =
        ds.ds_descriptor().track_event_descriptor_raw();
  }
  EXPECT_THAT(ds_found, ElementsAreArray(ds_expected));
}

// Disable cmdline tests on sanitizets because they use fork() and that messes
// up leak / races detections, which has been fixed only recently (see
// https://github.com/google/sanitizers/issues/836 ).
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER)
#define NoSanitizers(X) DISABLED_##X
#else
#define NoSanitizers(X) X
#endif

TEST_F(PerfettoCmdlineTest, NoSanitizers(InvalidCases)) {
  std::string cfg("duration_ms: 100");

  auto invalid_arg = ExecPerfetto({"--invalid-arg"});
  auto empty_config = ExecPerfetto({"-c", "-", "-o", "-"}, "");

  // Cannot make assertions on --dropbox because on standalone builds it fails
  // prematurely due to lack of dropbox.
  auto missing_dropbox =
      ExecPerfetto({"-c", "-", "--txt", "-o", "-", "--dropbox=foo"}, cfg);
  auto either_out_or_dropbox = ExecPerfetto({"-c", "-", "--txt"}, cfg);

  // Disallow mixing simple and file config.
  auto simple_and_file_1 =
      ExecPerfetto({"-o", "-", "-c", "-", "-t", "2s"}, cfg);
  auto simple_and_file_2 =
      ExecPerfetto({"-o", "-", "-c", "-", "-b", "2m"}, cfg);
  auto simple_and_file_3 =
      ExecPerfetto({"-o", "-", "-c", "-", "-s", "2m"}, cfg);

  // Invalid --attach / --detach cases.
  auto invalid_stop =
      ExecPerfetto({"-c", "-", "--txt", "-o", "-", "--stop"}, cfg);
  auto attach_and_config_1 =
      ExecPerfetto({"-c", "-", "--txt", "-o", "-", "--attach=foo"}, cfg);
  auto attach_and_config_2 =
      ExecPerfetto({"-t", "2s", "-o", "-", "--attach=foo"}, cfg);
  auto attach_needs_argument = ExecPerfetto({"--attach"}, cfg);
  auto detach_needs_argument =
      ExecPerfetto({"-t", "2s", "-o", "-", "--detach"}, cfg);
  auto detach_without_out_or_dropbox =
      ExecPerfetto({"-t", "2s", "--detach=foo"}, cfg);

  // Cannot trace and use --query.
  auto trace_and_query_1 = ExecPerfetto({"-t", "2s", "--query"}, cfg);
  auto trace_and_query_2 = ExecPerfetto({"-c", "-", "--query"}, cfg);

  // Ensure all Exec:: calls have been saved to prevent deadlocks.
  StartServiceIfRequiredNoNewExecsAfterThis();

  EXPECT_EQ(1, invalid_arg.Run(&stderr_));

  EXPECT_EQ(1, empty_config.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("TraceConfig is empty"));

  // Cannot make assertions on --dropbox because on standalone builds it fails
  // prematurely due to lack of dropbox.
  EXPECT_EQ(1, missing_dropbox.Run(&stderr_));

  EXPECT_EQ(1, either_out_or_dropbox.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Either --out or --dropbox"));

  // Disallow mixing simple and file config.
  EXPECT_EQ(1, simple_and_file_1.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify both -c"));

  EXPECT_EQ(1, simple_and_file_2.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify both -c"));

  EXPECT_EQ(1, simple_and_file_3.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify both -c"));

  // Invalid --attach / --detach cases.
  EXPECT_EQ(1, invalid_stop.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("--stop is supported only in combination"));

  EXPECT_EQ(1, attach_and_config_1.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify a trace config"));

  EXPECT_EQ(1, attach_and_config_2.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify a trace config"));

  EXPECT_EQ(1, attach_needs_argument.Run(&stderr_));
  EXPECT_THAT(stderr_, ContainsRegex("option.*--attach.*requires an argument"));

  EXPECT_EQ(1, detach_needs_argument.Run(&stderr_));
  EXPECT_THAT(stderr_, ContainsRegex("option.*--detach.*requires an argument"));

  EXPECT_EQ(1, detach_without_out_or_dropbox.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("--out or --dropbox is required"));

  // Cannot trace and use --query.
  EXPECT_EQ(1, trace_and_query_1.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify a trace config"));

  EXPECT_EQ(1, trace_and_query_2.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Cannot specify a trace config"));
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(TxtConfig)) {
  std::string cfg("duration_ms: 100");
  auto perfetto = ExecPerfetto({"-c", "-", "--txt", "-o", "-"}, cfg);
  StartServiceIfRequiredNoNewExecsAfterThis();
  EXPECT_EQ(0, perfetto.Run(&stderr_)) << stderr_;
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(SimpleConfig)) {
  auto perfetto = ExecPerfetto({"-o", "-", "-c", "-", "-t", "100ms"});
  StartServiceIfRequiredNoNewExecsAfterThis();
  EXPECT_EQ(0, perfetto.Run(&stderr_)) << stderr_;
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(DetachAndAttach)) {
  auto attach_to_not_existing = ExecPerfetto({"--attach=not_existent"});

  std::string cfg("duration_ms: 10000; write_into_file: true");
  auto detach_valid_stop =
      ExecPerfetto({"-o", "-", "-c", "-", "--txt", "--detach=valid_stop"}, cfg);
  auto stop_valid_stop = ExecPerfetto({"--attach=valid_stop", "--stop"});

  StartServiceIfRequiredNoNewExecsAfterThis();

  EXPECT_NE(0, attach_to_not_existing.Run(&stderr_));
  EXPECT_THAT(stderr_, HasSubstr("Session re-attach failed"));

  EXPECT_EQ(0, detach_valid_stop.Run(&stderr_)) << stderr_;
  EXPECT_EQ(0, stop_valid_stop.Run(&stderr_));
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(StartTracingTrigger)) {
  // See |message_count| and |message_size| in the TraceConfig above.
  constexpr size_t kMessageCount = 11;
  constexpr size_t kMessageSize = 32;
  protos::gen::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->mutable_for_testing()->set_message_count(kMessageCount);
  ds_config->mutable_for_testing()->set_message_size(kMessageSize);
  auto* trigger_cfg = trace_config.mutable_trigger_config();
  trigger_cfg->set_trigger_mode(
      protos::gen::TraceConfig::TriggerConfig::START_TRACING);
  trigger_cfg->set_trigger_timeout_ms(15000);
  auto* trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name");
  // |stop_delay_ms| must be long enough that we can write the packets in
  // before the trace finishes. This has to be long enough for the slowest
  // emulator. But as short as possible to prevent the test running a long
  // time.
  trigger->set_stop_delay_ms(500);

  // We have to construct all the processes we want to fork before we start the
  // service with |StartServiceIfRequired()|. this is because it is unsafe
  // (could deadlock) to fork after we've spawned some threads which might
  // printf (and thus hold locks).
  const std::string path = RandomTraceFileName();
  auto perfetto_proc = ExecPerfetto(
      {
          "-o",
          path,
          "-c",
          "-",
      },
      trace_config.SerializeAsString());

  auto trigger_proc = ExecTrigger({"trigger_name"});

  // Start the service and connect a simple fake producer.
  StartServiceIfRequiredNoNewExecsAfterThis();

  auto* fake_producer = ConnectFakeProducer();
  EXPECT_TRUE(fake_producer);

  // Start a background thread that will deliver the config now that we've
  // started the service. See |perfetto_proc| above for the args passed.
  std::thread background_trace([&perfetto_proc]() {
    std::string stderr_str;
    EXPECT_EQ(0, perfetto_proc.Run(&stderr_str)) << stderr_str;
  });

  WaitForProducerSetup();
  EXPECT_EQ(0, trigger_proc.Run(&stderr_));

  // Wait for the producer to start, and then write out 11 packets.
  WaitForProducerEnabled();
  auto on_data_written = task_runner_.CreateCheckpoint("data_written");
  fake_producer->ProduceEventBatch(WrapTask(on_data_written));
  task_runner_.RunUntilCheckpoint("data_written");
  background_trace.join();

  std::string trace_str;
  base::ReadFile(path, &trace_str);
  protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromString(trace_str));
  EXPECT_EQ(static_cast<int>(kBuiltinPackets + kMessageCount),
            trace.packet_size());
  for (const auto& packet : trace.packet()) {
    if (packet.has_trace_config()) {
      // Ensure the trace config properly includes the trigger mode we set.
      auto kStartTrig = protos::gen::TraceConfig::TriggerConfig::START_TRACING;
      EXPECT_EQ(kStartTrig,
                packet.trace_config().trigger_config().trigger_mode());
    } else if (packet.has_trigger()) {
      // validate that the triggers are properly added to the trace.
      EXPECT_EQ("trigger_name", packet.trigger().trigger_name());
    } else if (packet.has_for_testing()) {
      // Make sure that the data size is correctly set based on what we
      // requested.
      EXPECT_EQ(kMessageSize, packet.for_testing().str().size());
    }
  }
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(StopTracingTrigger)) {
  // See |message_count| and |message_size| in the TraceConfig above.
  constexpr size_t kMessageCount = 11;
  constexpr size_t kMessageSize = 32;
  protos::gen::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->mutable_for_testing()->set_message_count(kMessageCount);
  ds_config->mutable_for_testing()->set_message_size(kMessageSize);
  auto* trigger_cfg = trace_config.mutable_trigger_config();
  trigger_cfg->set_trigger_mode(
      protos::gen::TraceConfig::TriggerConfig::STOP_TRACING);
  trigger_cfg->set_trigger_timeout_ms(15000);
  auto* trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name");
  // |stop_delay_ms| must be long enough that we can write the packets in
  // before the trace finishes. This has to be long enough for the slowest
  // emulator. But as short as possible to prevent the test running a long
  // time.
  trigger->set_stop_delay_ms(500);
  trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name_3");
  trigger->set_stop_delay_ms(60000);

  // We have to construct all the processes we want to fork before we start the
  // service with |StartServiceIfRequired()|. this is because it is unsafe
  // (could deadlock) to fork after we've spawned some threads which might
  // printf (and thus hold locks).
  const std::string path = RandomTraceFileName();
  auto perfetto_proc = ExecPerfetto(
      {
          "-o",
          path,
          "-c",
          "-",
      },
      trace_config.SerializeAsString());

  auto trigger_proc =
      ExecTrigger({"trigger_name_2", "trigger_name", "trigger_name_3"});

  // Start the service and connect a simple fake producer.
  StartServiceIfRequiredNoNewExecsAfterThis();
  auto* fake_producer = ConnectFakeProducer();
  EXPECT_TRUE(fake_producer);

  // Start a background thread that will deliver the config now that we've
  // started the service. See |perfetto_proc| above for the args passed.
  std::thread background_trace([&perfetto_proc]() {
    std::string stderr_str;
    EXPECT_EQ(0, perfetto_proc.Run(&stderr_str)) << stderr_str;
  });

  WaitForProducerEnabled();
  // Wait for the producer to start, and then write out 11 packets, before the
  // trace actually starts (the trigger is seen).
  auto on_data_written = task_runner_.CreateCheckpoint("data_written_1");
  fake_producer->ProduceEventBatch(WrapTask(on_data_written));
  task_runner_.RunUntilCheckpoint("data_written_1");

  EXPECT_EQ(0, trigger_proc.Run(&stderr_)) << "stderr: " << stderr_;

  background_trace.join();

  std::string trace_str;
  base::ReadFile(path, &trace_str);
  protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromString(trace_str));
  EXPECT_EQ(static_cast<int>(kBuiltinPackets + 1 + kMessageCount),
            trace.packet_size());
  bool seen_first_trigger = false;
  for (const auto& packet : trace.packet()) {
    if (packet.has_trace_config()) {
      // Ensure the trace config properly includes the trigger mode we set.
      auto kStopTrig = protos::gen::TraceConfig::TriggerConfig::STOP_TRACING;
      EXPECT_EQ(kStopTrig,
                packet.trace_config().trigger_config().trigger_mode());
    } else if (packet.has_trigger()) {
      // validate that the triggers are properly added to the trace.
      if (!seen_first_trigger) {
        EXPECT_EQ("trigger_name", packet.trigger().trigger_name());
        seen_first_trigger = true;
      } else {
        EXPECT_EQ("trigger_name_3", packet.trigger().trigger_name());
      }
    } else if (packet.has_for_testing()) {
      // Make sure that the data size is correctly set based on what we
      // requested.
      EXPECT_EQ(kMessageSize, packet.for_testing().str().size());
    }
  }
}

// Dropbox on the commandline client only works on android builds. So disable
// this test on all other builds.
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
TEST_F(PerfettoCmdlineTest, NoSanitizers(NoDataNoFileWithoutTrigger)) {
#else
TEST_F(PerfettoCmdlineTest, DISABLED_NoDataNoFileWithoutTrigger) {
#endif
  // See |message_count| and |message_size| in the TraceConfig above.
  constexpr size_t kMessageCount = 11;
  constexpr size_t kMessageSize = 32;
  protos::gen::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.set_allow_user_build_tracing(true);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->mutable_for_testing()->set_message_count(kMessageCount);
  ds_config->mutable_for_testing()->set_message_size(kMessageSize);
  auto* trigger_cfg = trace_config.mutable_trigger_config();
  trigger_cfg->set_trigger_mode(
      protos::gen::TraceConfig::TriggerConfig::STOP_TRACING);
  trigger_cfg->set_trigger_timeout_ms(1000);
  auto* trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name");
  // |stop_delay_ms| must be long enough that we can write the packets in
  // before the trace finishes. This has to be long enough for the slowest
  // emulator. But as short as possible to prevent the test running a long
  // time.
  trigger->set_stop_delay_ms(500);
  trigger = trigger_cfg->add_triggers();

  // We have to construct all the processes we want to fork before we start the
  // service with |StartServiceIfRequired()|. this is because it is unsafe
  // (could deadlock) to fork after we've spawned some threads which might
  // printf (and thus hold locks).
  const std::string path = RandomTraceFileName();
  auto perfetto_proc = ExecPerfetto(
      {
          "--dropbox",
          "TAG",
          "--no-guardrails",
          "-c",
          "-",
      },
      trace_config.SerializeAsString());

  StartServiceIfRequiredNoNewExecsAfterThis();
  auto* fake_producer = ConnectFakeProducer();
  EXPECT_TRUE(fake_producer);

  std::string stderr_str;
  std::thread background_trace([&perfetto_proc, &stderr_str]() {
    EXPECT_EQ(0, perfetto_proc.Run(&stderr_str));
  });
  background_trace.join();

  EXPECT_THAT(stderr_str,
              ::testing::HasSubstr("Skipping write to dropbox. Empty trace."));
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(StopTracingTriggerFromConfig)) {
  // See |message_count| and |message_size| in the TraceConfig above.
  constexpr size_t kMessageCount = 11;
  constexpr size_t kMessageSize = 32;
  protos::gen::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->mutable_for_testing()->set_message_count(kMessageCount);
  ds_config->mutable_for_testing()->set_message_size(kMessageSize);
  auto* trigger_cfg = trace_config.mutable_trigger_config();
  trigger_cfg->set_trigger_mode(
      protos::gen::TraceConfig::TriggerConfig::STOP_TRACING);
  trigger_cfg->set_trigger_timeout_ms(15000);
  auto* trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name");
  // |stop_delay_ms| must be long enough that we can write the packets in
  // before the trace finishes. This has to be long enough for the slowest
  // emulator. But as short as possible to prevent the test running a long
  // time.
  trigger->set_stop_delay_ms(500);
  trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name_3");
  trigger->set_stop_delay_ms(60000);

  // We have to construct all the processes we want to fork before we start the
  // service with |StartServiceIfRequired()|. this is because it is unsafe
  // (could deadlock) to fork after we've spawned some threads which might
  // printf (and thus hold locks).
  const std::string path = RandomTraceFileName();
  auto perfetto_proc = ExecPerfetto(
      {
          "-o",
          path,
          "-c",
          "-",
      },
      trace_config.SerializeAsString());

  std::string triggers = R"(
    activate_triggers: "trigger_name_2"
    activate_triggers: "trigger_name"
    activate_triggers: "trigger_name_3"
  )";
  auto perfetto_proc_2 = ExecPerfetto(
      {
          "-o",
          path,
          "-c",
          "-",
          "--txt",
      },
      triggers);

  // Start the service and connect a simple fake producer.
  StartServiceIfRequiredNoNewExecsAfterThis();
  auto* fake_producer = ConnectFakeProducer();
  EXPECT_TRUE(fake_producer);

  std::thread background_trace([&perfetto_proc]() {
    std::string stderr_str;
    EXPECT_EQ(0, perfetto_proc.Run(&stderr_str)) << stderr_str;
  });

  WaitForProducerEnabled();
  // Wait for the producer to start, and then write out 11 packets, before the
  // trace actually starts (the trigger is seen).
  auto on_data_written = task_runner_.CreateCheckpoint("data_written_1");
  fake_producer->ProduceEventBatch(WrapTask(on_data_written));
  task_runner_.RunUntilCheckpoint("data_written_1");

  EXPECT_EQ(0, perfetto_proc_2.Run(&stderr_)) << "stderr: " << stderr_;

  background_trace.join();

  std::string trace_str;
  base::ReadFile(path, &trace_str);
  protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromString(trace_str));
  EXPECT_LT(static_cast<int>(kMessageCount), trace.packet_size());
  bool seen_first_trigger = false;
  for (const auto& packet : trace.packet()) {
    if (packet.has_trace_config()) {
      // Ensure the trace config properly includes the trigger mode we set.
      auto kStopTrig = protos::gen::TraceConfig::TriggerConfig::STOP_TRACING;
      EXPECT_EQ(kStopTrig,
                packet.trace_config().trigger_config().trigger_mode());
    } else if (packet.has_trigger()) {
      // validate that the triggers are properly added to the trace.
      if (!seen_first_trigger) {
        EXPECT_EQ("trigger_name", packet.trigger().trigger_name());
        seen_first_trigger = true;
      } else {
        EXPECT_EQ("trigger_name_3", packet.trigger().trigger_name());
      }
    } else if (packet.has_for_testing()) {
      // Make sure that the data size is correctly set based on what we
      // requested.
      EXPECT_EQ(kMessageSize, packet.for_testing().str().size());
    }
  }
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(TriggerFromConfigStopsFileOpening)) {
  // See |message_count| and |message_size| in the TraceConfig above.
  constexpr size_t kMessageCount = 11;
  constexpr size_t kMessageSize = 32;
  protos::gen::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->mutable_for_testing()->set_message_count(kMessageCount);
  ds_config->mutable_for_testing()->set_message_size(kMessageSize);
  auto* trigger_cfg = trace_config.mutable_trigger_config();
  trigger_cfg->set_trigger_mode(
      protos::gen::TraceConfig::TriggerConfig::STOP_TRACING);
  trigger_cfg->set_trigger_timeout_ms(15000);
  auto* trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name");
  // |stop_delay_ms| must be long enough that we can write the packets in
  // before the trace finishes. This has to be long enough for the slowest
  // emulator. But as short as possible to prevent the test running a long
  // time.
  trigger->set_stop_delay_ms(500);
  trigger = trigger_cfg->add_triggers();
  trigger->set_name("trigger_name_3");
  trigger->set_stop_delay_ms(60000);

  // We have to construct all the processes we want to fork before we start the
  // service with |StartServiceIfRequired()|. this is because it is unsafe
  // (could deadlock) to fork after we've spawned some threads which might
  // printf (and thus hold locks).
  const std::string path = RandomTraceFileName();
  std::string triggers = R"(
    activate_triggers: "trigger_name_2"
    activate_triggers: "trigger_name"
    activate_triggers: "trigger_name_3"
  )";
  auto perfetto_proc = ExecPerfetto(
      {
          "-o",
          path,
          "-c",
          "-",
          "--txt",
      },
      triggers);

  // Start the service and connect a simple fake producer.
  StartServiceIfRequiredNoNewExecsAfterThis();
  auto* fake_producer = ConnectFakeProducer();
  EXPECT_TRUE(fake_producer);

  std::string trace_str;
  EXPECT_FALSE(base::ReadFile(path, &trace_str));

  EXPECT_EQ(0, perfetto_proc.Run(&stderr_)) << "stderr: " << stderr_;

  EXPECT_FALSE(base::ReadFile(path, &trace_str));
}

TEST_F(PerfettoCmdlineTest, NoSanitizers(Query)) {
  auto query = ExecPerfetto({"--query"});
  auto query_raw = ExecPerfetto({"--query-raw"});
  StartServiceIfRequiredNoNewExecsAfterThis();
  EXPECT_EQ(0, query.Run(&stderr_)) << stderr_;
  EXPECT_EQ(0, query_raw.Run(&stderr_)) << stderr_;
}

}  // namespace perfetto
