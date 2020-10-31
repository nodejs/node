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

#include <algorithm>
#include <map>
#include <random>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/base/test/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

constexpr size_t kMaxChunkSize = 4 * 1024 * 1024;

class TraceProcessorIntegrationTest : public ::testing::Test {
 public:
  TraceProcessorIntegrationTest()
      : processor_(TraceProcessor::CreateInstance(Config())) {}

 protected:
  util::Status LoadTrace(const char* name, size_t min_chunk_size = 512) {
    EXPECT_LE(min_chunk_size, kMaxChunkSize);
    base::ScopedFstream f(fopen(
        base::GetTestDataPath(std::string("test/data/") + name).c_str(), "rb"));
    std::minstd_rand0 rnd_engine(0);
    std::uniform_int_distribution<size_t> dist(min_chunk_size, kMaxChunkSize);
    while (!feof(*f)) {
      size_t chunk_size = dist(rnd_engine);
      std::unique_ptr<uint8_t[]> buf(new uint8_t[chunk_size]);
      auto rsize = fread(reinterpret_cast<char*>(buf.get()), 1, chunk_size, *f);
      auto status = processor_->Parse(std::move(buf), rsize);
      if (!status.ok())
        return status;
    }
    processor_->NotifyEndOfFile();
    return util::OkStatus();
  }

  TraceProcessor::Iterator Query(const std::string& query) {
    return processor_->ExecuteQuery(query.c_str());
  }

  size_t RestoreInitialTables() { return processor_->RestoreInitialTables(); }

 private:
  std::unique_ptr<TraceProcessor> processor_;
};

TEST_F(TraceProcessorIntegrationTest, AndroidSchedAndPs) {
  ASSERT_TRUE(LoadTrace("android_sched_and_ps.pb").ok());
  auto it = Query(
      "select count(*), max(ts) - min(ts) from sched "
      "where dur != 0 and utid != 0");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 139787);
  ASSERT_EQ(it.Get(1).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(1).long_value, 19684308497);
  ASSERT_FALSE(it.Next());
}

TEST_F(TraceProcessorIntegrationTest, TraceBounds) {
  ASSERT_TRUE(LoadTrace("android_sched_and_ps.pb").ok());
  auto it = Query("select start_ts, end_ts from trace_bounds");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 81473009948313);
  ASSERT_EQ(it.Get(1).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(1).long_value, 81492700784311);
  ASSERT_FALSE(it.Next());
}

// Tests that the duration of the last slice is accounted in the computation
// of the trace boundaries. Linux ftraces tend to hide this problem because
// after the last sched_switch there's always a "wake" event which causes the
// raw table to fix the bounds.
TEST_F(TraceProcessorIntegrationTest, TraceBoundsUserspaceOnly) {
  ASSERT_TRUE(LoadTrace("sfgate.json").ok());
  auto it = Query("select start_ts, end_ts from trace_bounds");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 2213649212614000);
  ASSERT_EQ(it.Get(1).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(1).long_value, 2213689745140000);
  ASSERT_FALSE(it.Next());
}

TEST_F(TraceProcessorIntegrationTest, Hash) {
  auto it = Query("select HASH()");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, static_cast<int64_t>(0xcbf29ce484222325));

  it = Query("select HASH('test')");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, static_cast<int64_t>(0xf9e6e6ef197c2b25));

  it = Query("select HASH('test', 1)");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, static_cast<int64_t>(0xa9cb070fdc15f7a4));
}

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#define MAYBE_Demangle DISABLED_Demangle
#else
#define MAYBE_Demangle Demangle
#endif
TEST_F(TraceProcessorIntegrationTest, MAYBE_Demangle) {
  auto it = Query("select DEMANGLE('_Znwm')");
  ASSERT_TRUE(it.Next());
  ASSERT_STRCASEEQ(it.Get(0).string_value, "operator new(unsigned long)");

  it = Query("select DEMANGLE('_ZN3art6Thread14CreateCallbackEPv')");
  ASSERT_TRUE(it.Next());
  ASSERT_STRCASEEQ(it.Get(0).string_value,
                   "art::Thread::CreateCallback(void*)");

  it = Query("select DEMANGLE('test')");
  ASSERT_TRUE(it.Next());
  ASSERT_TRUE(it.Get(0).is_null());
}

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
TEST_F(TraceProcessorIntegrationTest, Sfgate) {
  ASSERT_TRUE(LoadTrace("sfgate.json", strlen("{\"traceEvents\":[")).ok());
  auto it = Query(
      "select count(*), max(ts) - min(ts) "
      "from slice s inner join thread_track t "
      "on s.track_id = t.id where utid != 0");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 39828);
  ASSERT_EQ(it.Get(1).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(1).long_value, 40532506000);
  ASSERT_FALSE(it.Next());
}

TEST_F(TraceProcessorIntegrationTest, UnsortedTrace) {
  ASSERT_TRUE(
      LoadTrace("unsorted_trace.json", strlen("{\"traceEvents\":[")).ok());
  auto it = Query("select ts, depth from slice order by ts");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 50000);
  ASSERT_EQ(it.Get(1).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(1).long_value, 0);
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 100000);
  ASSERT_EQ(it.Get(1).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(1).long_value, 1);
  ASSERT_FALSE(it.Next());
}

// TODO(hjd): Add trace to test_data.
TEST_F(TraceProcessorIntegrationTest, DISABLED_AndroidBuildTrace) {
  ASSERT_TRUE(LoadTrace("android_build_trace.json", strlen("[\n{")).ok());
}

TEST_F(TraceProcessorIntegrationTest, DISABLED_Clusterfuzz14357) {
  ASSERT_FALSE(LoadTrace("clusterfuzz_14357", 4096).ok());
}
#endif  // PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz14730) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_14730", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz14753) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_14753", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz14762) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_14762", 4096 * 1024).ok());
  auto it = Query("select sum(value) from stats where severity = 'error';");
  ASSERT_TRUE(it.Next());
  ASSERT_GT(it.Get(0).long_value, 0);
}

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz14767) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_14767", 4096 * 1024).ok());
  auto it = Query("select sum(value) from stats where severity = 'error';");
  ASSERT_TRUE(it.Next());
  ASSERT_GT(it.Get(0).long_value, 0);
}

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz14799) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_14799", 4096 * 1024).ok());
  auto it = Query("select sum(value) from stats where severity = 'error';");
  ASSERT_TRUE(it.Next());
  ASSERT_GT(it.Get(0).long_value, 0);
}

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz15252) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_15252", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, Clusterfuzz17805) {
  // This trace fails to load as it's detected as a systrace but is full of
  // garbage data.
  ASSERT_TRUE(!LoadTrace("clusterfuzz_17805", 4096).ok());
}

// Failing on DCHECKs during import because the traces aren't really valid.
#if PERFETTO_DCHECK_IS_ON()
#define MAYBE_Clusterfuzz20215 DISABLED_Clusterfuzz20215
#define MAYBE_Clusterfuzz20292 DISABLED_Clusterfuzz20292
#define MAYBE_Clusterfuzz21178 DISABLED_Clusterfuzz21178
#define MAYBE_Clusterfuzz21890 DISABLED_Clusterfuzz21890
#else  // PERFETTO_DCHECK_IS_ON()
#define MAYBE_Clusterfuzz20215 Clusterfuzz20215
#define MAYBE_Clusterfuzz20292 Clusterfuzz20292
#define MAYBE_Clusterfuzz21178 Clusterfuzz21178
#define MAYBE_Clusterfuzz21890 Clusterfuzz21890
#endif  // PERFETTO_DCHECK_IS_ON()

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz20215) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_20215", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz20292) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_20292", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz21178) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_21178", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz21890) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_21890", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTables) {
  ASSERT_TRUE(LoadTrace("android_sched_and_ps.pb").ok());

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);

    auto it = Query("CREATE TABLE user1(unused text);");
    it.Next();
    ASSERT_TRUE(it.Status().ok());

    it = Query("CREATE TEMPORARY TABLE user2(unused text);");
    it.Next();
    ASSERT_TRUE(it.Status().ok());

    it = Query("CREATE VIEW user3 AS SELECT * FROM stats;");
    it.Next();
    ASSERT_TRUE(it.Status().ok());

    ASSERT_EQ(RestoreInitialTables(), 3u);
  }
}

// This test checks that a ninja trace is tokenized properly even if read in
// small chunks of 1KB each. The values used in the test have been cross-checked
// with opening the same trace with ninjatracing + chrome://tracing.
TEST_F(TraceProcessorIntegrationTest, NinjaLog) {
  ASSERT_TRUE(LoadTrace("ninja_log", 1024).ok());
  auto it = Query("select count(*) from process where name like 'build';");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, 2);

  it = Query(
      "select count(*) from thread left join process using(upid) where "
      "thread.name like 'worker%' and process.pid=1");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, 14);

  it = Query(
      "create view slices_1st_build as select slices.* from slices left "
      "join thread_track on(slices.track_id == thread_track.id) left join "
      "thread using(utid) left join process using(upid) where pid=2");
  it.Next();
  ASSERT_TRUE(it.Status().ok());

  it = Query("select (max(ts) - min(ts)) / 1000000 from slices_1st_build");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, 12612);

  it = Query("select name from slices_1st_build order by ts desc limit 1");
  ASSERT_TRUE(it.Next());
  ASSERT_STREQ(it.Get(0).string_value,
               "obj/src/trace_processor/unittests.trace_sorter_unittest.o");

  it = Query("select sum(dur) / 1000000 from slices_1st_build");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, 276174);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
