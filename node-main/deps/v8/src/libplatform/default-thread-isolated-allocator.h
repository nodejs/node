// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_THREAD_ISOLATED_ALLOCATOR_H_
#define V8_LIBPLATFORM_DEFAULT_THREAD_ISOLATED_ALLOCATOR_H_

#include "include/libplatform/libplatform-export.h"
#include "include/v8-platform.h"
#include "src/base/build_config.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace platform {

class V8_PLATFORM_EXPORT DefaultThreadIsolatedAllocator
    : public NON_EXPORTED_BASE(ThreadIsolatedAllocator) {
 public:
  DefaultThreadIsolatedAllocator();

  ~DefaultThreadIsolatedAllocator() override;

  void* Allocate(size_t size) override;

  void Free(void* object) override;

  enum Type Type() const override;

  int Pkey() const override;

  bool Valid() const;

 private:
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  const int pkey_;
#endif
};

}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_DEFAULT_THREAD_ISOLATED_ALLOCATOR_H_
