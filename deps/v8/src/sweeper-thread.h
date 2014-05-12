// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SWEEPER_THREAD_H_
#define V8_SWEEPER_THREAD_H_

#include "atomicops.h"
#include "flags.h"
#include "platform.h"
#include "utils.h"

#include "spaces.h"

#include "heap.h"

namespace v8 {
namespace internal {

class SweeperThread : public Thread {
 public:
  explicit SweeperThread(Isolate* isolate);
  ~SweeperThread() {}

  void Run();
  void Stop();
  void StartSweeping();
  void WaitForSweeperThread();
  bool SweepingCompleted();

  static int NumberOfThreads(int max_available);

 private:
  Isolate* isolate_;
  Heap* heap_;
  MarkCompactCollector* collector_;
  Semaphore start_sweeping_semaphore_;
  Semaphore end_sweeping_semaphore_;
  Semaphore stop_semaphore_;
  volatile AtomicWord stop_thread_;
};

} }  // namespace v8::internal

#endif  // V8_SWEEPER_THREAD_H_
