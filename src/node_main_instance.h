#ifndef SRC_NODE_MAIN_INSTANCE_H_
#define SRC_NODE_MAIN_INSTANCE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

namespace node {

// TODO(joyeecheung): align this with the Worker/WorkerThreadData class.
// We may be able to create an abstract class to reuse some of the routines.
class NodeMainInstance {
 public:
  NodeMainInstance(const NodeMainInstance&) = delete;
  NodeMainInstance& operator=(const NodeMainInstance&) = delete;
  NodeMainInstance(NodeMainInstance&&) = delete;
  NodeMainInstance& operator=(NodeMainInstance&&) = delete;

  NodeMainInstance(uv_loop_t* event_loop,
                   const std::vector<std::string>& args,
                   const std::vector<std::string>& exec_args);
  ~NodeMainInstance();

  // Start running the Node.js instances, return the exit code when finished.
  int Run();

 private:
  // TODO(joyeecheung): align this with the CreateEnvironment exposed in node.h
  // and the environment creation routine in workers somehow.
  std::unique_ptr<Environment> CreateMainEnvironment(int* exit_code);

  std::vector<std::string> args_;
  std::vector<std::string> exec_args_;
  std::unique_ptr<ArrayBufferAllocator> array_buffer_allocator_;
  v8::Isolate* isolate_;
  std::unique_ptr<IsolateData> isolate_data_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_MAIN_INSTANCE_H_
