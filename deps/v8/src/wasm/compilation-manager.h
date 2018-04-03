// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_COMPILATION_MANAGER_H_
#define V8_WASM_COMPILATION_MANAGER_H_

#include <vector>

#include "src/handles.h"
#include "src/isolate.h"
#include "src/wasm/module-compiler.h"

namespace v8 {
namespace internal {
namespace wasm {

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

  // Removes {job} from the list of active compile jobs.
  std::shared_ptr<AsyncCompileJob> RemoveJob(AsyncCompileJob* job);

  void TearDown();

 private:
  AsyncCompileJob* CreateAsyncCompileJob(Isolate* isolate,
                                         std::unique_ptr<byte[]> bytes_copy,
                                         size_t length, Handle<Context> context,
                                         Handle<JSPromise> promise);

  // We use an AsyncCompileJob as the key for itself so that we can delete the
  // job from the map when it is finished.
  std::unordered_map<AsyncCompileJob*, std::shared_ptr<AsyncCompileJob>> jobs_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_COMPILATION_MANAGER_H_
