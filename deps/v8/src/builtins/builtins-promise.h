// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROMISE_H_
#define V8_BUILTINS_BUILTINS_PROMISE_H_

#include "src/contexts.h"

namespace v8 {
namespace internal {

class PromiseBuiltins {
 public:
  enum PromiseResolvingFunctionContextSlot {
    // The promise which resolve/reject callbacks fulfill.
    kPromiseSlot = Context::MIN_CONTEXT_SLOTS,

    // Whether the callback was already invoked.
    kAlreadyResolvedSlot,

    // Whether to trigger a debug event or not. Used in catch
    // prediction.
    kDebugEventSlot,
    kPromiseContextLength,
  };

  // TODO(bmeurer): Move this to a proper context map in contexts.h?
  // Similar to the AwaitContext that we introduced for await closures.
  enum PromiseAllResolveElementContextSlots {
    // Remaining elements count
    kPromiseAllResolveElementRemainingSlot = Context::MIN_CONTEXT_SLOTS,

    // Promise capability from Promise.all
    kPromiseAllResolveElementCapabilitySlot,

    // Values array from Promise.all
    kPromiseAllResolveElementValuesArraySlot,

    kPromiseAllResolveElementLength
  };

  enum FunctionContextSlot {
    kCapabilitySlot = Context::MIN_CONTEXT_SLOTS,

    kCapabilitiesContextLength,
  };

  // This is used by the Promise.prototype.finally builtin to store
  // onFinally callback and the Promise constructor.
  // TODO(gsathya): For native promises we can create a variant of
  // this without extra space for the constructor to save memory.
  enum PromiseFinallyContextSlot {
    kOnFinallySlot = Context::MIN_CONTEXT_SLOTS,
    kConstructorSlot,

    kPromiseFinallyContextLength,
  };

  // This is used by the ThenFinally and CatchFinally builtins to
  // store the value to return or reason to throw.
  enum PromiseValueThunkOrReasonContextSlot {
    kValueSlot = Context::MIN_CONTEXT_SLOTS,

    kPromiseValueThunkOrReasonContextLength,
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseBuiltins);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROMISE_H_
