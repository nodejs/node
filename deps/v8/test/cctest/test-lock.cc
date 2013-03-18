// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Tests of the TokenLock class from lock.h

#include <stdlib.h>

#include "v8.h"

#include "platform.h"
#include "cctest.h"


using namespace ::v8::internal;


// Simple test of locking logic
TEST(Simple) {
  Mutex* mutex = OS::CreateMutex();
  CHECK_EQ(0, mutex->Lock());  // acquire the lock with the right token
  CHECK_EQ(0, mutex->Unlock());  // can unlock with the right token
  delete mutex;
}


TEST(MultiLock) {
  Mutex* mutex = OS::CreateMutex();
  CHECK_EQ(0, mutex->Lock());
  CHECK_EQ(0, mutex->Unlock());
  delete mutex;
}


TEST(ShallowLock) {
  Mutex* mutex = OS::CreateMutex();
  CHECK_EQ(0, mutex->Lock());
  CHECK_EQ(0, mutex->Unlock());
  CHECK_EQ(0, mutex->Lock());
  CHECK_EQ(0, mutex->Unlock());
  delete mutex;
}


TEST(SemaphoreTimeout) {
  bool ok;
  Semaphore* sem = OS::CreateSemaphore(0);

  // Semaphore not signalled - timeout.
  ok = sem->Wait(0);
  CHECK(!ok);
  ok = sem->Wait(100);
  CHECK(!ok);
  ok = sem->Wait(1000);
  CHECK(!ok);

  // Semaphore signalled - no timeout.
  sem->Signal();
  ok = sem->Wait(0);
  sem->Signal();
  ok = sem->Wait(100);
  sem->Signal();
  ok = sem->Wait(1000);
  CHECK(ok);
  delete sem;
}
