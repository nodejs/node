// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_LIVENESS_BROKER_H_
#define V8_HEAP_CPPGC_LIVENESS_BROKER_H_

#include "include/cppgc/liveness-broker.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE LivenessBrokerFactory {
 public:
  static LivenessBroker Create();
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_LIVENESS_BROKER_H_
