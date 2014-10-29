// Copyright Fedor Indutny and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_V8_PLATFORM_H_
#define SRC_NODE_V8_PLATFORM_H_

#include "uv.h"
#include "v8-platform.h"

namespace node {

class TaskQueue {
 public:
  TaskQueue();
  ~TaskQueue();

  void Push(v8::Task* task);
  v8::Task* Shift();

 private:
  static unsigned int next(unsigned int n);
  bool can_read() const;
  bool can_write() const;
  uv_cond_t read_cond_;
  uv_cond_t write_cond_;
  uv_mutex_t mutex_;
  unsigned int read_off_;
  unsigned int write_off_;
  v8::Task* ring_[1024];
};

class Platform : public v8::Platform {
 public:
  explicit Platform(unsigned int worker_count);
  virtual ~Platform() override;

  void CallOnBackgroundThread(v8::Task* task,
                              ExpectedRuntime expected_runtime);
  void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task);

 protected:
  static void WorkerBody(void* arg);

  inline TaskQueue* global_queue() { return &global_queue_; }
  inline uv_thread_t* worker_at(unsigned int index) { return &workers_[index]; }
  inline unsigned int worker_count() const { return worker_count_; }

  uv_thread_t* workers_;
  unsigned int worker_count_;
  TaskQueue global_queue_;
};

}  // namespace node

#endif  // SRC_NODE_V8_PLATFORM_H_
