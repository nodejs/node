// Copyright Joyent, Inc. and other Node contributors.
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

#ifndef SRC_THREADPOOLWORK_INL_H_
#define SRC_THREADPOOLWORK_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util-inl.h"
#include "node_internals.h"

namespace node {

void ThreadPoolWork::ScheduleWork() {
  env_->IncreaseWaitingRequestCounter();
  int status = uv_queue_work(
      env_->event_loop(),
      &work_req_,
      [](uv_work_t* req) {
        ThreadPoolWork* self = ContainerOf(&ThreadPoolWork::work_req_, req);
        self->DoThreadPoolWork();
      },
      [](uv_work_t* req, int status) {
        ThreadPoolWork* self = ContainerOf(&ThreadPoolWork::work_req_, req);
        self->env_->DecreaseWaitingRequestCounter();
        self->AfterThreadPoolWork(status);
      });
  CHECK_EQ(status, 0);
}

int ThreadPoolWork::CancelWork() {
  return uv_cancel(reinterpret_cast<uv_req_t*>(&work_req_));
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_THREADPOOLWORK_INL_H_
