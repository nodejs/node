// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MICROTASK_H_
#define V8_OBJECTS_MICROTASK_H_

#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class StructBodyDescriptor;

#include "torque-generated/src/objects/microtask-tq.inc"

// Abstract base class for all microtasks that can be scheduled on the
// microtask queue. This class merely serves the purpose of a marker
// interface.
class Microtask : public TorqueGeneratedMicrotask<Microtask, Struct> {
 public:
  TQ_OBJECT_CONSTRUCTORS(Microtask)
};

// A CallbackTask is a special Microtask that allows us to schedule
// C++ microtask callbacks on the microtask queue. This is heavily
// used by Blink for example.
class CallbackTask
    : public TorqueGeneratedCallbackTask<CallbackTask, Microtask> {
 public:
  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(CallbackTask)
};

// A CallableTask is a special (internal) Microtask that allows us to
// schedule arbitrary callables on the microtask queue. We use this
// for various tests of the microtask queue.
class CallableTask
    : public TorqueGeneratedCallableTask<CallableTask, Microtask> {
 public:
  // Dispatched behavior.
  DECL_VERIFIER(CallableTask)
  void BriefPrintDetails(std::ostream& os);

  using BodyDescriptor = StructBodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(CallableTask)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_H_
