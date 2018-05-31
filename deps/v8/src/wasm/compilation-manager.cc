// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/compilation-manager.h"
#include "src/wasm/module-compiler.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

AsyncCompileJob* CompilationManager::CreateAsyncCompileJob(
    Isolate* isolate, std::unique_ptr<byte[]> bytes_copy, size_t length,
    Handle<Context> context, Handle<JSPromise> promise) {
  AsyncCompileJob* job = new AsyncCompileJob(isolate, std::move(bytes_copy),
                                             length, context, promise);
  // Pass ownership to the unique_ptr in {jobs_}.
  jobs_[job] = std::unique_ptr<AsyncCompileJob>(job);
  return job;
}

void CompilationManager::StartAsyncCompileJob(
    Isolate* isolate, std::unique_ptr<byte[]> bytes_copy, size_t length,
    Handle<Context> context, Handle<JSPromise> promise) {
  AsyncCompileJob* job = CreateAsyncCompileJob(isolate, std::move(bytes_copy),
                                               length, context, promise);
  job->Start();
}

std::shared_ptr<StreamingDecoder> CompilationManager::StartStreamingCompilation(
    Isolate* isolate, Handle<Context> context, Handle<JSPromise> promise) {
  AsyncCompileJob* job = CreateAsyncCompileJob(
      isolate, std::unique_ptr<byte[]>(nullptr), 0, context, promise);
  return job->CreateStreamingDecoder();
}

std::unique_ptr<AsyncCompileJob> CompilationManager::RemoveJob(
    AsyncCompileJob* job) {
  auto item = jobs_.find(job);
  DCHECK(item != jobs_.end());
  std::unique_ptr<AsyncCompileJob> result = std::move(item->second);
  jobs_.erase(item);
  return result;
}

void CompilationManager::TearDown() { jobs_.clear(); }

void CompilationManager::AbortAllJobs() {
  // Iterate over a copy of {jobs_}, because {job->Abort} modifies {jobs_}.
  std::vector<AsyncCompileJob*> copy;
  copy.reserve(jobs_.size());

  for (auto& entry : jobs_) copy.push_back(entry.first);

  for (auto* job : copy) job->Abort();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
