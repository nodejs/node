#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "inspector_socket_server.h"
#include "node_mutex.h"

#include "uv.h"

#include <cstddef>
#include <memory>

namespace node {
// Forward declaration to break recursive dependency chain with src/env.h.
class Environment;
namespace inspector {

class MainThreadHandle;
class RequestQueue;

class InspectorIo {
 public:
  // Start the inspector agent thread, waiting for it to initialize
  // bool Start();
  // Returns empty pointer if thread was not started
  static std::unique_ptr<InspectorIo> Start(
      std::shared_ptr<MainThreadHandle> main_thread,
      const std::string& path,
      std::shared_ptr<ExclusiveAccess<HostPort>> host_port,
      const InspectPublishUid& inspect_publish_uid);

  // Will block till the transport thread shuts down
  ~InspectorIo();

  void StopAcceptingNewConnections();
  std::string GetWsUrl() const;

 private:
  InspectorIo(std::shared_ptr<MainThreadHandle> handle,
              const std::string& path,
              std::shared_ptr<ExclusiveAccess<HostPort>> host_port,
              const InspectPublishUid& inspect_publish_uid);

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
  std::shared_ptr<ExclusiveAccess<HostPort>> host_port_;
  InspectPublishUid inspect_publish_uid_;

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
