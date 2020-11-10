// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_LIVENESS_BROKER_H_
#define INCLUDE_CPPGC_LIVENESS_BROKER_H_

#include "cppgc/heap.h"
#include "cppgc/member.h"
#include "cppgc/trace-trait.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

namespace internal {
class LivenessBrokerFactory;
}  // namespace internal

class V8_EXPORT LivenessBroker final {
 public:
  template <typename T>
  bool IsHeapObjectAlive(const T* object) const {
    return object &&
           IsHeapObjectAliveImpl(
               TraceTrait<T>::GetTraceDescriptor(object).base_object_payload);
  }

  template <typename T>
  bool IsHeapObjectAlive(const WeakMember<T>& weak_member) const {
    return (weak_member != kSentinelPointer) &&
           IsHeapObjectAlive<T>(weak_member.Get());
  }

  template <typename T>
  bool IsHeapObjectAlive(const UntracedMember<T>& untraced_member) const {
    return (untraced_member != kSentinelPointer) &&
           IsHeapObjectAlive<T>(untraced_member.Get());
  }

 private:
  LivenessBroker() = default;

  bool IsHeapObjectAliveImpl(const void*) const;

  friend class internal::LivenessBrokerFactory;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_LIVENESS_BROKER_H_
