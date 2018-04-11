#include "inspector_socket.h"
#include "gtest/gtest.h"

#include <queue>

#define PORT 9444

namespace {

using node::inspector::InspectorSocket;

static const int MAX_LOOP_ITERATIONS = 10000;

static uv_loop_t loop;

class Timeout {
 public:
  explicit Timeout(uv_loop_t* loop) : timed_out(false), done_(false) {
    uv_timer_init(loop, &timer_);
    uv_timer_start(&timer_, Timeout::set_flag, 5000, 0);
  }

  ~Timeout() {
    uv_timer_stop(&timer_);
    uv_close(reinterpret_cast<uv_handle_t*>(&timer_), mark_done);
    while (!done_) {
      uv_run(&loop, UV_RUN_NOWAIT);
    }
  }

  bool timed_out;

 private:
  static void set_flag(uv_timer_t* timer) {
    Timeout* t = node::ContainerOf(&Timeout::timer_, timer);
    t->timed_out = true;
  }

  static void mark_done(uv_handle_t* timer) {
    Timeout* t = node::ContainerOf(&Timeout::timer_,
        reinterpret_cast<uv_timer_t*>(timer));
    t->done_ = true;
  }

  bool done_;
  uv_timer_t timer_;
};

#define SPIN_WHILE(condition)                                                  \
  {                                                                            \
    Timeout timeout(&loop);                                                    \
    while ((condition) && !timeout.timed_out) {                                \
      uv_run(&loop, UV_RUN_NOWAIT);                                            \
    }                                                                          \
    ASSERT_FALSE((condition));                                                 \
  }

enum inspector_handshake_event {
  kInspectorHandshakeHttpGet,
  kInspectorHandshakeUpgraded,
  kInspectorHandshakeNoEvents
};

struct expectations {
  std::string actual_data;
  size_t actual_offset;
  size_t actual_end;
  int err_code;
};

static bool waiting_to_close = true;

void handle_closed(uv_handle_t* handle) {
  waiting_to_close = false;
}

static void really_close(uv_handle_t* handle) {
  waiting_to_close = true;
  if (!uv_is_closing(handle)) {
    uv_close(handle, handle_closed);
    SPIN_WHILE(waiting_to_close);
  }
}

static void buffer_alloc_cb(uv_handle_t* stream, size_t len, uv_buf_t* buf) {
  buf->base = new char[len];
  buf->len = len;
}

class TestInspectorDelegate;

static TestInspectorDelegate* delegate = nullptr;

// Gtest asserts can't be used in dtor directly.
static void assert_is_delegate(TestInspectorDelegate* d) {
  GTEST_ASSERT_EQ(delegate, d);
}

class TestInspectorDelegate : public InspectorSocket::Delegate {
 public:
  using delegate_fn = void(*)(inspector_handshake_event, const std::string&,
                              bool* should_continue);

  TestInspectorDelegate() : inspector_ready(false),
                            last_event(kInspectorHandshakeNoEvents),
                            handshake_events(0),
                            handshake_delegate_(stop_if_stop_path),
                            fail_on_ws_frame_(false) { }

  ~TestInspectorDelegate() {
    assert_is_delegate(this);
    delegate = nullptr;
  }

  void OnHttpGet(const std::string& host, const std::string& path) override {
    process(kInspectorHandshakeHttpGet, path);
  }

  void OnSocketUpgrade(const std::string& host, const std::string& path,
                       const std::string& ws_key) override {
    ws_key_ = ws_key;
    process(kInspectorHandshakeUpgraded, path);
  }

  void OnWsFrame(const std::vector<char>& buffer) override {
    ASSERT_FALSE(fail_on_ws_frame_);
    frames.push(buffer);
  }

  void SetDelegate(delegate_fn d) {
    handshake_delegate_ = d;
  }

  void SetInspector(InspectorSocket::Pointer inspector) {
    socket_ = std::move(inspector);
  }

  void Write(const char* buf, size_t len) {
    socket_->Write(buf, len);
  }

  void ExpectReadError() {
    SPIN_WHILE(frames.empty() || !frames.back().empty());
  }

  void ExpectData(const char* data, size_t len) {
    const char* cur = data;
    const char* end = data + len;
    while (cur < end) {
      SPIN_WHILE(frames.empty());
      const std::vector<char>& frame = frames.front();
      EXPECT_FALSE(frame.empty());
      auto c = frame.begin();
      for (; c < frame.end() && cur < end; c++) {
        GTEST_ASSERT_EQ(*cur, *c) << "Character #" << cur - data;
        cur = cur + 1;
      }
      EXPECT_EQ(c, frame.end());
      frames.pop();
    }
  }

  void FailOnWsFrame() {
    fail_on_ws_frame_ = true;
  }

  void WaitForDispose() {
    SPIN_WHILE(delegate != nullptr);
  }

  void Close() {
    socket_.reset();
  }

  bool inspector_ready;
  std::string last_path;  // NOLINT(runtime/string)
  inspector_handshake_event last_event;
  int handshake_events;
  std::queue<std::vector<char>> frames;

 private:
  static void stop_if_stop_path(enum inspector_handshake_event state,
                              const std::string& path, bool* cont) {
    *cont = path.empty() || path != "/close";
  }

  void process(inspector_handshake_event event, const std::string& path);

  delegate_fn handshake_delegate_;
  InspectorSocket::Pointer socket_;
  std::string ws_key_;
  bool fail_on_ws_frame_;
};

static bool connected = false;
static uv_tcp_t server, client_socket;
static const char SERVER_CLOSE_FRAME[] = {'\x88', '\x00'};

struct read_expects {
  const char* expected;
  size_t expected_len;
  size_t pos;
  bool read_expected;
  bool callback_called;
};

static const char HANDSHAKE_REQ[] = "GET /ws/path HTTP/1.1\r\n"
                                    "Host: localhost:9229\r\n"
                                    "Upgrade: websocket\r\n"
                                    "Connection: Upgrade\r\n"
                                    "Sec-WebSocket-Key: aaa==\r\n"
                                    "Sec-WebSocket-Version: 13\r\n\r\n";

void TestInspectorDelegate::process(inspector_handshake_event event,
                                    const std::string& path) {
  inspector_ready = event == kInspectorHandshakeUpgraded;
  last_event = event;
  if (path.empty()) {
    last_path = "@@@ Nothing received @@@";
  } else {
    last_path = path;
  }
  handshake_events++;
  bool should_continue = true;
  handshake_delegate_(event, path, &should_continue);
  if (should_continue) {
    if (inspector_ready)
      socket_->AcceptUpgrade(ws_key_);
  } else {
    socket_->CancelHandshake();
  }
}

static void on_new_connection(uv_stream_t* server, int status) {
  GTEST_ASSERT_EQ(0, status);
  connected = true;
  delegate = new TestInspectorDelegate();
  delegate->SetInspector(
      InspectorSocket::Accept(server,
                              InspectorSocket::DelegatePointer(delegate)));
  GTEST_ASSERT_NE(nullptr, delegate);
}

void write_done(uv_write_t* req, int status) { req->data = nullptr; }

static void do_write(const char* data, int len) {
  uv_write_t req;
  bool done = false;
  req.data = &done;
  uv_buf_t buf[1];
  buf[0].base = const_cast<char*>(data);
  buf[0].len = len;
  GTEST_ASSERT_EQ(0,
                  uv_write(&req, reinterpret_cast<uv_stream_t*>(&client_socket),
                           buf, 1, write_done));
  SPIN_WHILE(req.data);
}

static void check_data_cb(read_expects* expectation, ssize_t nread,
                          const uv_buf_t* buf, bool* retval) {
  *retval = false;
  EXPECT_TRUE(nread >= 0 && nread != UV_EOF);
  ssize_t i;
  char c, actual;
  CHECK_GT(expectation->expected_len, 0);
  for (i = 0; i < nread && expectation->pos <= expectation->expected_len; i++) {
    c = expectation->expected[expectation->pos++];
    actual = buf->base[i];
    if (c != actual) {
      fprintf(stderr, "Unexpected character at position %zd\n",
              expectation->pos - 1);
      GTEST_ASSERT_EQ(c, actual);
    }
  }
  GTEST_ASSERT_EQ(i, nread);
  delete[] buf->base;
  if (expectation->pos == expectation->expected_len) {
    expectation->read_expected = true;
    *retval = true;
  }
}

static void check_data_cb(uv_stream_t* stream, ssize_t nread,
                          const uv_buf_t* buf) {
  bool retval = false;
  read_expects* expects = static_cast<read_expects*>(stream->data);
  expects->callback_called = true;
  check_data_cb(expects, nread, buf, &retval);
  if (retval) {
    stream->data = nullptr;
    uv_read_stop(stream);
  }
}

static read_expects prepare_expects(const char* data, size_t len) {
  read_expects expectation;
  expectation.expected = data;
  expectation.expected_len = len;
  expectation.pos = 0;
  expectation.read_expected = false;
  expectation.callback_called = false;
  return expectation;
}

static void fail_callback(uv_stream_t* stream, ssize_t nread,
                          const uv_buf_t* buf) {
  if (nread < 0) {
    fprintf(stderr, "IO error: %s\n", uv_strerror(nread));
  } else {
    fprintf(stderr, "Read %zd bytes\n", nread);
  }
  ASSERT_TRUE(false);  // Shouldn't have been called
}

static void expect_nothing_on_client() {
  uv_stream_t* stream = reinterpret_cast<uv_stream_t*>(&client_socket);
  int err = uv_read_start(stream, buffer_alloc_cb, fail_callback);
  GTEST_ASSERT_EQ(0, err);
  for (int i = 0; i < MAX_LOOP_ITERATIONS; i++)
    uv_run(&loop, UV_RUN_NOWAIT);
  uv_read_stop(stream);
}

static void expect_on_client(const char* data, size_t len) {
  read_expects expectation = prepare_expects(data, len);
  client_socket.data = &expectation;
  uv_read_start(reinterpret_cast<uv_stream_t*>(&client_socket),
                buffer_alloc_cb, check_data_cb);
  SPIN_WHILE(!expectation.read_expected);
}

static void expect_handshake() {
  const char UPGRADE_RESPONSE[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: Dt87H1OULVZnSJo/KgMUYI7xPCg=\r\n\r\n";
  expect_on_client(UPGRADE_RESPONSE, sizeof(UPGRADE_RESPONSE) - 1);
}

static void expect_handshake_failure() {
  const char UPGRADE_RESPONSE[] =
      "HTTP/1.0 400 Bad Request\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n\r\n"
      "WebSockets request was expected\r\n";
  expect_on_client(UPGRADE_RESPONSE, sizeof(UPGRADE_RESPONSE) - 1);
}

static void on_connection(uv_connect_t* connect, int status) {
  GTEST_ASSERT_EQ(0, status);
  connect->data = connect;
}

class InspectorSocketTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    connected = false;
    GTEST_ASSERT_EQ(0, uv_loop_init(&loop));
    server = uv_tcp_t();
    client_socket = uv_tcp_t();
    sockaddr_in addr;
    uv_tcp_init(&loop, &server);
    uv_tcp_init(&loop, &client_socket);
    GTEST_ASSERT_EQ(0, uv_ip4_addr("127.0.0.1", PORT, &addr));
    uv_tcp_bind(&server, reinterpret_cast<const struct sockaddr*>(&addr), 0);
    GTEST_ASSERT_EQ(0, uv_listen(reinterpret_cast<uv_stream_t*>(&server),
                                 1, on_new_connection));
    uv_connect_t connect;
    connect.data = nullptr;
    GTEST_ASSERT_EQ(0, uv_tcp_connect(&connect, &client_socket,
                                      reinterpret_cast<const sockaddr*>(&addr),
                                      on_connection));
    uv_tcp_nodelay(&client_socket, 1);  // The buffering messes up the test
    SPIN_WHILE(!connect.data || !connected);
    really_close(reinterpret_cast<uv_handle_t*>(&server));
  }

  virtual void TearDown() {
    really_close(reinterpret_cast<uv_handle_t*>(&client_socket));
    SPIN_WHILE(delegate != nullptr);
    const int err = uv_loop_close(&loop);
    if (err != 0) {
      uv_print_all_handles(&loop, stderr);
    }
    EXPECT_EQ(0, err);
  }
};

TEST_F(InspectorSocketTest, ReadsAndWritesInspectorMessage) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!delegate->inspector_ready);
  expect_handshake();

  // 2. Brief exchange
  const char SERVER_MESSAGE[] = "abcd";
  const char CLIENT_FRAME[] = {'\x81', '\x04', 'a', 'b', 'c', 'd'};
  delegate->Write(SERVER_MESSAGE, sizeof(SERVER_MESSAGE) - 1);
  expect_on_client(CLIENT_FRAME, sizeof(CLIENT_FRAME));

  const char SERVER_FRAME[] = {'\x81', '\x84', '\x7F', '\xC2', '\x66',
                               '\x31', '\x4E', '\xF0', '\x55', '\x05'};
  const char CLIENT_MESSAGE[] = "1234";
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  delegate->ExpectData(CLIENT_MESSAGE, sizeof(CLIENT_MESSAGE) - 1);

  // 3. Close
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  const char SERVER_CLOSE_FRAME[] = {'\x88', '\x00'};
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
  expect_on_client(SERVER_CLOSE_FRAME, sizeof(SERVER_CLOSE_FRAME));
  GTEST_ASSERT_EQ(0, uv_is_active(
                         reinterpret_cast<uv_handle_t*>(&client_socket)));
}

TEST_F(InspectorSocketTest, BufferEdgeCases) {
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();

  const char MULTIPLE_REQUESTS[] = {
      '\x81', '\xCB', '\x76', '\xCA', '\x06', '\x0C', '\x0D', '\xE8', '\x6F',
      '\x68', '\x54', '\xF0', '\x37', '\x3E', '\x5A', '\xE8', '\x6B', '\x69',
      '\x02', '\xA2', '\x69', '\x68', '\x54', '\xF0', '\x24', '\x5B', '\x19',
      '\xB8', '\x6D', '\x69', '\x04', '\xE4', '\x75', '\x69', '\x02', '\x8B',
      '\x73', '\x78', '\x19', '\xA9', '\x69', '\x62', '\x18', '\xAF', '\x65',
      '\x78', '\x22', '\xA5', '\x51', '\x63', '\x04', '\xA1', '\x63', '\x7E',
      '\x05', '\xE8', '\x2A', '\x2E', '\x06', '\xAB', '\x74', '\x6D', '\x1B',
      '\xB9', '\x24', '\x36', '\x0D', '\xE8', '\x70', '\x6D', '\x1A', '\xBF',
      '\x63', '\x2E', '\x4C', '\xBE', '\x74', '\x79', '\x13', '\xB7', '\x7B',
      '\x81', '\xA2', '\xFC', '\x9E', '\x0D', '\x15', '\x87', '\xBC', '\x64',
      '\x71', '\xDE', '\xA4', '\x3C', '\x26', '\xD0', '\xBC', '\x60', '\x70',
      '\x88', '\xF6', '\x62', '\x71', '\xDE', '\xA4', '\x2F', '\x42', '\x93',
      '\xEC', '\x66', '\x70', '\x8E', '\xB0', '\x68', '\x7B', '\x9D', '\xFC',
      '\x61', '\x70', '\xDE', '\xE3', '\x81', '\xA4', '\x4E', '\x37', '\xB0',
      '\x22', '\x35', '\x15', '\xD9', '\x46', '\x6C', '\x0D', '\x81', '\x16',
      '\x62', '\x15', '\xDD', '\x47', '\x3A', '\x5F', '\xDF', '\x46', '\x6C',
      '\x0D', '\x92', '\x72', '\x3C', '\x58', '\xD6', '\x4B', '\x22', '\x52',
      '\xC2', '\x0C', '\x2B', '\x59', '\xD1', '\x40', '\x22', '\x52', '\x92',
      '\x5F', '\x81', '\xCB', '\xCD', '\xF0', '\x30', '\xC5', '\xB6', '\xD2',
      '\x59', '\xA1', '\xEF', '\xCA', '\x01', '\xF0', '\xE1', '\xD2', '\x5D',
      '\xA0', '\xB9', '\x98', '\x5F', '\xA1', '\xEF', '\xCA', '\x12', '\x95',
      '\xBF', '\x9F', '\x56', '\xAC', '\xA1', '\x95', '\x42', '\xEB', '\xBE',
      '\x95', '\x44', '\x96', '\xAC', '\x9D', '\x40', '\xA9', '\xA4', '\x9E',
      '\x57', '\x8C', '\xA3', '\x84', '\x55', '\xB7', '\xBB', '\x91', '\x5C',
      '\xE7', '\xE1', '\xD2', '\x40', '\xA4', '\xBF', '\x91', '\x5D', '\xB6',
      '\xEF', '\xCA', '\x4B', '\xE7', '\xA4', '\x9E', '\x44', '\xA0', '\xBF',
      '\x86', '\x51', '\xA9', '\xEF', '\xCA', '\x01', '\xF5', '\xFD', '\x8D',
      '\x4D', '\x81', '\xA9', '\x74', '\x6B', '\x72', '\x43', '\x0F', '\x49',
      '\x1B', '\x27', '\x56', '\x51', '\x43', '\x75', '\x58', '\x49', '\x1F',
      '\x26', '\x00', '\x03', '\x1D', '\x27', '\x56', '\x51', '\x50', '\x10',
      '\x11', '\x19', '\x04', '\x2A', '\x17', '\x0E', '\x25', '\x2C', '\x06',
      '\x00', '\x17', '\x31', '\x5A', '\x0E', '\x1C', '\x22', '\x16', '\x07',
      '\x17', '\x61', '\x09', '\x81', '\xB8', '\x7C', '\x1A', '\xEA', '\xEB',
      '\x07', '\x38', '\x83', '\x8F', '\x5E', '\x20', '\xDB', '\xDC', '\x50',
      '\x38', '\x87', '\x8E', '\x08', '\x72', '\x85', '\x8F', '\x5E', '\x20',
      '\xC8', '\xA5', '\x19', '\x6E', '\x9D', '\x84', '\x0E', '\x71', '\xC4',
      '\x88', '\x1D', '\x74', '\xAF', '\x86', '\x09', '\x76', '\x8B', '\x9F',
      '\x19', '\x54', '\x8F', '\x9F', '\x0B', '\x75', '\x98', '\x80', '\x3F',
      '\x75', '\x84', '\x8F', '\x15', '\x6E', '\x83', '\x84', '\x12', '\x69',
      '\xC8', '\x96'};

  const char EXPECT[] = {
      "{\"id\":12,\"method\":\"Worker.setAutoconnectToWorkers\","
      "\"params\":{\"value\":true}}"
      "{\"id\":13,\"method\":\"Worker.enable\"}"
      "{\"id\":14,\"method\":\"Profiler.enable\"}"
      "{\"id\":15,\"method\":\"Profiler.setSamplingInterval\","
      "\"params\":{\"interval\":100}}"
      "{\"id\":16,\"method\":\"ServiceWorker.enable\"}"
      "{\"id\":17,\"method\":\"Network.canEmulateNetworkConditions\"}"};

  do_write(MULTIPLE_REQUESTS, sizeof(MULTIPLE_REQUESTS));
  delegate->ExpectData(EXPECT, sizeof(EXPECT) - 1);
}

TEST_F(InspectorSocketTest, AcceptsRequestInSeveralWrites) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(delegate->inspector_ready);
  // Specifically, break up the request in the "Sec-WebSocket-Key" header name
  // and value
  const int write1 = 95;
  const int write2 = 5;
  const int write3 = sizeof(HANDSHAKE_REQ) - write1 - write2 - 1;
  do_write(const_cast<char*>(HANDSHAKE_REQ), write1);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ) + write1, write2);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ) + write1 + write2, write3);
  SPIN_WHILE(!delegate->inspector_ready);
  expect_handshake();
  GTEST_ASSERT_EQ(uv_is_active(reinterpret_cast<uv_handle_t*>(&client_socket)),
                  0);
}

TEST_F(InspectorSocketTest, ExtraTextBeforeRequest) {
  delegate->last_event = kInspectorHandshakeUpgraded;
  char UNCOOL_BRO[] = "Text before the first req, shouldn't be her\r\n";
  do_write(const_cast<char*>(UNCOOL_BRO), sizeof(UNCOOL_BRO) - 1);
  expect_handshake_failure();
  GTEST_ASSERT_EQ(nullptr, delegate);
}

TEST_F(InspectorSocketTest, RequestWithoutKey) {
  const char BROKEN_REQUEST[] = "GET / HTTP/1.1\r\n"
                                "Host: localhost:9229\r\n"
                                "Upgrade: websocket\r\n"
                                "Connection: Upgrade\r\n"
                                "Sec-WebSocket-Version: 13\r\n\r\n";

  do_write(const_cast<char*>(BROKEN_REQUEST), sizeof(BROKEN_REQUEST) - 1);
  expect_handshake_failure();
}

TEST_F(InspectorSocketTest, KillsConnectionOnProtocolViolation) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!delegate->inspector_ready);
  ASSERT_TRUE(delegate->inspector_ready);
  expect_handshake();
  const char SERVER_FRAME[] = "I'm not a good WS frame. Nope!";
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  SPIN_WHILE(delegate != nullptr);
  GTEST_ASSERT_EQ(uv_is_active(reinterpret_cast<uv_handle_t*>(&client_socket)),
                  0);
}

TEST_F(InspectorSocketTest, CanStopReadingFromInspector) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  ASSERT_TRUE(delegate->inspector_ready);

  // 2. Brief exchange
  const char SERVER_FRAME[] = {'\x81', '\x84', '\x7F', '\xC2', '\x66',
                               '\x31', '\x4E', '\xF0', '\x55', '\x05'};
  const char CLIENT_MESSAGE[] = "1234";
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  delegate->ExpectData(CLIENT_MESSAGE, sizeof(CLIENT_MESSAGE) - 1);

  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  GTEST_ASSERT_EQ(uv_is_active(
                      reinterpret_cast<uv_handle_t*>(&client_socket)), 0);
}

TEST_F(InspectorSocketTest, CloseDoesNotNotifyReadCallback) {
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();

  delegate->Close();
  char CLOSE_FRAME[] = {'\x88', '\x00'};
  expect_on_client(CLOSE_FRAME, sizeof(CLOSE_FRAME));
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  delegate->FailOnWsFrame();
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
  SPIN_WHILE(delegate != nullptr);
}

TEST_F(InspectorSocketTest, CloseWorksWithoutReadEnabled) {
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  delegate->Close();
  char CLOSE_FRAME[] = {'\x88', '\x00'};
  expect_on_client(CLOSE_FRAME, sizeof(CLOSE_FRAME));
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
}

// Make sure buffering works
static void send_in_chunks(const char* data, size_t len) {
  const int step = 7;
  size_t i = 0;
  // Do not send it all at once - test the buffering!
  for (; i < len - step; i += step) {
    do_write(data + i, step);
  }
  if (i < len) {
    do_write(data + i, len - i);
  }
}

static const char TEST_SUCCESS[] = "Test Success\n\n";
static int ReportsHttpGet_eventsCount = 0;

static void ReportsHttpGet_handshake(enum inspector_handshake_event state,
                                     const std::string& path, bool* cont) {
  *cont = true;
  enum inspector_handshake_event expected_state = kInspectorHandshakeHttpGet;
  std::string expected_path;
  switch (delegate->handshake_events) {
  case 1:
    expected_path = "/some/path";
    break;
  case 2:
    expected_path = "/respond/withtext";
    delegate->Write(TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);
    break;
  case 3:
    expected_path = "/some/path2";
    break;
  case 4:
    expected_path = "/close";
    *cont = false;
    break;
  default:
    ASSERT_TRUE(false);
  }
  EXPECT_EQ(expected_state, state);
  EXPECT_EQ(expected_path, path);
  ReportsHttpGet_eventsCount = delegate->handshake_events;
}

TEST_F(InspectorSocketTest, ReportsHttpGet) {
  delegate->SetDelegate(ReportsHttpGet_handshake);

  const char GET_REQ[] = "GET /some/path HTTP/1.1\r\n"
                         "Host: localhost:9229\r\n"
                         "Sec-WebSocket-Key: aaa==\r\n"
                         "Sec-WebSocket-Version: 13\r\n\r\n";
  send_in_chunks(GET_REQ, sizeof(GET_REQ) - 1);

  expect_nothing_on_client();
  const char WRITE_REQUEST[] = "GET /respond/withtext HTTP/1.1\r\n"
                               "Host: localhost:9229\r\n\r\n";
  send_in_chunks(WRITE_REQUEST, sizeof(WRITE_REQUEST) - 1);

  expect_on_client(TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);
  const char GET_REQS[] = "GET /some/path2 HTTP/1.1\r\n"
                          "Host: localhost:9229\r\n"
                          "Sec-WebSocket-Key: aaa==\r\n"
                          "Sec-WebSocket-Version: 13\r\n\r\n"
                          "GET /close HTTP/1.1\r\n"
                          "Host: localhost:9229\r\n"
                          "Sec-WebSocket-Key: aaa==\r\n"
                          "Sec-WebSocket-Version: 13\r\n\r\n";
  send_in_chunks(GET_REQS, sizeof(GET_REQS) - 1);
  expect_handshake_failure();
  EXPECT_EQ(4, ReportsHttpGet_eventsCount);
  EXPECT_EQ(nullptr, delegate);
}

static int HandshakeCanBeCanceled_eventCount = 0;

static
void HandshakeCanBeCanceled_handshake(enum inspector_handshake_event state,
                                      const std::string& path, bool* cont) {
  switch (delegate->handshake_events - 1) {
  case 0:
    EXPECT_EQ(kInspectorHandshakeUpgraded, state);
    EXPECT_EQ("/ws/path", path);
    break;
  default:
    EXPECT_TRUE(false);
    break;
  }
  *cont = false;
  HandshakeCanBeCanceled_eventCount = delegate->handshake_events;
}

TEST_F(InspectorSocketTest, HandshakeCanBeCanceled) {
  delegate->SetDelegate(HandshakeCanBeCanceled_handshake);

  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);

  expect_handshake_failure();
  EXPECT_EQ(1, HandshakeCanBeCanceled_eventCount);
  EXPECT_EQ(nullptr, delegate);
}

static void GetThenHandshake_handshake(enum inspector_handshake_event state,
                                       const std::string& path, bool* cont) {
  *cont = true;
  std::string expected_path = "/ws/path";
  switch (delegate->handshake_events - 1) {
  case 0:
    EXPECT_EQ(kInspectorHandshakeHttpGet, state);
    expected_path = "/respond/withtext";
    delegate->Write(TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);
    break;
  case 1:
    EXPECT_EQ(kInspectorHandshakeUpgraded, state);
    break;
  default:
    EXPECT_TRUE(false);
    break;
  }
  EXPECT_EQ(expected_path, path);
}

TEST_F(InspectorSocketTest, GetThenHandshake) {
  delegate->SetDelegate(GetThenHandshake_handshake);
  const char WRITE_REQUEST[] = "GET /respond/withtext HTTP/1.1\r\n"
                               "Host: localhost:9229\r\n\r\n";
  send_in_chunks(WRITE_REQUEST, sizeof(WRITE_REQUEST) - 1);

  expect_on_client(TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);

  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  EXPECT_EQ(2, delegate->handshake_events);
}

TEST_F(InspectorSocketTest, WriteBeforeHandshake) {
  const char MESSAGE1[] = "Message 1";
  const char MESSAGE2[] = "Message 2";
  const char EXPECTED[] = "Message 1Message 2";

  delegate->Write(MESSAGE1, sizeof(MESSAGE1) - 1);
  delegate->Write(MESSAGE2, sizeof(MESSAGE2) - 1);
  expect_on_client(EXPECTED, sizeof(EXPECTED) - 1);
  really_close(reinterpret_cast<uv_handle_t*>(&client_socket));
  SPIN_WHILE(delegate != nullptr);
}

TEST_F(InspectorSocketTest, CleanupSocketAfterEOF) {
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();

  for (int i = 0; i < MAX_LOOP_ITERATIONS; ++i) {
    uv_run(&loop, UV_RUN_NOWAIT);
  }

  uv_close(reinterpret_cast<uv_handle_t*>(&client_socket), nullptr);
  SPIN_WHILE(delegate != nullptr);
}

TEST_F(InspectorSocketTest, EOFBeforeHandshake) {
  const char MESSAGE[] = "We'll send EOF afterwards";
  delegate->Write(MESSAGE, sizeof(MESSAGE) - 1);
  expect_on_client(MESSAGE, sizeof(MESSAGE) - 1);
  uv_close(reinterpret_cast<uv_handle_t*>(&client_socket), nullptr);
  SPIN_WHILE(delegate != nullptr);
}

static void fill_message(std::string* buffer) {
  for (size_t i = 0; i < buffer->size(); i += 1) {
    (*buffer)[i] = 'a' + (i % ('z' - 'a'));
  }
}

static void mask_message(const std::string& message,
                         char* buffer, const char mask[]) {
  const size_t mask_len = 4;
  for (size_t i = 0; i < message.size(); i += 1) {
    buffer[i] = message[i] ^ mask[i % mask_len];
  }
}

TEST_F(InspectorSocketTest, Send1Mb) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!delegate->inspector_ready);
  expect_handshake();

  // 2. Brief exchange
  std::string message(1000000, '\0');
  fill_message(&message);

  // 1000000 is 0xF4240 hex
  const char EXPECTED_FRAME_HEADER[] = {
    '\x81', '\x7f', '\x00', '\x00', '\x00', '\x00', '\x00', '\x0F',
    '\x42', '\x40'
  };
  std::string expected(EXPECTED_FRAME_HEADER, sizeof(EXPECTED_FRAME_HEADER));
  expected.append(message);

  delegate->Write(&message[0], message.size());
  expect_on_client(&expected[0], expected.size());

  char MASK[4] = {'W', 'h', 'O', 'a'};

  const char FRAME_TO_SERVER_HEADER[] = {
    '\x81', '\xff', '\x00', '\x00', '\x00', '\x00', '\x00', '\x0F',
    '\x42', '\x40', MASK[0], MASK[1], MASK[2], MASK[3]
  };

  std::string outgoing(FRAME_TO_SERVER_HEADER, sizeof(FRAME_TO_SERVER_HEADER));
  outgoing.resize(outgoing.size() + message.size());
  mask_message(message, &outgoing[sizeof(FRAME_TO_SERVER_HEADER)], MASK);

  do_write(&outgoing[0], outgoing.size());
  delegate->ExpectData(&message[0], message.size());

  // 3. Close
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
  expect_on_client(SERVER_CLOSE_FRAME, sizeof(SERVER_CLOSE_FRAME));
  GTEST_ASSERT_EQ(0, uv_is_active(
                         reinterpret_cast<uv_handle_t*>(&client_socket)));
}

TEST_F(InspectorSocketTest, ErrorCleansUpTheSocket) {
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  const char NOT_A_GOOD_FRAME[] = {'H', 'e', 'l', 'l', 'o'};
  do_write(NOT_A_GOOD_FRAME, sizeof(NOT_A_GOOD_FRAME));
  SPIN_WHILE(delegate != nullptr);
}

TEST_F(InspectorSocketTest, NoCloseResponseFromClient) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(delegate->inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!delegate->inspector_ready);
  expect_handshake();

  // 2. Brief exchange
  const char SERVER_MESSAGE[] = "abcd";
  const char CLIENT_FRAME[] = {'\x81', '\x04', 'a', 'b', 'c', 'd'};
  delegate->Write(SERVER_MESSAGE, sizeof(SERVER_MESSAGE) - 1);
  expect_on_client(CLIENT_FRAME, sizeof(CLIENT_FRAME));

  delegate->Close();
  expect_on_client(SERVER_CLOSE_FRAME, sizeof(SERVER_CLOSE_FRAME));
  uv_close(reinterpret_cast<uv_handle_t*>(&client_socket), nullptr);
  GTEST_ASSERT_EQ(0, uv_is_active(
                  reinterpret_cast<uv_handle_t*>(&client_socket)));
  delegate->WaitForDispose();
}

static bool delegate_called = false;

void shouldnt_be_called(enum inspector_handshake_event state,
                        const std::string& path, bool* cont) {
  delegate_called = true;
}

void expect_failure_no_delegate(const std::string& request) {
  delegate->SetDelegate(shouldnt_be_called);
  delegate_called = false;
  send_in_chunks(request.c_str(), request.length());
  expect_handshake_failure();
  SPIN_WHILE(delegate != nullptr);
  ASSERT_FALSE(delegate_called);
}

TEST_F(InspectorSocketTest, HostCheckedForGET) {
  const char GET_REQUEST[] = "GET /respond/withtext HTTP/1.1\r\n"
                             "Host: notlocalhost:9229\r\n\r\n";
  expect_failure_no_delegate(GET_REQUEST);
}

TEST_F(InspectorSocketTest, HostCheckedForUPGRADE) {
  const char UPGRADE_REQUEST[] = "GET /ws/path HTTP/1.1\r\n"
                                 "Host: nonlocalhost:9229\r\n"
                                 "Upgrade: websocket\r\n"
                                 "Connection: Upgrade\r\n"
                                 "Sec-WebSocket-Key: aaa==\r\n"
                                 "Sec-WebSocket-Version: 13\r\n\r\n";
  expect_failure_no_delegate(UPGRADE_REQUEST);
}

}  // anonymous namespace
