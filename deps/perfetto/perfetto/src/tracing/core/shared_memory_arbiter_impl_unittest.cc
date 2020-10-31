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

#include "src/tracing/core/shared_memory_arbiter_impl.h"

#include <bitset>
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/base/test/gtest_test_suite.h"
#include "src/base/test/test_task_runner.h"
#include "src/tracing/core/patch_list.h"
#include "src/tracing/test/aligned_buffer_test.h"
#include "src/tracing/test/fake_producer_endpoint.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

using testing::Invoke;
using testing::_;

class MockProducerEndpoint : public TracingService::ProducerEndpoint {
 public:
  void RegisterDataSource(const DataSourceDescriptor&) override {}
  void UnregisterDataSource(const std::string&) override {}
  void NotifyFlushComplete(FlushRequestID) override {}
  void NotifyDataSourceStarted(DataSourceInstanceID) override {}
  void NotifyDataSourceStopped(DataSourceInstanceID) override {}
  void ActivateTriggers(const std::vector<std::string>&) {}
  void Sync(std::function<void()>) override {}
  SharedMemory* shared_memory() const override { return nullptr; }
  size_t shared_buffer_page_size_kb() const override { return 0; }
  std::unique_ptr<TraceWriter> CreateTraceWriter(
      BufferID,
      BufferExhaustedPolicy) override {
    return nullptr;
  }
  SharedMemoryArbiter* MaybeSharedMemoryArbiter() override { return nullptr; }
  bool IsShmemProvidedByProducer() const override { return false; }

  MOCK_METHOD2(CommitData, void(const CommitDataRequest&, CommitDataCallback));
  MOCK_METHOD2(RegisterTraceWriter, void(uint32_t, uint32_t));
  MOCK_METHOD1(UnregisterTraceWriter, void(uint32_t));
};

class SharedMemoryArbiterImplTest : public AlignedBufferTest {
 public:
  void SetUp() override {
    AlignedBufferTest::SetUp();
    task_runner_.reset(new base::TestTaskRunner());
    arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                               &mock_producer_endpoint_,
                                               task_runner_.get()));
  }

  bool IsArbiterFullyBound() { return arbiter_->fully_bound_; }

  void TearDown() override {
    arbiter_.reset();
    task_runner_.reset();
  }

  std::unique_ptr<base::TestTaskRunner> task_runner_;
  std::unique_ptr<SharedMemoryArbiterImpl> arbiter_;
  MockProducerEndpoint mock_producer_endpoint_;
  std::function<void(const std::vector<uint32_t>&)> on_pages_complete_;
};

size_t const kPageSizes[] = {4096, 65536};
INSTANTIATE_TEST_SUITE_P(PageSize,
                         SharedMemoryArbiterImplTest,
                         ::testing::ValuesIn(kPageSizes));

// The buffer has 14 pages (kNumPages), each will be partitioned in 14 chunks.
// The test requests 30 chunks (2 full pages + 2 chunks from a 3rd page) and
// releases them in different batches. It tests the consistency of the batches
// and the releasing order.
TEST_P(SharedMemoryArbiterImplTest, GetAndReturnChunks) {
  SharedMemoryArbiterImpl::set_default_layout_for_testing(
      SharedMemoryABI::PageLayout::kPageDiv14);
  static constexpr size_t kTotChunks = kNumPages * 14;
  SharedMemoryABI::Chunk chunks[kTotChunks];
  for (size_t i = 0; i < 14 * 2 + 2; i++) {
    chunks[i] = arbiter_->GetNewChunk({}, BufferExhaustedPolicy::kStall);
    ASSERT_TRUE(chunks[i].is_valid());
  }

  // Finally return the first 28 chunks (full 2 pages) and only the 2nd chunk of
  // the 2rd page. Chunks are release in interleaved order: 1,0,3,2,5,4,7,6.
  // Check that the notification callback is posted and order is consistent.
  auto on_commit_1 = task_runner_->CreateCheckpoint("on_commit_1");
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([on_commit_1](const CommitDataRequest& req,
                                     MockProducerEndpoint::CommitDataCallback) {
        ASSERT_EQ(14 * 2 + 1, req.chunks_to_move_size());
        for (size_t i = 0; i < 14 * 2; i++) {
          ASSERT_EQ(i / 14, req.chunks_to_move()[i].page());
          ASSERT_EQ((i % 14) ^ 1, req.chunks_to_move()[i].chunk());
          ASSERT_EQ(i % 5 + 1, req.chunks_to_move()[i].target_buffer());
        }
        ASSERT_EQ(2u, req.chunks_to_move()[28].page());
        ASSERT_EQ(1u, req.chunks_to_move()[28].chunk());
        ASSERT_EQ(42u, req.chunks_to_move()[28].target_buffer());
        on_commit_1();
      }));
  PatchList ignored;
  for (size_t i = 0; i < 14 * 2; i++) {
    arbiter_->ReturnCompletedChunk(std::move(chunks[i ^ 1]), i % 5 + 1,
                                   &ignored);
  }
  arbiter_->ReturnCompletedChunk(std::move(chunks[29]), 42, &ignored);
  task_runner_->RunUntilCheckpoint("on_commit_1");

  // Then release the 1st chunk of the 3rd page, and check that we get a
  // notification for that as well.
  auto on_commit_2 = task_runner_->CreateCheckpoint("on_commit_2");
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([on_commit_2](const CommitDataRequest& req,
                                     MockProducerEndpoint::CommitDataCallback) {
        ASSERT_EQ(1, req.chunks_to_move_size());
        ASSERT_EQ(2u, req.chunks_to_move()[0].page());
        ASSERT_EQ(0u, req.chunks_to_move()[0].chunk());
        ASSERT_EQ(43u, req.chunks_to_move()[0].target_buffer());
        on_commit_2();
      }));
  arbiter_->ReturnCompletedChunk(std::move(chunks[28]), 43, &ignored);
  task_runner_->RunUntilCheckpoint("on_commit_2");
}

// Helper for verifying trace writer id allocations.
class TraceWriterIdChecker : public FakeProducerEndpoint {
 public:
  TraceWriterIdChecker(std::function<void()> checkpoint)
      : checkpoint_(std::move(checkpoint)) {}

  void RegisterTraceWriter(uint32_t id, uint32_t) override {
    EXPECT_GT(id, 0u);
    EXPECT_LE(id, kMaxWriterID);
    if (id > 0 && id <= kMaxWriterID) {
      registered_ids_.set(id - 1);
    }
  }

  void UnregisterTraceWriter(uint32_t id) override {
    if (++unregister_calls_ == kMaxWriterID)
      checkpoint_();

    EXPECT_GT(id, 0u);
    EXPECT_LE(id, kMaxWriterID);
    if (id > 0 && id <= kMaxWriterID) {
      unregistered_ids_.set(id - 1);
    }
  }

  // bit N corresponds to id N+1
  std::bitset<kMaxWriterID> registered_ids_;
  std::bitset<kMaxWriterID> unregistered_ids_;

  int unregister_calls_ = 0;

 private:
  std::function<void()> checkpoint_;
};

// Check that we can actually create up to kMaxWriterID TraceWriter(s).
TEST_P(SharedMemoryArbiterImplTest, WriterIDsAllocation) {
  auto checkpoint = task_runner_->CreateCheckpoint("last_unregistered");

  TraceWriterIdChecker id_checking_endpoint(checkpoint);
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             &id_checking_endpoint,
                                             task_runner_.get()));
  {
    std::map<WriterID, std::unique_ptr<TraceWriter>> writers;

    for (size_t i = 0; i < kMaxWriterID; i++) {
      std::unique_ptr<TraceWriter> writer = arbiter_->CreateTraceWriter(1);
      ASSERT_TRUE(writer);
      WriterID writer_id = writer->writer_id();
      ASSERT_TRUE(writers.emplace(writer_id, std::move(writer)).second);
    }

    // A further call should return a null impl of trace writer as we exhausted
    // writer IDs.
    ASSERT_EQ(arbiter_->CreateTraceWriter(1)->writer_id(), 0);
  }

  // This should run the Register/UnregisterTraceWriter tasks enqueued by the
  // memory arbiter.
  task_runner_->RunUntilCheckpoint("last_unregistered", 15000);

  EXPECT_TRUE(id_checking_endpoint.registered_ids_.all());
  EXPECT_TRUE(id_checking_endpoint.unregistered_ids_.all());
}

// Verify that getting a new chunk doesn't stall when kDrop policy is chosen.
TEST_P(SharedMemoryArbiterImplTest, BufferExhaustedPolicyDrop) {
  // Grab all chunks in the SMB.
  SharedMemoryArbiterImpl::set_default_layout_for_testing(
      SharedMemoryABI::PageLayout::kPageDiv1);
  static constexpr size_t kTotChunks = kNumPages;
  SharedMemoryABI::Chunk chunks[kTotChunks];
  for (size_t i = 0; i < kTotChunks; i++) {
    chunks[i] = arbiter_->GetNewChunk({}, BufferExhaustedPolicy::kDrop);
    ASSERT_TRUE(chunks[i].is_valid());
  }

  // SMB is exhausted, thus GetNewChunk() should return an invalid chunk. In
  // kStall mode, this would stall.
  SharedMemoryABI::Chunk invalid_chunk =
      arbiter_->GetNewChunk({}, BufferExhaustedPolicy::kDrop);
  ASSERT_FALSE(invalid_chunk.is_valid());

  // Returning the chunk is not enough to be able to reacquire it.
  PatchList ignored;
  arbiter_->ReturnCompletedChunk(std::move(chunks[0]), 1, &ignored);

  invalid_chunk = arbiter_->GetNewChunk({}, BufferExhaustedPolicy::kDrop);
  ASSERT_FALSE(invalid_chunk.is_valid());

  // After releasing the chunk as free, we can reacquire it.
  chunks[0] =
      arbiter_->shmem_abi_for_testing()->TryAcquireChunkForReading(0, 0);
  ASSERT_TRUE(chunks[0].is_valid());
  arbiter_->shmem_abi_for_testing()->ReleaseChunkAsFree(std::move(chunks[0]));

  chunks[0] = arbiter_->GetNewChunk({}, BufferExhaustedPolicy::kDrop);
  ASSERT_TRUE(chunks[0].is_valid());
}

TEST_P(SharedMemoryArbiterImplTest, CreateUnboundAndBind) {
  auto checkpoint_writer = task_runner_->CreateCheckpoint("writer_registered");
  auto checkpoint_flush = task_runner_->CreateCheckpoint("flush_completed");

  // Create an unbound arbiter and bind immediately.
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             nullptr, nullptr));
  arbiter_->BindToProducerEndpoint(&mock_producer_endpoint_,
                                   task_runner_.get());
  EXPECT_TRUE(IsArbiterFullyBound());

  // Trace writer should be registered in a non-delayed task.
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, 1))
      .WillOnce(testing::InvokeWithoutArgs(checkpoint_writer));
  std::unique_ptr<TraceWriter> writer = arbiter_->CreateTraceWriter(1);
  task_runner_->RunUntilCheckpoint("writer_registered", 5000);

  // Commits/flushes should be sent right away.
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(testing::InvokeArgument<1>());
  writer->Flush(checkpoint_flush);
  task_runner_->RunUntilCheckpoint("flush_completed", 5000);
}

TEST_P(SharedMemoryArbiterImplTest, StartupTracing) {
  constexpr uint16_t kTargetBufferReservationId1 = 1;
  constexpr uint16_t kTargetBufferReservationId2 = 2;

  // Create an unbound arbiter and a startup writer.
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             nullptr, nullptr));
  std::unique_ptr<TraceWriter> writer =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId1);

  // Write two packet while unbound and flush the chunk after each packet. The
  // writer will return the chunk to the arbiter and grab a new chunk for the
  // second packet. The flush should only add the chunk into the queued commit
  // request.
  for (int i = 0; i < 2; i++) {
    {
      auto packet = writer->NewTracePacket();
      packet->set_for_testing()->set_str("foo");
    }
    writer->Flush();
  }

  // Bind to producer endpoint. This should not register the trace writer yet,
  // because it's buffer reservation is still unbound.
  arbiter_->BindToProducerEndpoint(&mock_producer_endpoint_,
                                   task_runner_.get());
  EXPECT_FALSE(IsArbiterFullyBound());

  // Write another packet into another chunk and queue it.
  {
    auto packet = writer->NewTracePacket();
    packet->set_for_testing()->set_str("foo");
  }
  bool flush_completed = false;
  writer->Flush([&flush_completed] { flush_completed = true; });

  // Bind the buffer reservation to a buffer. Trace writer should be registered
  // and queued commits flushed.
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, 42));
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([](const CommitDataRequest& req,
                          MockProducerEndpoint::CommitDataCallback callback) {
        ASSERT_EQ(3, req.chunks_to_move_size());
        EXPECT_EQ(42u, req.chunks_to_move()[0].target_buffer());
        EXPECT_EQ(42u, req.chunks_to_move()[1].target_buffer());
        EXPECT_EQ(42u, req.chunks_to_move()[2].target_buffer());
        callback();
      }));

  arbiter_->BindStartupTargetBuffer(kTargetBufferReservationId1, 42);
  EXPECT_TRUE(IsArbiterFullyBound());

  testing::Mock::VerifyAndClearExpectations(&mock_producer_endpoint_);
  EXPECT_TRUE(flush_completed);

  // Creating a new startup writer for the same buffer posts an immediate task
  // to register it.
  auto checkpoint_register1b =
      task_runner_->CreateCheckpoint("writer1b_registered");
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, 42))
      .WillOnce(testing::InvokeWithoutArgs(checkpoint_register1b));
  std::unique_ptr<TraceWriter> writer1b =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId1);
  task_runner_->RunUntilCheckpoint("writer1b_registered", 5000);

  // And a commit on this new writer should be flushed to the right buffer, too.
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([](const CommitDataRequest& req,
                          MockProducerEndpoint::CommitDataCallback callback) {
        ASSERT_EQ(1, req.chunks_to_move_size());
        EXPECT_EQ(42u, req.chunks_to_move()[0].target_buffer());
        callback();
      }));
  {
    auto packet = writer1b->NewTracePacket();
    packet->set_for_testing()->set_str("foo");
  }
  flush_completed = false;
  writer1b->Flush([&flush_completed] { flush_completed = true; });

  testing::Mock::VerifyAndClearExpectations(&mock_producer_endpoint_);
  EXPECT_TRUE(flush_completed);

  // Create another startup writer for another target buffer, which puts the
  // arbiter back into unbound state.
  std::unique_ptr<TraceWriter> writer2 =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId2);
  EXPECT_FALSE(IsArbiterFullyBound());

  // Write a chunk into both writers. Both should be queued up into the next
  // commit request.
  {
    auto packet = writer->NewTracePacket();
    packet->set_for_testing()->set_str("foo");
  }
  writer->Flush();
  {
    auto packet = writer2->NewTracePacket();
    packet->set_for_testing()->set_str("bar");
  }
  flush_completed = false;
  writer2->Flush([&flush_completed] { flush_completed = true; });

  // Destroy the first trace writer, which should cause the arbiter to post a
  // task to unregister it.
  auto checkpoint_writer =
      task_runner_->CreateCheckpoint("writer_unregistered");
  EXPECT_CALL(mock_producer_endpoint_,
              UnregisterTraceWriter(writer->writer_id()))
      .WillOnce(testing::InvokeWithoutArgs(checkpoint_writer));
  writer.reset();
  task_runner_->RunUntilCheckpoint("writer_unregistered", 5000);

  // Bind the second buffer reservation to a buffer. Second trace writer should
  // be registered and queued commits flushed.
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, 23));
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([](const CommitDataRequest& req,
                          MockProducerEndpoint::CommitDataCallback callback) {
        ASSERT_EQ(2, req.chunks_to_move_size());
        EXPECT_EQ(42u, req.chunks_to_move()[0].target_buffer());
        EXPECT_EQ(23u, req.chunks_to_move()[1].target_buffer());
        callback();
      }));

  arbiter_->BindStartupTargetBuffer(kTargetBufferReservationId2, 23);
  EXPECT_TRUE(IsArbiterFullyBound());

  testing::Mock::VerifyAndClearExpectations(&mock_producer_endpoint_);
  EXPECT_TRUE(flush_completed);
}

TEST_P(SharedMemoryArbiterImplTest, AbortStartupTracingForReservation) {
  constexpr uint16_t kTargetBufferReservationId1 = 1;
  constexpr uint16_t kTargetBufferReservationId2 = 2;

  // Create an unbound arbiter and a startup writer.
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             nullptr, nullptr));
  SharedMemoryABI* shmem_abi = arbiter_->shmem_abi_for_testing();
  std::unique_ptr<TraceWriter> writer =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId1);
  std::unique_ptr<TraceWriter> writer2 =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId1);

  // Write two packet while unbound and flush the chunk after each packet. The
  // writer will return the chunk to the arbiter and grab a new chunk for the
  // second packet. The flush should only add the chunk into the queued commit
  // request.
  for (int i = 0; i < 2; i++) {
    {
      auto packet = writer->NewTracePacket();
      packet->set_for_testing()->set_str("foo");
    }
    writer->Flush();
  }

  // Abort the first session. This should clear resolve the two chunks committed
  // up to this point to an invalid target buffer (ID 0). They will remain
  // buffered until bound to an endpoint.
  arbiter_->AbortStartupTracingForReservation(kTargetBufferReservationId1);

  // Destroy a writer that was created before the abort. This should not cause
  // crashes.
  writer2.reset();

  // Bind to producer endpoint. The trace writer should not be registered as its
  // target buffer is invalid. Since no startup sessions are active anymore, the
  // arbiter should be fully bound. The commit data request is flushed.
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, _)).Times(0);
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([shmem_abi](const CommitDataRequest& req,
                                   MockProducerEndpoint::CommitDataCallback) {
        ASSERT_EQ(2, req.chunks_to_move_size());
        for (size_t i = 0; i < 2; i++) {
          EXPECT_EQ(0u, req.chunks_to_move()[i].target_buffer());
          SharedMemoryABI::Chunk chunk = shmem_abi->TryAcquireChunkForReading(
              req.chunks_to_move()[i].page(), req.chunks_to_move()[i].chunk());
          shmem_abi->ReleaseChunkAsFree(std::move(chunk));
        }
      }));
  arbiter_->BindToProducerEndpoint(&mock_producer_endpoint_,
                                   task_runner_.get());
  EXPECT_TRUE(IsArbiterFullyBound());

  // SMB should be free again, as no writer holds on to any chunk anymore.
  for (size_t i = 0; i < shmem_abi->num_pages(); i++)
    EXPECT_TRUE(shmem_abi->is_page_free(i));

  // Write another packet into another chunk and commit it. It should be sent
  // to the arbiter with invalid target buffer (ID 0).
  {
    auto packet = writer->NewTracePacket();
    packet->set_for_testing()->set_str("foo");
  }
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([shmem_abi](
                           const CommitDataRequest& req,
                           MockProducerEndpoint::CommitDataCallback callback) {
        ASSERT_EQ(1, req.chunks_to_move_size());
        EXPECT_EQ(0u, req.chunks_to_move()[0].target_buffer());
        SharedMemoryABI::Chunk chunk = shmem_abi->TryAcquireChunkForReading(
            req.chunks_to_move()[0].page(), req.chunks_to_move()[0].chunk());
        shmem_abi->ReleaseChunkAsFree(std::move(chunk));
        callback();
      }));
  bool flush_completed = false;
  writer->Flush([&flush_completed] { flush_completed = true; });
  EXPECT_TRUE(flush_completed);

  // Creating a new startup writer for the same buffer does not cause it to
  // register.
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, _)).Times(0);
  std::unique_ptr<TraceWriter> writer1b =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId1);

  // And a commit on this new writer should again be flushed to the invalid
  // target buffer.
  {
    auto packet = writer1b->NewTracePacket();
    packet->set_for_testing()->set_str("foo");
  }
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([shmem_abi](
                           const CommitDataRequest& req,
                           MockProducerEndpoint::CommitDataCallback callback) {
        ASSERT_EQ(1, req.chunks_to_move_size());
        EXPECT_EQ(0u, req.chunks_to_move()[0].target_buffer());
        SharedMemoryABI::Chunk chunk = shmem_abi->TryAcquireChunkForReading(
            req.chunks_to_move()[0].page(), req.chunks_to_move()[0].chunk());
        shmem_abi->ReleaseChunkAsFree(std::move(chunk));
        callback();
      }));
  flush_completed = false;
  writer1b->Flush([&flush_completed] { flush_completed = true; });
  EXPECT_TRUE(flush_completed);

  // Create another startup writer for another target buffer, which puts the
  // arbiter back into unbound state.
  std::unique_ptr<TraceWriter> writer3 =
      arbiter_->CreateStartupTraceWriter(kTargetBufferReservationId2);
  EXPECT_FALSE(IsArbiterFullyBound());

  // Write a chunk into both writers. Both should be queued up into the next
  // commit request.
  {
    auto packet = writer->NewTracePacket();
    packet->set_for_testing()->set_str("foo");
  }
  writer->Flush();
  {
    auto packet = writer3->NewTracePacket();
    packet->set_for_testing()->set_str("bar");
  }
  flush_completed = false;
  writer3->Flush([&flush_completed] { flush_completed = true; });

  // Destroy the first trace writer, which should cause the arbiter to post a
  // task to unregister it.
  auto checkpoint_writer =
      task_runner_->CreateCheckpoint("writer_unregistered");
  EXPECT_CALL(mock_producer_endpoint_,
              UnregisterTraceWriter(writer->writer_id()))
      .WillOnce(testing::InvokeWithoutArgs(checkpoint_writer));
  writer.reset();
  task_runner_->RunUntilCheckpoint("writer_unregistered", 5000);

  // Abort the second session. Its commits should now also be associated with
  // target buffer 0, and both writers' commits flushed.
  EXPECT_CALL(mock_producer_endpoint_, RegisterTraceWriter(_, _)).Times(0);
  EXPECT_CALL(mock_producer_endpoint_, CommitData(_, _))
      .WillOnce(Invoke([shmem_abi](
                           const CommitDataRequest& req,
                           MockProducerEndpoint::CommitDataCallback callback) {
        ASSERT_EQ(2, req.chunks_to_move_size());
        for (size_t i = 0; i < 2; i++) {
          EXPECT_EQ(0u, req.chunks_to_move()[i].target_buffer());
          SharedMemoryABI::Chunk chunk = shmem_abi->TryAcquireChunkForReading(
              req.chunks_to_move()[i].page(), req.chunks_to_move()[i].chunk());
          shmem_abi->ReleaseChunkAsFree(std::move(chunk));
        }
        callback();
      }));

  arbiter_->AbortStartupTracingForReservation(kTargetBufferReservationId2);
  EXPECT_TRUE(IsArbiterFullyBound());
  EXPECT_TRUE(flush_completed);

  // SMB should be free again, as no writer holds on to any chunk anymore.
  for (size_t i = 0; i < shmem_abi->num_pages(); i++)
    EXPECT_TRUE(shmem_abi->is_page_free(i));
}

// TODO(primiano): add multi-threaded tests.

}  // namespace perfetto
