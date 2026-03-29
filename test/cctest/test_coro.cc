#include "coro/uv_awaitable.h"
#include "coro/uv_task.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "uv.h"

using node::coro::UvFs;
using node::coro::UvFsStat;
using node::coro::UvGetAddrInfo;
using node::coro::UvTask;
using node::coro::UvWork;

// ===========================================================================
// Test fixture — owns a uv_loop_t and provides helpers.
// We use a minimal fixture (no V8/Node.js) to prove the coroutine
// layer works purely at the libuv level.
// ===========================================================================

class CoroTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_EQ(0, uv_loop_init(&loop_)); }

  void TearDown() override {
    // Drain any remaining handles/requests
    while (uv_loop_alive(&loop_)) {
      uv_run(&loop_, UV_RUN_ONCE);
    }
    ASSERT_EQ(0, uv_loop_close(&loop_));
  }

  // Run the event loop until all pending work is done.
  void RunLoop() { uv_run(&loop_, UV_RUN_DEFAULT); }

  uv_loop_t loop_;
};

// ===========================================================================
// Test 1: Basic coroutine — open, write, read, close a temp file.
// Demonstrates sequential async operations with co_await.
// ===========================================================================

TEST_F(CoroTest, OpenWriteReadClose) {
  bool completed = false;
  std::string read_back;

  auto coro = [&]() -> UvTask<void> {
    const char* path = "test_coro_tmp.txt";
    const char* msg = "hello from coroutine";
    const size_t msg_len = strlen(msg);

    // 1. Open for writing
    ssize_t fd = co_await UvFs(
        &loop_, uv_fs_open, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    EXPECT_GE(fd, 0) << "open for write failed: " << uv_strerror(fd);
    if (fd < 0) co_return;

    // 2. Write
    uv_buf_t write_buf = uv_buf_init(const_cast<char*>(msg), msg_len);
    ssize_t nwritten =
        co_await UvFs(&loop_, uv_fs_write, (uv_file)fd, &write_buf, 1, 0);
    EXPECT_EQ(nwritten, static_cast<ssize_t>(msg_len));

    // 3. Close
    ssize_t rc = co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);
    EXPECT_EQ(rc, 0);

    // 4. Re-open for reading
    fd = co_await UvFs(&loop_, uv_fs_open, path, O_RDONLY, 0);
    EXPECT_GE(fd, 0) << "open for read failed: " << uv_strerror(fd);
    if (fd < 0) co_return;

    // 5. Read back
    char buf[256] = {};
    uv_buf_t read_buf = uv_buf_init(buf, sizeof(buf));
    ssize_t nread =
        co_await UvFs(&loop_, uv_fs_read, (uv_file)fd, &read_buf, 1, 0);
    EXPECT_EQ(nread, static_cast<ssize_t>(msg_len));
    read_back.assign(buf, nread);

    // 6. Close
    rc = co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);
    EXPECT_EQ(rc, 0);

    // 7. Unlink (cleanup)
    co_await UvFs(&loop_, uv_fs_unlink, path);

    completed = true;
  };

  auto task = coro();
  task.Start();  // Runs until first co_await, then returns here
  RunLoop();     // Pump the event loop until all operations complete

  EXPECT_TRUE(completed);
  EXPECT_EQ(read_back, "hello from coroutine");
}

// ===========================================================================
// Test 2: Stat operation using the stat-specific awaitable.
// ===========================================================================

TEST_F(CoroTest, StatFile) {
  bool completed = false;
  uint64_t file_size = 0;

  auto coro = [&]() -> UvTask<void> {
    const char* path = "test_coro_stat_tmp.txt";
    const char* data = "some test data for stat";

    // Create a file with known content
    ssize_t fd = co_await UvFs(
        &loop_, uv_fs_open, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    EXPECT_GE(fd, 0);
    if (fd < 0) co_return;

    uv_buf_t buf = uv_buf_init(const_cast<char*>(data), strlen(data));
    co_await UvFs(&loop_, uv_fs_write, (uv_file)fd, &buf, 1, 0);
    co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);

    // Now stat it
    auto [err, stat] = co_await UvFsStat(&loop_, uv_fs_stat, path);
    EXPECT_EQ(err, 0) << "stat failed: " << uv_strerror(err);
    file_size = stat.st_size;

    // Cleanup
    co_await UvFs(&loop_, uv_fs_unlink, path);

    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
  EXPECT_EQ(file_size, strlen("some test data for stat"));
}

// ===========================================================================
// Test 3: Coroutine composition — co_await one UvTask from another.
// ===========================================================================

TEST_F(CoroTest, TaskComposition) {
  bool completed = false;

  // An "inner" coroutine that opens a file and returns the fd.
  auto open_file =
      [&](const char* path, int flags, int mode) -> UvTask<ssize_t> {
    ssize_t fd = co_await UvFs(&loop_, uv_fs_open, path, flags, mode);
    co_return fd;
  };

  // An "outer" coroutine that uses the inner one.
  auto coro = [&]() -> UvTask<void> {
    const char* path = "test_coro_compose_tmp.txt";

    // Use the inner coroutine
    ssize_t fd = co_await open_file(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    EXPECT_GE(fd, 0);
    if (fd < 0) co_return;

    co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);
    co_await UvFs(&loop_, uv_fs_unlink, path);

    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
}

// ===========================================================================
// Test 4: Error handling — open a nonexistent file.
// ===========================================================================

TEST_F(CoroTest, ErrorHandling) {
  bool completed = false;
  ssize_t open_result = 0;

  auto coro = [&]() -> UvTask<void> {
    // This should fail with ENOENT
    open_result = co_await UvFs(&loop_,
                                uv_fs_open,
                                "/nonexistent/path/that/does/not/exist",
                                O_RDONLY,
                                0);
    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
  EXPECT_EQ(open_result, UV_ENOENT);
}

// ===========================================================================
// Test 5: UvWork — run work on the thread pool.
// ===========================================================================

TEST_F(CoroTest, ThreadPoolWork) {
  bool completed = false;
  int computed_value = 0;

  auto coro = [&]() -> UvTask<void> {
    // Run some "computation" on the thread pool
    int result = 0;
    int status = co_await UvWork(&loop_, [&result]() {
      // This runs on a worker thread
      result = 0;
      for (int i = 1; i <= 100; i++) result += i;
    });

    EXPECT_EQ(status, 0);
    computed_value = result;
    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
  EXPECT_EQ(computed_value, 5050);
}

// ===========================================================================
// Test 6: DNS resolution with UvGetAddrInfo.
// ===========================================================================

TEST_F(CoroTest, GetAddrInfo) {
  bool completed = false;
  int resolve_status = -1;

  auto coro = [&]() -> UvTask<void> {
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    auto [status, info] =
        co_await UvGetAddrInfo(&loop_, "localhost", nullptr, &hints);

    resolve_status = status;
    if (status == 0 && info != nullptr) {
      // Successfully resolved — just verify we got at least one result
      EXPECT_NE(info->ai_addr, nullptr);
      uv_freeaddrinfo(info);
    }

    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
  EXPECT_EQ(resolve_status, 0);
}

// ===========================================================================
// Test 7: Mixed operations — thread pool work + fs in one coroutine.
// Demonstrates that different awaitable types compose naturally.
// ===========================================================================

TEST_F(CoroTest, MixedOperations) {
  bool completed = false;
  std::string final_content;

  auto coro = [&]() -> UvTask<void> {
    const char* path = "test_coro_mixed_tmp.txt";

    // Generate data on the thread pool
    std::string data;
    co_await UvWork(&loop_, [&data]() { data = "computed-on-threadpool"; });

    // Write it to a file using async fs
    ssize_t fd = co_await UvFs(
        &loop_, uv_fs_open, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    EXPECT_GE(fd, 0);
    if (fd < 0) co_return;

    uv_buf_t buf = uv_buf_init(data.data(), data.size());
    co_await UvFs(&loop_, uv_fs_write, (uv_file)fd, &buf, 1, 0);
    co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);

    // Read it back
    fd = co_await UvFs(&loop_, uv_fs_open, path, O_RDONLY, 0);
    EXPECT_GE(fd, 0);
    if (fd < 0) co_return;

    char read_buf[256] = {};
    uv_buf_t rbuf = uv_buf_init(read_buf, sizeof(read_buf));
    ssize_t nread = co_await UvFs(&loop_, uv_fs_read, (uv_file)fd, &rbuf, 1, 0);
    EXPECT_GT(nread, 0);
    final_content.assign(read_buf, nread);

    co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);
    co_await UvFs(&loop_, uv_fs_unlink, path);

    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
  EXPECT_EQ(final_content, "computed-on-threadpool");
}

// ===========================================================================
// Test 8: Returning values through coroutine chain.
// ===========================================================================

TEST_F(CoroTest, ReturnValueChain) {
  // A coroutine that reads a file and returns its content as a string.
  auto read_file = [&](const char* path) -> UvTask<std::string> {
    ssize_t fd = co_await UvFs(&loop_, uv_fs_open, path, O_RDONLY, 0);
    if (fd < 0) co_return std::string{};

    char buf[4096] = {};
    uv_buf_t iov = uv_buf_init(buf, sizeof(buf));
    ssize_t nread = co_await UvFs(&loop_, uv_fs_read, (uv_file)fd, &iov, 1, 0);
    co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);

    co_return std::string(buf, nread > 0 ? nread : 0);
  };

  // A coroutine that writes a file.
  auto write_file = [&](const char* path,
                        const std::string& content) -> UvTask<bool> {
    ssize_t fd = co_await UvFs(
        &loop_, uv_fs_open, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) co_return false;

    uv_buf_t buf =
        uv_buf_init(const_cast<char*>(content.data()), content.size());
    ssize_t nwritten =
        co_await UvFs(&loop_, uv_fs_write, (uv_file)fd, &buf, 1, 0);
    co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);

    co_return nwritten == static_cast<ssize_t>(content.size());
  };

  bool completed = false;
  std::string result;

  auto coro = [&]() -> UvTask<void> {
    const char* path = "test_coro_chain_tmp.txt";

    bool ok = co_await write_file(path, "chained-coroutine-value");
    EXPECT_TRUE(ok);

    result = co_await read_file(path);

    co_await UvFs(&loop_, uv_fs_unlink, path);
    completed = true;
  };

  auto task = coro();
  task.Start();
  RunLoop();

  EXPECT_TRUE(completed);
  EXPECT_EQ(result, "chained-coroutine-value");
}

// ===========================================================================
// Test 9: Multiple sequential coroutines on the same loop.
// ===========================================================================

TEST_F(CoroTest, SequentialTasks) {
  int counter = 0;

  auto increment = [&]() -> UvTask<void> {
    // Do a trivial async op to force a real suspension
    const char* path = "test_coro_seq_tmp.txt";
    ssize_t fd = co_await UvFs(
        &loop_, uv_fs_open, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      co_await UvFs(&loop_, uv_fs_close, (uv_file)fd);
      co_await UvFs(&loop_, uv_fs_unlink, path);
    }
    counter++;
  };

  // Start three fire-and-forget coroutines
  auto t1 = increment();
  t1.Start();
  auto t2 = increment();
  t2.Start();
  auto t3 = increment();
  t3.Start();

  RunLoop();

  EXPECT_EQ(counter, 3);
}
