#ifndef SRC_NODE_MAIN_INSTANCE_H_
#define SRC_NODE_MAIN_INSTANCE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstddef>
#include <memory>

#include "node.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

namespace node {

class ExternalReferenceRegistry;
struct EnvSerializeInfo;
struct SnapshotData;

// TODO(joyeecheung): align this with the Worker/WorkerThreadData class.
// We may be able to create an abstract class to reuse some of the routines.
class NodeMainInstance {
 public:
  // To create a main instance that does not own the isolate,
  // The caller needs to do:
  //
  //   Isolate* isolate = Isolate::Allocate();
  //   platform->RegisterIsolate(isolate, loop);
  //   isolate->Initialize(...);
  //   isolate->Enter();
  //   std::unique_ptr<NodeMainInstance> main_instance =
  //       NodeMainInstance::Create(isolate, loop, args, exec_args);
  //
  // When tearing it down:
  //
  //   main_instance->Cleanup();  // While the isolate is entered
  //   isolate->Exit();
  //   isolate->Dispose();
  //   platform->UnregisterIsolate(isolate);
  //
  // After calling Dispose() the main_instance is no longer accessible.
  static std::unique_ptr<NodeMainInstance> Create(
      v8::Isolate* isolate,
      uv_loop_t* event_loop,
      MultiIsolatePlatform* platform,
      const std::vector<std::string>& args,
      const std::vector<std::string>& exec_args);

  void Dispose();

  // Create a main instance that owns the isolate
  NodeMainInstance(const SnapshotData* snapshot_data,
                   uv_loop_t* event_loop,
                   MultiIsolatePlatform* platform,
                   const std::vector<std::string>& args,
                   const std::vector<std::string>& exec_args);
  ~NodeMainInstance();

  // Start running the Node.js instances, return the exit code when finished.
  int Run();
  void Run(int* exit_code, Environment* env);

  IsolateData* isolate_data() { return isolate_data_.get(); }

  DeleteFnPtr<Environment, FreeEnvironment> CreateMainEnvironment(
      int* exit_code);

  NodeMainInstance(const NodeMainInstance&) = delete;
  NodeMainInstance& operator=(const NodeMainInstance&) = delete;
  NodeMainInstance(NodeMainInstance&&) = delete;
  NodeMainInstance& operator=(NodeMainInstance&&) = delete;

 private:
  NodeMainInstance(v8::Isolate* isolate,
                   uv_loop_t* event_loop,
                   MultiIsolatePlatform* platform,
                   const std::vector<std::string>& args,
                   const std::vector<std::string>& exec_args);

  static std::unique_ptr<ExternalReferenceRegistry> registry_;
  std::vector<std::string> args_;
  std::vector<std::string> exec_args_;
  std::unique_ptr<ArrayBufferAllocator> array_buffer_allocator_;
  v8::Isolate* isolate_;
  MultiIsolatePlatform* platform_;
  std::unique_ptr<IsolateData> isolate_data_;
  std::unique_ptr<v8::Isolate::CreateParams> isolate_params_;
  const SnapshotData* snapshot_data_ = nullptr;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_MAIN_INSTANCE_H_
