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

#include "node_v8_platform.h"

#include "node.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8-platform.h"

namespace node {

using v8::Task;
using v8::Isolate;

// The last task to encounter before killing the worker
class StopTask : public Task {
 public:
  StopTask() {}
  ~StopTask() {}

  void Run() {}
};

static StopTask stop_task_;


Platform::Platform(unsigned int worker_count) : worker_count_(worker_count) {
  workers_ = new uv_thread_t[worker_count_];

  for (unsigned int i = 0; i < worker_count_; i++) {
    int err;

    err = uv_thread_create(worker_at(i), WorkerBody, this);
    CHECK_EQ(err, 0);
  }
}


Platform::~Platform() {
  // Push stop task
  for (unsigned int i = 0; i < worker_count(); i++)
    global_queue()->Push(&stop_task_);

  // And wait for workers to exit
  for (unsigned int i = 0; i < worker_count(); i++) {
    int err;

    err = uv_thread_join(worker_at(i));
    CHECK_EQ(err, 0);
  }
  delete[] workers_;
}


void Platform::CallOnBackgroundThread(Task* task,
                                      ExpectedRuntime expected_runtime) {
  global_queue()->Push(task);
}


void Platform::CallOnForegroundThread(Isolate* isolate, Task* task) {
  // TODO(indutny): create per-isolate thread pool
  global_queue()->Push(task);
}


void Platform::WorkerBody(void* arg) {
  Platform* p = static_cast<Platform*>(arg);

  for (;;) {
    Task* task = p->global_queue()->Shift();
    if (task == &stop_task_)
      break;

    task->Run();
    delete task;
  }
}


TaskQueue::TaskQueue() {
  int err;

  STATIC_ASSERT(kRingSize == (kRingSize & (~(kRingSize - 1))));

  size_ = kRingSize;
  ring_ = new Task*[size_];
  mask_ = size_ - 1;
  read_off_ = 0;
  write_off_ = 0;

  err = uv_sem_init(&sem_, 0);
  CHECK_EQ(err, 0);

  err = uv_cond_init(&cond_);
  CHECK_EQ(err, 0);

  err = uv_mutex_init(&mutex_);
  CHECK_EQ(err, 0);
}


TaskQueue::~TaskQueue() {
  CHECK_EQ(read_off_, write_off_);

  delete[] ring_;
  ring_ = NULL;
  uv_sem_destroy(&sem_);
  uv_cond_destroy(&cond_);
  uv_mutex_destroy(&mutex_);
}


void TaskQueue::Push(Task* task) {
  uv_mutex_lock(&mutex_);

  // Wait for empty cell
  while (ring_[write_off_] != NULL)
    uv_cond_wait(&cond_, &mutex_);

  ring_[write_off_] = task;
  write_off_++;
  write_off_ &= mask_;
  uv_mutex_unlock(&mutex_);

  uv_sem_post(&sem_);
}


Task* TaskQueue::Shift() {
  uv_sem_wait(&sem_);

  uv_mutex_lock(&mutex_);
  Task* task = ring_[read_off_];
  ring_[read_off_] = NULL;
  uv_cond_signal(&cond_);

  read_off_++;
  read_off_ &= mask_;
  uv_mutex_unlock(&mutex_);

  return task;
}


}  // namespace node
