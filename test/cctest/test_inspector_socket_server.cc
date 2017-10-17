#include "inspector_socket_server.h"

#include "node.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <sstream>

static uv_loop_t loop;

static const char HOST[] = "127.0.0.1";

static const char CLIENT_CLOSE_FRAME[] = "\x88\x80\x2D\x0E\x1E\xFA";
static const char SERVER_CLOSE_FRAME[] = "\x88\x00";

static const char MAIN_TARGET_ID[] = "main-target";
static const char UNCONNECTABLE_TARGET_ID[] = "unconnectable-target";

static const char WS_HANDSHAKE_RESPONSE[] =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: Dt87H1OULVZnSJo/KgMUYI7xPCg=\r\n\r\n";

#define SPIN_WHILE(condition)                                                  \
  {                                                                            \
    Timeout timeout(&loop);                                                    \
    while ((condition) && !timeout.timed_out) {                                \
      uv_run(&loop, UV_RUN_ONCE);                                              \
    }                                                                          \
    ASSERT_FALSE((condition));                                                 \
  }

namespace {

using InspectorSocketServer = node::inspector::InspectorSocketServer;
using SocketServerDelegate = node::inspector::SocketServerDelegate;

class Timeout {
 public:
  explicit Timeout(uv_loop_t* loop) : timed_out(false), done_(false) {
    uv_timer_init(loop, &timer_);
    uv_timer_start(&timer_, Timeout::set_flag, 5000, 0);
    uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
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

class InspectorSocketServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_EQ(0, uv_loop_init(&loop));
  }

  void TearDown() override {
    const int err = uv_loop_close(&loop);
    if (err != 0) {
      uv_print_all_handles(&loop, stderr);
    }
    EXPECT_EQ(0, err);
  }
};

class TestInspectorServerDelegate : public SocketServerDelegate {
 public:
  TestInspectorServerDelegate() : connected(0), disconnected(0), done(false),
                                  targets_({ MAIN_TARGET_ID,
                                             UNCONNECTABLE_TARGET_ID }),
                                  session_id_(0) {}

  void Connect(InspectorSocketServer* server) {
    server_ = server;
  }

  bool StartSession(int session_id, const std::string& target_id) override {
    buffer_.clear();
    CHECK_NE(targets_.end(),
             std::find(targets_.begin(), targets_.end(), target_id));
    if (target_id == UNCONNECTABLE_TARGET_ID) {
      return false;
    }
    connected++;
    session_id_ = session_id;
    return true;
  }

  void MessageReceived(int session_id, const std::string& message) override {
    CHECK_EQ(session_id_, session_id);
    buffer_.insert(buffer_.end(), message.begin(), message.end());
  }

  void EndSession(int session_id) override {
    CHECK_EQ(session_id_, session_id);
    disconnected++;
  }

  std::vector<std::string> GetTargetIds() override {
    return targets_;
  }

  std::string GetTargetTitle(const std::string& id) override {
    return id + " Target Title";
  }

  std::string GetTargetUrl(const std::string& id) override {
    return "file://" + id + "/script.js";
  }

  void Expect(const std::string& expects) {
    SPIN_WHILE(buffer_.size() < expects.length());
    ASSERT_STREQ(std::string(buffer_.data(), expects.length()).c_str(),
                 expects.c_str());
    buffer_.erase(buffer_.begin(), buffer_.begin() + expects.length());
  }

  void Write(const std::string& message) {
    server_->Send(session_id_, message);
  }

  void ServerDone() override {
    done = true;
  }

  int connected;
  int disconnected;
  bool done;

 private:
  const std::vector<std::string> targets_;
  InspectorSocketServer* server_;
  int session_id_;
  std::vector<char> buffer_;
};

class SocketWrapper {
 public:
  explicit SocketWrapper(uv_loop_t* loop) : closed_(false),
                                            eof_(false),
                                            loop_(loop),
                                            socket_(uv_tcp_t()),
                                            connected_(false),
                                            sending_(false) { }

  void Connect(std::string host, int port, bool v6 = false) {
    closed_ = false;
    connection_failed_ = false;
    connected_ = false;
    eof_ = false;
    contents_.clear();
    uv_tcp_init(loop_, &socket_);
    union {sockaddr generic; sockaddr_in v4; sockaddr_in6 v6;} addr;
    int err = 0;
    if (v6) {
      err = uv_ip6_addr(host.c_str(), port, &addr.v6);
    } else {
      err = uv_ip4_addr(host.c_str(), port, &addr.v4);
    }
    CHECK_EQ(0, err);
    err = uv_tcp_connect(&connect_, &socket_, &addr.generic, Connected_);
    CHECK_EQ(0, err);
    SPIN_WHILE(!connected_)
    uv_read_start(reinterpret_cast<uv_stream_t*>(&socket_), AllocCallback,
                  ReadCallback);
  }

  void ExpectFailureToConnect(std::string host, int port) {
    connected_ = false;
    connection_failed_ = false;
    closed_ = false;
    eof_ = false;
    contents_.clear();
    uv_tcp_init(loop_, &socket_);
    sockaddr_in addr;
    int err = uv_ip4_addr(host.c_str(), port, &addr);
    CHECK_EQ(0, err);
    err = uv_tcp_connect(&connect_, &socket_,
                         reinterpret_cast<const sockaddr*>(&addr),
                         ConnectionMustFail_);
    CHECK_EQ(0, err);
    SPIN_WHILE(!connection_failed_)
    uv_read_start(reinterpret_cast<uv_stream_t*>(&socket_), AllocCallback,
                  ReadCallback);
  }

  void Close() {
    uv_close(reinterpret_cast<uv_handle_t*>(&socket_), ClosedCallback);
    SPIN_WHILE(!closed_);
  }

  void Expect(const std::string& expects) {
    SPIN_WHILE(contents_.size() < expects.length());
    ASSERT_STREQ(expects.c_str(),
                 std::string(contents_.data(), expects.length()).c_str());
    contents_.erase(contents_.begin(), contents_.begin() + expects.length());
  }

  void ExpectEOF() {
    SPIN_WHILE(!eof_);
    Close();
  }

  void TestHttpRequest(const std::string& path,
                       const std::string& expected_reply) {
    std::ostringstream expectations;
    expectations << "HTTP/1.0 200 OK\r\n"
                    "Content-Type: application/json; charset=UTF-8\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Content-Length: ";
    expectations << expected_reply.length() + 2;
    expectations << "\r\n\r\n" << expected_reply << "\n\n";
    Write("GET " + path + " HTTP/1.1\r\n"
          "Host: localhost:9229\r\n\r\n");
    Expect(expectations.str());
  }

  void Write(const std::string& data) {
    ASSERT_FALSE(sending_);
    uv_buf_t buf[1];
    buf[0].base = const_cast<char*>(data.data());
    buf[0].len = data.length();
    sending_ = true;
    int err = uv_write(&write_, reinterpret_cast<uv_stream_t*>(&socket_),
                       buf, 1, WriteDone_);
    CHECK_EQ(err, 0);
    SPIN_WHILE(sending_);
  }

 private:
  static void AllocCallback(uv_handle_t*, size_t size, uv_buf_t* buf) {
    *buf = uv_buf_init(new char[size], size);
  }

  static void ClosedCallback(uv_handle_t* handle) {
    SocketWrapper* wrapper =
        node::ContainerOf(&SocketWrapper::socket_,
                          reinterpret_cast<uv_tcp_t*>(handle));
    ASSERT_FALSE(wrapper->closed_);
    wrapper->closed_ = true;
  }

  static void Connected_(uv_connect_t* connect, int status) {
    EXPECT_EQ(0, status) << "Unable to connect: " << uv_strerror(status);
    SocketWrapper* wrapper =
        node::ContainerOf(&SocketWrapper::connect_, connect);
    wrapper->connected_ = true;
  }

  static void ConnectionMustFail_(uv_connect_t* connect, int status) {
    EXPECT_EQ(UV_ECONNREFUSED, status);
    SocketWrapper* wrapper =
        node::ContainerOf(&SocketWrapper::connect_, connect);
    wrapper->connection_failed_ = true;
  }

  static void ReadCallback(uv_stream_t* stream, ssize_t read,
                           const uv_buf_t* buf) {
    SocketWrapper* wrapper =
        node::ContainerOf(&SocketWrapper::socket_,
                          reinterpret_cast<uv_tcp_t*>(stream));
    if (read == UV_EOF) {
      wrapper->eof_ = true;
    } else {
      wrapper->contents_.insert(wrapper->contents_.end(), buf->base,
                                buf->base + read);
    }
    delete[] buf->base;
  }
  static void WriteDone_(uv_write_t* req, int err) {
    CHECK_EQ(0, err);
    SocketWrapper* wrapper =
        node::ContainerOf(&SocketWrapper::write_, req);
    ASSERT_TRUE(wrapper->sending_);
    wrapper->sending_ = false;
  }
  bool IsConnected() { return connected_; }

  bool closed_;
  bool eof_;
  uv_loop_t* loop_;
  uv_tcp_t socket_;
  uv_connect_t connect_;
  uv_write_t write_;
  bool connected_;
  bool connection_failed_;
  bool sending_;
  std::vector<char> contents_;
};

class ServerHolder {
 public:
  template <typename Delegate>
  ServerHolder(Delegate* delegate, uv_loop_t* loop, int port)
               : ServerHolder(delegate, loop, HOST, port, NULL) { }

  template <typename Delegate>
  ServerHolder(Delegate* delegate, uv_loop_t* loop, const std::string host,
               int port, FILE* out)
               : closed(false), paused(false),
                 server_(delegate, loop, host, port, out) {
    delegate->Connect(&server_);
  }

  InspectorSocketServer* operator->() {
    return &server_;
  }

  int port() {
    return server_.Port();
  }

  static void CloseCallback(InspectorSocketServer* server) {
    ServerHolder* holder = node::ContainerOf(&ServerHolder::server_, server);
    holder->closed = true;
  }

  static void PausedCallback(InspectorSocketServer* server) {
    ServerHolder* holder = node::ContainerOf(&ServerHolder::server_, server);
    holder->paused = true;
  }

  bool closed;
  bool paused;

 private:
  InspectorSocketServer server_;
};

class ServerDelegateNoTargets : public SocketServerDelegate {
 public:
  void Connect(InspectorSocketServer* server) { }
  void MessageReceived(int session_id, const std::string& message) override { }
  void EndSession(int session_id) override { }

  bool StartSession(int session_id, const std::string& target_id) override {
    return false;
  }

  std::vector<std::string> GetTargetIds() override {
    return std::vector<std::string>();
  }

  std::string GetTargetTitle(const std::string& id) override {
    return "";
  }

  std::string GetTargetUrl(const std::string& id) override {
    return "";
  }

  void ServerDone() override {
    done = true;
  }

  bool done = false;
};

static void TestHttpRequest(int port, const std::string& path,
                            const std::string& expected_body) {
  SocketWrapper socket(&loop);
  socket.Connect(HOST, port);
  socket.TestHttpRequest(path, expected_body);
  socket.Close();
}

static const std::string WsHandshakeRequest(const std::string& target_id) {
  return "GET /" + target_id + " HTTP/1.1\r\n"
         "Host: localhost:9229\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: aaa==\r\n"
         "Sec-WebSocket-Version: 13\r\n\r\n";
}
}  // namespace


TEST_F(InspectorSocketServerTest, InspectorSessions) {
  TestInspectorServerDelegate delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());

  SocketWrapper well_behaved_socket(&loop);
  // Regular connection
  well_behaved_socket.Connect(HOST, server.port());
  well_behaved_socket.Write(WsHandshakeRequest(MAIN_TARGET_ID));
  well_behaved_socket.Expect(WS_HANDSHAKE_RESPONSE);


  EXPECT_EQ(1, delegate.connected);

  well_behaved_socket.Write("\x81\x84\x7F\xC2\x66\x31\x4E\xF0\x55\x05");

  delegate.Expect("1234");
  delegate.Write("5678");

  well_behaved_socket.Expect("\x81\x4" "5678");

  well_behaved_socket.Write(CLIENT_CLOSE_FRAME);
  well_behaved_socket.Expect(SERVER_CLOSE_FRAME);

  EXPECT_EQ(1, delegate.disconnected);

  well_behaved_socket.Close();

  // Declined connection
  SocketWrapper declined_target_socket(&loop);
  declined_target_socket.Connect(HOST, server.port());
  declined_target_socket.Write(WsHandshakeRequest(UNCONNECTABLE_TARGET_ID));
  declined_target_socket.Expect("HTTP/1.0 400 Bad Request");
  declined_target_socket.ExpectEOF();
  EXPECT_EQ(1, delegate.connected);
  EXPECT_EQ(1, delegate.disconnected);

  // Bogus target - start session callback should not even be invoked
  SocketWrapper bogus_target_socket(&loop);
  bogus_target_socket.Connect(HOST, server.port());
  bogus_target_socket.Write(WsHandshakeRequest("bogus_target"));
  bogus_target_socket.Expect("HTTP/1.0 400 Bad Request");
  bogus_target_socket.ExpectEOF();
  EXPECT_EQ(1, delegate.connected);
  EXPECT_EQ(1, delegate.disconnected);

  // Drop connection (no proper close frames)
  SocketWrapper dropped_connection_socket(&loop);
  dropped_connection_socket.Connect(HOST, server.port());
  dropped_connection_socket.Write(WsHandshakeRequest(MAIN_TARGET_ID));
  dropped_connection_socket.Expect(WS_HANDSHAKE_RESPONSE);

  EXPECT_EQ(2, delegate.connected);

  delegate.Write("5678");
  dropped_connection_socket.Expect("\x81\x4" "5678");

  dropped_connection_socket.Close();
  SPIN_WHILE(delegate.disconnected < 2);

  // Reconnect regular connection
  SocketWrapper stays_till_termination_socket(&loop);
  stays_till_termination_socket.Connect(HOST, server.port());
  stays_till_termination_socket.Write(WsHandshakeRequest(MAIN_TARGET_ID));
  stays_till_termination_socket.Expect(WS_HANDSHAKE_RESPONSE);

  SPIN_WHILE(3 != delegate.connected);

  delegate.Write("5678");
  stays_till_termination_socket.Expect("\x81\x4" "5678");

  stays_till_termination_socket
      .Write("\x81\x84\x7F\xC2\x66\x31\x4E\xF0\x55\x05");
  delegate.Expect("1234");

  server->Stop(ServerHolder::CloseCallback);
  server->TerminateConnections();

  stays_till_termination_socket.Write(CLIENT_CLOSE_FRAME);
  stays_till_termination_socket.Expect(SERVER_CLOSE_FRAME);

  SPIN_WHILE(3 != delegate.disconnected);

  SPIN_WHILE(!server.closed);
  stays_till_termination_socket.ExpectEOF();
}

TEST_F(InspectorSocketServerTest, ServerDoesNothing) {
  TestInspectorServerDelegate delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());

  server->Stop(ServerHolder::CloseCallback);
  server->TerminateConnections();
  SPIN_WHILE(!server.closed);
  ASSERT_TRUE(delegate.done);
}

TEST_F(InspectorSocketServerTest, ServerWithoutTargets) {
  ServerDelegateNoTargets delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());
  TestHttpRequest(server.port(), "/json/list", "[ ]");
  TestHttpRequest(server.port(), "/json", "[ ]");

  // Declined connection
  SocketWrapper socket(&loop);
  socket.Connect(HOST, server.port());
  socket.Write(WsHandshakeRequest(UNCONNECTABLE_TARGET_ID));
  socket.Expect("HTTP/1.0 400 Bad Request");
  socket.ExpectEOF();
  server->Stop(ServerHolder::CloseCallback);
  server->TerminateConnections();
  SPIN_WHILE(!server.closed);
}

TEST_F(InspectorSocketServerTest, ServerCannotStart) {
  ServerDelegateNoTargets delegate1, delegate2;
  ServerHolder server1(&delegate1, &loop, 0);
  ASSERT_TRUE(server1->Start());
  ServerHolder server2(&delegate2, &loop, server1.port());
  ASSERT_FALSE(server2->Start());
  server1->Stop(ServerHolder::CloseCallback);
  server1->TerminateConnections();
  SPIN_WHILE(!server1.closed);
  ASSERT_TRUE(delegate1.done);
}

TEST_F(InspectorSocketServerTest, StoppingServerDoesNotKillConnections) {
  ServerDelegateNoTargets delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());
  SocketWrapper socket1(&loop);
  socket1.Connect(HOST, server.port());
  socket1.TestHttpRequest("/json/list", "[ ]");
  server->Stop(ServerHolder::CloseCallback);
  SPIN_WHILE(!server.closed);
  socket1.TestHttpRequest("/json/list", "[ ]");
  socket1.Close();
  uv_run(&loop, UV_RUN_DEFAULT);
  ASSERT_TRUE(delegate.done);
}

TEST_F(InspectorSocketServerTest, ClosingConnectionReportsDone) {
  ServerDelegateNoTargets delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());
  SocketWrapper socket1(&loop);
  socket1.Connect(HOST, server.port());
  socket1.TestHttpRequest("/json/list", "[ ]");
  server->Stop(ServerHolder::CloseCallback);
  SPIN_WHILE(!server.closed);
  socket1.TestHttpRequest("/json/list", "[ ]");
  socket1.Close();
  uv_run(&loop, UV_RUN_DEFAULT);
  ASSERT_TRUE(delegate.done);
}

TEST_F(InspectorSocketServerTest, ClosingSocketReportsDone) {
  TestInspectorServerDelegate delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());
  SocketWrapper socket1(&loop);
  socket1.Connect(HOST, server.port());
  socket1.Write(WsHandshakeRequest(MAIN_TARGET_ID));
  socket1.Expect(WS_HANDSHAKE_RESPONSE);
  server->Stop(ServerHolder::CloseCallback);
  SPIN_WHILE(!server.closed);
  ASSERT_FALSE(delegate.done);
  socket1.Close();
  SPIN_WHILE(!delegate.done);
}

TEST_F(InspectorSocketServerTest, TerminatingSessionReportsDone) {
  TestInspectorServerDelegate delegate;
  ServerHolder server(&delegate, &loop, 0);
  ASSERT_TRUE(server->Start());
  SocketWrapper socket1(&loop);
  socket1.Connect(HOST, server.port());
  socket1.Write(WsHandshakeRequest(MAIN_TARGET_ID));
  socket1.Expect(WS_HANDSHAKE_RESPONSE);
  server->Stop(ServerHolder::CloseCallback);
  SPIN_WHILE(!server.closed);
  ASSERT_FALSE(delegate.done);
  server->TerminateConnections();
  socket1.Expect(SERVER_CLOSE_FRAME);
  socket1.Write(CLIENT_CLOSE_FRAME);
  socket1.ExpectEOF();
  SPIN_WHILE(!delegate.done);
}

TEST_F(InspectorSocketServerTest, FailsToBindToNodejsHost) {
  TestInspectorServerDelegate delegate;
  ServerHolder server(&delegate, &loop, "nodejs.org", 80, nullptr);
  ASSERT_FALSE(server->Start());
  SPIN_WHILE(uv_loop_alive(&loop));
}

bool has_ipv6_address() {
  uv_interface_address_s* addresses = nullptr;
  int address_count = 0;
  int err = uv_interface_addresses(&addresses, &address_count);
  if (err != 0) {
    return false;
  }
  bool has_address = false;
  for (int i = 0; i < address_count; i++) {
    if (addresses[i].address.address6.sin6_family == AF_INET6) {
      has_address = true;
    }
  }
  uv_free_interface_addresses(addresses, address_count);
  return has_address;
}

TEST_F(InspectorSocketServerTest, BindsToIpV6) {
  if (!has_ipv6_address()) {
    fprintf(stderr, "No IPv6 network detected\n");
    return;
  }
  TestInspectorServerDelegate delegate;
  ServerHolder server(&delegate, &loop, "::", 0, NULL);
  ASSERT_TRUE(server->Start());

  SocketWrapper socket1(&loop);
  socket1.Connect("::1", server.port(), true);
  socket1.Write(WsHandshakeRequest(MAIN_TARGET_ID));
  socket1.Expect(WS_HANDSHAKE_RESPONSE);
  server->Stop(ServerHolder::CloseCallback);
  SPIN_WHILE(!server.closed);
  ASSERT_FALSE(delegate.done);
  socket1.Close();
  SPIN_WHILE(!delegate.done);
}
