// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-thread-isolated-allocator.h"

#if V8_HAS_PKU_JIT_WRITE_PROTECT

#if !V8_OS_LINUX
#error pkey support in this file is only implemented on Linux
#endif

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if V8_HAS_PKU_JIT_WRITE_PROTECT

extern int pkey_alloc(unsigned int flags, unsigned int access_rights) V8_WEAK;
extern int pkey_free(int pkey) V8_WEAK;

namespace {

int PkeyAlloc() {
#ifdef PKEY_DISABLE_WRITE
  if (pkey_alloc) {
    return pkey_alloc(0, PKEY_DISABLE_WRITE);
  }
#endif
  return -1;
}

int PkeyFree(int pkey) {
  DCHECK(pkey_free);
  return pkey_free(pkey);
}

}  // namespace

#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT

namespace v8::platform {

DefaultThreadIsolatedAllocator::DefaultThreadIsolatedAllocator()
#if V8_HAS_PKU_JIT_WRITE_PROTECT
    : pkey_(PkeyAlloc())
#endif
{
}

DefaultThreadIsolatedAllocator::~DefaultThreadIsolatedAllocator() {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  if (pkey_ != -1) {
    PkeyFree(pkey_);
  }
#endif
}

// TODO(sroettger): this should return thread isolated (e.g. pkey-tagged) memory
//                  for testing.
void* DefaultThreadIsolatedAllocator::Allocate(size_t size) {
  return malloc(size);
}

void DefaultThreadIsolatedAllocator::Free(void* object) { free(object); }

enum DefaultThreadIsolatedAllocator::Type DefaultThreadIsolatedAllocator::Type()
    const {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  return Type::kPkey;
#else
  UNREACHABLE();
#endif
}

int DefaultThreadIsolatedAllocator::Pkey() const {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  return pkey_;
#else
  UNREACHABLE();
#endif
}

bool DefaultThreadIsolatedAllocator::Valid() const {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  return pkey_ != -1;
#else
  return false;
#endif
}

}  // namespace v8::platform
