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


double Platform::MonotonicallyIncreasingTime() {
  // uv_hrtime() returns a uint64_t but doubles can only represent integrals up
  // to 2^53 accurately.  Take steps to prevent loss of precision on overflow.
  const uint64_t timestamp = uv_hrtime();
  const uint64_t billion = 1000 * 1000 * 1000;
  const uint64_t seconds = timestamp / billion;
  const uint64_t nanoseconds = timestamp % billion;
  return seconds + 1.0 / nanoseconds;
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


TaskQueue::TaskQueue() : read_off_(0), write_off_(0) {
  CHECK_EQ(0, uv_cond_init(&read_cond_));
  CHECK_EQ(0, uv_cond_init(&write_cond_));
  CHECK_EQ(0, uv_mutex_init(&mutex_));
}


TaskQueue::~TaskQueue() {
  uv_mutex_lock(&mutex_);
  CHECK_EQ(read_off_, write_off_);
  uv_mutex_unlock(&mutex_);
  uv_cond_destroy(&read_cond_);
  uv_cond_destroy(&write_cond_);
  uv_mutex_destroy(&mutex_);
}


void TaskQueue::Push(Task* task) {
  uv_mutex_lock(&mutex_);

  while (can_write() == false)
    uv_cond_wait(&write_cond_, &mutex_);  // Wait until there is a free slot.

  ring_[write_off_] = task;
  write_off_ = next(write_off_);
  uv_cond_signal(&read_cond_);
  uv_mutex_unlock(&mutex_);
}


Task* TaskQueue::Shift() {
  uv_mutex_lock(&mutex_);

  while (can_read() == false)
    uv_cond_wait(&read_cond_, &mutex_);

  Task* task = ring_[read_off_];
  if (can_write() == false)
    uv_cond_signal(&write_cond_);  // Signal waiters that we freed up a slot.
  read_off_ = next(read_off_);
  uv_mutex_unlock(&mutex_);

  return task;
}


unsigned int TaskQueue::next(unsigned int n) {
  return (n + 1) % ARRAY_SIZE(TaskQueue {}.ring_);
}


bool TaskQueue::can_read() const {
  return read_off_ != write_off_;
}


// The read pointer chases the write pointer in the circular queue.
// This method checks that the write pointer hasn't advanced so much
// that it has gone full circle and caught up with the read pointer.
//
// can_write() returns false when there is an empty slot but the read pointer
// points to the first element and the write pointer to the last element.
// That should be rare enough that it is not worth the extra bookkeeping
// to work around that.  It's not harmful either, just mildly inefficient.
bool TaskQueue::can_write() const {
  return next(write_off_) != read_off_;
}


}  // namespace node
