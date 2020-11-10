// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_PREFINALIZER_H_
#define INCLUDE_CPPGC_PREFINALIZER_H_

#include "cppgc/internal/accessors.h"
#include "cppgc/internal/compiler-specific.h"
#include "cppgc/internal/prefinalizer-handler.h"
#include "cppgc/liveness-broker.h"
#include "cppgc/macros.h"

namespace cppgc {

namespace internal {

template <typename T>
class PrefinalizerRegistration final {
 public:
  explicit PrefinalizerRegistration(T* self) {
    static_assert(sizeof(&T::InvokePreFinalizer) > 0,
                  "USING_PRE_FINALIZER(T) must be defined.");

    cppgc::internal::PreFinalizerRegistrationDispatcher::RegisterPrefinalizer(
        internal::GetHeapFromPayload(self), {self, T::InvokePreFinalizer});
  }

  void* operator new(size_t, void* location) = delete;
  void* operator new(size_t) = delete;
};

}  // namespace internal

#define CPPGC_USING_PRE_FINALIZER(Class, PreFinalizer)                      \
 public:                                                                    \
  static bool InvokePreFinalizer(const LivenessBroker& liveness_broker,     \
                                 void* object) {                            \
    static_assert(internal::IsGarbageCollectedTypeV<Class>,                 \
                  "Only garbage collected objects can have prefinalizers"); \
    Class* self = static_cast<Class*>(object);                              \
    if (liveness_broker.IsHeapObjectAlive(self)) return false;              \
    self->Class::PreFinalizer();                                            \
    return true;                                                            \
  }                                                                         \
                                                                            \
 private:                                                                   \
  CPPGC_NO_UNIQUE_ADDRESS internal::PrefinalizerRegistration<Class>         \
      prefinalizer_dummy_{this};                                            \
  friend class internal::__thisIsHereToForceASemicolonAfterThisMacro

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_PREFINALIZER_H_
