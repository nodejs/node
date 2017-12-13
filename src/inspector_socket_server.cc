#include "inspector_socket_server.h"

#include "node.h"
#include "uv.h"
#include "zlib.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace node {
namespace inspector {

// Function is declared in inspector_io.h so the rest of the node does not
// depend on inspector_socket_server.h
std::string FormatWsAddress(const std::string& host, int port,
                            const std::string& target_id,
                            bool include_protocol) {
  // Host is valid (socket was bound) so colon means it's a v6 IP address
  bool v6 = host.find(':') != std::string::npos;
  std::ostringstream url;
  if (include_protocol)
    url << "ws://";
  if (v6) {
    url << '[';
  }
  url << host;
  if (v6) {
    url << ']';
  }
  url << ':' << port << '/' << target_id;
  return url.str();
}

namespace {

static const uint8_t PROTOCOL_JSON[] = {
  #include "v8_inspector_protocol_json.h"  // NOLINT(build/include_order)
};

void Escape(std::string* string) {
  for (char& c : *string) {
    c = (c == '\"' || c == '\\') ? '_' : c;
  }
}

std::string MapToString(const std::map<std::string, std::string>& object) {
  bool first = true;
  std::ostringstream json;
  json << "{\n";
  for (const auto& name_value : object) {
    if (!first)
      json << ",\n";
    first = false;
    json << "  \"" << name_value.first << "\": \"";
    json << name_value.second << "\"";
  }
  json << "\n} ";
  return json.str();
}

std::string MapsToString(
    const std::vector<std::map<std::string, std::string>>& array) {
  bool first = true;
  std::ostringstream json;
  json << "[ ";
  for (const auto& object : array) {
    if (!first)
      json << ", ";
    first = false;
    json << MapToString(object);
  }
  json << "]\n\n";
  return json.str();
}

const char* MatchPathSegment(const char* path, const char* expected) {
  size_t len = strlen(expected);
  if (StringEqualNoCaseN(path, expected, len)) {
    if (path[len] == '/') return path + len + 1;
    if (path[len] == '\0') return path + len;
  }
  return nullptr;
}

void PrintDebuggerReadyMessage(const std::string& host,
                               int port,
                               const std::vector<std::string>& ids,
                               FILE* out) {
  if (out == nullptr) {
    return;
  }
  for (const std::string& id : ids) {
    fprintf(out, "Debugger listening on %s\n",
            FormatWsAddress(host, port, id, true).c_str());
  }
  fprintf(out, "For help see %s\n",
          "https://nodejs.org/en/docs/inspector");
  fflush(out);
}

void SendHttpResponse(InspectorSocket* socket, const std::string& response) {
  const char HEADERS[] = "HTTP/1.0 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n";
  char header[sizeof(HEADERS) + 20];
  int header_len = snprintf(header, sizeof(header), HEADERS, response.size());
  socket->Write(header, header_len);
  socket->Write(response.data(), response.size());
}

void SendVersionResponse(InspectorSocket* socket) {
  std::map<std::string, std::string> response;
  response["Browser"] = "node.js/" NODE_VERSION;
  response["Protocol-Version"] = "1.1";
  SendHttpResponse(socket, MapToString(response));
}

void SendProtocolJson(InspectorSocket* socket) {
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  CHECK_EQ(Z_OK, inflateInit(&strm));
  static const size_t kDecompressedSize =
      PROTOCOL_JSON[0] * 0x10000u +
      PROTOCOL_JSON[1] * 0x100u +
      PROTOCOL_JSON[2];
  strm.next_in = const_cast<uint8_t*>(PROTOCOL_JSON + 3);
  strm.avail_in = sizeof(PROTOCOL_JSON) - 3;
  std::string data(kDecompressedSize, '\0');
  strm.next_out = reinterpret_cast<Byte*>(&data[0]);
  strm.avail_out = data.size();
  CHECK_EQ(Z_STREAM_END, inflate(&strm, Z_FINISH));
  CHECK_EQ(0, strm.avail_out);
  CHECK_EQ(Z_OK, inflateEnd(&strm));
  SendHttpResponse(socket, data);
}
}  // namespace


class Closer {
 public:
  explicit Closer(InspectorSocketServer* server) : server_(server),
                                                   close_count_(0) { }

  void AddCallback(InspectorSocketServer::ServerCallback callback) {
    if (callback == nullptr)
      return;
    callbacks_.insert(callback);
  }

  void DecreaseExpectedCount() {
    --close_count_;
    NotifyIfDone();
  }

  void IncreaseExpectedCount() {
    ++close_count_;
  }

  void NotifyIfDone() {
    if (close_count_ == 0) {
      for (auto callback : callbacks_) {
        callback(server_);
      }
      InspectorSocketServer* server = server_;
      delete server->closer_;
      server->closer_ = nullptr;
    }
  }

 private:
  InspectorSocketServer* server_;
  std::set<InspectorSocketServer::ServerCallback> callbacks_;
  int close_count_;
};

class SocketSession {
 public:
  SocketSession(InspectorSocketServer* server, int id, int server_port);
  void Close() {
    ws_socket_.reset();
  }
  void Send(const std::string& message);
  void Own(InspectorSocket::Pointer ws_socket) {
    ws_socket_ = std::move(ws_socket);
  }
  int id() const { return id_; }
  int server_port() {
    return server_port_;
  }
  InspectorSocket* ws_socket() {
    return ws_socket_.get();
  }
  void set_ws_key(const std::string& ws_key) {
    ws_key_ = ws_key;
  }
  void Accept() {
    ws_socket_->AcceptUpgrade(ws_key_);
  }
  void Decline() {
    ws_socket_->CancelHandshake();
  }

  class Delegate : public InspectorSocket::Delegate {
   public:
    Delegate(InspectorSocketServer* server, int session_id)
             : server_(server), session_id_(session_id) { }
    ~Delegate() {
      server_->SessionTerminated(session_id_);
    }
    void OnHttpGet(const std::string& path) override;
    void OnSocketUpgrade(const std::string& path,
                         const std::string& ws_key) override;
    void OnWsFrame(const std::vector<char>& data) override;

   private:
    SocketSession* Session() {
      return server_->Session(session_id_);
    }

    InspectorSocketServer* server_;
    int session_id_;
  };

 private:
  const int id_;
  InspectorSocket::Pointer ws_socket_;
  const int server_port_;
  std::string ws_key_;
};

class ServerSocket {
 public:
  static int Listen(InspectorSocketServer* inspector_server,
                    sockaddr* addr, uv_loop_t* loop);
  void Close() {
    uv_close(reinterpret_cast<uv_handle_t*>(&tcp_socket_),
             SocketClosedCallback);
  }
  int port() const { return port_; }

 private:
  explicit ServerSocket(InspectorSocketServer* server)
      : tcp_socket_(uv_tcp_t()), server_(server), port_(-1) {}
  template<typename UvHandle>
  static ServerSocket* FromTcpSocket(UvHandle* socket) {
    return node::ContainerOf(&ServerSocket::tcp_socket_,
                             reinterpret_cast<uv_tcp_t*>(socket));
  }
  static void SocketConnectedCallback(uv_stream_t* tcp_socket, int status);
  static void SocketClosedCallback(uv_handle_t* tcp_socket);
  static void FreeOnCloseCallback(uv_handle_t* tcp_socket_) {
    delete FromTcpSocket(tcp_socket_);
  }
  int DetectPort();

  uv_tcp_t tcp_socket_;
  InspectorSocketServer* server_;
  int port_;
};

InspectorSocketServer::InspectorSocketServer(SocketServerDelegate* delegate,
                                             uv_loop_t* loop,
                                             const std::string& host,
                                             int port,
                                             FILE* out) : loop_(loop),
                                                          delegate_(delegate),
                                                          host_(host),
                                                          port_(port),
                                                          closer_(nullptr),
                                                          next_session_id_(0),
                                                          out_(out) {
  state_ = ServerState::kNew;
}

InspectorSocketServer::~InspectorSocketServer() = default;

SocketSession* InspectorSocketServer::Session(int session_id) {
  auto it = connected_sessions_.find(session_id);
  return it == connected_sessions_.end() ? nullptr : it->second.second.get();
}

void InspectorSocketServer::SessionStarted(int session_id,
                                           const std::string& id,
                                           const std::string& ws_key) {
  SocketSession* session = Session(session_id);
  if (!TargetExists(id)) {
    Session(session_id)->Decline();
    return;
  }
  connected_sessions_[session_id].first = id;
  session->set_ws_key(ws_key);
  delegate_->StartSession(session_id, id);
}

void InspectorSocketServer::SessionTerminated(int session_id) {
  if (Session(session_id) == nullptr) {
    return;
  }
  bool was_attached = connected_sessions_[session_id].first != "";
  if (was_attached) {
    delegate_->EndSession(session_id);
  }
  connected_sessions_.erase(session_id);
  if (connected_sessions_.empty()) {
    if (was_attached && state_ == ServerState::kRunning
        && !server_sockets_.empty()) {
      PrintDebuggerReadyMessage(host_, server_sockets_[0]->port(),
                                delegate_->GetTargetIds(), out_);
    }
    if (state_ == ServerState::kStopped) {
      delegate_->ServerDone();
    }
  }
}

bool InspectorSocketServer::HandleGetRequest(int session_id,
                                             const std::string& path) {
  SocketSession* session = Session(session_id);
  InspectorSocket* socket = session->ws_socket();
  const char* command = MatchPathSegment(path.c_str(), "/json");
  if (command == nullptr)
    return false;

  if (MatchPathSegment(command, "list") || command[0] == '\0') {
    SendListResponse(socket, session);
    return true;
  } else if (MatchPathSegment(command, "protocol")) {
    SendProtocolJson(socket);
    return true;
  } else if (MatchPathSegment(command, "version")) {
    SendVersionResponse(socket);
    return true;
  } else if (const char* target_id = MatchPathSegment(command, "activate")) {
    if (TargetExists(target_id)) {
      SendHttpResponse(socket, "Target activated");
      return true;
    }
    return false;
  }
  return false;
}

void InspectorSocketServer::SendListResponse(InspectorSocket* socket,
                                             SocketSession* session) {
  std::vector<std::map<std::string, std::string>> response;
  for (const std::string& id : delegate_->GetTargetIds()) {
    response.push_back(std::map<std::string, std::string>());
    std::map<std::string, std::string>& target_map = response.back();
    target_map["description"] = "node.js instance";
    target_map["faviconUrl"] = "https://nodejs.org/static/favicon.ico";
    target_map["id"] = id;
    target_map["title"] = delegate_->GetTargetTitle(id);
    Escape(&target_map["title"]);
    target_map["type"] = "node";
    // This attribute value is a "best effort" URL that is passed as a JSON
    // string. It is not guaranteed to resolve to a valid resource.
    target_map["url"] = delegate_->GetTargetUrl(id);
    Escape(&target_map["url"]);

    bool connected = false;
    for (const auto& session : connected_sessions_) {
      if (session.second.first == id) {
        connected = true;
        break;
      }
    }
    if (!connected) {
      std::string host = socket->GetHost();
      int port = session->server_port();
      std::ostringstream frontend_url;
      frontend_url << "chrome-devtools://devtools/bundled";
      frontend_url << "/inspector.html?experiments=true&v8only=true&ws=";
      frontend_url << FormatWsAddress(host, port, id, false);
      target_map["devtoolsFrontendUrl"] += frontend_url.str();
      target_map["webSocketDebuggerUrl"] =
          FormatWsAddress(host, port, id, true);
    }
  }
  SendHttpResponse(socket, MapsToString(response));
}

bool InspectorSocketServer::Start() {
  CHECK_EQ(state_, ServerState::kNew);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_NUMERICSERV;
  hints.ai_socktype = SOCK_STREAM;
  uv_getaddrinfo_t req;
  const std::string port_string = std::to_string(port_);
  int err = uv_getaddrinfo(loop_, &req, nullptr, host_.c_str(),
                           port_string.c_str(), &hints);
  if (err < 0) {
    if (out_ != nullptr) {
      fprintf(out_, "Unable to resolve \"%s\": %s\n", host_.c_str(),
              uv_strerror(err));
    }
    return false;
  }
  for (addrinfo* address = req.addrinfo; address != nullptr;
       address = address->ai_next) {
    err = ServerSocket::Listen(this, address->ai_addr, loop_);
  }
  uv_freeaddrinfo(req.addrinfo);

  if (!connected_sessions_.empty()) {
    return true;
  }
  // We only show error if we failed to start server on all addresses. We only
  // show one error, for the last address.
  if (server_sockets_.empty()) {
    if (out_ != nullptr) {
      fprintf(out_, "Starting inspector on %s:%d failed: %s\n",
              host_.c_str(), port_, uv_strerror(err));
      fflush(out_);
    }
    return false;
  }
  state_ = ServerState::kRunning;
  // getaddrinfo sorts the addresses, so the first port is most relevant.
  PrintDebuggerReadyMessage(host_, server_sockets_[0]->port(),
                            delegate_->GetTargetIds(), out_);
  return true;
}

void InspectorSocketServer::Stop(ServerCallback cb) {
  CHECK_EQ(state_, ServerState::kRunning);
  if (closer_ == nullptr) {
    closer_ = new Closer(this);
  }
  closer_->AddCallback(cb);
  closer_->IncreaseExpectedCount();
  state_ = ServerState::kStopping;
  for (ServerSocket* server_socket : server_sockets_)
    server_socket->Close();
  closer_->NotifyIfDone();
}

void InspectorSocketServer::TerminateConnections() {
  for (const auto& key_value : connected_sessions_)
    key_value.second.second->Close();
}

bool InspectorSocketServer::TargetExists(const std::string& id) {
  const std::vector<std::string>& target_ids = delegate_->GetTargetIds();
  const auto& found = std::find(target_ids.begin(), target_ids.end(), id);
  return found != target_ids.end();
}

void InspectorSocketServer::ServerSocketListening(ServerSocket* server_socket) {
  server_sockets_.push_back(server_socket);
}

void InspectorSocketServer::ServerSocketClosed(ServerSocket* server_socket) {
  CHECK_EQ(state_, ServerState::kStopping);

  server_sockets_.erase(std::remove(server_sockets_.begin(),
                                    server_sockets_.end(), server_socket),
                        server_sockets_.end());
  if (!server_sockets_.empty())
    return;

  if (closer_ != nullptr) {
    closer_->DecreaseExpectedCount();
  }
  if (connected_sessions_.empty()) {
    delegate_->ServerDone();
  }
  state_ = ServerState::kStopped;
}

int InspectorSocketServer::Port() const {
  if (!server_sockets_.empty()) {
    return server_sockets_[0]->port();
  }
  return port_;
}

void InspectorSocketServer::Accept(int server_port,
                                   uv_stream_t* server_socket) {
  std::unique_ptr<SocketSession> session(
      new SocketSession(this, next_session_id_++, server_port));

  InspectorSocket::DelegatePointer delegate =
      InspectorSocket::DelegatePointer(
          new SocketSession::Delegate(this, session->id()));

  InspectorSocket::Pointer inspector =
      InspectorSocket::Accept(server_socket, std::move(delegate));
  if (inspector) {
    session->Own(std::move(inspector));
    connected_sessions_[session->id()].second = std::move(session);
  }
}

void InspectorSocketServer::AcceptSession(int session_id) {
  SocketSession* session = Session(session_id);
  if (session == nullptr) {
    delegate_->EndSession(session_id);
  } else {
    session->Accept();
  }
}

void InspectorSocketServer::DeclineSession(int session_id) {
  auto it = connected_sessions_.find(session_id);
  if (it != connected_sessions_.end()) {
    it->second.first.clear();
    it->second.second->Decline();
  }
}

void InspectorSocketServer::Send(int session_id, const std::string& message) {
  SocketSession* session = Session(session_id);
  if (session != nullptr) {
    session->Send(message);
  }
}

// InspectorSession tracking
SocketSession::SocketSession(InspectorSocketServer* server, int id,
                             int server_port)
    : id_(id), server_port_(server_port) {}

void SocketSession::Send(const std::string& message) {
  ws_socket_->Write(message.data(), message.length());
}

void SocketSession::Delegate::OnHttpGet(const std::string& path) {
  if (!server_->HandleGetRequest(session_id_, path))
    Session()->ws_socket()->CancelHandshake();
}

void SocketSession::Delegate::OnSocketUpgrade(const std::string& path,
                                              const std::string& ws_key) {
  std::string id = path.empty() ? path : path.substr(1);
  server_->SessionStarted(session_id_, id, ws_key);
}

void SocketSession::Delegate::OnWsFrame(const std::vector<char>& data) {
  server_->MessageReceived(session_id_,
                           std::string(data.data(), data.size()));
}

// ServerSocket implementation
int ServerSocket::DetectPort() {
  sockaddr_storage addr;
  int len = sizeof(addr);
  int err = uv_tcp_getsockname(&tcp_socket_,
                               reinterpret_cast<struct sockaddr*>(&addr), &len);
  if (err != 0)
    return err;
  int port;
  if (addr.ss_family == AF_INET6)
    port = reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_port;
  else
    port = reinterpret_cast<const sockaddr_in*>(&addr)->sin_port;
  port_ = ntohs(port);
  return err;
}

// static
int ServerSocket::Listen(InspectorSocketServer* inspector_server,
                         sockaddr* addr, uv_loop_t* loop) {
  ServerSocket* server_socket = new ServerSocket(inspector_server);
  uv_tcp_t* server = &server_socket->tcp_socket_;
  CHECK_EQ(0, uv_tcp_init(loop, server));
  int err = uv_tcp_bind(server, addr, 0);
  if (err == 0) {
    err = uv_listen(reinterpret_cast<uv_stream_t*>(server), 1,
                    ServerSocket::SocketConnectedCallback);
  }
  if (err == 0) {
    err = server_socket->DetectPort();
  }
  if (err == 0) {
    inspector_server->ServerSocketListening(server_socket);
  } else {
    uv_close(reinterpret_cast<uv_handle_t*>(server), FreeOnCloseCallback);
  }
  return err;
}

// static
void ServerSocket::SocketConnectedCallback(uv_stream_t* tcp_socket,
                                           int status) {
  if (status == 0) {
    ServerSocket* server_socket = ServerSocket::FromTcpSocket(tcp_socket);
    // Memory is freed when the socket closes.
    server_socket->server_->Accept(server_socket->port_, tcp_socket);
  }
}

// static
void ServerSocket::SocketClosedCallback(uv_handle_t* tcp_socket) {
  ServerSocket* server_socket = ServerSocket::FromTcpSocket(tcp_socket);
  server_socket->server_->ServerSocketClosed(server_socket);
  delete server_socket;
}

}  // namespace inspector
}  // namespace node
