// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_THREAD_LOCAL_TOP_H_
#define V8_EXECUTION_THREAD_LOCAL_TOP_H_

#include "src/common/globals.h"
#include "src/execution/thread-id.h"
#include "src/objects/contexts.h"
#include "src/utils/utils.h"

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
#include "src/heap/base/stack.h"
#endif

namespace v8 {

class TryCatch;

namespace internal {

class ExternalCallbackScope;
class Isolate;
class PromiseOnStack;
class Simulator;

class ThreadLocalTop {
 public:
  // TODO(all): This is not particularly beautiful. We should probably
  // refactor this to really consist of just Addresses and 32-bit
  // integer fields.
#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  static constexpr uint32_t kSizeInBytes = 25 * kSystemPointerSize;
#else
  static constexpr uint32_t kSizeInBytes = 24 * kSystemPointerSize;
#endif

  // Does early low-level initialization that does not depend on the
  // isolate being present.
  ThreadLocalTop() = default;

  // Initialize the thread data.
  void Initialize(Isolate*);

  // The top C++ try catch handler or nullptr if none are registered.
  //
  // This field is not guaranteed to hold an address that can be
  // used for comparison with addresses into the JS stack. If such
  // an address is needed, use try_catch_handler_address.
  v8::TryCatch* try_catch_handler_ = nullptr;

  // Get the address of the top C++ try catch handler or nullptr if
  // none are registered.
  //
  // This method always returns an address that can be compared to
  // pointers into the JavaScript stack.  When running on actual
  // hardware, try_catch_handler_address and TryCatchHandler return
  // the same pointer.  When running on a simulator with a separate JS
  // stack, try_catch_handler_address returns a JS stack address that
  // corresponds to the place on the JS stack where the C++ handler
  // would have been if the stack were not separate.
  Address try_catch_handler_address() {
    return reinterpret_cast<Address>(
        v8::TryCatch::JSStackComparableAddress(try_catch_handler_));
  }

  // Call depth represents nested v8 api calls. Instead of storing the nesting
  // level as an integer, we store the stack height of the last API entry. This
  // additional information is used when we decide whether to trigger a debug
  // break at a function entry.
  template <typename Scope>
  void IncrementCallDepth(Scope* stack_allocated_scope) {
    stack_allocated_scope->previous_stack_height_ = last_api_entry_;
#if defined(USE_SIMULATOR) || defined(V8_USE_ADDRESS_SANITIZER)
    StoreCurrentStackPosition();
#else
    last_api_entry_ = reinterpret_cast<i::Address>(stack_allocated_scope);
#endif
  }

#if defined(USE_SIMULATOR) || defined(V8_USE_ADDRESS_SANITIZER)
  void StoreCurrentStackPosition();
#endif

  template <typename Scope>
  void DecrementCallDepth(Scope* stack_allocated_scope) {
    last_api_entry_ = stack_allocated_scope->previous_stack_height_;
  }

  bool CallDepthIsZero() const { return last_api_entry_ == kNullAddress; }

  void Free();

  Isolate* isolate_ = nullptr;
  // The context where the current execution method is created and for variable
  // lookups.
  // TODO(3770): This field is read/written from generated code, so it would
  // be cleaner to make it an "Address raw_context_", and construct a Context
  // object in the getter. Same for {pending_handler_context_} below. In the
  // meantime, assert that the memory layout is the same.
  STATIC_ASSERT(sizeof(Context) == kSystemPointerSize);
  Context context_;
  ThreadId thread_id_ = ThreadId::Invalid();
  Object pending_exception_;

  // Communication channel between Isolate::FindHandler and the CEntry.
  Context pending_handler_context_;
  Address pending_handler_entrypoint_ = kNullAddress;
  Address pending_handler_constant_pool_ = kNullAddress;
  Address pending_handler_fp_ = kNullAddress;
  Address pending_handler_sp_ = kNullAddress;

  Address last_api_entry_ = kNullAddress;

  // Communication channel between Isolate::Throw and message consumers.
  Object pending_message_obj_;
  bool rethrowing_message_ = false;

  // Use a separate value for scheduled exceptions to preserve the
  // invariants that hold about pending_exception.  We may want to
  // unify them later.
  bool external_caught_exception_ = false;
  Object scheduled_exception_;

  // Stack.
  // The frame pointer of the top c entry frame.
  Address c_entry_fp_ = kNullAddress;
  // Try-blocks are chained through the stack.
  Address handler_ = kNullAddress;
  // C function that was called at c entry.
  Address c_function_ = kNullAddress;

  // Throwing an exception may cause a Promise rejection.  For this purpose
  // we keep track of a stack of nested promises and the corresponding
  // try-catch handlers.
  PromiseOnStack* promise_on_stack_ = nullptr;

  // Simulator field is always present to get predictable layout.
  Simulator* simulator_ = nullptr;

  // The stack pointer of the bottom JS entry frame.
  Address js_entry_sp_ = kNullAddress;
  // The external callback we're currently in.
  ExternalCallbackScope* external_callback_scope_ = nullptr;
  StateTag current_vm_state_ = EXTERNAL;

  // Call back function to report unsafe JS accesses.
  v8::FailedAccessCheckCallback failed_access_check_callback_ = nullptr;

  // Address of the thread-local "thread in wasm" flag.
  Address thread_in_wasm_flag_address_ = kNullAddress;

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  ::heap::base::Stack stack_ = ::heap::base::Stack(nullptr);
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_THREAD_LOCAL_TOP_H_
