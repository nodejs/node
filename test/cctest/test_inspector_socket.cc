#include "inspector_socket.h"
#include "gtest/gtest.h"

#define PORT 9444

namespace {

using node::inspector::InspectorSocket;
using node::inspector::inspector_from_stream;
using node::inspector::inspector_handshake_event;
using node::inspector::kInspectorHandshakeFailed;
using node::inspector::kInspectorHandshakeHttpGet;
using node::inspector::kInspectorHandshakeUpgraded;
using node::inspector::kInspectorHandshakeUpgrading;

static const int MAX_LOOP_ITERATIONS = 10000;

#define SPIN_WHILE(condition)                                                  \
  {                                                                            \
    Timeout timeout(&loop);                                                    \
    while ((condition) && !timeout.timed_out) {                              \
      uv_run(&loop, UV_RUN_NOWAIT);                                            \
    }                                                                          \
    ASSERT_FALSE((condition));                                                 \
  }

static bool connected = false;
static bool inspector_ready = false;
static int handshake_events = 0;
static enum inspector_handshake_event last_event = kInspectorHandshakeHttpGet;
static uv_loop_t loop;
static uv_tcp_t server, client_socket;
static InspectorSocket inspector;
static std::string last_path;  // NOLINT(runtime/string)
static void (*handshake_delegate)(enum inspector_handshake_event state,
                                  const std::string& path,
                                  bool* should_continue);

struct read_expects {
  const char* expected;
  size_t expected_len;
  size_t pos;
  bool read_expected;
  bool callback_called;
};

static const char HANDSHAKE_REQ[] = "GET /ws/path HTTP/1.1\r\n"
                                    "Host: localhost:9222\r\n"
                                    "Upgrade: websocket\r\n"
                                    "Connection: Upgrade\r\n"
                                    "Sec-WebSocket-Key: aaa==\r\n"
                                    "Sec-WebSocket-Version: 13\r\n\r\n";

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

static void stop_if_stop_path(enum inspector_handshake_event state,
                              const std::string& path, bool* cont) {
  *cont = path.empty() || path != "/close";
}

static bool connected_cb(InspectorSocket* socket,
                         enum inspector_handshake_event state,
                         const std::string& path) {
  inspector_ready = state == kInspectorHandshakeUpgraded;
  last_event = state;
  if (path.empty()) {
    last_path = "@@@ Nothing received @@@";
  } else {
    last_path = path;
  }
  handshake_events++;
  bool should_continue = true;
  handshake_delegate(state, path, &should_continue);
  return should_continue;
}

static void on_new_connection(uv_stream_t* server, int status) {
  GTEST_ASSERT_EQ(0, status);
  connected = true;
  inspector_accept(server, static_cast<InspectorSocket*>(server->data),
                   connected_cb);
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

static void buffer_alloc_cb(uv_handle_t* stream, size_t len, uv_buf_t* buf) {
  buf->base = new char[len];
  buf->len = len;
}

static void check_data_cb(read_expects* expectation, ssize_t nread,
                          const uv_buf_t* buf, bool* retval) {
  *retval = false;
  EXPECT_TRUE(nread >= 0 && nread != UV_EOF);
  ssize_t i;
  char c, actual;
  ASSERT_GT(expectation->expected_len, 0);
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

struct expectations {
  std::string actual_data;
  size_t actual_offset;
  size_t actual_end;
  int err_code;
};

static void grow_expects_buffer(uv_handle_t* stream, size_t size, uv_buf_t* b) {
  expectations* expects = static_cast<expectations*>(
      inspector_from_stream(stream)->data);
  size_t end = expects->actual_end;
  // Grow the buffer in chunks of 64k.
  size_t new_length = (end + size + 65535) & ~((size_t) 0xFFFF);
  expects->actual_data.resize(new_length);
  *b = uv_buf_init(&expects->actual_data[end], new_length - end);
}

// static void dump_hex(const char* buf, size_t len) {
//   const char* ptr = buf;
//   const char* end = ptr + len;
//   const char* cptr;
//   char c;
//   int i;

//   while (ptr < end) {
//     cptr = ptr;
//     for (i = 0; i < 16 && ptr < end; i++) {
//       printf("%2.2X  ", *(ptr++));
//     }
//     for (i = 72 - (i * 4); i > 0; i--) {
//       printf(" ");
//     }
//     for (i = 0; i < 16 && cptr < end; i++) {
//       c = *(cptr++);
//       printf("%c", (c > 0x19) ? c : '.');
//     }
//     printf("\n");
//   }
//   printf("\n\n");
// }

static void save_read_data(uv_stream_t* stream, ssize_t nread,
                           const uv_buf_t* buf) {
  expectations* expects = static_cast<expectations*>(
      inspector_from_stream(stream)->data);
  expects->err_code = nread < 0 ? nread : 0;
  if (nread > 0) {
    expects->actual_end += nread;
  }
}

static void setup_inspector_expecting() {
  if (inspector.data) {
    return;
  }
  expectations* expects = new expectations();
  inspector.data = expects;
  inspector_read_start(&inspector, grow_expects_buffer, save_read_data);
}

static void expect_on_server(const char* data, size_t len) {
  setup_inspector_expecting();
  expectations* expects = static_cast<expectations*>(inspector.data);
  for (size_t i = 0; i < len;) {
    SPIN_WHILE(expects->actual_offset == expects->actual_end);
    for (; i < len && expects->actual_offset < expects->actual_end; i++) {
      char actual = expects->actual_data[expects->actual_offset++];
      char expected = data[i];
      if (expected != actual) {
        fprintf(stderr, "Character %zu:\n", i);
        GTEST_ASSERT_EQ(expected, actual);
      }
    }
  }
  expects->actual_end -= expects->actual_offset;
  if (!expects->actual_end) {
    memmove(&expects->actual_data[0],
            &expects->actual_data[expects->actual_offset],
            expects->actual_end);
  }
  expects->actual_offset = 0;
}

static void inspector_record_error_code(uv_stream_t* stream, ssize_t nread,
                                        const uv_buf_t* buf) {
  InspectorSocket *inspector = inspector_from_stream(stream);
  // Increment instead of assign is to ensure the function is only called once
  *(static_cast<int*>(inspector->data)) += nread;
}

static void expect_server_read_error() {
  setup_inspector_expecting();
  expectations* expects = static_cast<expectations*>(inspector.data);
  SPIN_WHILE(expects->err_code != UV_EPROTO);
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

static bool waiting_to_close = true;

void handle_closed(uv_handle_t* handle) { waiting_to_close = false; }

static void really_close(uv_handle_t* handle) {
  waiting_to_close = true;
  if (!uv_is_closing(handle)) {
    uv_close(handle, handle_closed);
    SPIN_WHILE(waiting_to_close);
  }
}

// Called when the test leaves inspector socket in active state
static void manual_inspector_socket_cleanup() {
  EXPECT_EQ(0, uv_is_active(
                   reinterpret_cast<uv_handle_t*>(&inspector.client)));
  really_close(reinterpret_cast<uv_handle_t*>(&inspector.client));
  delete inspector.ws_state;
  inspector.ws_state = nullptr;
  delete inspector.http_parsing_state;
  inspector.http_parsing_state = nullptr;
  inspector.buffer.clear();
}

static void assert_both_sockets_closed() {
  SPIN_WHILE(uv_is_active(reinterpret_cast<uv_handle_t*>(&client_socket)));
  SPIN_WHILE(uv_is_active(reinterpret_cast<uv_handle_t*>(&inspector.client)));
}

static void on_connection(uv_connect_t* connect, int status) {
  GTEST_ASSERT_EQ(0, status);
  connect->data = connect;
}

class InspectorSocketTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    inspector.reinit();
    handshake_delegate = stop_if_stop_path;
    handshake_events = 0;
    connected = false;
    inspector_ready = false;
    last_event = kInspectorHandshakeHttpGet;
    GTEST_ASSERT_EQ(0, uv_loop_init(&loop));
    server = uv_tcp_t();
    client_socket = uv_tcp_t();
    server.data = &inspector;
    sockaddr_in addr;
    uv_tcp_init(&loop, &server);
    uv_tcp_init(&loop, &client_socket);
    uv_ip4_addr("127.0.0.1", PORT, &addr);
    uv_tcp_bind(&server, reinterpret_cast<const struct sockaddr*>(&addr), 0);
    int err = uv_listen(reinterpret_cast<uv_stream_t*>(&server),
                        1, on_new_connection);
    GTEST_ASSERT_EQ(0, err);
    uv_connect_t connect;
    connect.data = nullptr;
    uv_tcp_connect(&connect, &client_socket,
                   reinterpret_cast<const sockaddr*>(&addr), on_connection);
    uv_tcp_nodelay(&client_socket, 1);  // The buffering messes up the test
    SPIN_WHILE(!connect.data || !connected);
    really_close(reinterpret_cast<uv_handle_t*>(&server));
  }

  virtual void TearDown() {
    really_close(reinterpret_cast<uv_handle_t*>(&client_socket));
    EXPECT_TRUE(inspector.buffer.empty());
    expectations* expects = static_cast<expectations*>(inspector.data);
    if (expects != nullptr) {
      GTEST_ASSERT_EQ(expects->actual_end, expects->actual_offset);
      delete expects;
      inspector.data = nullptr;
    }
    const int err = uv_loop_close(&loop);
    if (err != 0) {
      uv_print_all_handles(&loop, stderr);
    }
    EXPECT_EQ(0, err);
  }
};

TEST_F(InspectorSocketTest, ReadsAndWritesInspectorMessage) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!inspector_ready);
  expect_handshake();

  // 2. Brief exchange
  const char SERVER_MESSAGE[] = "abcd";
  const char CLIENT_FRAME[] = {'\x81', '\x04', 'a', 'b', 'c', 'd'};
  inspector_write(&inspector, SERVER_MESSAGE, sizeof(SERVER_MESSAGE) - 1);
  expect_on_client(CLIENT_FRAME, sizeof(CLIENT_FRAME));

  const char SERVER_FRAME[] = {'\x81', '\x84', '\x7F', '\xC2', '\x66',
                               '\x31', '\x4E', '\xF0', '\x55', '\x05'};
  const char CLIENT_MESSAGE[] = "1234";
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  expect_on_server(CLIENT_MESSAGE, sizeof(CLIENT_MESSAGE) - 1);

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
  expect_on_server(EXPECT, sizeof(EXPECT) - 1);
  inspector_read_stop(&inspector);
  manual_inspector_socket_cleanup();
}

TEST_F(InspectorSocketTest, AcceptsRequestInSeveralWrites) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(inspector_ready);
  // Specifically, break up the request in the "Sec-WebSocket-Key" header name
  // and value
  const int write1 = 95;
  const int write2 = 5;
  const int write3 = sizeof(HANDSHAKE_REQ) - write1 - write2 - 1;
  do_write(const_cast<char*>(HANDSHAKE_REQ), write1);
  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ) + write1, write2);
  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ) + write1 + write2, write3);
  SPIN_WHILE(!inspector_ready);
  expect_handshake();
  inspector_read_stop(&inspector);
  GTEST_ASSERT_EQ(uv_is_active(reinterpret_cast<uv_handle_t*>(&client_socket)),
                  0);
  manual_inspector_socket_cleanup();
}

TEST_F(InspectorSocketTest, ExtraTextBeforeRequest) {
  last_event = kInspectorHandshakeUpgraded;
  char UNCOOL_BRO[] = "Uncool, bro: Text before the first req\r\n";
  do_write(const_cast<char*>(UNCOOL_BRO), sizeof(UNCOOL_BRO) - 1);

  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(last_event != kInspectorHandshakeFailed);
  expect_handshake_failure();
  assert_both_sockets_closed();
}

TEST_F(InspectorSocketTest, ExtraLettersBeforeRequest) {
  char UNCOOL_BRO[] = "Uncool!!";
  do_write(const_cast<char*>(UNCOOL_BRO), sizeof(UNCOOL_BRO) - 1);

  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(last_event != kInspectorHandshakeFailed);
  expect_handshake_failure();
  assert_both_sockets_closed();
}

TEST_F(InspectorSocketTest, RequestWithoutKey) {
  const char BROKEN_REQUEST[] = "GET / HTTP/1.1\r\n"
                                "Host: localhost:9222\r\n"
                                "Upgrade: websocket\r\n"
                                "Connection: Upgrade\r\n"
                                "Sec-WebSocket-Version: 13\r\n\r\n";

  do_write(const_cast<char*>(BROKEN_REQUEST), sizeof(BROKEN_REQUEST) - 1);
  SPIN_WHILE(last_event != kInspectorHandshakeFailed);
  expect_handshake_failure();
  assert_both_sockets_closed();
}

TEST_F(InspectorSocketTest, KillsConnectionOnProtocolViolation) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!inspector_ready);
  ASSERT_TRUE(inspector_ready);
  expect_handshake();
  const char SERVER_FRAME[] = "I'm not a good WS frame. Nope!";
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  expect_server_read_error();
  GTEST_ASSERT_EQ(uv_is_active(reinterpret_cast<uv_handle_t*>(&client_socket)),
                  0);
}

TEST_F(InspectorSocketTest, CanStopReadingFromInspector) {
  ASSERT_TRUE(connected);
  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  ASSERT_TRUE(inspector_ready);

  // 2. Brief exchange
  const char SERVER_FRAME[] = {'\x81', '\x84', '\x7F', '\xC2', '\x66',
                               '\x31', '\x4E', '\xF0', '\x55', '\x05'};
  const char CLIENT_MESSAGE[] = "1234";
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  expect_on_server(CLIENT_MESSAGE, sizeof(CLIENT_MESSAGE) - 1);

  inspector_read_stop(&inspector);
  do_write(SERVER_FRAME, sizeof(SERVER_FRAME));
  GTEST_ASSERT_EQ(uv_is_active(
                      reinterpret_cast<uv_handle_t*>(&client_socket)), 0);
  manual_inspector_socket_cleanup();
}

static int inspector_closed = 0;

void inspector_closed_cb(InspectorSocket *inspector, int code) {
  inspector_closed++;
}

TEST_F(InspectorSocketTest, CloseDoesNotNotifyReadCallback) {
  inspector_closed = 0;
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();

  int error_code = 0;
  inspector.data = &error_code;
  inspector_read_start(&inspector, buffer_alloc_cb,
                       inspector_record_error_code);
  inspector_close(&inspector, inspector_closed_cb);
  char CLOSE_FRAME[] = {'\x88', '\x00'};
  expect_on_client(CLOSE_FRAME, sizeof(CLOSE_FRAME));
  EXPECT_EQ(0, inspector_closed);
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
  EXPECT_NE(UV_EOF, error_code);
  SPIN_WHILE(inspector_closed == 0);
  EXPECT_EQ(1, inspector_closed);
  inspector.data = nullptr;
}

TEST_F(InspectorSocketTest, CloseWorksWithoutReadEnabled) {
  inspector_closed = 0;
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  inspector_close(&inspector, inspector_closed_cb);
  char CLOSE_FRAME[] = {'\x88', '\x00'};
  expect_on_client(CLOSE_FRAME, sizeof(CLOSE_FRAME));
  EXPECT_EQ(0, inspector_closed);
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
  SPIN_WHILE(inspector_closed == 0);
  EXPECT_EQ(1, inspector_closed);
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

static void ReportsHttpGet_handshake(enum inspector_handshake_event state,
                                     const std::string& path, bool* cont) {
  *cont = true;
  enum inspector_handshake_event expected_state = kInspectorHandshakeHttpGet;
  std::string expected_path;
  switch (handshake_events) {
  case 1:
    expected_path = "/some/path";
    break;
  case 2:
    expected_path = "/respond/withtext";
    inspector_write(&inspector, TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);
    break;
  case 3:
    expected_path = "/some/path2";
    break;
  case 5:
    expected_state = kInspectorHandshakeFailed;
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
}

TEST_F(InspectorSocketTest, ReportsHttpGet) {
  handshake_delegate = ReportsHttpGet_handshake;

  const char GET_REQ[] = "GET /some/path HTTP/1.1\r\n"
                         "Host: localhost:9222\r\n"
                         "Sec-WebSocket-Key: aaa==\r\n"
                         "Sec-WebSocket-Version: 13\r\n\r\n";
  send_in_chunks(GET_REQ, sizeof(GET_REQ) - 1);

  expect_nothing_on_client();

  const char WRITE_REQUEST[] = "GET /respond/withtext HTTP/1.1\r\n"
                               "Host: localhost:9222\r\n\r\n";
  send_in_chunks(WRITE_REQUEST, sizeof(WRITE_REQUEST) - 1);

  expect_on_client(TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);

  const char GET_REQS[] = "GET /some/path2 HTTP/1.1\r\n"
                          "Host: localhost:9222\r\n"
                          "Sec-WebSocket-Key: aaa==\r\n"
                          "Sec-WebSocket-Version: 13\r\n\r\n"
                          "GET /close HTTP/1.1\r\n"
                          "Host: localhost:9222\r\n"
                          "Sec-WebSocket-Key: aaa==\r\n"
                          "Sec-WebSocket-Version: 13\r\n\r\n";
  send_in_chunks(GET_REQS, sizeof(GET_REQS) - 1);

  expect_handshake_failure();
  EXPECT_EQ(5, handshake_events);
}

static void
HandshakeCanBeCanceled_handshake(enum inspector_handshake_event state,
                                 const std::string& path, bool* cont) {
  switch (handshake_events - 1) {
  case 0:
    EXPECT_EQ(kInspectorHandshakeUpgrading, state);
    EXPECT_EQ("/ws/path", path);
    break;
  case 1:
    EXPECT_EQ(kInspectorHandshakeFailed, state);
    EXPECT_TRUE(path.empty());
    break;
  default:
    EXPECT_TRUE(false);
    break;
  }
  *cont = false;
}

TEST_F(InspectorSocketTest, HandshakeCanBeCanceled) {
  handshake_delegate = HandshakeCanBeCanceled_handshake;

  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);

  expect_handshake_failure();
  EXPECT_EQ(2, handshake_events);
}

static void GetThenHandshake_handshake(enum inspector_handshake_event state,
                                       const std::string& path, bool* cont) {
  *cont = true;
  std::string expected_path = "/ws/path";
  switch (handshake_events - 1) {
  case 0:
    EXPECT_EQ(kInspectorHandshakeHttpGet, state);
    expected_path = "/respond/withtext";
    inspector_write(&inspector, TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);
    break;
  case 1:
    EXPECT_EQ(kInspectorHandshakeUpgrading, state);
    break;
  case 2:
    EXPECT_EQ(kInspectorHandshakeUpgraded, state);
    break;
  default:
    EXPECT_TRUE(false);
    break;
  }
  EXPECT_EQ(expected_path, path);
}

TEST_F(InspectorSocketTest, GetThenHandshake) {
  handshake_delegate = GetThenHandshake_handshake;
  const char WRITE_REQUEST[] = "GET /respond/withtext HTTP/1.1\r\n"
                               "Host: localhost:9222\r\n\r\n";
  send_in_chunks(WRITE_REQUEST, sizeof(WRITE_REQUEST) - 1);

  expect_on_client(TEST_SUCCESS, sizeof(TEST_SUCCESS) - 1);

  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  EXPECT_EQ(3, handshake_events);
  manual_inspector_socket_cleanup();
}

static void WriteBeforeHandshake_inspector_delegate(inspector_handshake_event e,
                                                    const std::string& path,
                                                    bool* cont) {
  if (e == kInspectorHandshakeFailed)
    inspector_closed = 1;
}

TEST_F(InspectorSocketTest, WriteBeforeHandshake) {
  const char MESSAGE1[] = "Message 1";
  const char MESSAGE2[] = "Message 2";
  const char EXPECTED[] = "Message 1Message 2";

  inspector_write(&inspector, MESSAGE1, sizeof(MESSAGE1) - 1);
  inspector_write(&inspector, MESSAGE2, sizeof(MESSAGE2) - 1);
  expect_on_client(EXPECTED, sizeof(EXPECTED) - 1);
  inspector_closed = 0;
  handshake_delegate = WriteBeforeHandshake_inspector_delegate;
  really_close(reinterpret_cast<uv_handle_t*>(&client_socket));
  SPIN_WHILE(inspector_closed == 0);
}

static void CleanupSocketAfterEOF_close_cb(InspectorSocket* inspector,
                                           int status) {
  *(static_cast<bool*>(inspector->data)) = true;
}

static void CleanupSocketAfterEOF_read_cb(uv_stream_t* stream, ssize_t nread,
                                          const uv_buf_t* buf) {
  EXPECT_EQ(UV_EOF, nread);
  InspectorSocket* insp = inspector_from_stream(stream);
  inspector_close(insp, CleanupSocketAfterEOF_close_cb);
}

TEST_F(InspectorSocketTest, CleanupSocketAfterEOF) {
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();

  inspector_read_start(&inspector, buffer_alloc_cb,
                       CleanupSocketAfterEOF_read_cb);

  for (int i = 0; i < MAX_LOOP_ITERATIONS; ++i) {
    uv_run(&loop, UV_RUN_NOWAIT);
  }

  uv_close(reinterpret_cast<uv_handle_t*>(&client_socket), nullptr);
  bool flag = false;
  inspector.data = &flag;
  SPIN_WHILE(!flag);
  inspector.data = nullptr;
}

TEST_F(InspectorSocketTest, EOFBeforeHandshake) {
  const char MESSAGE[] = "We'll send EOF afterwards";
  inspector_write(&inspector, MESSAGE, sizeof(MESSAGE) - 1);
  expect_on_client(MESSAGE, sizeof(MESSAGE) - 1);
  uv_close(reinterpret_cast<uv_handle_t*>(&client_socket), nullptr);
  SPIN_WHILE(last_event != kInspectorHandshakeFailed);
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
  ASSERT_FALSE(inspector_ready);
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  SPIN_WHILE(!inspector_ready);
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

  inspector_write(&inspector, &message[0], message.size());
  expect_on_client(&expected[0], expected.size());

  char MASK[4] = {'W', 'h', 'O', 'a'};

  const char FRAME_TO_SERVER_HEADER[] = {
    '\x81', '\xff', '\x00', '\x00', '\x00', '\x00', '\x00', '\x0F',
    '\x42', '\x40', MASK[0], MASK[1], MASK[2], MASK[3]
  };

  std::string outgoing(FRAME_TO_SERVER_HEADER, sizeof(FRAME_TO_SERVER_HEADER));
  outgoing.resize(outgoing.size() + message.size());
  mask_message(message, &outgoing[sizeof(FRAME_TO_SERVER_HEADER)], MASK);

  setup_inspector_expecting();  // Buffer on the client side.
  do_write(&outgoing[0], outgoing.size());
  expect_on_server(&message[0], message.size());

  // 3. Close
  const char CLIENT_CLOSE_FRAME[] = {'\x88', '\x80', '\x2D',
                                     '\x0E', '\x1E', '\xFA'};
  const char SERVER_CLOSE_FRAME[] = {'\x88', '\x00'};
  do_write(CLIENT_CLOSE_FRAME, sizeof(CLIENT_CLOSE_FRAME));
  expect_on_client(SERVER_CLOSE_FRAME, sizeof(SERVER_CLOSE_FRAME));
  GTEST_ASSERT_EQ(0, uv_is_active(
                         reinterpret_cast<uv_handle_t*>(&client_socket)));
}

static ssize_t err;

void ErrorCleansUpTheSocket_cb(uv_stream_t* stream, ssize_t read,
                               const uv_buf_t* buf) {
  err = read;
}

TEST_F(InspectorSocketTest, ErrorCleansUpTheSocket) {
  inspector_closed = 0;
  do_write(const_cast<char*>(HANDSHAKE_REQ), sizeof(HANDSHAKE_REQ) - 1);
  expect_handshake();
  const char NOT_A_GOOD_FRAME[] = {'H', 'e', 'l', 'l', 'o'};
  err = 42;
  inspector_read_start(&inspector, buffer_alloc_cb,
                       ErrorCleansUpTheSocket_cb);
  do_write(NOT_A_GOOD_FRAME, sizeof(NOT_A_GOOD_FRAME));
  SPIN_WHILE(err > 0);
  EXPECT_EQ(UV_EPROTO, err);
}

}  // anonymous namespace
