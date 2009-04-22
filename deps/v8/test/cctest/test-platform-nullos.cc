// Copyright 2006-2008 the V8 project authors. All rights reserved.
//
// Tests of the TokenLock class from lock.h

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>  // for usleep()

#include "v8.h"

#include "platform.h"
#include "cctest.h"

using namespace ::v8::internal;


static void yield() {
  UNIMPLEMENTED();
}

static const int kLockCounterLimit = 50;
static int busy_lock_counter = 0;


static void LoopIncrement(Mutex* mutex, int rem) {
  while (true) {
    int count = 0;
    int last_count = -1;
    do {
      CHECK_EQ(0, mutex->Lock());
      count = busy_lock_counter;
      CHECK_EQ(0, mutex->Unlock());
      yield();
    } while (count % 2 == rem && count < kLockCounterLimit);
    if (count >= kLockCounterLimit) break;
    CHECK_EQ(0, mutex->Lock());
    CHECK_EQ(count, busy_lock_counter);
    CHECK(last_count == -1 || count == last_count + 1);
    busy_lock_counter++;
    last_count = count;
    CHECK_EQ(0, mutex->Unlock());
    yield();
  }
}


static void* RunTestBusyLock(void* arg) {
  LoopIncrement(static_cast<Mutex*>(arg), 0);
  return 0;
}


// Runs two threads that repeatedly acquire the lock and conditionally
// increment a variable.
TEST(BusyLock) {
  pthread_t other;
  Mutex* mutex = OS::CreateMutex();
  int thread_created = pthread_create(&other,
                                      NULL,
                                      &RunTestBusyLock,
                                      mutex);
  CHECK_EQ(0, thread_created);
  LoopIncrement(mutex, 1);
  pthread_join(other, NULL);
  delete mutex;
}


TEST(VirtualMemory) {
  VirtualMemory* vm = new VirtualMemory(1 * MB);
  CHECK(vm->IsReserved());
  void* block_addr = vm->address();
  size_t block_size = 4 * KB;
  CHECK(vm->Commit(block_addr, block_size, false));
  // Check whether we can write to memory.
  int* addr = static_cast<int*>(block_addr);
  addr[KB-1] = 2;
  CHECK(vm->Uncommit(block_addr, block_size));
  delete vm;
}
