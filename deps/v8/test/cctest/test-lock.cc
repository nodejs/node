// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
