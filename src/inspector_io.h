#ifndef SRC_INSPECTOR_IO_H_
#define SRC_INSPECTOR_IO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "inspector_socket_server.h"
#include "node_mutex.h"
#include "uv.h"

#include <memory>
#include <stddef.h>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif


namespace v8_inspector {
class StringBuffer;
class StringView;
}  // namespace v8_inspector

namespace node {
// Forward declaration to break recursive dependency chain with src/env.h.
class Environment;
namespace inspector {

std::string FormatWsAddress(const std::string& host, int port,
                            const std::string& target_id,
                            bool include_protocol);

class InspectorIoDelegate;
class MainThreadHandle;
class RequestQueue;

// kKill closes connections and stops the server, kStop only stops the server
enum class TransportAction {
  kKill,
  kSendMessage,
  kStop
};

class InspectorIo {
 public:
  // Start the inspector agent thread, waiting for it to initialize
  // bool Start();
  // Returns empty pointer if thread was not started
  static std::unique_ptr<InspectorIo> Start(
      std::shared_ptr<MainThreadHandle> main_thread,
      const std::string& path,
      std::shared_ptr<HostPort> host_port);

  // Will block till the transport thread shuts down
  ~InspectorIo();

  void StopAcceptingNewConnections();
  const std::string& host() const { return host_port_->host(); }
  int port() const { return host_port_->port(); }
  std::vector<std::string> GetTargetIds() const;

 private:
  InspectorIo(std::shared_ptr<MainThreadHandle> handle,
              const std::string& path,
              std::shared_ptr<HostPort> host_port);

  // Wrapper for agent->ThreadMain()
  static void ThreadMain(void* agent);

  // Runs a uv_loop_t
  void ThreadMain();

  // This is a thread-safe object that will post async tasks. It lives as long
  // as an Inspector object lives (almost as long as an Isolate).
  std::shared_ptr<MainThreadHandle> main_thread_;
  // Used to post on a frontend interface thread, lives while the server is
  // running
  std::shared_ptr<RequestQueue> request_queue_;
  std::shared_ptr<HostPort> host_port_;

  // The IO thread runs its own uv_loop to implement the TCP server off
  // the main thread.
  uv_thread_t thread_;

  // For setting up interthread communications
  Mutex thread_start_lock_;
  ConditionVariable thread_start_condition_;
  std::string script_name_;
  // May be accessed from any thread
  const std::string id_;
};

}  // namespace inspector
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_INSPECTOR_IO_H_
