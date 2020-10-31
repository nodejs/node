/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/tracing/core/tracing_service_impl.h"

#include <string.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/observable_events.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "src/base/test/test_task_runner.h"
#include "src/tracing/core/shared_memory_arbiter_impl.h"
#include "src/tracing/core/trace_writer_impl.h"
#include "src/tracing/test/mock_consumer.h"
#include "src/tracing/test/mock_producer.h"
#include "src/tracing/test/test_shared_memory.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/perfetto/tracing_service_event.gen.h"
#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/trigger.gen.h"

using ::testing::_;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Not;
using ::testing::Property;
using ::testing::StrictMock;
using ::testing::StringMatchResultListener;

namespace perfetto {

namespace {
constexpr size_t kDefaultShmSizeKb = TracingServiceImpl::kDefaultShmSize / 1024;
constexpr size_t kDefaultShmPageSizeKb =
    TracingServiceImpl::kDefaultShmPageSize / 1024;
constexpr size_t kMaxShmSizeKb = TracingServiceImpl::kMaxShmSize / 1024;

AssertionResult HasTriggerModeInternal(
    const std::vector<protos::gen::TracePacket>& packets,
    protos::gen::TraceConfig::TriggerConfig::TriggerMode mode) {
  StringMatchResultListener matcher_result_string;
  bool contains = ExplainMatchResult(
      Contains(Property(
          &protos::gen::TracePacket::trace_config,
          Property(
              &protos::gen::TraceConfig::trigger_config,
              Property(&protos::gen::TraceConfig::TriggerConfig::trigger_mode,
                       Eq(mode))))),
      packets, &matcher_result_string);
  if (contains) {
    return AssertionSuccess();
  }
  return AssertionFailure() << matcher_result_string.str();
}

MATCHER_P(HasTriggerMode, mode, "") {
  return HasTriggerModeInternal(arg, mode);
}

}  // namespace

class TracingServiceImplTest : public testing::Test {
 public:
  using DataSourceInstanceState =
      TracingServiceImpl::DataSourceInstance::DataSourceInstanceState;

  TracingServiceImplTest() {
    auto shm_factory =
        std::unique_ptr<SharedMemory::Factory>(new TestSharedMemory::Factory());
    svc.reset(static_cast<TracingServiceImpl*>(
        TracingService::CreateInstance(std::move(shm_factory), &task_runner)
            .release()));
    svc->min_write_period_ms_ = 1;
  }

  std::unique_ptr<MockProducer> CreateMockProducer() {
    return std::unique_ptr<MockProducer>(
        new StrictMock<MockProducer>(&task_runner));
  }

  std::unique_ptr<MockConsumer> CreateMockConsumer() {
    return std::unique_ptr<MockConsumer>(
        new StrictMock<MockConsumer>(&task_runner));
  }

  ProducerID* last_producer_id() { return &svc->last_producer_id_; }

  uid_t GetProducerUid(ProducerID producer_id) {
    return svc->GetProducer(producer_id)->uid_;
  }

  TracingServiceImpl::TracingSession* GetTracingSession(TracingSessionID tsid) {
    auto* session = svc->GetTracingSession(tsid);
    EXPECT_NE(nullptr, session);
    return session;
  }

  TracingServiceImpl::TracingSession* tracing_session() {
    return GetTracingSession(GetTracingSessionID());
  }

  TracingSessionID GetTracingSessionID() {
    return svc->last_tracing_session_id_;
  }

  const std::set<BufferID>& GetAllowedTargetBuffers(ProducerID producer_id) {
    return svc->GetProducer(producer_id)->allowed_target_buffers_;
  }

  const std::map<WriterID, BufferID>& GetWriters(ProducerID producer_id) {
    return svc->GetProducer(producer_id)->writers_;
  }

  std::unique_ptr<SharedMemoryArbiterImpl> TakeShmemArbiterForProducer(
      ProducerID producer_id) {
    return std::move(svc->GetProducer(producer_id)->inproc_shmem_arbiter_);
  }

  size_t GetNumPendingFlushes() {
    return tracing_session()->pending_flushes.size();
  }

  void WaitForNextSyncMarker() {
    tracing_session()->last_snapshot_time = base::TimeMillis(0);
    static int attempt = 0;
    while (tracing_session()->last_snapshot_time == base::TimeMillis(0)) {
      auto checkpoint_name = "wait_snapshot_" + std::to_string(attempt++);
      auto timer_expired = task_runner.CreateCheckpoint(checkpoint_name);
      task_runner.PostDelayedTask([timer_expired] { timer_expired(); }, 1);
      task_runner.RunUntilCheckpoint(checkpoint_name);
    }
  }

  void WaitForTraceWritersChanged(ProducerID producer_id) {
    static int i = 0;
    auto checkpoint_name = "writers_changed_" + std::to_string(producer_id) +
                           "_" + std::to_string(i++);
    auto writers_changed = task_runner.CreateCheckpoint(checkpoint_name);
    auto writers = GetWriters(producer_id);
    std::function<void()> task;
    task = [&task, writers, writers_changed, producer_id, this]() {
      if (writers != GetWriters(producer_id)) {
        writers_changed();
        return;
      }
      task_runner.PostDelayedTask(task, 1);
    };
    task_runner.PostDelayedTask(task, 1);
    task_runner.RunUntilCheckpoint(checkpoint_name);
  }

  DataSourceInstanceState GetDataSourceInstanceState(const std::string& name) {
    for (const auto& kv : tracing_session()->data_source_instances) {
      if (kv.second.data_source_name == name)
        return kv.second.state;
    }
    PERFETTO_FATAL("Can't find data source instance with name %s",
                   name.c_str());
  }

  base::TestTaskRunner task_runner;
  std::unique_ptr<TracingServiceImpl> svc;
};

TEST_F(TracingServiceImplTest, AtMostOneConfig) {
  std::unique_ptr<MockConsumer> consumer_a = CreateMockConsumer();
  std::unique_ptr<MockConsumer> consumer_b = CreateMockConsumer();

  consumer_a->Connect(svc.get());
  consumer_b->Connect(svc.get());

  TraceConfig trace_config_a;
  trace_config_a.add_buffers()->set_size_kb(128);
  trace_config_a.set_duration_ms(0);
  trace_config_a.set_unique_session_name("foo");

  TraceConfig trace_config_b;
  trace_config_b.add_buffers()->set_size_kb(128);
  trace_config_b.set_duration_ms(0);
  trace_config_b.set_unique_session_name("foo");

  consumer_a->EnableTracing(trace_config_a);
  consumer_b->EnableTracing(trace_config_b);

  // This will stop immediately since it has the same unique session name.
  consumer_b->WaitForTracingDisabled();

  consumer_a->DisableTracing();
  consumer_a->WaitForTracingDisabled();

  EXPECT_THAT(consumer_b->ReadBuffers(), IsEmpty());
}

TEST_F(TracingServiceImplTest, CantBackToBackConfigsForWithExtraGuardrails) {
  {
    std::unique_ptr<MockConsumer> consumer_a = CreateMockConsumer();
    consumer_a->Connect(svc.get());

    TraceConfig trace_config_a;
    trace_config_a.add_buffers()->set_size_kb(128);
    trace_config_a.set_duration_ms(0);
    trace_config_a.set_enable_extra_guardrails(true);
    trace_config_a.set_unique_session_name("foo");

    consumer_a->EnableTracing(trace_config_a);
    consumer_a->DisableTracing();
    consumer_a->WaitForTracingDisabled();
    EXPECT_THAT(consumer_a->ReadBuffers(), Not(IsEmpty()));
  }

  {
    std::unique_ptr<MockConsumer> consumer_b = CreateMockConsumer();
    consumer_b->Connect(svc.get());

    TraceConfig trace_config_b;
    trace_config_b.add_buffers()->set_size_kb(128);
    trace_config_b.set_duration_ms(10000);
    trace_config_b.set_enable_extra_guardrails(true);
    trace_config_b.set_unique_session_name("foo");

    consumer_b->EnableTracing(trace_config_b);
    consumer_b->WaitForTracingDisabled(2000);
    EXPECT_THAT(consumer_b->ReadBuffers(), IsEmpty());
  }
}

TEST_F(TracingServiceImplTest, RegisterAndUnregister) {
  std::unique_ptr<MockProducer> mock_producer_1 = CreateMockProducer();
  std::unique_ptr<MockProducer> mock_producer_2 = CreateMockProducer();

  mock_producer_1->Connect(svc.get(), "mock_producer_1", 123u /* uid */);
  mock_producer_2->Connect(svc.get(), "mock_producer_2", 456u /* uid */);

  ASSERT_EQ(2u, svc->num_producers());
  ASSERT_EQ(mock_producer_1->endpoint(), svc->GetProducer(1));
  ASSERT_EQ(mock_producer_2->endpoint(), svc->GetProducer(2));
  ASSERT_EQ(123u, GetProducerUid(1));
  ASSERT_EQ(456u, GetProducerUid(2));

  mock_producer_1->RegisterDataSource("foo");
  mock_producer_2->RegisterDataSource("bar");

  mock_producer_1->UnregisterDataSource("foo");
  mock_producer_2->UnregisterDataSource("bar");

  mock_producer_1.reset();
  ASSERT_EQ(1u, svc->num_producers());
  ASSERT_EQ(nullptr, svc->GetProducer(1));

  mock_producer_2.reset();
  ASSERT_EQ(nullptr, svc->GetProducer(2));

  ASSERT_EQ(0u, svc->num_producers());
}

TEST_F(TracingServiceImplTest, EnableAndDisableTracing) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds = trace_config.add_data_sources();
  *ds->add_producer_name_regex_filter() = "mock_[p]roducer";
  auto* ds_config = ds->mutable_config();
  ds_config->set_name("data_source");
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

// Creates a tracing session with a START_TRACING trigger and checks that data
// sources are started only after the service receives a trigger.
TEST_F(TracingServiceImplTest, StartTracingTriggerDeferredStart) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);

  trigger_config->set_trigger_timeout_ms(8.64e+7);

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  EXPECT_CALL(*producer, StartDataSource(_, _)).Times(0);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // The trace won't start until we send the trigger. since we have a
  // START_TRACING trigger defined.
  std::vector<std::string> req;
  req.push_back("trigger_name");
  producer->endpoint()->ActivateTriggers(req);

  producer->WaitForDataSourceStart("ds_1");

  auto writer1 = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer1.get());

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();

  ASSERT_EQ(1u, tracing_session()->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session()->received_triggers[0].trigger_name);

  EXPECT_THAT(
      consumer->ReadBuffers(),
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::START_TRACING));
}

// Creates a tracing session with a START_TRACING trigger and checks that the
// session is cleaned up when no trigger is received after |trigger_timeout_ms|.
TEST_F(TracingServiceImplTest, StartTracingTriggerTimeOut) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(8.64e+7);

  trigger_config->set_trigger_timeout_ms(1);

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  EXPECT_CALL(*producer, StartDataSource(_, _)).Times(0);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // The trace won't start until we send the trigger. since we have a
  // START_TRACING trigger defined. This is where we'd expect to have an
  // ActivateTriggers call to the producer->endpoint().

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());
}

// Creates a tracing session with a START_TRACING trigger and checks that
// the session is not started when the configured trigger producer is different
// than the producer that sent the trigger.
TEST_F(TracingServiceImplTest, StartTracingTriggerDifferentProducer) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(8.64e+7);
  trigger->set_producer_name_regex("correct_name");

  trigger_config->set_trigger_timeout_ms(1);

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  EXPECT_CALL(*producer, StartDataSource(_, _)).Times(0);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // The trace won't start until we send the trigger called "trigger_name"
  // coming from a producer called "correct_name", since we have a
  // START_TRACING trigger defined. This is where we'd expect to have an
  // ActivateTriggers call to the producer->endpoint(), but we send the trigger
  // from a different producer so it is ignored.
  std::vector<std::string> req;
  req.push_back("trigger_name");
  producer->endpoint()->ActivateTriggers(req);

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());
}

// Creates a tracing session with a START_TRACING trigger and checks that the
// session is started when the trigger is received from the correct producer.
TEST_F(TracingServiceImplTest, StartTracingTriggerCorrectProducer) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);
  trigger->set_producer_name_regex("mock_produc[e-r]+");

  trigger_config->set_trigger_timeout_ms(8.64e+7);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // Start the trace at this point with ActivateTriggers.
  std::vector<std::string> req;
  req.push_back("trigger_name");
  producer->endpoint()->ActivateTriggers(req);

  producer->WaitForDataSourceStart("ds_1");

  auto writer = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(
      consumer->ReadBuffers(),
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::START_TRACING));
}

// Creates a tracing session with a START_TRACING trigger and checks that the
// session is cleaned up even when a different trigger is received.
TEST_F(TracingServiceImplTest, StartTracingTriggerDifferentTrigger) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(8.64e+7);

  trigger_config->set_trigger_timeout_ms(1);

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  EXPECT_CALL(*producer, StartDataSource(_, _)).Times(0);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // The trace won't start until we send the trigger called "trigger_name",
  // since we have a START_TRACING trigger defined. This is where we'd expect to
  // have an ActivateTriggers call to the producer->endpoint(), but we send a
  // different trigger.
  std::vector<std::string> req;
  req.push_back("not_correct_trigger");
  producer->endpoint()->ActivateTriggers(req);

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());
}

// Creates a tracing session with a START_TRACING trigger and checks that any
// trigger can start the TracingSession.
TEST_F(TracingServiceImplTest, StartTracingTriggerMultipleTriggers) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);

  trigger_config->set_trigger_timeout_ms(8.64e+7);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  std::vector<std::string> req;
  req.push_back("not_correct_trigger");
  req.push_back("trigger_name");
  producer->endpoint()->ActivateTriggers(req);

  producer->WaitForDataSourceStart("ds_1");

  auto writer = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(
      consumer->ReadBuffers(),
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::START_TRACING));
}

// Creates two tracing sessions with a START_TRACING trigger and checks that
// both are able to be triggered simultaneously.
TEST_F(TracingServiceImplTest, StartTracingTriggerMultipleTraces) {
  std::unique_ptr<MockConsumer> consumer_1 = CreateMockConsumer();
  consumer_1->Connect(svc.get());
  std::unique_ptr<MockConsumer> consumer_2 = CreateMockConsumer();
  consumer_2->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but each TracingSession will only enable one of
  // them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);

  trigger_config->set_trigger_timeout_ms(8.64e+7);

  consumer_1->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  auto tracing_session_1_id = GetTracingSessionID();

  (*trace_config.mutable_data_sources())[0].mutable_config()->set_name("ds_2");
  trigger = trace_config.mutable_trigger_config()->add_triggers();
  trigger->set_name("trigger_name_2");
  trigger->set_stop_delay_ms(8.64e+7);

  consumer_2->EnableTracing(trace_config);

  producer->WaitForDataSourceSetup("ds_2");

  auto tracing_session_2_id = GetTracingSessionID();
  EXPECT_NE(tracing_session_1_id, tracing_session_2_id);

  const DataSourceInstanceID id1 = producer->GetDataSourceInstanceId("ds_1");
  const DataSourceInstanceID id2 = producer->GetDataSourceInstanceId("ds_2");

  std::vector<std::string> req;
  req.push_back("not_correct_trigger");
  req.push_back("trigger_name");
  req.push_back("trigger_name_2");
  producer->endpoint()->ActivateTriggers(req);

  // The order has to be the same as the triggers or else we're incorrectly wait
  // on the wrong checkpoint in the |task_runner|.
  producer->WaitForDataSourceStart("ds_1");
  producer->WaitForDataSourceStart("ds_2");

  // Now that they've started we can check the triggers they've seen.
  auto* tracing_session_1 = GetTracingSession(tracing_session_1_id);
  ASSERT_EQ(1u, tracing_session_1->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session_1->received_triggers[0].trigger_name);

  // This is actually dependent on the order in which the triggers were received
  // but there isn't really a better way than iteration order so probably not to
  // brittle of a test. And this caught a real bug in implementation.
  auto* tracing_session_2 = GetTracingSession(tracing_session_2_id);
  ASSERT_EQ(2u, tracing_session_2->received_triggers.size());

  EXPECT_EQ("trigger_name",
            tracing_session_2->received_triggers[0].trigger_name);

  EXPECT_EQ("trigger_name_2",
            tracing_session_2->received_triggers[1].trigger_name);

  auto writer1 = producer->CreateTraceWriter("ds_1");
  auto writer2 = producer->CreateTraceWriter("ds_2");

  // We can't use the standard WaitForX in the MockProducer and MockConsumer
  // because they assume only a single trace is going on. So we perform our own
  // expectations and wait at the end for the two consumers to receive
  // OnTracingDisabled.
  bool flushed_writer_1 = false;
  bool flushed_writer_2 = false;
  auto flush_correct_writer = [&](FlushRequestID flush_req_id,
                                  const DataSourceInstanceID* id, size_t) {
    if (*id == id1) {
      flushed_writer_1 = true;
      writer1->Flush();
      producer->endpoint()->NotifyFlushComplete(flush_req_id);
    } else if (*id == id2) {
      flushed_writer_2 = true;
      writer2->Flush();
      producer->endpoint()->NotifyFlushComplete(flush_req_id);
    }
  };
  EXPECT_CALL(*producer, Flush(_, _, _))
      .WillOnce(Invoke(flush_correct_writer))
      .WillOnce(Invoke(flush_correct_writer));

  auto checkpoint_name = "on_tracing_disabled_consumer_1_and_2";
  auto on_tracing_disabled = task_runner.CreateCheckpoint(checkpoint_name);
  std::atomic<size_t> counter(0);
  EXPECT_CALL(*consumer_1, OnTracingDisabled()).WillOnce(Invoke([&]() {
    if (++counter == 2u) {
      on_tracing_disabled();
    }
  }));
  EXPECT_CALL(*consumer_2, OnTracingDisabled()).WillOnce(Invoke([&]() {
    if (++counter == 2u) {
      on_tracing_disabled();
    }
  }));

  EXPECT_CALL(*producer, StopDataSource(id1));
  EXPECT_CALL(*producer, StopDataSource(id2));

  task_runner.RunUntilCheckpoint(checkpoint_name, 1000);

  EXPECT_TRUE(flushed_writer_1);
  EXPECT_TRUE(flushed_writer_2);
  EXPECT_THAT(
      consumer_1->ReadBuffers(),
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::START_TRACING));
  EXPECT_THAT(
      consumer_2->ReadBuffers(),
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::START_TRACING));
}

// Creates a tracing session with a START_TRACING trigger and checks that the
// received_triggers are emitted as packets.
TEST_F(TracingServiceImplTest, EmitTriggersWithStartTracingTrigger) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer", /* uid = */ 123u);

  producer->RegisterDataSource("ds_1");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::START_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);
  trigger->set_producer_name_regex("mock_produc[e-r]+");

  trigger_config->set_trigger_timeout_ms(30000);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("ds_1");

  // The trace won't start until we send the trigger since we have a
  // START_TRACING trigger defined.
  std::vector<std::string> req;
  req.push_back("trigger_name");
  req.push_back("trigger_name_2");
  req.push_back("trigger_name_3");
  producer->endpoint()->ActivateTriggers(req);

  producer->WaitForDataSourceStart("ds_1");
  auto writer1 = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer1.get());
  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();

  ASSERT_EQ(1u, tracing_session()->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session()->received_triggers[0].trigger_name);

  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(
      packets,
      Contains(Property(
          &protos::gen::TracePacket::trace_config,
          Property(
              &protos::gen::TraceConfig::trigger_config,
              Property(&protos::gen::TraceConfig::TriggerConfig::trigger_mode,
                       Eq(protos::gen::TraceConfig::TriggerConfig::
                              START_TRACING))))));
  auto expect_received_trigger = [&](const std::string& name) {
    return Contains(AllOf(
        Property(&protos::gen::TracePacket::trigger,
                 AllOf(Property(&protos::gen::Trigger::trigger_name, Eq(name)),
                       Property(&protos::gen::Trigger::trusted_producer_uid,
                                Eq(123)),
                       Property(&protos::gen::Trigger::producer_name,
                                Eq("mock_producer")))),
        Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                 Eq(kServicePacketSequenceID))));
  };
  EXPECT_THAT(packets, expect_received_trigger("trigger_name"));
  EXPECT_THAT(packets, Not(expect_received_trigger("trigger_name_2")));
  EXPECT_THAT(packets, Not(expect_received_trigger("trigger_name_3")));
}

// Creates a tracing session with a START_TRACING trigger and checks that the
// received_triggers are emitted as packets.
TEST_F(TracingServiceImplTest, EmitTriggersWithStopTracingTrigger) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer", /* uid = */ 321u);

  producer->RegisterDataSource("ds_1");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::STOP_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);
  trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name_3");
  trigger->set_stop_delay_ms(30000);

  trigger_config->set_trigger_timeout_ms(30000);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("ds_1");
  producer->WaitForDataSourceStart("ds_1");

  // The trace won't start until we send the trigger since we have a
  // START_TRACING trigger defined.
  std::vector<std::string> req;
  req.push_back("trigger_name");
  req.push_back("trigger_name_2");
  req.push_back("trigger_name_3");
  producer->endpoint()->ActivateTriggers(req);

  auto writer1 = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer1.get());
  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();

  ASSERT_EQ(2u, tracing_session()->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session()->received_triggers[0].trigger_name);
  EXPECT_EQ("trigger_name_3",
            tracing_session()->received_triggers[1].trigger_name);

  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(
      packets,
      Contains(Property(
          &protos::gen::TracePacket::trace_config,
          Property(
              &protos::gen::TraceConfig::trigger_config,
              Property(&protos::gen::TraceConfig::TriggerConfig::trigger_mode,
                       Eq(protos::gen::TraceConfig::TriggerConfig::
                              STOP_TRACING))))));

  auto expect_received_trigger = [&](const std::string& name) {
    return Contains(AllOf(
        Property(&protos::gen::TracePacket::trigger,
                 AllOf(Property(&protos::gen::Trigger::trigger_name, Eq(name)),
                       Property(&protos::gen::Trigger::trusted_producer_uid,
                                Eq(321)),
                       Property(&protos::gen::Trigger::producer_name,
                                Eq("mock_producer")))),
        Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                 Eq(kServicePacketSequenceID))));
  };
  EXPECT_THAT(packets, expect_received_trigger("trigger_name"));
  EXPECT_THAT(packets, Not(expect_received_trigger("trigger_name_2")));
  EXPECT_THAT(packets, expect_received_trigger("trigger_name_3"));
}

// Creates a tracing session with a START_TRACING trigger and checks that the
// received_triggers are emitted as packets even ones after the initial
// ReadBuffers() call.
TEST_F(TracingServiceImplTest, EmitTriggersRepeatedly) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::STOP_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);
  trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name_2");
  trigger->set_stop_delay_ms(1);

  trigger_config->set_trigger_timeout_ms(30000);

  auto expect_received_trigger = [&](const std::string& name) {
    return Contains(AllOf(
        Property(&protos::gen::TracePacket::trigger,
                 AllOf(Property(&protos::gen::Trigger::trigger_name, Eq(name)),
                       Property(&protos::gen::Trigger::producer_name,
                                Eq("mock_producer")))),
        Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                 Eq(kServicePacketSequenceID))));
  };

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("ds_1");
  producer->WaitForDataSourceStart("ds_1");

  // The trace won't start until we send the trigger. since we have a
  // START_TRACING trigger defined.
  producer->endpoint()->ActivateTriggers({"trigger_name"});

  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(
      packets,
      Contains(Property(
          &protos::gen::TracePacket::trace_config,
          Property(
              &protos::gen::TraceConfig::trigger_config,
              Property(&protos::gen::TraceConfig::TriggerConfig::trigger_mode,
                       Eq(protos::gen::TraceConfig::TriggerConfig::
                              STOP_TRACING))))));
  EXPECT_THAT(packets, expect_received_trigger("trigger_name"));
  EXPECT_THAT(packets, Not(expect_received_trigger("trigger_name_2")));

  // Send a new trigger.
  producer->endpoint()->ActivateTriggers({"trigger_name_2"});

  auto writer1 = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer1.get());
  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();

  ASSERT_EQ(2u, tracing_session()->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session()->received_triggers[0].trigger_name);
  EXPECT_EQ("trigger_name_2",
            tracing_session()->received_triggers[1].trigger_name);

  packets = consumer->ReadBuffers();
  // We don't rewrite the old trigger.
  EXPECT_THAT(packets, Not(expect_received_trigger("trigger_name")));
  EXPECT_THAT(packets, expect_received_trigger("trigger_name_2"));
}

// Creates a tracing session with a STOP_TRACING trigger and checks that the
// session is cleaned up after |trigger_timeout_ms|.
TEST_F(TracingServiceImplTest, StopTracingTriggerTimeout) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::STOP_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(8.64e+7);

  trigger_config->set_trigger_timeout_ms(1);

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  EXPECT_CALL(*producer, StartDataSource(_, _)).Times(0);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");
  producer->WaitForDataSourceStart("ds_1");

  // The trace won't return data until unless we send a trigger at this point.
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());

  auto writer = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer.get());

  ASSERT_EQ(0u, tracing_session()->received_triggers.size());

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());
}

// Creates a tracing session with a STOP_TRACING trigger and checks that the
// session returns data after a trigger is received, but only what is currently
// in the buffer.
TEST_F(TracingServiceImplTest, StopTracingTriggerRingBuffer) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::STOP_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);

  trigger_config->set_trigger_timeout_ms(8.64e+7);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");
  producer->WaitForDataSourceStart("ds_1");

  // The trace won't return data until unless we send a trigger at this point.
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());

  // We write into the buffer a large packet which takes up the whole buffer. We
  // then add a bunch of smaller ones which causes the larger packet to be
  // dropped. After we activate the session we should only see a bunch of the
  // smaller ones.
  static const size_t kNumTestPackets = 10;
  static const char kPayload[] = "1234567890abcdef-";

  auto writer = producer->CreateTraceWriter("ds_1");
  // Buffer is 1kb so we write a packet which is slightly smaller so it fits in
  // the buffer.
  const std::string large_payload(1024 * 128 - 20, 'a');
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str(large_payload.c_str(), large_payload.size());
  }

  // Now we add a bunch of data before the trigger and after.
  for (size_t i = 0; i < kNumTestPackets; i++) {
    if (i == kNumTestPackets / 2) {
      std::vector<std::string> req;
      req.push_back("trigger_name");
      producer->endpoint()->ActivateTriggers(req);
    }
    auto tp = writer->NewTracePacket();
    std::string payload(kPayload);
    payload.append(std::to_string(i));
    tp->set_for_testing()->set_str(payload.c_str(), payload.size());
  }
  producer->WaitForFlush(writer.get());

  ASSERT_EQ(1u, tracing_session()->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session()->received_triggers[0].trigger_name);

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();

  auto packets = consumer->ReadBuffers();
  EXPECT_LT(kNumTestPackets, packets.size());
  // We expect for the TraceConfig preamble packet to be there correctly and
  // then we expect each payload to be there, but not the |large_payload|
  // packet.
  EXPECT_THAT(
      packets,
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::STOP_TRACING));
  for (size_t i = 0; i < kNumTestPackets; i++) {
    std::string payload = kPayload;
    payload += std::to_string(i);
    EXPECT_THAT(packets,
                Contains(Property(
                    &protos::gen::TracePacket::for_testing,
                    Property(&protos::gen::TestEvent::str, Eq(payload)))));
  }

  // The large payload was overwritten before we trigger and ReadBuffers so it
  // should not be in the returned data.
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq(large_payload))))));
}

// Creates a tracing session with a STOP_TRACING trigger and checks that the
// session only cleans up once even with multiple triggers.
TEST_F(TracingServiceImplTest, StopTracingTriggerMultipleTriggers) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  auto* trigger_config = trace_config.mutable_trigger_config();
  trigger_config->set_trigger_mode(TraceConfig::TriggerConfig::STOP_TRACING);
  auto* trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name");
  trigger->set_stop_delay_ms(1);
  trigger = trigger_config->add_triggers();
  trigger->set_name("trigger_name_2");
  trigger->set_stop_delay_ms(8.64e+7);

  trigger_config->set_trigger_timeout_ms(8.64e+7);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");
  producer->WaitForDataSourceStart("ds_1");

  // The trace won't return data until unless we send a trigger at this point.
  EXPECT_THAT(consumer->ReadBuffers(), IsEmpty());

  std::vector<std::string> req;
  req.push_back("trigger_name");
  req.push_back("trigger_name_3");
  req.push_back("trigger_name_2");
  producer->endpoint()->ActivateTriggers(req);

  auto writer = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer.get());

  ASSERT_EQ(2u, tracing_session()->received_triggers.size());
  EXPECT_EQ("trigger_name",
            tracing_session()->received_triggers[0].trigger_name);
  EXPECT_EQ("trigger_name_2",
            tracing_session()->received_triggers[1].trigger_name);

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(
      consumer->ReadBuffers(),
      HasTriggerMode(protos::gen::TraceConfig::TriggerConfig::STOP_TRACING));
}

TEST_F(TracingServiceImplTest, LockdownMode) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer_sameuid", geteuid());
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  trace_config.set_lockdown_mode(TraceConfig::LOCKDOWN_SET);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<MockProducer> producer_otheruid = CreateMockProducer();
  auto x = svc->ConnectProducer(producer_otheruid.get(), geteuid() + 1,
                                "mock_producer_ouid");
  EXPECT_CALL(*producer_otheruid, OnConnect()).Times(0);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(producer_otheruid.get());

  consumer->DisableTracing();
  consumer->FreeBuffers();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  trace_config.set_lockdown_mode(TraceConfig::LOCKDOWN_CLEAR);
  consumer->EnableTracing(trace_config);
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<MockProducer> producer_otheruid2 = CreateMockProducer();
  producer_otheruid->Connect(svc.get(), "mock_producer_ouid2", geteuid() + 1);

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ProducerNameFilterChange) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer1 = CreateMockProducer();
  producer1->Connect(svc.get(), "mock_producer_1");
  producer1->RegisterDataSource("data_source");

  std::unique_ptr<MockProducer> producer2 = CreateMockProducer();
  producer2->Connect(svc.get(), "mock_producer_2");
  producer2->RegisterDataSource("data_source");

  std::unique_ptr<MockProducer> producer3 = CreateMockProducer();
  producer3->Connect(svc.get(), "mock_producer_3");
  producer3->RegisterDataSource("data_source");
  producer3->RegisterDataSource("unused_data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* data_source = trace_config.add_data_sources();
  data_source->mutable_config()->set_name("data_source");
  *data_source->add_producer_name_filter() = "mock_producer_1";

  // Enable tracing with only mock_producer_1 enabled;
  // the rest should not start up.
  consumer->EnableTracing(trace_config);

  producer1->WaitForTracingSetup();
  producer1->WaitForDataSourceSetup("data_source");
  producer1->WaitForDataSourceStart("data_source");

  EXPECT_CALL(*producer2, OnConnect()).Times(0);
  EXPECT_CALL(*producer3, OnConnect()).Times(0);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(producer2.get());
  Mock::VerifyAndClearExpectations(producer3.get());

  // Enable mock_producer_2, the third one should still
  // not get connected.
  *data_source->add_producer_name_regex_filter() = ".*_producer_[2]";
  consumer->ChangeTraceConfig(trace_config);

  producer2->WaitForTracingSetup();
  producer2->WaitForDataSourceSetup("data_source");
  producer2->WaitForDataSourceStart("data_source");

  // Enable mock_producer_3 but also try to do an
  // unsupported change (adding a new data source);
  // mock_producer_3 should get enabled but not
  // for the new data source.
  *data_source->add_producer_name_filter() = "mock_producer_3";
  auto* dummy_data_source = trace_config.add_data_sources();
  dummy_data_source->mutable_config()->set_name("unused_data_source");
  *dummy_data_source->add_producer_name_filter() = "mock_producer_3";

  consumer->ChangeTraceConfig(trace_config);

  producer3->WaitForTracingSetup();
  EXPECT_CALL(*producer3, SetupDataSource(_, _)).Times(1);
  EXPECT_CALL(*producer3, StartDataSource(_, _)).Times(1);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(producer3.get());

  consumer->DisableTracing();
  consumer->FreeBuffers();
  producer1->WaitForDataSourceStop("data_source");
  producer2->WaitForDataSourceStop("data_source");

  EXPECT_CALL(*producer3, StopDataSource(_)).Times(1);

  consumer->WaitForTracingDisabled();

  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(producer3.get());
}

TEST_F(TracingServiceImplTest, DisconnectConsumerWhileTracing) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Disconnecting the consumer while tracing should trigger data source
  // teardown.
  consumer.reset();
  producer->WaitForDataSourceStop("data_source");
}

TEST_F(TracingServiceImplTest, ReconnectProducerWhileTracing) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Disconnecting and reconnecting a producer with a matching data source.
  // The Producer should see that data source getting enabled again.
  producer.reset();
  producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer_2");
  producer->RegisterDataSource("data_source");
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");
}

TEST_F(TracingServiceImplTest, ProducerIDWrapping) {
  std::vector<std::unique_ptr<MockProducer>> producers;
  producers.push_back(nullptr);

  auto connect_producer_and_get_id = [&producers,
                                      this](const std::string& name) {
    producers.emplace_back(CreateMockProducer());
    producers.back()->Connect(svc.get(), "mock_producer_" + name);
    return *last_producer_id();
  };

  // Connect producers 1-4.
  for (ProducerID i = 1; i <= 4; i++)
    ASSERT_EQ(i, connect_producer_and_get_id(std::to_string(i)));

  // Disconnect producers 1,3.
  producers[1].reset();
  producers[3].reset();

  *last_producer_id() = kMaxProducerID - 1;
  ASSERT_EQ(kMaxProducerID, connect_producer_and_get_id("maxid"));
  ASSERT_EQ(1u, connect_producer_and_get_id("1_again"));
  ASSERT_EQ(3u, connect_producer_and_get_id("3_again"));
  ASSERT_EQ(5u, connect_producer_and_get_id("5"));
  ASSERT_EQ(6u, connect_producer_and_get_id("6"));
}

// Note: file_write_period_ms is set to a large enough to have exactly one flush
// of the tracing buffers (and therefore at most one synchronization section),
// unless the test runs unrealistically slowly, or the implementation of the
// tracing snapshot packets changes.
TEST_F(TracingServiceImplTest, WriteIntoFileAndStopOnMaxSize) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  trace_config.set_write_into_file(true);
  trace_config.set_file_write_period_ms(100000);  // 100s
  const uint64_t kMaxFileSize = 1024;
  trace_config.set_max_file_size_bytes(kMaxFileSize);
  base::TempFile tmp_file = base::TempFile::Create();
  consumer->EnableTracing(trace_config, base::ScopedFile(dup(tmp_file.fd())));

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // The preamble packets are:
  // Trace start clock snapshot
  // Config
  // SystemInfo
  // Trace read clock snapshot
  // Trace synchronisation
  // All data source started (TracingServiceEvent)
  static const int kNumPreamblePackets = 6;
  static const int kNumTestPackets = 9;
  static const char kPayload[] = "1234567890abcdef-";

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  // Tracing service will emit a preamble of packets (a synchronization section,
  // followed by a tracing config packet). The preamble and these test packets
  // should fit within kMaxFileSize.
  for (int i = 0; i < kNumTestPackets; i++) {
    auto tp = writer->NewTracePacket();
    std::string payload(kPayload);
    payload.append(std::to_string(i));
    tp->set_for_testing()->set_str(payload.c_str(), payload.size());
  }

  // Finally add a packet that overflows kMaxFileSize. This should cause the
  // implicit stop of the trace and should *not* be written in the trace.
  {
    auto tp = writer->NewTracePacket();
    char big_payload[kMaxFileSize] = "BIG!";
    tp->set_for_testing()->set_str(big_payload, sizeof(big_payload));
  }
  writer->Flush();
  writer.reset();

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  // Verify the contents of the file.
  std::string trace_raw;
  ASSERT_TRUE(base::ReadFile(tmp_file.path().c_str(), &trace_raw));
  protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromString(trace_raw));

  ASSERT_EQ(trace.packet_size(), kNumPreamblePackets + kNumTestPackets);
  for (size_t i = 0; i < kNumTestPackets; i++) {
    const protos::gen::TracePacket& tp =
        trace.packet()[kNumPreamblePackets + i];
    ASSERT_EQ(kPayload + std::to_string(i++), tp.for_testing().str());
  }
}

TEST_F(TracingServiceImplTest, WriteIntoFileWithPath) {
  auto tmp_file = base::TempFile::Create();
  // Deletes the file (the service would refuse to overwrite an existing file)
  // without telling it to the underlying TempFile, so that its dtor will
  // unlink the file created by the service.
  unlink(tmp_file.path().c_str());

  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  trace_config.set_write_into_file(true);
  trace_config.set_output_path(tmp_file.path());
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");
  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");

  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }
  writer->Flush();
  writer.reset();

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  // Verify the contents of the file.
  std::string trace_raw;
  ASSERT_TRUE(base::ReadFile(tmp_file.path(), &trace_raw));
  protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromString(trace_raw));
  // ASSERT_EQ(trace.packet_size(), 33);
  EXPECT_THAT(trace.packet(),
              Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload")))));
}

// Test the logic that allows the trace config to set the shm total size and
// page size from the trace config. Also check that, if the config doesn't
// specify a value we fall back on the hint provided by the producer.
TEST_F(TracingServiceImplTest, ProducerShmAndPageSizeOverriddenByTraceConfig) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());
  const size_t kMaxPageSizeKb = 32;

  struct ConfiguredAndExpectedSizes {
    size_t config_page_size_kb;
    size_t hint_page_size_kb;
    size_t expected_page_size_kb;

    size_t config_size_kb;
    size_t hint_size_kb;
    size_t expected_size_kb;
  };

  ConfiguredAndExpectedSizes kSizes[] = {
      // Config and hint are 0, fallback to default values.
      {0, 0, kDefaultShmPageSizeKb, 0, 0, kDefaultShmSizeKb},
      // Use configured sizes.
      {16, 0, 16, 16, 0, 16},
      // Config is 0, use hint.
      {0, 4, 4, 0, 16, 16},
      // Config takes precendence over hint.
      {4, 8, 4, 16, 32, 16},
      // Config takes precendence over hint, even if it's larger.
      {8, 4, 8, 32, 16, 32},
      // Config page size % 4 != 0, fallback to defaults.
      {3, 0, kDefaultShmPageSizeKb, 0, 0, kDefaultShmSizeKb},
      // Config page size less than system page size, fallback to defaults.
      {2, 0, kDefaultShmPageSizeKb, 0, 0, kDefaultShmSizeKb},
      // Config sizes too large, use max.
      {4096, 0, kMaxPageSizeKb, 4096000, 0, kMaxShmSizeKb},
      // Hint sizes too large, use max.
      {0, 4096, kMaxPageSizeKb, 0, 4096000, kMaxShmSizeKb},
      // Config buffer size isn't a multiple of 4KB, fallback to defaults.
      {0, 0, kDefaultShmPageSizeKb, 18, 0, kDefaultShmSizeKb},
      // Invalid page size -> also ignore buffer size config.
      {2, 0, kDefaultShmPageSizeKb, 32, 0, kDefaultShmSizeKb},
      // Invalid buffer size -> also ignore page size config.
      {16, 0, kDefaultShmPageSizeKb, 18, 0, kDefaultShmSizeKb},
      // Config page size % buffer size != 0, fallback to defaults.
      {8, 0, kDefaultShmPageSizeKb, 20, 0, kDefaultShmSizeKb},
      // Config page size % default buffer size != 0, fallback to defaults.
      {28, 0, kDefaultShmPageSizeKb, 0, 0, kDefaultShmSizeKb},
  };

  const size_t kNumProducers = base::ArraySize(kSizes);
  std::unique_ptr<MockProducer> producer[kNumProducers];
  for (size_t i = 0; i < kNumProducers; i++) {
    auto name = "mock_producer_" + std::to_string(i);
    producer[i] = CreateMockProducer();
    producer[i]->Connect(svc.get(), name, geteuid(),
                         kSizes[i].hint_size_kb * 1024,
                         kSizes[i].hint_page_size_kb * 1024);
    producer[i]->RegisterDataSource("data_source");
  }

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  for (size_t i = 0; i < kNumProducers; i++) {
    auto* producer_config = trace_config.add_producers();
    producer_config->set_producer_name("mock_producer_" + std::to_string(i));
    producer_config->set_shm_size_kb(
        static_cast<uint32_t>(kSizes[i].config_size_kb));
    producer_config->set_page_size_kb(
        static_cast<uint32_t>(kSizes[i].config_page_size_kb));
  }

  consumer->EnableTracing(trace_config);
  size_t expected_shm_sizes_kb[kNumProducers]{};
  size_t expected_page_sizes_kb[kNumProducers]{};
  size_t actual_shm_sizes_kb[kNumProducers]{};
  size_t actual_page_sizes_kb[kNumProducers]{};
  for (size_t i = 0; i < kNumProducers; i++) {
    expected_shm_sizes_kb[i] = kSizes[i].expected_size_kb;
    expected_page_sizes_kb[i] = kSizes[i].expected_page_size_kb;

    producer[i]->WaitForTracingSetup();
    producer[i]->WaitForDataSourceSetup("data_source");
    actual_shm_sizes_kb[i] =
        producer[i]->endpoint()->shared_memory()->size() / 1024;
    actual_page_sizes_kb[i] =
        producer[i]->endpoint()->shared_buffer_page_size_kb();
  }
  for (size_t i = 0; i < kNumProducers; i++) {
    producer[i]->WaitForDataSourceStart("data_source");
  }
  ASSERT_THAT(actual_page_sizes_kb, ElementsAreArray(expected_page_sizes_kb));
  ASSERT_THAT(actual_shm_sizes_kb, ElementsAreArray(expected_shm_sizes_kb));
}

TEST_F(TracingServiceImplTest, ExplicitFlush) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  auto flush_request = consumer->Flush();
  producer->WaitForFlush(writer.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(),
              Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload")))));
}

TEST_F(TracingServiceImplTest, ImplicitFlushOnTimedTraces) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  trace_config.set_duration_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  EXPECT_THAT(consumer->ReadBuffers(),
              Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload")))));
}

// Tests the monotonic semantic of flush request IDs, i.e., once a producer
// acks flush request N, all flush requests <= N are considered successful and
// acked to the consumer.
TEST_F(TracingServiceImplTest, BatchFlushes) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  auto flush_req_1 = consumer->Flush();
  auto flush_req_2 = consumer->Flush();
  auto flush_req_3 = consumer->Flush();

  // We'll deliberately let the 4th flush request timeout. Use a lower timeout
  // to keep test time short.
  auto flush_req_4 = consumer->Flush(/*timeout_ms=*/10);
  ASSERT_EQ(4u, GetNumPendingFlushes());

  // Make the producer reply only to the 3rd flush request.
  InSequence seq;
  producer->WaitForFlush(nullptr, /*reply=*/false);  // Do NOT reply to flush 1.
  producer->WaitForFlush(nullptr, /*reply=*/false);  // Do NOT reply to flush 2.
  producer->WaitForFlush(writer.get());              // Reply only to flush 3.
  producer->WaitForFlush(nullptr, /*reply=*/false);  // Do NOT reply to flush 4.

  // Even if the producer explicily replied only to flush ID == 3, all the
  // previous flushed < 3 should be implicitly acked.
  ASSERT_TRUE(flush_req_1.WaitForReply());
  ASSERT_TRUE(flush_req_2.WaitForReply());
  ASSERT_TRUE(flush_req_3.WaitForReply());

  // At this point flush id == 4 should still be pending and should fail because
  // of reaching its timeout.
  ASSERT_FALSE(flush_req_4.WaitForReply());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(),
              Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload")))));
}

TEST_F(TracingServiceImplTest, PeriodicFlush) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.set_flush_period_ms(1);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");

  const int kNumFlushes = 3;
  auto checkpoint = task_runner.CreateCheckpoint("all_flushes_done");
  int flushes_seen = 0;
  EXPECT_CALL(*producer, Flush(_, _, _))
      .WillRepeatedly(Invoke([&producer, &writer, &flushes_seen, checkpoint](
                                 FlushRequestID flush_req_id,
                                 const DataSourceInstanceID*, size_t) {
        {
          auto tp = writer->NewTracePacket();
          char payload[32];
          sprintf(payload, "f_%d", flushes_seen);
          tp->set_for_testing()->set_str(payload);
        }
        writer->Flush();
        producer->endpoint()->NotifyFlushComplete(flush_req_id);
        if (++flushes_seen == kNumFlushes)
          checkpoint();
      }));
  task_runner.RunUntilCheckpoint("all_flushes_done");

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  auto trace_packets = consumer->ReadBuffers();
  for (int i = 0; i < kNumFlushes; i++) {
    EXPECT_THAT(trace_packets,
                Contains(Property(&protos::gen::TracePacket::for_testing,
                                  Property(&protos::gen::TestEvent::str,
                                           Eq("f_" + std::to_string(i))))));
  }
}

TEST_F(TracingServiceImplTest, PeriodicClearIncrementalState) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());
  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Incremental data source that expects to receive the clear.
  producer->RegisterDataSource("ds_incremental1", false, false,
                               /*handles_incremental_state_clear=*/true);

  // Incremental data source that expects to receive the clear.
  producer->RegisterDataSource("ds_incremental2", false, false,
                               /*handles_incremental_state_clear=*/true);

  // Data source that does *not* advertise itself as supporting incremental
  // state clears.
  producer->RegisterDataSource("ds_selfcontained", false, false,
                               /*handles_incremental_state_clear=*/false);

  // Incremental data source that is registered, but won't be active within the
  // test's tracing session.
  producer->RegisterDataSource("ds_inactive", false, false,
                               /*handles_incremental_state_clear=*/true);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.mutable_incremental_state_config()->set_clear_period_ms(1);
  trace_config.add_data_sources()->mutable_config()->set_name(
      "ds_selfcontained");
  trace_config.add_data_sources()->mutable_config()->set_name(
      "ds_incremental1");
  trace_config.add_data_sources()->mutable_config()->set_name(
      "ds_incremental2");

  // note: the mocking is very brittle, and has to assume a specific order of
  // the data sources' setup/start.
  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("ds_selfcontained");
  producer->WaitForDataSourceSetup("ds_incremental1");
  producer->WaitForDataSourceSetup("ds_incremental2");
  producer->WaitForDataSourceStart("ds_selfcontained");
  producer->WaitForDataSourceStart("ds_incremental1");
  producer->WaitForDataSourceStart("ds_incremental2");

  DataSourceInstanceID ds_incremental1 =
      producer->GetDataSourceInstanceId("ds_incremental1");
  DataSourceInstanceID ds_incremental2 =
      producer->GetDataSourceInstanceId("ds_incremental2");

  const size_t kNumClears = 3;
  std::function<void()> checkpoint =
      task_runner.CreateCheckpoint("clears_received");
  std::vector<std::vector<DataSourceInstanceID>> clears_seen;
  EXPECT_CALL(*producer, ClearIncrementalState(_, _))
      .WillRepeatedly(Invoke([&clears_seen, &checkpoint](
                                 const DataSourceInstanceID* data_source_ids,
                                 size_t num_data_sources) {
        std::vector<DataSourceInstanceID> ds_ids;
        for (size_t i = 0; i < num_data_sources; i++) {
          ds_ids.push_back(*data_source_ids++);
        }
        clears_seen.push_back(ds_ids);
        if (clears_seen.size() >= kNumClears)
          checkpoint();
      }));
  task_runner.RunUntilCheckpoint("clears_received");

  consumer->DisableTracing();

  // Assert that the clears were only for the active incremental data sources.
  ASSERT_EQ(clears_seen.size(), kNumClears);
  for (const std::vector<DataSourceInstanceID>& ds_ids : clears_seen) {
    ASSERT_THAT(ds_ids, ElementsAreArray({ds_incremental1, ds_incremental2}));
  }
}

// Creates a tracing session where some of the data sources set the
// |will_notify_on_stop| flag and checks that the OnTracingDisabled notification
// to the consumer is delayed until the acks are received.
TEST_F(TracingServiceImplTest, OnTracingDisabledWaitsForDataSourceStopAcks) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds_will_ack_1", /*ack_stop=*/true,
                               /*ack_start=*/true);
  producer->RegisterDataSource("ds_wont_ack");
  producer->RegisterDataSource("ds_will_ack_2", /*ack_stop=*/true,
                               /*ack_start=*/false);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_will_ack_1");
  trace_config.add_data_sources()->mutable_config()->set_name("ds_wont_ack");
  trace_config.add_data_sources()->mutable_config()->set_name("ds_will_ack_2");
  trace_config.set_duration_ms(1);
  trace_config.set_deferred_start(true);

  consumer->EnableTracing(trace_config);

  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_1"),
            DataSourceInstanceState::CONFIGURED);
  EXPECT_EQ(GetDataSourceInstanceState("ds_wont_ack"),
            DataSourceInstanceState::CONFIGURED);
  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_2"),
            DataSourceInstanceState::CONFIGURED);

  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_will_ack_1");
  producer->WaitForDataSourceSetup("ds_wont_ack");
  producer->WaitForDataSourceSetup("ds_will_ack_2");

  DataSourceInstanceID id1 = producer->GetDataSourceInstanceId("ds_will_ack_1");
  DataSourceInstanceID id2 = producer->GetDataSourceInstanceId("ds_will_ack_2");

  consumer->StartTracing();

  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_1"),
            DataSourceInstanceState::STARTING);
  EXPECT_EQ(GetDataSourceInstanceState("ds_wont_ack"),
            DataSourceInstanceState::STARTED);
  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_2"),
            DataSourceInstanceState::STARTED);

  producer->WaitForDataSourceStart("ds_will_ack_1");
  producer->WaitForDataSourceStart("ds_wont_ack");
  producer->WaitForDataSourceStart("ds_will_ack_2");

  producer->endpoint()->NotifyDataSourceStarted(id1);

  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_1"),
            DataSourceInstanceState::STARTED);

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("ds_wont_ack");
  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("ds_will_ack_1");
  producer->WaitForDataSourceStop("ds_wont_ack");
  producer->WaitForDataSourceStop("ds_will_ack_2");

  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_1"),
            DataSourceInstanceState::STOPPING);
  EXPECT_EQ(GetDataSourceInstanceState("ds_wont_ack"),
            DataSourceInstanceState::STOPPED);
  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_2"),
            DataSourceInstanceState::STOPPING);

  producer->endpoint()->NotifyDataSourceStopped(id1);
  producer->endpoint()->NotifyDataSourceStopped(id2);

  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_1"),
            DataSourceInstanceState::STOPPED);
  EXPECT_EQ(GetDataSourceInstanceState("ds_will_ack_2"),
            DataSourceInstanceState::STOPPED);

  // Wait for at most half of the service timeout, so that this test fails if
  // the service falls back on calling the OnTracingDisabled() because some of
  // the expected acks weren't received.
  consumer->WaitForTracingDisabled(
      TracingServiceImpl::kDataSourceStopTimeoutMs / 2);
}

// Creates a tracing session where a second data source
// is added while the service is waiting for DisableTracing
// acks; the service should not enable the new datasource
// and should not hit any asserts when the consumer is
// subsequently destroyed.
TEST_F(TracingServiceImplTest, OnDataSourceAddedWhilePendingDisableAcks) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds_will_ack", /*ack_stop=*/true);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_will_ack");
  trace_config.add_data_sources()->mutable_config()->set_name("ds_wont_ack");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  consumer->DisableTracing();

  producer->RegisterDataSource("ds_wont_ack");

  consumer.reset();
}

// Similar to OnTracingDisabledWaitsForDataSourceStopAcks, but deliberately
// skips the ack and checks that the service invokes the OnTracingDisabled()
// after the timeout.
TEST_F(TracingServiceImplTest, OnTracingDisabledCalledAnywaysInCaseOfTimeout) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source", /*ack_stop=*/true);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("data_source");
  trace_config.set_duration_ms(1);
  trace_config.set_data_source_stop_timeout_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

// Tests the session_id logic. Two data sources in the same tracing session
// should see the same session id.
TEST_F(TracingServiceImplTest, SessionId) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer1 = CreateMockProducer();
  producer1->Connect(svc.get(), "mock_producer1");
  producer1->RegisterDataSource("ds_1A");
  producer1->RegisterDataSource("ds_1B");

  std::unique_ptr<MockProducer> producer2 = CreateMockProducer();
  producer2->Connect(svc.get(), "mock_producer2");
  producer2->RegisterDataSource("ds_2A");

  InSequence seq;
  TracingSessionID last_session_id = 0;
  for (int i = 0; i < 3; i++) {
    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(128);
    trace_config.add_data_sources()->mutable_config()->set_name("ds_1A");
    trace_config.add_data_sources()->mutable_config()->set_name("ds_1B");
    trace_config.add_data_sources()->mutable_config()->set_name("ds_2A");
    trace_config.set_duration_ms(1);

    consumer->EnableTracing(trace_config);

    if (i == 0)
      producer1->WaitForTracingSetup();

    producer1->WaitForDataSourceSetup("ds_1A");
    producer1->WaitForDataSourceSetup("ds_1B");
    if (i == 0)
      producer2->WaitForTracingSetup();
    producer2->WaitForDataSourceSetup("ds_2A");

    producer1->WaitForDataSourceStart("ds_1A");
    producer1->WaitForDataSourceStart("ds_1B");
    producer2->WaitForDataSourceStart("ds_2A");

    auto* ds1 = producer1->GetDataSourceInstance("ds_1A");
    auto* ds2 = producer1->GetDataSourceInstance("ds_1B");
    auto* ds3 = producer2->GetDataSourceInstance("ds_2A");
    ASSERT_EQ(ds1->session_id, ds2->session_id);
    ASSERT_EQ(ds1->session_id, ds3->session_id);
    ASSERT_NE(ds1->session_id, last_session_id);
    last_session_id = ds1->session_id;

    auto writer1 = producer1->CreateTraceWriter("ds_1A");
    producer1->WaitForFlush(writer1.get());

    auto writer2 = producer2->CreateTraceWriter("ds_2A");
    producer2->WaitForFlush(writer2.get());

    producer1->WaitForDataSourceStop("ds_1A");
    producer1->WaitForDataSourceStop("ds_1B");
    producer2->WaitForDataSourceStop("ds_2A");
    consumer->WaitForTracingDisabled();
    consumer->FreeBuffers();
  }
}

// Writes a long trace and then tests that the trace parsed in partitions
// derived by the synchronization markers is identical to the whole trace parsed
// in one go.
TEST_F(TracingServiceImplTest, ResynchronizeTraceStreamUsingSyncMarker) {
  // Setup tracing.
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());
  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");
  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  trace_config.set_write_into_file(true);
  trace_config.set_file_write_period_ms(1);
  base::TempFile tmp_file = base::TempFile::Create();
  consumer->EnableTracing(trace_config, base::ScopedFile(dup(tmp_file.fd())));
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Write some variable length payload, waiting for sync markers every now
  // and then.
  const int kNumMarkers = 5;
  auto writer = producer->CreateTraceWriter("data_source");
  for (int i = 1; i <= 100; i++) {
    std::string payload(static_cast<size_t>(i), 'A' + (i % 25));
    writer->NewTracePacket()->set_for_testing()->set_str(payload.c_str());
    if (i % (100 / kNumMarkers) == 0) {
      writer->Flush();
      WaitForNextSyncMarker();
    }
  }
  writer->Flush();
  writer.reset();
  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  std::string trace_raw;
  ASSERT_TRUE(base::ReadFile(tmp_file.path().c_str(), &trace_raw));

  const auto kMarkerSize = sizeof(TracingServiceImpl::kSyncMarker);
  const std::string kSyncMarkerStr(
      reinterpret_cast<const char*>(TracingServiceImpl::kSyncMarker),
      kMarkerSize);

  // Read back the trace in partitions derived from the marker.
  // The trace should look like this:
  // [uid, marker] [event] [event] [uid, marker] [event] [event]
  size_t num_markers = 0;
  size_t start = 0;
  size_t end = 0;
  std::string merged_trace_raw;
  for (size_t pos = 0; pos != std::string::npos; start = end) {
    pos = trace_raw.find(kSyncMarkerStr, pos + 1);
    num_markers++;
    end = (pos == std::string::npos) ? trace_raw.size() : pos + kMarkerSize;
    size_t size = end - start;
    ASSERT_GT(size, 0u);
    std::string trace_partition_raw = trace_raw.substr(start, size);
    protos::gen::Trace trace_partition;
    ASSERT_TRUE(trace_partition.ParseFromString(trace_partition_raw));
    merged_trace_raw += trace_partition_raw;
  }
  EXPECT_GE(num_markers, static_cast<size_t>(kNumMarkers));

  protos::gen::Trace whole_trace;
  ASSERT_TRUE(whole_trace.ParseFromString(trace_raw));

  protos::gen::Trace merged_trace;
  merged_trace.ParseFromString(merged_trace_raw);

  ASSERT_EQ(whole_trace.packet_size(), merged_trace.packet_size());
  EXPECT_EQ(whole_trace.SerializeAsString(), merged_trace.SerializeAsString());
}

// Creates a tracing session with |deferred_start| and checks that data sources
// are started only after calling StartTracing().
TEST_F(TracingServiceImplTest, DeferredStart) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  trace_config.set_deferred_start(true);
  trace_config.set_duration_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  task_runner.RunUntilIdle();

  consumer->StartTracing();

  producer->WaitForDataSourceStart("ds_1");

  auto writer = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ProducerUIDsAndPacketSequenceIDs) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer1 = CreateMockProducer();
  producer1->Connect(svc.get(), "mock_producer1", 123u /* uid */);
  producer1->RegisterDataSource("data_source");

  std::unique_ptr<MockProducer> producer2 = CreateMockProducer();
  producer2->Connect(svc.get(), "mock_producer2", 456u /* uid */);
  producer2->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer1->WaitForTracingSetup();
  producer1->WaitForDataSourceSetup("data_source");
  producer2->WaitForTracingSetup();
  producer2->WaitForDataSourceSetup("data_source");
  producer1->WaitForDataSourceStart("data_source");
  producer2->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer1a =
      producer1->CreateTraceWriter("data_source");
  std::unique_ptr<TraceWriter> writer1b =
      producer1->CreateTraceWriter("data_source");
  std::unique_ptr<TraceWriter> writer2a =
      producer2->CreateTraceWriter("data_source");
  {
    auto tp = writer1a->NewTracePacket();
    tp->set_for_testing()->set_str("payload1a1");
    tp = writer1b->NewTracePacket();
    tp->set_for_testing()->set_str("payload1b1");
    tp = writer1a->NewTracePacket();
    tp->set_for_testing()->set_str("payload1a2");
    tp = writer2a->NewTracePacket();
    tp->set_for_testing()->set_str("payload2a1");
    tp = writer1b->NewTracePacket();
    tp->set_for_testing()->set_str("payload1b2");
  }

  auto flush_request = consumer->Flush();
  producer1->WaitForFlush({writer1a.get(), writer1b.get()});
  producer2->WaitForFlush(writer2a.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  consumer->DisableTracing();
  producer1->WaitForDataSourceStop("data_source");
  producer2->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(
      packets,
      Contains(AllOf(
          Property(&protos::gen::TracePacket::for_testing,
                   Property(&protos::gen::TestEvent::str, Eq("payload1a1"))),
          Property(&protos::gen::TracePacket::trusted_uid, Eq(123)),
          Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                   Eq(2u)))));
  EXPECT_THAT(
      packets,
      Contains(AllOf(
          Property(&protos::gen::TracePacket::for_testing,
                   Property(&protos::gen::TestEvent::str, Eq("payload1a2"))),
          Property(&protos::gen::TracePacket::trusted_uid, Eq(123)),
          Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                   Eq(2u)))));
  EXPECT_THAT(
      packets,
      Contains(AllOf(
          Property(&protos::gen::TracePacket::for_testing,
                   Property(&protos::gen::TestEvent::str, Eq("payload1b1"))),
          Property(&protos::gen::TracePacket::trusted_uid, Eq(123)),
          Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                   Eq(3u)))));
  EXPECT_THAT(
      packets,
      Contains(AllOf(
          Property(&protos::gen::TracePacket::for_testing,
                   Property(&protos::gen::TestEvent::str, Eq("payload1b2"))),
          Property(&protos::gen::TracePacket::trusted_uid, Eq(123)),
          Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                   Eq(3u)))));
  EXPECT_THAT(
      packets,
      Contains(AllOf(
          Property(&protos::gen::TracePacket::for_testing,
                   Property(&protos::gen::TestEvent::str, Eq("payload2a1"))),
          Property(&protos::gen::TracePacket::trusted_uid, Eq(456)),
          Property(&protos::gen::TracePacket::trusted_packet_sequence_id,
                   Eq(4u)))));
}

TEST_F(TracingServiceImplTest, AllowedBuffers) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer1 = CreateMockProducer();
  producer1->Connect(svc.get(), "mock_producer1");
  ProducerID producer1_id = *last_producer_id();
  producer1->RegisterDataSource("data_source1");
  std::unique_ptr<MockProducer> producer2 = CreateMockProducer();
  producer2->Connect(svc.get(), "mock_producer2");
  ProducerID producer2_id = *last_producer_id();
  producer2->RegisterDataSource("data_source2.1");
  producer2->RegisterDataSource("data_source2.2");
  producer2->RegisterDataSource("data_source2.3");

  EXPECT_EQ(std::set<BufferID>(), GetAllowedTargetBuffers(producer1_id));
  EXPECT_EQ(std::set<BufferID>(), GetAllowedTargetBuffers(producer2_id));

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config1 = trace_config.add_data_sources()->mutable_config();
  ds_config1->set_name("data_source1");
  ds_config1->set_target_buffer(0);
  auto* ds_config21 = trace_config.add_data_sources()->mutable_config();
  ds_config21->set_name("data_source2.1");
  ds_config21->set_target_buffer(1);
  auto* ds_config22 = trace_config.add_data_sources()->mutable_config();
  ds_config22->set_name("data_source2.2");
  ds_config22->set_target_buffer(2);
  auto* ds_config23 = trace_config.add_data_sources()->mutable_config();
  ds_config23->set_name("data_source2.3");
  ds_config23->set_target_buffer(2);  // same buffer as data_source2.2.
  consumer->EnableTracing(trace_config);

  ASSERT_EQ(3u, tracing_session()->num_buffers());
  std::set<BufferID> expected_buffers_producer1 = {
      tracing_session()->buffers_index[0]};
  std::set<BufferID> expected_buffers_producer2 = {
      tracing_session()->buffers_index[1], tracing_session()->buffers_index[2]};
  EXPECT_EQ(expected_buffers_producer1, GetAllowedTargetBuffers(producer1_id));
  EXPECT_EQ(expected_buffers_producer2, GetAllowedTargetBuffers(producer2_id));

  producer1->WaitForTracingSetup();
  producer1->WaitForDataSourceSetup("data_source1");

  producer2->WaitForTracingSetup();
  producer2->WaitForDataSourceSetup("data_source2.1");
  producer2->WaitForDataSourceSetup("data_source2.2");
  producer2->WaitForDataSourceSetup("data_source2.3");

  producer1->WaitForDataSourceStart("data_source1");
  producer2->WaitForDataSourceStart("data_source2.1");
  producer2->WaitForDataSourceStart("data_source2.2");
  producer2->WaitForDataSourceStart("data_source2.3");

  producer2->UnregisterDataSource("data_source2.3");
  producer2->WaitForDataSourceStop("data_source2.3");

  // Should still be allowed to write to buffers 1 (data_source2.1) and 2
  // (data_source2.2).
  EXPECT_EQ(expected_buffers_producer2, GetAllowedTargetBuffers(producer2_id));

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  consumer->DisableTracing();
  producer1->WaitForDataSourceStop("data_source1");
  producer2->WaitForDataSourceStop("data_source2.1");
  producer2->WaitForDataSourceStop("data_source2.2");
  consumer->WaitForTracingDisabled();

  consumer->FreeBuffers();
  EXPECT_EQ(std::set<BufferID>(), GetAllowedTargetBuffers(producer1_id));
  EXPECT_EQ(std::set<BufferID>(), GetAllowedTargetBuffers(producer2_id));
}

#if !PERFETTO_DCHECK_IS_ON()
TEST_F(TracingServiceImplTest, CommitToForbiddenBufferIsDiscarded) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  ProducerID producer_id = *last_producer_id();
  producer->RegisterDataSource("data_source");

  EXPECT_EQ(std::set<BufferID>(), GetAllowedTargetBuffers(producer_id));

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  consumer->EnableTracing(trace_config);

  ASSERT_EQ(2u, tracing_session()->num_buffers());
  std::set<BufferID> expected_buffers = {tracing_session()->buffers_index[0]};
  EXPECT_EQ(expected_buffers, GetAllowedTargetBuffers(producer_id));

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  // Try to write to the correct buffer.
  std::unique_ptr<TraceWriter> writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[0]);
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("good_payload");
  }

  auto flush_request = consumer->Flush();
  producer->WaitForFlush(writer.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  // Try to write to the wrong buffer.
  writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[1]);
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("bad_payload");
  }

  flush_request = consumer->Flush();
  producer->WaitForFlush(writer.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("good_payload")))));
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("bad_payload"))))));

  consumer->FreeBuffers();
  EXPECT_EQ(std::set<BufferID>(), GetAllowedTargetBuffers(producer_id));
}
#endif  // !PERFETTO_DCHECK_IS_ON()

TEST_F(TracingServiceImplTest, RegisterAndUnregisterTraceWriter) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  ProducerID producer_id = *last_producer_id();
  producer->RegisterDataSource("data_source");

  EXPECT_TRUE(GetWriters(producer_id).empty());

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  // Creating the trace writer should register it with the service.
  std::unique_ptr<TraceWriter> writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[0]);

  WaitForTraceWritersChanged(producer_id);

  std::map<WriterID, BufferID> expected_writers;
  expected_writers[writer->writer_id()] = tracing_session()->buffers_index[0];
  EXPECT_EQ(expected_writers, GetWriters(producer_id));

  // Verify writing works.
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  auto flush_request = consumer->Flush();
  producer->WaitForFlush(writer.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  // Destroying the writer should unregister it.
  writer.reset();
  WaitForTraceWritersChanged(producer_id);
  EXPECT_TRUE(GetWriters(producer_id).empty());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload")))));
}

TEST_F(TracingServiceImplTest, ScrapeBuffersOnFlush) {
  svc->SetSMBScrapingEnabled(true);

  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  ProducerID producer_id = *last_producer_id();
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  std::unique_ptr<TraceWriter> writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[0]);
  WaitForTraceWritersChanged(producer_id);

  // Write a few trace packets.
  writer->NewTracePacket()->set_for_testing()->set_str("payload1");
  writer->NewTracePacket()->set_for_testing()->set_str("payload2");
  writer->NewTracePacket()->set_for_testing()->set_str("payload3");

  // Flush but don't actually flush the chunk from TraceWriter.
  auto flush_request = consumer->Flush();
  producer->WaitForFlush(nullptr, /*reply=*/true);
  ASSERT_TRUE(flush_request.WaitForReply());

  // Chunk with the packets should have been scraped. The service can't know
  // whether the last packet was completed, so shouldn't read it.
  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload1")))));
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload2")))));
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload3"))))));

  // Write some more packets.
  writer->NewTracePacket()->set_for_testing()->set_str("payload4");
  writer->NewTracePacket()->set_for_testing()->set_str("payload5");

  // Don't reply to flush, causing a timeout. This should scrape again.
  flush_request = consumer->Flush(/*timeout=*/100);
  producer->WaitForFlush(nullptr, /*reply=*/false);
  ASSERT_FALSE(flush_request.WaitForReply());

  // Chunk with the packets should have been scraped again, overriding the
  // original one. Again, the last packet should be ignored and the first two
  // should not be read twice.
  packets = consumer->ReadBuffers();
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload1"))))));
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload2"))))));
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload3")))));
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload4")))));
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload5"))))));

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ScrapeBuffersFromAnotherThread) {
  // This test verifies that there are no reported TSAN races while scraping
  // buffers from a producer which is actively writing more trace data
  // concurrently.
  svc->SetSMBScrapingEnabled(true);

  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  ProducerID producer_id = *last_producer_id();
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");
  consumer->StartTracing();

  std::unique_ptr<TraceWriter> writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[0]);
  WaitForTraceWritersChanged(producer_id);

  constexpr int kPacketCount = 10;
  std::atomic<int> packets_written{};
  std::thread writer_thread([&] {
    for (int i = 0; i < kPacketCount; i++) {
      writer->NewTracePacket()->set_for_testing()->set_str("payload");
      packets_written.store(i, std::memory_order_relaxed);
    }
  });

  // Wait until the thread has had some time to write some packets.
  while (packets_written.load(std::memory_order_relaxed) < kPacketCount / 2)
    base::SleepMicroseconds(5000);

  // Disabling tracing will trigger scraping.
  consumer->DisableTracing();
  writer_thread.join();

  // Because we don't synchronize with the producer thread, we can't make any
  // guarantees about the number of packets we will successfully read. We just
  // verify that no TSAN races are reported.
  consumer->ReadBuffers();

  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

// Test scraping on producer disconnect.
TEST_F(TracingServiceImplTest, ScrapeBuffersOnProducerDisconnect) {
  svc->SetSMBScrapingEnabled(true);

  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  ProducerID producer_id = *last_producer_id();
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  std::unique_ptr<TraceWriter> writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[0]);
  WaitForTraceWritersChanged(producer_id);

  // Write a few trace packets.
  writer->NewTracePacket()->set_for_testing()->set_str("payload1");
  writer->NewTracePacket()->set_for_testing()->set_str("payload2");
  writer->NewTracePacket()->set_for_testing()->set_str("payload3");

  // Disconnect the producer without committing the chunk. This should cause a
  // scrape of the SMB. Avoid destroying the ShmemArbiter until writer is
  // destroyed.
  auto shmem_arbiter = TakeShmemArbiterForProducer(producer_id);
  producer.reset();

  // Chunk with the packets should have been scraped. The service can't know
  // whether the last packet was completed, so shouldn't read it.
  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload1")))));
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload2")))));
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload3"))))));

  // Cleanup writer without causing a crash because the producer already went
  // away.
  static_cast<TraceWriterImpl*>(writer.get())->ResetChunkForTesting();
  writer.reset();
  shmem_arbiter.reset();

  consumer->DisableTracing();
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ScrapeBuffersOnDisable) {
  svc->SetSMBScrapingEnabled(true);

  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  ProducerID producer_id = *last_producer_id();
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  std::unique_ptr<TraceWriter> writer = producer->endpoint()->CreateTraceWriter(
      tracing_session()->buffers_index[0]);
  WaitForTraceWritersChanged(producer_id);

  // Write a few trace packets.
  writer->NewTracePacket()->set_for_testing()->set_str("payload1");
  writer->NewTracePacket()->set_for_testing()->set_str("payload2");
  writer->NewTracePacket()->set_for_testing()->set_str("payload3");

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  // Chunk with the packets should have been scraped. The service can't know
  // whether the last packet was completed, so shouldn't read it.
  auto packets = consumer->ReadBuffers();
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload1")))));
  EXPECT_THAT(packets, Contains(Property(&protos::gen::TracePacket::for_testing,
                                         Property(&protos::gen::TestEvent::str,
                                                  Eq("payload2")))));
  EXPECT_THAT(packets,
              Not(Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload3"))))));
}

TEST_F(TracingServiceImplTest, AbortIfTraceDurationIsTooLong) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("datasource");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("datasource");
  trace_config.set_duration_ms(0x7fffffff);

  EXPECT_CALL(*producer, SetupDataSource(_, _)).Times(0);
  consumer->EnableTracing(trace_config);

  // The trace is aborted immediately, 5s here is just some slack for the thread
  // ping-pongs for slow devices.
  consumer->WaitForTracingDisabled(5000);
}

TEST_F(TracingServiceImplTest, GetTraceStats) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  consumer->GetTraceStats();
  consumer->WaitForTraceStats(false);

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  consumer->GetTraceStats();
  consumer->WaitForTraceStats(true);

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ObserveEventsDataSourceInstances) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  // Start tracing before the consumer is interested in events. The consumer's
  // OnObservableEvents() should not be called yet.
  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling ObserveEvents should cause an event for the initial instance state.
  consumer->ObserveEvents(ObservableEvents::TYPE_DATA_SOURCES_INSTANCES);
  {
    auto events = consumer->WaitForObservableEvents();

    ObservableEvents::DataSourceInstanceStateChange change;
    change.set_producer_name("mock_producer");
    change.set_data_source_name("data_source");
    change.set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
    EXPECT_EQ(events.instance_state_changes_size(), 1);
    EXPECT_THAT(events.instance_state_changes(), Contains(Eq(change)));
  }

  // Disabling should cause an instance state change to STOPPED.
  consumer->DisableTracing();

  {
    auto events = consumer->WaitForObservableEvents();

    ObservableEvents::DataSourceInstanceStateChange change;
    change.set_producer_name("mock_producer");
    change.set_data_source_name("data_source");
    change.set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED);
    EXPECT_EQ(events.instance_state_changes_size(), 1);
    EXPECT_THAT(events.instance_state_changes(), Contains(Eq(change)));
  }

  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  consumer->FreeBuffers();

  // Enable again, this should cause a state change for a new instance to
  // its initial state STOPPED.
  trace_config.set_deferred_start(true);
  consumer->EnableTracing(trace_config);

  {
    auto events = consumer->WaitForObservableEvents();

    ObservableEvents::DataSourceInstanceStateChange change;
    change.set_producer_name("mock_producer");
    change.set_data_source_name("data_source");
    change.set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED);
    EXPECT_EQ(events.instance_state_changes_size(), 1);
    EXPECT_THAT(events.instance_state_changes(), Contains(Eq(change)));
  }

  producer->WaitForDataSourceSetup("data_source");

  // Should move the instance into STARTED state and thus cause an event.
  consumer->StartTracing();

  {
    auto events = consumer->WaitForObservableEvents();

    ObservableEvents::DataSourceInstanceStateChange change;
    change.set_producer_name("mock_producer");
    change.set_data_source_name("data_source");
    change.set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
    EXPECT_EQ(events.instance_state_changes_size(), 1);
    EXPECT_THAT(events.instance_state_changes(), Contains(Eq(change)));
  }

  producer->WaitForDataSourceStart("data_source");

  // Stop observing events.
  consumer->ObserveEvents(0);

  // Disabling should now no longer cause events to be sent to the consumer.
  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ObserveEventsDataSourceInstancesUnregister) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  // Start tracing before the consumer is interested in events. The consumer's
  // OnObservableEvents() should not be called yet.
  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling ObserveEvents should cause an event for the initial instance state.
  consumer->ObserveEvents(ObservableEvents::TYPE_DATA_SOURCES_INSTANCES);
  {
    ObservableEvents event;
    ObservableEvents::DataSourceInstanceStateChange* change =
        event.add_instance_state_changes();
    change->set_producer_name("mock_producer");
    change->set_data_source_name("data_source");
    change->set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
    EXPECT_CALL(*consumer, OnObservableEvents(Eq(event)))
        .WillOnce(InvokeWithoutArgs(
            task_runner.CreateCheckpoint("data_source_started")));

    task_runner.RunUntilCheckpoint("data_source_started");
  }
  {
    ObservableEvents event;
    ObservableEvents::DataSourceInstanceStateChange* change =
        event.add_instance_state_changes();
    change->set_producer_name("mock_producer");
    change->set_data_source_name("data_source");
    change->set_state(ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED);
    EXPECT_CALL(*consumer, OnObservableEvents(Eq(event)))
        .WillOnce(InvokeWithoutArgs(
            task_runner.CreateCheckpoint("data_source_stopped")));
  }
  producer->UnregisterDataSource("data_source");
  producer->WaitForDataSourceStop("data_source");
  task_runner.RunUntilCheckpoint("data_source_stopped");

  consumer->DisableTracing();
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, ObserveAllDataSourceStarted) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds1", /*ack_stop=*/false, /*ack_start=*/true);
  producer->RegisterDataSource("ds2", /*ack_stop=*/false, /*ack_start=*/true);

  TraceConfig trace_config;
  trace_config.set_deferred_start(true);
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("ds1");
  ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("ds2");

  for (int repetition = 0; repetition < 3; repetition++) {
    consumer->EnableTracing(trace_config);

    if (repetition == 0)
      producer->WaitForTracingSetup();

    producer->WaitForDataSourceSetup("ds1");
    producer->WaitForDataSourceSetup("ds2");
    task_runner.RunUntilIdle();

    consumer->ObserveEvents(ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED);
    consumer->StartTracing();
    producer->WaitForDataSourceStart("ds1");
    producer->WaitForDataSourceStart("ds2");

    DataSourceInstanceID id1 = producer->GetDataSourceInstanceId("ds1");
    producer->endpoint()->NotifyDataSourceStarted(id1);

    // The notification shouldn't happen yet, ds2 has not acked.
    task_runner.RunUntilIdle();
    Mock::VerifyAndClearExpectations(consumer.get());

    EXPECT_THAT(
        consumer->ReadBuffers(),
        Contains(Property(
            &protos::gen::TracePacket::service_event,
            Property(
                &protos::gen::TracingServiceEvent::all_data_sources_started,
                Eq(false)))));

    DataSourceInstanceID id2 = producer->GetDataSourceInstanceId("ds2");
    producer->endpoint()->NotifyDataSourceStarted(id2);

    // Now the |all_data_sources_started| notification should be sent.

    auto events = consumer->WaitForObservableEvents();
    ObservableEvents::DataSourceInstanceStateChange change;
    EXPECT_TRUE(events.all_data_sources_started());

    // Disabling should cause an instance state change to STOPPED.
    consumer->DisableTracing();
    producer->WaitForDataSourceStop("ds1");
    producer->WaitForDataSourceStop("ds2");
    consumer->WaitForTracingDisabled();

    EXPECT_THAT(
        consumer->ReadBuffers(),
        Contains(Property(
            &protos::gen::TracePacket::service_event,
            Property(
                &protos::gen::TracingServiceEvent::all_data_sources_started,
                Eq(true)))));
    consumer->FreeBuffers();

    task_runner.RunUntilIdle();

    Mock::VerifyAndClearExpectations(consumer.get());
    Mock::VerifyAndClearExpectations(producer.get());
  }
}

// Similar to ObserveAllDataSourceStarted, but covers the case of some data
// sources not supporting the |notify_on_start|.
TEST_F(TracingServiceImplTest, ObserveAllDataSourceStartedOnlySomeWillAck) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds1", /*ack_stop=*/false, /*ack_start=*/true);
  producer->RegisterDataSource("ds2_no_ack");

  TraceConfig trace_config;
  trace_config.set_deferred_start(true);
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("ds1");
  ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("ds2_no_ack");

  for (int repetition = 0; repetition < 3; repetition++) {
    consumer->EnableTracing(trace_config);

    if (repetition == 0)
      producer->WaitForTracingSetup();

    producer->WaitForDataSourceSetup("ds1");
    producer->WaitForDataSourceSetup("ds2_no_ack");
    task_runner.RunUntilIdle();

    consumer->ObserveEvents(ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED);
    consumer->StartTracing();
    producer->WaitForDataSourceStart("ds1");
    producer->WaitForDataSourceStart("ds2_no_ack");

    DataSourceInstanceID id1 = producer->GetDataSourceInstanceId("ds1");
    producer->endpoint()->NotifyDataSourceStarted(id1);

    auto events = consumer->WaitForObservableEvents();
    ObservableEvents::DataSourceInstanceStateChange change;
    EXPECT_TRUE(events.all_data_sources_started());

    // Disabling should cause an instance state change to STOPPED.
    consumer->DisableTracing();
    producer->WaitForDataSourceStop("ds1");
    producer->WaitForDataSourceStop("ds2_no_ack");
    consumer->FreeBuffers();
    consumer->WaitForTracingDisabled();

    task_runner.RunUntilIdle();
    Mock::VerifyAndClearExpectations(consumer.get());
    Mock::VerifyAndClearExpectations(producer.get());
  }
}

// Similar to ObserveAllDataSourceStarted, but covers the case of no data
// sources supporting the |notify_on_start|. In this case the
// TYPE_ALL_DATA_SOURCES_STARTED notification should be sent immediately after
// calling Start().
TEST_F(TracingServiceImplTest, ObserveAllDataSourceStartedNoAck) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds1_no_ack");
  producer->RegisterDataSource("ds2_no_ack");

  TraceConfig trace_config;
  trace_config.set_deferred_start(true);
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("ds1_no_ack");
  ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("ds2_no_ack");

  for (int repetition = 0; repetition < 3; repetition++) {
    consumer->EnableTracing(trace_config);

    if (repetition == 0)
      producer->WaitForTracingSetup();

    producer->WaitForDataSourceSetup("ds1_no_ack");
    producer->WaitForDataSourceSetup("ds2_no_ack");
    task_runner.RunUntilIdle();

    consumer->ObserveEvents(ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED);
    consumer->StartTracing();
    producer->WaitForDataSourceStart("ds1_no_ack");
    producer->WaitForDataSourceStart("ds2_no_ack");

    auto events = consumer->WaitForObservableEvents();
    ObservableEvents::DataSourceInstanceStateChange change;
    EXPECT_TRUE(events.all_data_sources_started());

    // Disabling should cause an instance state change to STOPPED.
    consumer->DisableTracing();
    producer->WaitForDataSourceStop("ds1_no_ack");
    producer->WaitForDataSourceStop("ds2_no_ack");
    consumer->FreeBuffers();
    consumer->WaitForTracingDisabled();

    task_runner.RunUntilIdle();
    Mock::VerifyAndClearExpectations(consumer.get());
    Mock::VerifyAndClearExpectations(producer.get());
  }
}

TEST_F(TracingServiceImplTest, QueryServiceState) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer1 = CreateMockProducer();
  producer1->Connect(svc.get(), "producer1");

  std::unique_ptr<MockProducer> producer2 = CreateMockProducer();
  producer2->Connect(svc.get(), "producer2");

  producer1->RegisterDataSource("common_ds");
  producer2->RegisterDataSource("common_ds");

  producer1->RegisterDataSource("p1_ds");
  producer2->RegisterDataSource("p2_ds");

  TracingServiceState svc_state = consumer->QueryServiceState();

  EXPECT_EQ(svc_state.producers_size(), 2);
  EXPECT_EQ(svc_state.producers().at(0).id(), 1);
  EXPECT_EQ(svc_state.producers().at(0).name(), "producer1");
  EXPECT_EQ(svc_state.producers().at(1).id(), 2);
  EXPECT_EQ(svc_state.producers().at(1).name(), "producer2");

  EXPECT_EQ(svc_state.data_sources_size(), 4);

  EXPECT_EQ(svc_state.data_sources().at(0).producer_id(), 1);
  EXPECT_EQ(svc_state.data_sources().at(0).ds_descriptor().name(), "common_ds");

  EXPECT_EQ(svc_state.data_sources().at(1).producer_id(), 2);
  EXPECT_EQ(svc_state.data_sources().at(1).ds_descriptor().name(), "common_ds");

  EXPECT_EQ(svc_state.data_sources().at(2).producer_id(), 1);
  EXPECT_EQ(svc_state.data_sources().at(2).ds_descriptor().name(), "p1_ds");

  EXPECT_EQ(svc_state.data_sources().at(3).producer_id(), 2);
  EXPECT_EQ(svc_state.data_sources().at(3).ds_descriptor().name(), "p2_ds");

  // Test that descriptors are cleared when a producer disconnects.
  producer1.reset();
  svc_state = consumer->QueryServiceState();

  EXPECT_EQ(svc_state.producers_size(), 1);
  EXPECT_EQ(svc_state.data_sources_size(), 2);

  EXPECT_EQ(svc_state.data_sources().at(0).producer_id(), 2);
  EXPECT_EQ(svc_state.data_sources().at(0).ds_descriptor().name(), "common_ds");
  EXPECT_EQ(svc_state.data_sources().at(1).producer_id(), 2);
  EXPECT_EQ(svc_state.data_sources().at(1).ds_descriptor().name(), "p2_ds");
}

TEST_F(TracingServiceImplTest, LimitSessionsPerUid) {
  std::vector<std::unique_ptr<MockConsumer>> consumers;

  auto start_new_session = [&](uid_t uid) -> MockConsumer* {
    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(128);
    trace_config.set_duration_ms(0);  // Unlimited.
    consumers.emplace_back(CreateMockConsumer());
    consumers.back()->Connect(svc.get(), uid);
    consumers.back()->EnableTracing(trace_config);
    return &*consumers.back();
  };

  const int kMaxConcurrentTracingSessionsPerUid = 5;
  const int kUids = 2;

  // Create a bunch of legit sessions (2 uids * 5 sessions).
  for (int i = 0; i < kMaxConcurrentTracingSessionsPerUid * kUids; i++) {
    start_new_session(/*uid=*/i % kUids);
  }

  // Any other session now should fail for the two uids.
  for (int i = 0; i <= kUids; i++) {
    auto* consumer = start_new_session(/*uid=*/i % kUids);
    auto on_fail = task_runner.CreateCheckpoint("uid_" + std::to_string(i));
    EXPECT_CALL(*consumer, OnTracingDisabled()).WillOnce(Invoke(on_fail));
  }

  // Wait for failure (only after both attempts).
  for (int i = 0; i <= kUids; i++) {
    task_runner.RunUntilCheckpoint("uid_" + std::to_string(i));
  }

  // The destruction of |consumers| will tear down and stop the good sessions.
}

TEST_F(TracingServiceImplTest, ProducerProvidedSMB) {
  static constexpr size_t kShmSizeBytes = 1024 * 1024;
  static constexpr size_t kShmPageSizeBytes = 4 * 1024;

  std::unique_ptr<MockProducer> producer = CreateMockProducer();

  TestSharedMemory::Factory factory;
  auto shm = factory.CreateSharedMemory(kShmSizeBytes);
  SharedMemory* shm_raw = shm.get();

  // Service should adopt the SMB provided by the producer.
  producer->Connect(svc.get(), "mock_producer", /*uid=*/42,
                    /*shared_memory_size_hint_bytes=*/0, kShmPageSizeBytes,
                    std::move(shm));
  EXPECT_TRUE(producer->endpoint()->IsShmemProvidedByProducer());
  EXPECT_NE(producer->endpoint()->MaybeSharedMemoryArbiter(), nullptr);
  EXPECT_EQ(producer->endpoint()->shared_memory(), shm_raw);

  producer->WaitForTracingSetup();
  producer->RegisterDataSource("data_source");

  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Verify that data written to the producer-provided SMB ends up in trace
  // buffer correctly.
  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  auto flush_request = consumer->Flush();
  producer->WaitForFlush(writer.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(consumer->ReadBuffers(),
              Contains(Property(
                  &protos::gen::TracePacket::for_testing,
                  Property(&protos::gen::TestEvent::str, Eq("payload")))));
}

TEST_F(TracingServiceImplTest, ProducerProvidedSMBInvalidSizes) {
  static constexpr size_t kShmSizeBytes = 1024 * 1024;
  static constexpr size_t kShmPageSizeBytes = 20 * 1024;

  std::unique_ptr<MockProducer> producer = CreateMockProducer();

  TestSharedMemory::Factory factory;
  auto shm = factory.CreateSharedMemory(kShmSizeBytes);

  // Service should not adopt the SMB provided by the producer, because the SMB
  // size isn't a multiple of the page size.
  producer->Connect(svc.get(), "mock_producer", /*uid=*/42,
                    /*shared_memory_size_hint_bytes=*/0, kShmPageSizeBytes,
                    std::move(shm));
  EXPECT_FALSE(producer->endpoint()->IsShmemProvidedByProducer());
  EXPECT_EQ(producer->endpoint()->shared_memory(), nullptr);
}

}  // namespace perfetto
