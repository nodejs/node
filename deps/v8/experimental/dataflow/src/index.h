// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INDEX_H
#define INDEX_H

#include <functional>
#include <memory>
#include <string>

#include <queue>
#include <mutex>
#include <condition_variable>

#include "clang/Tooling/CompilationDatabase.h"
#include "database.h"

namespace dfg {

template <typename T>
class ConcurrentQueue {
 public:
  explicit ConcurrentQueue(size_t MaxCapacity = 32) : MaxCapacity(MaxCapacity) {}

  void Push(T Value) {
    std::unique_lock<std::mutex> Lock(Mutex);
    NotFullCond.wait(Lock, [this]() {
      return Queue.size() < MaxCapacity || Shutdown;
    });
    if (Shutdown) return;
    Queue.push(std::move(Value));
    NotEmptyCond.notify_one();
  }

  bool Pop(T& Value) {
    std::unique_lock<std::mutex> Lock(Mutex);
    NotEmptyCond.wait(Lock, [this]() { return !Queue.empty() || Shutdown; });
    if (Queue.empty() && Shutdown) {
      return false;
    }
    Value = std::move(Queue.front());
    Queue.pop();
    NotFullCond.notify_one();
    return true;
  }

  bool TryPop(T& Value) {
    std::lock_guard<std::mutex> Lock(Mutex);
    if (Queue.empty()) return false;
    Value = std::move(Queue.front());
    Queue.pop();
    NotFullCond.notify_one();
    return true;
  }

  void SignalShutdown() {
    std::lock_guard<std::mutex> Lock(Mutex);
    Shutdown = true;
    NotEmptyCond.notify_all();
    NotFullCond.notify_all();
  }

 private:
  std::queue<T> Queue;
  std::mutex Mutex;
  std::condition_variable NotEmptyCond;
  std::condition_variable NotFullCond;
  size_t MaxCapacity;
  bool Shutdown = false;
};

class Indexer {
 public:
  using ProgressCallback = std::function<void(unsigned Current, unsigned Total,
                                              const std::string& File)>;

  explicit Indexer(const std::string& WorkspaceRoot, Database& Db,
                   unsigned NumThreads = 1, bool EnableTelemetry = false);

  // Build or update the index using compilation database
  bool BuildIndex(clang::tooling::CompilationDatabase& Compilations,
                  ProgressCallback Callback = nullptr,
                  const std::vector<std::string>& Filters = {});

 private:
  std::string WorkspaceRoot;
  Database& Db;
  unsigned NumThreads;
  bool EnableTelemetry;
};

std::string GetClangResourceDir();

}  // namespace dfg

#endif  // INDEX_H
