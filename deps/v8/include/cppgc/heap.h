// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_HEAP_H_
#define INCLUDE_CPPGC_HEAP_H_

#include <memory>

#include "include/v8config.h"

namespace cppgc {
namespace internal {
class Heap;
}  // namespace internal

class V8_EXPORT Heap {
 public:
  static std::unique_ptr<Heap> Create();

  virtual ~Heap() = default;

 private:
  Heap() = default;

  friend class internal::Heap;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_HEAP_H_
