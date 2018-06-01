// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_COMPILATION_MANAGER_H_
#define V8_WASM_COMPILATION_MANAGER_H_

#include <unordered_map>

#include "src/handles.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {
namespace wasm {

class AsyncCompileJob;

// The CompilationManager manages a list of active WebAssembly compile jobs. The
// manager owns the memory of the compile jobs and can trigger the abortion of
// compile jobs. If the isolate tears down, the CompilationManager makes sure
// that all compile jobs finish executing before the isolate becomes
// unavailable.
class CompilationManager {
 public:
  void StartAsyncCompileJob(Isolate* isolate,
                            std::unique_ptr<byte[]> bytes_copy, size_t length,
                            Handle<Context> context, Handle<JSPromise> promise);

  std::shared_ptr<StreamingDecoder> StartStreamingCompilation(
      Isolate* isolate, Handle<Context> context, Handle<JSPromise> promise);

  // Remove {job} from the list of active compile jobs.
  std::unique_ptr<AsyncCompileJob> RemoveJob(AsyncCompileJob* job);

  // Cancel all AsyncCompileJobs and delete their state immediately.
  void TearDown();

  // Cancel all AsyncCompileJobs so that they are not processed any further,
  // but delay the deletion of their state until all tasks accessing the
  // AsyncCompileJob finish their execution.
  void AbortAllJobs();

  // Returns true if at lease one AsyncCompileJob is currently running.
  bool HasRunningCompileJob() const { return !jobs_.empty(); }

 private:
  AsyncCompileJob* CreateAsyncCompileJob(Isolate* isolate,
                                         std::unique_ptr<byte[]> bytes_copy,
                                         size_t length, Handle<Context> context,
                                         Handle<JSPromise> promise);

  // We use an AsyncCompileJob as the key for itself so that we can delete the
  // job from the map when it is finished.
  std::unordered_map<AsyncCompileJob*, std::unique_ptr<AsyncCompileJob>> jobs_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_COMPILATION_MANAGER_H_
