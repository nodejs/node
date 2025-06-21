#include "inspector_socket_server.h"

#include "node.h"
#include "util-inl.h"
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
                            bool include_protocol);
namespace {

static const uint8_t PROTOCOL_JSON[] = {
  #include "v8_inspector_protocol_json.h"  // NOLINT(build/include_order)
};

void Escape(std::string* string) {
  for (char& c : *string) {
    c = (c == '\"' || c == '\\') ? '_' : c;
  }
}

std::string FormatHostPort(const std::string& host, int port) {
  // Host is valid (socket was bound) so colon means it's a v6 IP address
  bool v6 = host.find(':') != std::string::npos;
  std::ostringstream url;
  if (v6) {
    url << '[';
  }
  url << host;
  if (v6) {
    url << ']';
  }
  url << ':' << port;
  return url.str();
}

std::string FormatAddress(const std::string& host,
                          const std::string& target_id,
                          bool include_protocol) {
  std::ostringstream url;
  if (include_protocol)
    url << "ws://";
  url << host << '/' << target_id;
  return url.str();
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

void SendHttpResponse(InspectorSocket* socket,
                      const std::string& response,
                      int code) {
  const char HEADERS[] = "HTTP/1.0 %d OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n";
  char header[sizeof(HEADERS) + 20];
  int header_len = snprintf(header,
                            sizeof(header),
                            HEADERS,
                            code,
                            response.size());
  socket->Write(header, header_len);
  socket->Write(response.data(), response.size());
}

void SendVersionResponse(InspectorSocket* socket) {
  std::map<std::string, std::string> response;
  response["Browser"] = "node.js/" NODE_VERSION;
  response["Protocol-Version"] = "1.1";
  SendHttpResponse(socket, MapToString(response), 200);
}

void SendHttpNotFound(InspectorSocket* socket) {
  SendHttpResponse(socket, "", 404);
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
  strm.next_out = reinterpret_cast<Byte*>(data.data());
  strm.avail_out = data.size();
  CHECK_EQ(Z_STREAM_END, inflate(&strm, Z_FINISH));
  CHECK_EQ(0, strm.avail_out);
  CHECK_EQ(Z_OK, inflateEnd(&strm));
  SendHttpResponse(socket, data, 200);
}
}  // namespace

std::string FormatWsAddress(const std::string& host, int port,
                            const std::string& target_id,
                            bool include_protocol) {
  return FormatAddress(FormatHostPort(host, port), target_id, include_protocol);
}

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
  void Accept(const std::string& ws_key) {
    ws_socket_->AcceptUpgrade(ws_key);
  }
  void Decline() {
    ws_socket_->CancelHandshake();
  }

  class Delegate : public InspectorSocket::Delegate {
   public:
    Delegate(InspectorSocketServer* server, int session_id)
             : server_(server), session_id_(session_id) { }
    ~Delegate() override {
      server_->SessionTerminated(session_id_);
    }
    void OnHttpGet(const std::string& host, const std::string& path) override;
    void OnSocketUpgrade(const std::string& host, const std::string& path,
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
};

class ServerSocket {
 public:
  explicit ServerSocket(InspectorSocketServer* server)
                        : tcp_socket_(uv_tcp_t()), server_(server) {}
  int Listen(sockaddr* addr, uv_loop_t* loop);
  void Close() {
    uv_close(reinterpret_cast<uv_handle_t*>(&tcp_socket_), FreeOnCloseCallback);
  }
  int port() const { return port_; }

 private:
  template <typename UvHandle>
  static ServerSocket* FromTcpSocket(UvHandle* socket) {
    return node::ContainerOf(&ServerSocket::tcp_socket_,
                             reinterpret_cast<uv_tcp_t*>(socket));
  }
  static void SocketConnectedCallback(uv_stream_t* tcp_socket, int status);
  static void FreeOnCloseCallback(uv_handle_t* tcp_socket_) {
    delete FromTcpSocket(tcp_socket_);
  }
  int DetectPort();
  ~ServerSocket() = default;

  uv_tcp_t tcp_socket_;
  InspectorSocketServer* server_;
  int port_ = -1;
};

void PrintDebuggerReadyMessage(
    const std::string& host,
    const std::vector<InspectorSocketServer::ServerSocketPtr>& server_sockets,
    const std::vector<std::string>& ids,
    const char* verb,
    bool publish_uid_stderr,
    FILE* out) {
  if (!publish_uid_stderr || out == nullptr) {
    return;
  }
  for (const auto& server_socket : server_sockets) {
    for (const std::string& id : ids) {
      fprintf(out, "Debugger %s on %s\n",
              verb,
              FormatWsAddress(host, server_socket->port(), id, true).c_str());
    }
  }
  fprintf(out, "For help, see: %s\n",
          "https://nodejs.org/en/docs/inspector");
  fflush(out);
}

InspectorSocketServer::InspectorSocketServer(
    std::unique_ptr<SocketServerDelegate> delegate, uv_loop_t* loop,
    const std::string& host, int port,
    const InspectPublishUid& inspect_publish_uid, FILE* out)
    : loop_(loop),
      delegate_(std::move(delegate)),
      host_(host),
      port_(port),
      inspect_publish_uid_(inspect_publish_uid),
      next_session_id_(0),
      out_(out) {
  delegate_->AssignServer(this);
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
    session->Decline();
    return;
  }
  connected_sessions_[session_id].first = id;
  session->Accept(ws_key);
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
      PrintDebuggerReadyMessage(host_,
                                server_sockets_,
                                delegate_->GetTargetIds(),
                                "ending",
                                inspect_publish_uid_.console,
                                out_);
    }
    if (state_ == ServerState::kStopped) {
      delegate_.reset();
    }
  }
}

bool InspectorSocketServer::HandleGetRequest(int session_id,
                                             const std::string& host,
                                             const std::string& path) {
  SocketSession* session = Session(session_id);
  InspectorSocket* socket = session->ws_socket();
  if (!inspect_publish_uid_.http) {
    SendHttpNotFound(socket);
    return true;
  }
  const char* command = MatchPathSegment(path.c_str(), "/json");
  if (command == nullptr)
    return false;

  if (MatchPathSegment(command, "list") || command[0] == '\0') {
    SendListResponse(socket, host, session);
    return true;
  } else if (MatchPathSegment(command, "protocol")) {
    SendProtocolJson(socket);
    return true;
  } else if (MatchPathSegment(command, "version")) {
    SendVersionResponse(socket);
    return true;
  }
  return false;
}

void InspectorSocketServer::SendListResponse(InspectorSocket* socket,
                                             const std::string& host,
                                             SocketSession* session) {
  std::vector<std::map<std::string, std::string>> response;
  for (const std::string& id : delegate_->GetTargetIds()) {
    response.push_back(std::map<std::string, std::string>());
    std::map<std::string, std::string>& target_map = response.back();
    target_map["description"] = "node.js instance";
    target_map["faviconUrl"] =
                        "https://nodejs.org/static/images/favicons/favicon.ico";
    target_map["id"] = id;
    target_map["title"] = delegate_->GetTargetTitle(id);
    Escape(&target_map["title"]);
    target_map["type"] = "node";
    // This attribute value is a "best effort" URL that is passed as a JSON
    // string. It is not guaranteed to resolve to a valid resource.
    target_map["url"] = delegate_->GetTargetUrl(id);
    Escape(&target_map["url"]);

    std::string detected_host = host;
    if (detected_host.empty()) {
      detected_host = FormatHostPort(socket->GetHost(),
                                     session->server_port());
    }
    std::string formatted_address = FormatAddress(detected_host, id, false);
    target_map["devtoolsFrontendUrl"] = GetFrontendURL(false,
                                                       formatted_address);
    // The compat URL is for Chrome browsers older than 66.0.3345.0
    target_map["devtoolsFrontendUrlCompat"] = GetFrontendURL(true,
                                                             formatted_address);
    target_map["webSocketDebuggerUrl"] = FormatAddress(detected_host, id, true);
  }
  SendHttpResponse(socket, MapsToString(response), 200);
}

std::string InspectorSocketServer::GetFrontendURL(bool is_compat,
    const std::string &formatted_address) {
  std::ostringstream frontend_url;
  frontend_url << "devtools://devtools/bundled/";
  frontend_url << (is_compat ? "inspector" : "js_app");
  frontend_url << ".html?experiments=true&v8only=true&ws=";
  frontend_url << formatted_address;
  return frontend_url.str();
}

bool InspectorSocketServer::Start() {
  CHECK_NOT_NULL(delegate_);
  CHECK_EQ(state_, ServerState::kNew);
  std::unique_ptr<SocketServerDelegate> delegate_holder;
  // We will return it if startup is successful
  delegate_.swap(delegate_holder);
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
    auto server_socket = ServerSocketPtr(new ServerSocket(this));
    err = server_socket->Listen(address->ai_addr, loop_);
    if (err == 0)
      server_sockets_.push_back(std::move(server_socket));
  }
  uv_freeaddrinfo(req.addrinfo);

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
  delegate_.swap(delegate_holder);
  state_ = ServerState::kRunning;
  PrintDebuggerReadyMessage(host_,
                            server_sockets_,
                            delegate_->GetTargetIds(),
                            "listening",
                            inspect_publish_uid_.console,
                            out_);
  return true;
}

void InspectorSocketServer::Stop() {
  if (state_ == ServerState::kStopped)
    return;
  CHECK_EQ(state_, ServerState::kRunning);
  state_ = ServerState::kStopped;
  server_sockets_.clear();
  if (done())
    delegate_.reset();
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

void InspectorSocketServer::Send(int session_id, const std::string& message) {
  SocketSession* session = Session(session_id);
  if (session != nullptr) {
    session->Send(message);
  }
}

void InspectorSocketServer::CloseServerSocket(ServerSocket* server) {
  server->Close();
}

// InspectorSession tracking
SocketSession::SocketSession(InspectorSocketServer* server, int id,
                             int server_port)
    : id_(id), server_port_(server_port) {}

void SocketSession::Send(const std::string& message) {
  ws_socket_->Write(message.data(), message.length());
}

void SocketSession::Delegate::OnHttpGet(const std::string& host,
                                        const std::string& path) {
  if (!server_->HandleGetRequest(session_id_, host, path))
    Session()->ws_socket()->CancelHandshake();
}

void SocketSession::Delegate::OnSocketUpgrade(const std::string& host,
                                              const std::string& path,
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

int ServerSocket::Listen(sockaddr* addr, uv_loop_t* loop) {
  uv_tcp_t* server = &tcp_socket_;
  CHECK_EQ(0, uv_tcp_init(loop, server));
  int err = uv_tcp_bind(server, addr, 0);
  if (err == 0) {
    // 511 is the value used by a 'net' module by default
    err = uv_listen(reinterpret_cast<uv_stream_t*>(server), 511,
                    ServerSocket::SocketConnectedCallback);
  }
  if (err == 0) {
    err = DetectPort();
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
}  // namespace inspector
}  // namespace node
