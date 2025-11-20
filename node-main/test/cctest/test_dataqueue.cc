#include <dataqueue/queue.h>
#include <gtest/gtest.h>
#include <node_bob-inl.h>
#include <util-inl.h>
#include <v8.h>
#include <memory>
#include <vector>

using node::DataQueue;
using v8::ArrayBuffer;
using v8::BackingStore;

TEST(DataQueue, InMemoryEntry) {
  char buffer[] = "hello world";
  size_t len = strlen(buffer);

  std::shared_ptr<BackingStore> store = ArrayBuffer::NewBackingStore(
      &buffer, len, [](void*, size_t, void*) {}, nullptr);

  // We can create an InMemoryEntry from a v8::BackingStore.
  std::unique_ptr<DataQueue::Entry> entry =
      DataQueue::CreateInMemoryEntryFromBackingStore(store, 0, len);

  // The entry is idempotent.
  CHECK(entry->is_idempotent());

  // The size is known.
  CHECK_EQ(entry->size().value(), len);

  // We can slice it.
  // slice: "llo world"
  std::unique_ptr<DataQueue::Entry> slice1 = entry->slice(2);

  // The slice is idempotent.
  CHECK(slice1->is_idempotent());

  // The slice size is known.
  CHECK_EQ(slice1->size().value(), len - 2);

  // We can slice the slice with a length.
  // slice: "o w"
  uint64_t end = 5;
  std::unique_ptr<DataQueue::Entry> slice2 = slice1->slice(2, end);

  // That slice is idempotent.
  CHECK(slice2->is_idempotent());

  // That slice size is known.
  CHECK_EQ(slice2->size().value(), 3);

  // The slice end can extend beyond the actual size and will be adjusted.
  // slice: "orld"
  end = 100;
  std::unique_ptr<DataQueue::Entry> slice3 = slice1->slice(5, end);
  CHECK_NOT_NULL(slice3);

  // The slice size is known.
  CHECK_EQ(slice3->size().value(), 4);

  // If the slice start is greater than the length, we get a zero length slice.
  std::unique_ptr<DataQueue::Entry> slice4 = entry->slice(100);
  CHECK_NOT_NULL(slice4);
  CHECK_EQ(slice4->size().value(), 0);

  // If the slice end is less than the start, we get a zero length slice.
  end = 1;
  std::unique_ptr<DataQueue::Entry> slice5 = entry->slice(2, end);
  CHECK_NOT_NULL(slice5);
  CHECK_EQ(slice5->size().value(), 0);

  // If the slice end equal to the start, we get a zero length slice.
  end = 2;
  std::unique_ptr<DataQueue::Entry> slice6 = entry->slice(2, end);
  CHECK_NOT_NULL(slice6);
  CHECK_EQ(slice6->size().value(), 0);

  // The shared_ptr for the BackingStore should show only 5 uses because
  // the zero-length slices do not maintain a reference to it.
  CHECK_EQ(store.use_count(), 5);
}

TEST(DataQueue, IdempotentDataQueue) {
  char buffer1[] = "hello world";
  char buffer2[] = "what fun this is";
  char buffer3[] = "not added";
  size_t len1 = strlen(buffer1);
  size_t len2 = strlen(buffer2);
  size_t len3 = strlen(buffer3);

  std::shared_ptr<BackingStore> store1 = ArrayBuffer::NewBackingStore(
      &buffer1, len1, [](void*, size_t, void*) {}, nullptr);

  std::shared_ptr<BackingStore> store2 = ArrayBuffer::NewBackingStore(
      &buffer2, len2, [](void*, size_t, void*) {}, nullptr);

  std::vector<std::unique_ptr<DataQueue::Entry>> list;
  list.push_back(
      DataQueue::CreateInMemoryEntryFromBackingStore(store1, 0, len1));
  list.push_back(
      DataQueue::CreateInMemoryEntryFromBackingStore(store2, 0, len2));

  // We can create an idempotent DataQueue from a list of entries.
  std::shared_ptr<DataQueue> data_queue =
      DataQueue::CreateIdempotent(std::move(list));

  CHECK_NOT_NULL(data_queue);

  // The data_queue is idempotent.
  CHECK(data_queue->is_idempotent());

  // The data_queue is capped.
  CHECK(data_queue->is_capped());

  // maybeCapRemaining() returns zero.
  CHECK_EQ(data_queue->maybeCapRemaining().value(), 0);

  // Calling cap() is a nonop but doesn't crash or error.
  data_queue->cap();
  data_queue->cap(100);

  // maybeCapRemaining() still returns zero.
  CHECK_EQ(data_queue->maybeCapRemaining().value(), 0);

  // The size is known to be the sum of the in memory-entries.
  CHECK_EQ(data_queue->size().value(), len1 + len2);

  std::shared_ptr<BackingStore> store3 = ArrayBuffer::NewBackingStore(
      &buffer3, len3, [](void*, size_t, void*) {}, nullptr);

  // Trying to append a new entry does not crash, but returns std::nullopt.
  CHECK(!data_queue
             ->append(DataQueue::CreateInMemoryEntryFromBackingStore(
                 store3, 0, len3))
             .has_value());

  // The size has not changed after the append.
  CHECK_EQ(data_queue->size().value(), len1 + len2);

  // We can acquire multiple readers from the data_queue.
  std::shared_ptr<DataQueue::Reader> reader1 = data_queue->get_reader();
  std::shared_ptr<DataQueue::Reader> reader2 = data_queue->get_reader();

  CHECK_NOT_NULL(reader1);
  CHECK_NOT_NULL(reader2);

  const auto testRead = [&](auto& reader) {
    // We can read the expected data from reader. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    bool waitingForPull = true;

    // The first read produces buffer1
    int status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len1);
          CHECK_EQ(memcmp(vecs[0].base, buffer1, len1), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // We can read the expected data from reader1. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    waitingForPull = true;

    // The second read should have status CONTINUE but no buffer.
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The third read produces buffer2, and should be the end.
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len2);
          CHECK_EQ(memcmp(vecs[0].base, buffer2, len2), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_EOS);
          CHECK_EQ(count, 0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_EOS);
  };

  // Both reader1 and reader2 should pass identical tests.
  testRead(reader1);
  testRead(reader2);

  // We can slice the data queue.
  std::shared_ptr<DataQueue> slice1 = data_queue->slice(2);

  CHECK_NOT_NULL(slice1);

  // The slice is idempotent.
  CHECK(slice1->is_idempotent());

  // And capped.
  CHECK(slice1->is_capped());

  // The size is two-bytes less than the original.
  CHECK_EQ(slice1->size().value(), data_queue->size().value() - 2);

  const auto testSlice = [&](auto& reader) {
    // We can read the expected data from reader. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    bool waitingForPull = true;

    // The first read produces a slice of buffer1
    int status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len1 - 2);
          CHECK_EQ(memcmp(vecs[0].base, buffer1 + 2, len1 - 2), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // We can read the expected data from reader1. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    waitingForPull = true;

    // The second read should have status CONTINUE but no buffer.
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The second read produces buffer2, and should be the end.
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len2);
          CHECK_EQ(memcmp(vecs[0].base, buffer2, len2), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The third read produces EOS
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_EOS);
          CHECK_EQ(count, 0);
          CHECK_NULL(vecs);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_EOS);
  };

  // We can read the expected slice data.
  std::shared_ptr<DataQueue::Reader> reader3 = slice1->get_reader();
  testSlice(reader3);

  // We can slice correctly across boundaries.
  uint64_t end = 20;
  std::shared_ptr<DataQueue> slice2 = data_queue->slice(5, end);

  // The size is known.
  CHECK_EQ(slice2->size().value(), 15);

  const auto testSlice2 = [&](auto& reader) {
    // We can read the expected data from reader. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    bool waitingForPull = true;

    // The first read produces a slice of buffer1
    int status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;

          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len1 - 5);
          CHECK_EQ(memcmp(vecs[0].base, buffer1 + 5, len1 - 5), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // We can read the expected data from reader1. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    waitingForPull = true;

    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The next read produces buffer2, and should be the end.
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len2 - 7);
          CHECK_EQ(memcmp(vecs[0].base, buffer2, len2 - 7), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The next read produces EOS
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_EOS);
          CHECK_EQ(count, 0);
          CHECK_NULL(vecs);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_EOS);
  };

  // We can read the expected slice data.
  std::shared_ptr<DataQueue::Reader> reader4 = slice2->get_reader();
  testSlice2(reader4);
}

TEST(DataQueue, NonIdempotentDataQueue) {
  char buffer1[] = "hello world";
  char buffer2[] = "what fun this is";
  char buffer3[] = "not added";
  size_t len1 = strlen(buffer1);
  size_t len2 = strlen(buffer2);
  size_t len3 = strlen(buffer3);

  std::shared_ptr<BackingStore> store1 = ArrayBuffer::NewBackingStore(
      &buffer1, len1, [](void*, size_t, void*) {}, nullptr);

  std::shared_ptr<BackingStore> store2 = ArrayBuffer::NewBackingStore(
      &buffer2, len2, [](void*, size_t, void*) {}, nullptr);

  std::shared_ptr<BackingStore> store3 = ArrayBuffer::NewBackingStore(
      &buffer3, len3, [](void*, size_t, void*) {}, nullptr);

  // We can create an non-idempotent DataQueue from a list of entries.
  std::shared_ptr<DataQueue> data_queue = DataQueue::Create();

  CHECK(!data_queue->is_idempotent());
  CHECK_EQ(data_queue->size().value(), 0);

  data_queue->append(
      DataQueue::CreateInMemoryEntryFromBackingStore(store1, 0, len1));
  CHECK_EQ(data_queue->size().value(), len1);

  data_queue->append(
      DataQueue::CreateInMemoryEntryFromBackingStore(store2, 0, len2));
  CHECK_EQ(data_queue->size().value(), len1 + len2);

  CHECK(!data_queue->is_capped());
  CHECK(!data_queue->maybeCapRemaining().has_value());

  data_queue->cap(100);
  CHECK(data_queue->is_capped());
  CHECK_EQ(data_queue->maybeCapRemaining().value(), 100 - (len1 + len2));

  data_queue->cap(101);
  CHECK(data_queue->is_capped());
  CHECK_EQ(data_queue->maybeCapRemaining().value(), 100 - (len1 + len2));

  data_queue->cap();
  CHECK(data_queue->is_capped());
  CHECK_EQ(data_queue->maybeCapRemaining().value(), 0);

  // We can't add any more because the data queue is capped.
  CHECK_EQ(data_queue
               ->append(DataQueue::CreateInMemoryEntryFromBackingStore(
                   store3, 0, len3))
               .value(),
           false);

  // We cannot slice a non-idempotent data queue
  std::shared_ptr<DataQueue> slice1 = data_queue->slice(2);
  CHECK_NULL(slice1);

  // We can acquire only a single reader for a non-idempotent data queue
  std::shared_ptr<DataQueue::Reader> reader1 = data_queue->get_reader();
  std::shared_ptr<DataQueue::Reader> reader2 = data_queue->get_reader();

  CHECK_NOT_NULL(reader1);
  CHECK_NULL(reader2);

  const auto testRead = [&](auto& reader) {
    // We can read the expected data from reader. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    bool waitingForPull = true;

    // The first read produces buffer1
    int status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len1);
          CHECK_EQ(memcmp(vecs[0].base, buffer1, len1), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // We can read the expected data from reader1. Because the entries are
    // InMemoryEntry instances, reads will be fully synchronous here.
    waitingForPull = true;

    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The next read produces buffer2, and should be the end.
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_CONTINUE);
          CHECK_EQ(count, 1);
          CHECK_EQ(vecs[0].len, len2);
          CHECK_EQ(memcmp(vecs[0].base, buffer2, len2), 0);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_CONTINUE);

    // The next read produces EOS
    status = reader->Pull(
        [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
          waitingForPull = false;
          CHECK_EQ(status, node::bob::STATUS_EOS);
          CHECK_EQ(count, 0);
          CHECK_NULL(vecs);
          std::move(done)(0);
        },
        node::bob::OPTIONS_SYNC,
        nullptr,
        0,
        node::bob::kMaxCountHint);

    CHECK(!waitingForPull);
    CHECK_EQ(status, node::bob::STATUS_EOS);
  };

  // Reading produces the expected results.
  testRead(reader1);

  // We still cannot acquire another reader.
  std::shared_ptr<DataQueue::Reader> reader3 = data_queue->get_reader();
  CHECK_NULL(reader3);

  CHECK_NOT_NULL(data_queue);
}

TEST(DataQueue, DataQueueEntry) {
  char buffer1[] = "hello world";
  char buffer2[] = "what fun this is";
  size_t len1 = strlen(buffer1);
  size_t len2 = strlen(buffer2);

  std::shared_ptr<BackingStore> store1 = ArrayBuffer::NewBackingStore(
      &buffer1, len1, [](void*, size_t, void*) {}, nullptr);

  std::shared_ptr<BackingStore> store2 = ArrayBuffer::NewBackingStore(
      &buffer2, len2, [](void*, size_t, void*) {}, nullptr);

  std::vector<std::unique_ptr<DataQueue::Entry>> list;
  list.push_back(
      DataQueue::CreateInMemoryEntryFromBackingStore(store1, 0, len1));
  list.push_back(
      DataQueue::CreateInMemoryEntryFromBackingStore(store2, 0, len2));

  // We can create an idempotent DataQueue from a list of entries.
  std::shared_ptr<DataQueue> data_queue =
      DataQueue::CreateIdempotent(std::move(list));

  CHECK_NOT_NULL(data_queue);

  // We can create an Entry from a data queue.
  std::unique_ptr<DataQueue::Entry> entry =
      DataQueue::CreateDataQueueEntry(data_queue);

  // The entry should be idempotent since the data queue is idempotent.
  CHECK(entry->is_idempotent());

  // The entry size should match the data queue size.
  CHECK_EQ(entry->size().value(), data_queue->size().value());

  // We can slice it since it is idempotent.
  uint64_t end = 20;
  std::unique_ptr<DataQueue::Entry> slice = entry->slice(5, end);

  // The slice has the expected length.
  CHECK_EQ(slice->size().value(), 15);

  // We can add it to another data queue, even if the new one is not
  // idempotent.

  std::shared_ptr<DataQueue> data_queue2 = DataQueue::Create();
  CHECK(data_queue2->append(std::move(slice)).value());

  // Our original data queue should have a use count of 2.
  CHECK_EQ(data_queue.use_count(), 2);

  std::shared_ptr<DataQueue::Reader> reader = data_queue2->get_reader();

  bool pullIsPending = true;

  int status = reader->Pull(
      [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
        pullIsPending = false;
        CHECK_EQ(count, 1);
        CHECK_EQ(memcmp(vecs[0].base, buffer1 + 5, len1 - 5), 0);
        CHECK_EQ(status, node::bob::STATUS_CONTINUE);
      },
      node::bob::OPTIONS_SYNC,
      nullptr,
      0);

  // All of the actual entries are in-memory entries so reads should be sync.
  CHECK(!pullIsPending);
  CHECK_EQ(status, node::bob::STATUS_CONTINUE);

  // Read to completion...
  while (status != node::bob::STATUS_EOS) {
    status = reader->Pull(
        [&](auto, auto, auto, auto) {}, node::bob::OPTIONS_SYNC, nullptr, 0);
  }

  // Because the original data queue is idempotent, we can still read from it,
  // even though we have already consumed the non-idempotent data queue that
  // contained it.

  std::shared_ptr<DataQueue::Reader> reader2 = data_queue->get_reader();
  CHECK_NOT_NULL(reader2);

  pullIsPending = true;

  status = reader2->Pull(
      [&](int status, const DataQueue::Vec* vecs, size_t count, auto done) {
        pullIsPending = false;
        CHECK_EQ(count, 1);
        CHECK_EQ(memcmp(vecs[0].base, buffer1, len1), 0);
        CHECK_EQ(status, node::bob::STATUS_CONTINUE);
      },
      node::bob::OPTIONS_SYNC,
      nullptr,
      0);

  // All of the actual entries are in-memory entries so reads should be sync.
  CHECK(!pullIsPending);
  CHECK_EQ(status, node::bob::STATUS_CONTINUE);
}
