#include "pprof/cpu_clock.h"
#include "pprof/sigprof_handler.h"
#include "pprof/time_profile.h"

#include <cstdint>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using node::pprof::AttributeSamples;
using node::pprof::AttributionByNode;
using node::pprof::CurrentThreadCpuNanos;
using node::pprof::Midpoint;
using node::pprof::ProfileSample;
using node::pprof::SampleRecord;
using node::pprof::ThreadCpuClock;

namespace {

// Stand-ins for profile nodes.
char nodes[3];
const void* const a = &nodes[0];
const void* const b = &nodes[1];
const void* const program = &nodes[2];

void Spin(int64_t cpu_nanos) {
  int64_t end = CurrentThreadCpuNanos() + cpu_nanos;
  volatile uint64_t x = 0;
  while (CurrentThreadCpuNanos() < end) x = x + 1;
}

}  // namespace

TEST(PprofAttributeSamples, Empty) {
  EXPECT_TRUE(AttributeSamples({}, {}, 0).empty());
  EXPECT_TRUE(
      AttributeSamples({{10, a, false}, {20, a, false}}, {}, 0).empty());
  EXPECT_TRUE(AttributeSamples({}, {{9, 11, 100}}, 0).empty());
}

TEST(PprofAttributeSamples, MatchesRecordsAroundSamples) {
  std::vector<ProfileSample> samples = {
      // The starting sample is skipped, even if it matches a record.
      {10, a, false},
      {20, a, false},
      {30, b, false},
      // No record matches this one, e.g. a deoptimization sample.
      {35, b, false},
      {40, a, false},
  };
  std::vector<SampleRecord> records = {
      {9, 11, 50},
      // Too old for any sample.
      {12, 13, 60},
      {19, 21, 100},
      {29, 30, 250},
      {40, 40, 400},
      // Too recent for any sample.
      {50, 51, 1000},
  };
  AttributionByNode attribution = AttributeSamples(samples, records, 40);
  ASSERT_EQ(attribution.size(), 2u);
  EXPECT_EQ(attribution[a].hit_count, 2u);
  // 100 - 40 for the sample at 20, and 400 - 250 for the sample at 40.
  EXPECT_EQ(attribution[a].cpu_time, 60 + 150);
  EXPECT_EQ(attribution[b].hit_count, 1u);
  EXPECT_EQ(attribution[b].cpu_time, 250 - 100);
}

TEST(PprofAttributeSamples, OutOfOrderSamples) {
  std::vector<ProfileSample> samples = {
      {0, a, false},
      {10, a, false},
      // These two are swapped.
      {30, b, false},
      {20, a, false},
      {40, b, false},
  };
  std::vector<SampleRecord> records = {
      {10, 10, 10},
      {20, 20, 20},
      {30, 30, 30},
      {40, 40, 40},
  };
  AttributionByNode attribution = AttributeSamples(samples, records, 0);
  EXPECT_EQ(attribution[a].hit_count, 2u);
  EXPECT_EQ(attribution[a].cpu_time, 20);
  EXPECT_EQ(attribution[b].hit_count, 2u);
  EXPECT_EQ(attribution[b].cpu_time, 20);
}

TEST(PprofAttributeSamples, ProgramCpuTimeGoesToTheNextSample) {
  std::vector<ProfileSample> samples = {
      {0, a, false},
      {10, program, true},
      {20, a, false},
  };
  std::vector<SampleRecord> records = {
      {10, 10, 100},
      {20, 20, 300},
  };
  AttributionByNode attribution = AttributeSamples(samples, records, 0);
  EXPECT_EQ(attribution[program].hit_count, 1u);
  EXPECT_EQ(attribution[program].cpu_time, 0);
  EXPECT_EQ(attribution[a].hit_count, 1u);
  EXPECT_EQ(attribution[a].cpu_time, 300);
}

TEST(PprofMidpoint, RoundsDown) {
  EXPECT_EQ(Midpoint(0, 0), 0);
  EXPECT_EQ(Midpoint(1, 2), 1);
  EXPECT_EQ(Midpoint(2, 4), 3);
  EXPECT_EQ(Midpoint(-1, -2), -2);
  EXPECT_EQ(Midpoint(INT64_MAX, INT64_MAX - 2), INT64_MAX - 1);
}

TEST(PprofCpuClock, ThreadCpuTime) {
  int64_t start = CurrentThreadCpuNanos();
  Spin(5 * 1000 * 1000);
  EXPECT_GE(CurrentThreadCpuNanos() - start, 5 * 1000 * 1000);
}

TEST(PprofCpuClock, ThreadCpuClockReadsItsThread) {
  ThreadCpuClock clock;
  int64_t start = clock.NowNanos();
  // Another thread that spins does not add to this thread's CPU time.
  int64_t other_thread_cpu = 0;
  std::thread thread([&]() {
    ThreadCpuClock other_clock;
    Spin(20 * 1000 * 1000);
    other_thread_cpu = other_clock.NowNanos();
  });
  thread.join();
  EXPECT_GE(other_thread_cpu, 20 * 1000 * 1000);
  EXPECT_LT(clock.NowNanos() - start, other_thread_cpu);
}

#ifndef _WIN32

using node::pprof::MonotonicNowMicros;
using node::pprof::SignalHandler;
using node::pprof::SignalSampleSink;

TEST(PprofCpuClock, MonotonicClock) {
  node::pprof::InitializeMonotonicClock();
  int64_t previous = MonotonicNowMicros();
  EXPECT_GT(previous, 0);
  for (int i = 0; i < 1000; i++) {
    int64_t now = MonotonicNowMicros();
    EXPECT_GE(now, previous);
    previous = now;
  }
}

namespace {

int previous_handler_calls = 0;
void PreviousHandler(int, siginfo_t*, void*) {
  previous_handler_calls++;
}

class CountingSink : public SignalSampleSink {
 public:
  void OnProfilerSignal(int sig, siginfo_t* info, void* context) override {
    calls++;
    SignalHandler::ForwardSignal(sig, info, context);
  }
  int calls = 0;
};

}  // namespace

TEST(PprofSignalHandler, ChainsToThePreviousHandler) {
  struct sigaction original;
  ASSERT_EQ(sigaction(SIGPROF, nullptr, &original), 0);

  struct sigaction previous;
  previous.sa_sigaction = &PreviousHandler;
  sigemptyset(&previous.sa_mask);
  previous.sa_flags = SA_SIGINFO;
  ASSERT_EQ(sigaction(SIGPROF, &previous, nullptr), 0);

  ASSERT_TRUE(SignalHandler::IncreaseUseCount());
  // Signals delivered to a thread without a sink go to the previous handler.
  previous_handler_calls = 0;
  raise(SIGPROF);
  EXPECT_EQ(previous_handler_calls, 1);

  CountingSink sink;
  SignalHandler::SetThreadSink(&sink);
  EXPECT_EQ(SignalHandler::GetThreadSink(), &sink);
  raise(SIGPROF);
  EXPECT_EQ(sink.calls, 1);
  EXPECT_EQ(previous_handler_calls, 2);
  SignalHandler::SetThreadSink(nullptr);

  // The handler stays installed until its last use is released.
  ASSERT_TRUE(SignalHandler::IncreaseUseCount());
  SignalHandler::DecreaseUseCount();
  struct sigaction current;
  ASSERT_EQ(sigaction(SIGPROF, nullptr, &current), 0);
  EXPECT_NE(current.sa_sigaction, &PreviousHandler);
  SignalHandler::DecreaseUseCount();
  ASSERT_EQ(sigaction(SIGPROF, nullptr, &current), 0);
  EXPECT_EQ(current.sa_sigaction, &PreviousHandler);

  // Installing refuses to chain to itself.
  ASSERT_TRUE(SignalHandler::IncreaseUseCount());
  struct sigaction installed;
  ASSERT_EQ(sigaction(SIGPROF, nullptr, &installed), 0);
  SignalHandler::DecreaseUseCount();
  ASSERT_EQ(sigaction(SIGPROF, &installed, nullptr), 0);
  EXPECT_FALSE(SignalHandler::IncreaseUseCount());

  ASSERT_EQ(sigaction(SIGPROF, &original, nullptr), 0);
}

#endif  // _WIN32
