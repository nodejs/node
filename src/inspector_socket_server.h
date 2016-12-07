#ifndef SRC_INSPECTOR_SOCKET_SERVER_H_
#define SRC_INSPECTOR_SOCKET_SERVER_H_

#include "inspector_agent.h"
#include "inspector_socket.h"
#include "uv.h"

#include <map>
#include <string>
#include <vector>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

namespace node {
namespace inspector {

class Closer;
class SocketSession;

class SocketServerDelegate {
 public:
  virtual bool StartSession(int session_id, const std::string& target_id) = 0;
  virtual void EndSession(int session_id) = 0;
  virtual void MessageReceived(int session_id, const std::string& message) = 0;
  virtual std::vector<std::string> GetTargetIds() = 0;
  virtual std::string GetTargetTitle(const std::string& id) = 0;
  virtual std::string GetTargetUrl(const std::string& id) = 0;
};

class InspectorSocketServer {
 public:
  using ServerCallback = void (*)(InspectorSocketServer*);
  InspectorSocketServer(SocketServerDelegate* delegate, int port);
  bool Start(uv_loop_t* loop);
  void Stop(ServerCallback callback);
  void Send(int session_id, const std::string& message);
  void TerminateConnections(ServerCallback callback);

 private:
  static bool HandshakeCallback(InspectorSocket* socket,
                                enum inspector_handshake_event state,
                                const std::string& path);
  static void SocketConnectedCallback(uv_stream_t* server, int status);
  static void ServerClosedCallback(uv_handle_t* server);
  template<typename SomeUvStruct>
  static InspectorSocketServer* From(SomeUvStruct* server) {
    return node::ContainerOf(&InspectorSocketServer::server_,
                             reinterpret_cast<uv_tcp_t*>(server));
  }
  bool RespondToGet(InspectorSocket* socket, const std::string& path);
  void SendListResponse(InspectorSocket* socket);
  void ReadCallback(InspectorSocket* socket, ssize_t read, const uv_buf_t* buf);
  bool SessionStarted(SocketSession* session, const std::string& id);
  void SessionTerminated(int id);
  bool TargetExists(const std::string& id);
  static void SocketSessionDeleter(SocketSession*);
  SocketServerDelegate* Delegate() { return delegate_; }

  uv_loop_t* loop_;
  SocketServerDelegate* const delegate_;
  const int port_;
  std::string path_;
  uv_tcp_t server_;
  Closer* closer_;
  std::map<int, SocketSession*> connected_sessions_;
  int next_session_id_;

  friend class SocketSession;
  friend class Closer;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_SOCKET_SERVER_H_
