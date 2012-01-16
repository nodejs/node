// Copyright 2006-2008 the V8 project authors. All rights reserved.
//
// Tests of the TokenLock class from lock.h

#include <stdlib.h>

#include "v8.h"

#include "platform.h"
#include "cctest.h"

using namespace ::v8::internal;


TEST(VirtualMemory) {
  OS::SetUp();
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
