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

namespace {

static const uint8_t PROTOCOL_JSON[] = {
  #include "v8_inspector_protocol_json.h"  // NOLINT(build/include_order)
};

void Escape(std::string* string) {
  for (char& c : *string) {
    c = (c == '\"' || c == '\\') ? '_' : c;
  }
}

std::string GetWsUrl(int port, const std::string& id) {
  char buf[1024];
  snprintf(buf, sizeof(buf), "127.0.0.1:%d/%s", port, id.c_str());
  return buf;
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

void OnBufferAlloc(uv_handle_t* handle, size_t len, uv_buf_t* buf) {
  buf->base = new char[len];
  buf->len = len;
}

void PrintDebuggerReadyMessage(int port, const std::vector<std::string>& ids) {
  fprintf(stderr,
          "Debugger listening on port %d.\n"
          "Warning: This is an experimental feature "
          "and could change at any time.\n",
          port);
  if (ids.size() == 1)
    fprintf(stderr, "To start debugging, open the following URL in Chrome:\n");
  if (ids.size() > 1)
    fprintf(stderr, "To start debugging, open the following URLs in Chrome:\n");
  for (const std::string& id : ids) {
    fprintf(stderr,
            "    chrome-devtools://devtools/bundled/inspector.html?"
            "experiments=true&v8only=true&ws=%s\n", GetWsUrl(port, id).c_str());
  }
  fflush(stderr);
}

void SendHttpResponse(InspectorSocket* socket, const std::string& response) {
  const char HEADERS[] = "HTTP/1.0 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n";
  char header[sizeof(HEADERS) + 20];
  int header_len = snprintf(header, sizeof(header), HEADERS, response.size());
  inspector_write(socket, header, header_len);
  inspector_write(socket, response.data(), response.size());
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
  SocketSession(InspectorSocketServer* server, int id);
  void Close(bool socket_cleanup, Closer* closer);
  void Declined() { state_ = State::kDeclined; }
  static SocketSession* From(InspectorSocket* socket) {
    return node::ContainerOf(&SocketSession::socket_, socket);
  }
  void FrontendConnected();
  InspectorSocketServer* GetServer() { return server_; }
  int Id() { return id_; }
  void Send(const std::string& message);
  void SetTargetId(const std::string& target_id) {
    CHECK(target_id_.empty());
    target_id_ = target_id;
  }
  InspectorSocket* Socket() { return &socket_; }
  const std::string TargetId() { return target_id_; }

 private:
  enum class State { kHttp, kWebSocket, kClosing, kEOF, kDeclined };
  static void CloseCallback_(InspectorSocket* socket, int code);
  static void ReadCallback_(uv_stream_t* stream, ssize_t read,
                            const uv_buf_t* buf);
  void OnRemoteDataIO(InspectorSocket* socket, ssize_t read,
                      const uv_buf_t* buf);
  const int id_;
  Closer* closer_;
  InspectorSocket socket_;
  InspectorSocketServer* server_;
  std::string target_id_;
  State state_;
};

InspectorSocketServer::InspectorSocketServer(SocketServerDelegate* delegate,
                                             int port) : loop_(nullptr),
                                                         delegate_(delegate),
                                                         port_(port),
                                                         closer_(nullptr),
                                                         next_session_id_(0) { }


// static
bool InspectorSocketServer::HandshakeCallback(InspectorSocket* socket,
                                              inspector_handshake_event event,
                                              const std::string& path) {
  InspectorSocketServer* server = SocketSession::From(socket)->GetServer();
  const std::string& id = path.empty() ? path : path.substr(1);
  switch (event) {
  case kInspectorHandshakeHttpGet:
    return server->RespondToGet(socket, path);
  case kInspectorHandshakeUpgrading:
    return server->SessionStarted(SocketSession::From(socket), id);
  case kInspectorHandshakeUpgraded:
    SocketSession::From(socket)->FrontendConnected();
    return true;
  case kInspectorHandshakeFailed:
    SocketSession::From(socket)->Close(false, nullptr);
    return false;
  default:
    UNREACHABLE();
    return false;
  }
}

bool InspectorSocketServer::SessionStarted(SocketSession* session,
                                           const std::string& id) {
  bool connected = false;
  if (TargetExists(id)) {
    connected = delegate_->StartSession(session->Id(), id);
  }
  if (connected) {
    connected_sessions_[session->Id()] = session;
    session->SetTargetId(id);
  } else {
    session->Declined();
  }
  return connected;
}

void InspectorSocketServer::SessionTerminated(int session_id) {
  if (connected_sessions_.erase(session_id) == 0) {
    return;
  }
  delegate_->EndSession(session_id);
  if (connected_sessions_.empty() &&
      uv_is_active(reinterpret_cast<uv_handle_t*>(&server_))) {
    PrintDebuggerReadyMessage(port_, delegate_->GetTargetIds());
  }
}

bool InspectorSocketServer::RespondToGet(InspectorSocket* socket,
                                         const std::string& path) {
  const char* command = MatchPathSegment(path.c_str(), "/json");
  if (command == nullptr)
    return false;

  if (MatchPathSegment(command, "list") || command[0] == '\0') {
    SendListResponse(socket);
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

void InspectorSocketServer::SendListResponse(InspectorSocket* socket) {
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
      if (session.second->TargetId() == id) {
        connected = true;
        break;
      }
    }
    if (!connected) {
      std::string address = GetWsUrl(port_, id);
      std::ostringstream frontend_url;
      frontend_url << "chrome-devtools://devtools/bundled";
      frontend_url << "/inspector.html?experiments=true&v8only=true&ws=";
      frontend_url << address;
      target_map["devtoolsFrontendUrl"] += frontend_url.str();
      target_map["webSocketDebuggerUrl"] = "ws://" + address;
    }
  }
  SendHttpResponse(socket, MapsToString(response));
}

bool InspectorSocketServer::Start(uv_loop_t* loop) {
  loop_ = loop;
  sockaddr_in addr;
  uv_tcp_init(loop_, &server_);
  uv_ip4_addr("0.0.0.0", port_, &addr);
  int err = uv_tcp_bind(&server_,
                        reinterpret_cast<const struct sockaddr*>(&addr), 0);
  if (err == 0) {
    err = uv_listen(reinterpret_cast<uv_stream_t*>(&server_), 1,
                    SocketConnectedCallback);
  }
  if (err == 0 && connected_sessions_.empty()) {
    PrintDebuggerReadyMessage(port_, delegate_->GetTargetIds());
  }
  if (err != 0 && connected_sessions_.empty()) {
    fprintf(stderr, "Unable to open devtools socket: %s\n", uv_strerror(err));
    uv_close(reinterpret_cast<uv_handle_t*>(&server_), nullptr);
    return false;
  }
  return true;
}

void InspectorSocketServer::Stop(ServerCallback cb) {
  if (closer_ == nullptr) {
    closer_ = new Closer(this);
  }
  closer_->AddCallback(cb);

  uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(&server_);
  if (uv_is_active(handle)) {
    closer_->IncreaseExpectedCount();
    uv_close(reinterpret_cast<uv_handle_t*>(&server_), ServerClosedCallback);
  }
  closer_->NotifyIfDone();
}

void InspectorSocketServer::TerminateConnections(ServerCallback cb) {
  if (closer_ == nullptr) {
    closer_ = new Closer(this);
  }
  closer_->AddCallback(cb);
  std::map<int, SocketSession*> sessions;
  std::swap(sessions, connected_sessions_);
  for (const auto& session : sessions) {
    int id = session.second->Id();
    session.second->Close(true, closer_);
    delegate_->EndSession(id);
  }
  closer_->NotifyIfDone();
}

bool InspectorSocketServer::TargetExists(const std::string& id) {
  const std::vector<std::string>& target_ids = delegate_->GetTargetIds();
  const auto& found = std::find(target_ids.begin(), target_ids.end(), id);
  return found != target_ids.end();
}

void InspectorSocketServer::Send(int session_id, const std::string& message) {
  auto session_iterator = connected_sessions_.find(session_id);
  if (session_iterator != connected_sessions_.end()) {
    session_iterator->second->Send(message);
  }
}

// static
void InspectorSocketServer::ServerClosedCallback(uv_handle_t* server) {
  InspectorSocketServer* socket_server = InspectorSocketServer::From(server);
  if (socket_server->closer_)
    socket_server->closer_->DecreaseExpectedCount();
}

// static
void InspectorSocketServer::SocketConnectedCallback(uv_stream_t* server,
                                                    int status) {
  if (status == 0) {
    InspectorSocketServer* socket_server = InspectorSocketServer::From(server);
    SocketSession* session =
        new SocketSession(socket_server, socket_server->next_session_id_++);
    if (inspector_accept(server, session->Socket(), HandshakeCallback) != 0) {
      delete session;
    }
  }
}

// InspectorSession tracking
SocketSession::SocketSession(InspectorSocketServer* server, int id)
                             : id_(id), closer_(nullptr), server_(server),
                               state_(State::kHttp) { }

void SocketSession::Close(bool socket_cleanup, Closer* closer) {
  CHECK_EQ(closer_, nullptr);
  CHECK_NE(state_, State::kClosing);
  server_->SessionTerminated(id_);
  if (socket_cleanup) {
    state_ = State::kClosing;
    closer_ = closer;
    if (closer_ != nullptr)
      closer->IncreaseExpectedCount();
    inspector_close(&socket_, CloseCallback_);
  } else {
    delete this;
  }
}

// static
void SocketSession::CloseCallback_(InspectorSocket* socket, int code) {
  SocketSession* session = SocketSession::From(socket);
  CHECK_EQ(State::kClosing, session->state_);
  Closer* closer = session->closer_;
  if (closer != nullptr)
    closer->DecreaseExpectedCount();
  delete session;
}

void SocketSession::FrontendConnected() {
  CHECK_EQ(State::kHttp, state_);
  state_ = State::kWebSocket;
  inspector_read_start(&socket_, OnBufferAlloc, ReadCallback_);
}

// static
void SocketSession::ReadCallback_(uv_stream_t* stream, ssize_t read,
                                  const uv_buf_t* buf) {
  InspectorSocket* socket = inspector_from_stream(stream);
  SocketSession::From(socket)->OnRemoteDataIO(socket, read, buf);
}

void SocketSession::OnRemoteDataIO(InspectorSocket* socket, ssize_t read,
                                   const uv_buf_t* buf) {
  if (read > 0) {
    server_->Delegate()->MessageReceived(id_, std::string(buf->base, read));
  } else {
    server_->SessionTerminated(id_);
    Close(true, nullptr);
  }
  if (buf != nullptr && buf->base != nullptr)
    delete[] buf->base;
}

void SocketSession::Send(const std::string& message) {
  inspector_write(&socket_, message.data(), message.length());
}

}  // namespace inspector
}  // namespace node
