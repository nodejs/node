// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-job.h"

#include "src/compiler-dispatcher/unoptimized-compile-job.h"

namespace v8 {
namespace internal {

const UnoptimizedCompileJob* CompilerDispatcherJob::AsUnoptimizedCompileJob()
    const {
  DCHECK_EQ(type(), Type::kUnoptimizedCompile);
  return static_cast<const UnoptimizedCompileJob*>(this);
}

}  // namespace internal
}  // namespace v8
