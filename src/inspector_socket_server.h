#ifndef SRC_INSPECTOR_SOCKET_SERVER_H_
#define SRC_INSPECTOR_SOCKET_SERVER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

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

std::string FormatWsAddress(const std::string& host, int port,
                            const std::string& target_id,
                            bool include_protocol);

class InspectorSocketServer;
class SocketSession;
class ServerSocket;

class SocketServerDelegate {
 public:
  virtual void AssignServer(InspectorSocketServer* server) = 0;
  virtual void StartSession(int session_id, const std::string& target_id) = 0;
  virtual void EndSession(int session_id) = 0;
  virtual void MessageReceived(int session_id, const std::string& message) = 0;
  virtual std::vector<std::string> GetTargetIds() = 0;
  virtual std::string GetTargetTitle(const std::string& id) = 0;
  virtual std::string GetTargetUrl(const std::string& id) = 0;
  virtual ~SocketServerDelegate() = default;
};

// HTTP Server, writes messages requested as TransportActions, and responds
// to HTTP requests and WS upgrades.

class InspectorSocketServer {
 public:
  InspectorSocketServer(std::unique_ptr<SocketServerDelegate> delegate,
                        uv_loop_t* loop,
                        const std::string& host,
                        int port,
                        const InspectPublishUid& inspect_publish_uid,
                        FILE* out = stderr);
  ~InspectorSocketServer();

  // Start listening on host/port
  bool Start();

  // Called by the TransportAction sent with InspectorIo::Write():
  //   kKill and kStop
  void Stop();
  //   kSendMessage
  void Send(int session_id, const std::string& message);
  //   kKill
  void TerminateConnections();
  int Port() const;

  // Session connection lifecycle
  void Accept(int server_port, uv_stream_t* server_socket);
  bool HandleGetRequest(int session_id, const std::string& host,
                        const std::string& path);
  void SessionStarted(int session_id, const std::string& target_id,
                      const std::string& ws_id);
  void SessionTerminated(int session_id);
  void MessageReceived(int session_id, const std::string& message) {
    delegate_->MessageReceived(session_id, message);
  }
  SocketSession* Session(int session_id);
  bool done() const {
    return server_sockets_.empty() && connected_sessions_.empty();
  }

  static void CloseServerSocket(ServerSocket*);
  using ServerSocketPtr = DeleteFnPtr<ServerSocket, CloseServerSocket>;

 private:
  void SendListResponse(InspectorSocket* socket, const std::string& host,
                        SocketSession* session);
  std::string GetFrontendURL(bool is_compat,
                             const std::string &formatted_address);
  bool TargetExists(const std::string& id);

  enum class ServerState {kNew, kRunning, kStopped};
  uv_loop_t* loop_;
  std::unique_ptr<SocketServerDelegate> delegate_;
  const std::string host_;
  int port_;
  InspectPublishUid inspect_publish_uid_;
  std::vector<ServerSocketPtr> server_sockets_;
  std::map<int, std::pair<std::string, std::unique_ptr<SocketSession>>>
      connected_sessions_;
  int next_session_id_;
  FILE* out_;
  ServerState state_;
};

}  // namespace inspector
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_INSPECTOR_SOCKET_SERVER_H_
