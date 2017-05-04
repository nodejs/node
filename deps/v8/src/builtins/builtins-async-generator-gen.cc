// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/frames-inl.h"

namespace v8 {
namespace internal {

using compiler::Node;

namespace {

// Describe fields of Context associated with AsyncGeneratorAwait resume
// closures.
class AwaitContext {
 public:
  enum Fields { kGeneratorSlot = Context::MIN_CONTEXT_SLOTS, kLength };
};

class AsyncGeneratorBuiltinsAssembler : public AsyncBuiltinsAssembler {
 public:
  explicit AsyncGeneratorBuiltinsAssembler(CodeAssemblerState* state)
      : AsyncBuiltinsAssembler(state) {}

  inline Node* LoadGeneratorState(Node* const generator) {
    return LoadObjectField(generator, JSGeneratorObject::kContinuationOffset);
  }

  inline Node* IsGeneratorStateClosed(Node* const state) {
    return SmiEqual(state, SmiConstant(JSGeneratorObject::kGeneratorClosed));
  }
  inline Node* IsGeneratorClosed(Node* const generator) {
    return IsGeneratorStateClosed(LoadGeneratorState(generator));
  }

  inline Node* IsGeneratorStateSuspended(Node* const state) {
    return SmiGreaterThanOrEqual(state, SmiConstant(0));
  }

  inline Node* IsGeneratorSuspended(Node* const generator) {
    return IsGeneratorStateSuspended(LoadGeneratorState(generator));
  }

  inline Node* IsGeneratorStateSuspendedAtStart(Node* const state) {
    return SmiEqual(state, SmiConstant(0));
  }

  inline Node* IsGeneratorStateNotExecuting(Node* const state) {
    return SmiNotEqual(state,
                       SmiConstant(JSGeneratorObject::kGeneratorExecuting));
  }
  inline Node* IsGeneratorNotExecuting(Node* const generator) {
    return IsGeneratorStateNotExecuting(LoadGeneratorState(generator));
  }

  inline Node* LoadGeneratorAwaitedPromise(Node* const generator) {
    return LoadObjectField(generator,
                           JSAsyncGeneratorObject::kAwaitedPromiseOffset);
  }

  inline Node* IsGeneratorNotSuspendedForAwait(Node* const generator) {
    return IsUndefined(LoadGeneratorAwaitedPromise(generator));
  }

  inline Node* IsGeneratorSuspendedForAwait(Node* const generator) {
    return HasInstanceType(LoadGeneratorAwaitedPromise(generator),
                           JS_PROMISE_TYPE);
  }

  inline void ClearAwaitedPromise(Node* const generator) {
    StoreObjectFieldRoot(generator,
                         JSAsyncGeneratorObject::kAwaitedPromiseOffset,
                         Heap::kUndefinedValueRootIndex);
  }

  inline void CloseGenerator(Node* const generator) {
    StoreObjectFieldNoWriteBarrier(
        generator, JSGeneratorObject::kContinuationOffset,
        SmiConstant(JSGeneratorObject::kGeneratorClosed));
  }

  inline Node* IsFastJSIterResult(Node* const value, Node* const context) {
    CSA_ASSERT(this, TaggedIsNotSmi(value));
    Node* const native_context = LoadNativeContext(context);
    return WordEqual(
        LoadMap(value),
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX));
  }

  inline Node* LoadFirstAsyncGeneratorRequestFromQueue(Node* const generator) {
    return LoadObjectField(generator, JSAsyncGeneratorObject::kQueueOffset);
  }

  inline Node* LoadResumeTypeFromAsyncGeneratorRequest(Node* const request) {
    return LoadObjectField(request, AsyncGeneratorRequest::kResumeModeOffset);
  }

  inline Node* LoadPromiseFromAsyncGeneratorRequest(Node* const request) {
    return LoadObjectField(request, AsyncGeneratorRequest::kPromiseOffset);
  }

  inline Node* LoadValueFromAsyncGeneratorRequest(Node* const request) {
    return LoadObjectField(request, AsyncGeneratorRequest::kValueOffset);
  }

  inline Node* IsAbruptResumeType(Node* const resume_type) {
    return SmiNotEqual(resume_type, SmiConstant(JSGeneratorObject::kNext));
  }

  void AsyncGeneratorEnqueue(Node* context, Node* generator, Node* value,
                             JSAsyncGeneratorObject::ResumeMode resume_mode,
                             const char* method_name);

  Node* TakeFirstAsyncGeneratorRequestFromQueue(Node* generator);
  Node* TakeFirstAsyncGeneratorRequestFromQueueIfPresent(Node* generator,
                                                         Label* if_not_present);
  void AddAsyncGeneratorRequestToQueue(Node* generator, Node* request);

  Node* AllocateAsyncGeneratorRequest(
      JSAsyncGeneratorObject::ResumeMode resume_mode, Node* resume_value,
      Node* promise);

  // Shared implementation of the catchable and uncatchable variations of Await
  // for AsyncGenerators.
  template <typename Descriptor>
  void AsyncGeneratorAwait(bool is_catchable);
  void AsyncGeneratorAwaitResumeClosure(
      Node* context, Node* value,
      JSAsyncGeneratorObject::ResumeMode resume_mode);
};

// Shared implementation for the 3 Async Iterator protocol methods of Async
// Generators.
void AsyncGeneratorBuiltinsAssembler::AsyncGeneratorEnqueue(
    Node* context, Node* generator, Node* value,
    JSAsyncGeneratorObject::ResumeMode resume_mode, const char* method_name) {
  // AsyncGeneratorEnqueue produces a new Promise, and appends it to the list
  // of async generator requests to be executed. If the generator is not
  // presently executing, then this method will loop through, processing each
  // request from front to back.
  // This loop resides in AsyncGeneratorResumeNext.
  Node* promise = AllocateAndInitJSPromise(context);

  Label enqueue(this), if_receiverisincompatible(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(generator), &if_receiverisincompatible);
  Branch(HasInstanceType(generator, JS_ASYNC_GENERATOR_OBJECT_TYPE), &enqueue,
         &if_receiverisincompatible);

  BIND(&enqueue);
  {
    Label done(this);
    Node* const req =
        AllocateAsyncGeneratorRequest(resume_mode, value, promise);

    AddAsyncGeneratorRequestToQueue(generator, req);

    // Let state be generator.[[AsyncGeneratorState]]
    // If state is not "executing", then
    //     Perform AsyncGeneratorResumeNext(Generator)
    // Check if the {receiver} is running or already closed.
    Node* continuation = LoadGeneratorState(generator);

    GotoIf(SmiEqual(continuation,
                    SmiConstant(JSAsyncGeneratorObject::kGeneratorExecuting)),
           &done);

    CallBuiltin(Builtins::kAsyncGeneratorResumeNext, context, generator);

    Goto(&done);
    BIND(&done);
    Return(promise);
  }

  BIND(&if_receiverisincompatible);
  {
    Node* const error =
        MakeTypeError(MessageTemplate::kIncompatibleMethodReceiver, context,
                      CStringConstant(method_name), generator);

    CallBuiltin(Builtins::kRejectNativePromise, context, promise, error,
                TrueConstant());
    Return(promise);
  }
}

Node* AsyncGeneratorBuiltinsAssembler::AllocateAsyncGeneratorRequest(
    JSAsyncGeneratorObject::ResumeMode resume_mode, Node* resume_value,
    Node* promise) {
  CSA_SLOW_ASSERT(this, HasInstanceType(promise, JS_PROMISE_TYPE));
  Node* request = Allocate(AsyncGeneratorRequest::kSize);
  StoreMapNoWriteBarrier(request, Heap::kAsyncGeneratorRequestMapRootIndex);
  StoreObjectFieldNoWriteBarrier(request, AsyncGeneratorRequest::kNextOffset,
                                 UndefinedConstant());
  StoreObjectFieldNoWriteBarrier(request,
                                 AsyncGeneratorRequest::kResumeModeOffset,
                                 SmiConstant(resume_mode));
  StoreObjectFieldNoWriteBarrier(request, AsyncGeneratorRequest::kValueOffset,
                                 resume_value);
  StoreObjectFieldNoWriteBarrier(request, AsyncGeneratorRequest::kPromiseOffset,
                                 promise);
  StoreObjectFieldRoot(request, AsyncGeneratorRequest::kNextOffset,
                       Heap::kUndefinedValueRootIndex);
  return request;
}

void AsyncGeneratorBuiltinsAssembler::AsyncGeneratorAwaitResumeClosure(
    Node* context, Node* value,
    JSAsyncGeneratorObject::ResumeMode resume_mode) {
  Node* const generator =
      LoadContextElement(context, AwaitContext::kGeneratorSlot);
  CSA_SLOW_ASSERT(this,
                  HasInstanceType(generator, JS_ASYNC_GENERATOR_OBJECT_TYPE));

#if defined(DEBUG) && defined(ENABLE_SLOW_DCHECKS)
  Node* const awaited_promise = LoadGeneratorAwaitedPromise(generator);
  CSA_SLOW_ASSERT(this, HasInstanceType(awaited_promise, JS_PROMISE_TYPE));
  CSA_SLOW_ASSERT(this, SmiNotEqual(LoadObjectField(awaited_promise,
                                                    JSPromise::kStatusOffset),
                                    SmiConstant(v8::Promise::kPending)));
#endif

  ClearAwaitedPromise(generator);

  CSA_SLOW_ASSERT(this, IsGeneratorSuspended(generator));

  CallStub(CodeFactory::ResumeGenerator(isolate()), context, value, generator,
           SmiConstant(resume_mode),
           SmiConstant(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));

  TailCallStub(CodeFactory::AsyncGeneratorResumeNext(isolate()), context,
               generator);
}

template <typename Descriptor>
void AsyncGeneratorBuiltinsAssembler::AsyncGeneratorAwait(bool is_catchable) {
  Node* generator = Parameter(1);
  Node* value = Parameter(2);
  Node* context = Parameter(5);

  CSA_SLOW_ASSERT(this,
                  HasInstanceType(generator, JS_ASYNC_GENERATOR_OBJECT_TYPE));

  Node* const request = LoadFirstAsyncGeneratorRequestFromQueue(generator);
  CSA_ASSERT(this, WordNotEqual(request, UndefinedConstant()));

  NodeGenerator1 closure_context = [&](Node* native_context) -> Node* {
    Node* const context =
        CreatePromiseContext(native_context, AwaitContext::kLength);
    StoreContextElementNoWriteBarrier(context, AwaitContext::kGeneratorSlot,
                                      generator);
    return context;
  };

  Node* outer_promise =
      LoadObjectField(request, AsyncGeneratorRequest::kPromiseOffset);

  const int resolve_index = Context::ASYNC_GENERATOR_AWAIT_RESOLVE_SHARED_FUN;
  const int reject_index = Context::ASYNC_GENERATOR_AWAIT_REJECT_SHARED_FUN;

  Node* promise =
      Await(context, generator, value, outer_promise, closure_context,
            resolve_index, reject_index, is_catchable);

  CSA_SLOW_ASSERT(this, IsGeneratorNotSuspendedForAwait(generator));
  StoreObjectField(generator, JSAsyncGeneratorObject::kAwaitedPromiseOffset,
                   promise);
  Return(UndefinedConstant());
}

void AsyncGeneratorBuiltinsAssembler::AddAsyncGeneratorRequestToQueue(
    Node* generator, Node* request) {
  VARIABLE(var_current, MachineRepresentation::kTagged);
  Label empty(this), loop(this, &var_current), done(this);

  var_current.Bind(
      LoadObjectField(generator, JSAsyncGeneratorObject::kQueueOffset));
  Branch(IsUndefined(var_current.value()), &empty, &loop);

  BIND(&empty);
  {
    StoreObjectField(generator, JSAsyncGeneratorObject::kQueueOffset, request);
    Goto(&done);
  }

  BIND(&loop);
  {
    Label loop_next(this), next_empty(this);
    Node* current = var_current.value();
    Node* next = LoadObjectField(current, AsyncGeneratorRequest::kNextOffset);

    Branch(IsUndefined(next), &next_empty, &loop_next);
    BIND(&next_empty);
    {
      StoreObjectField(current, AsyncGeneratorRequest::kNextOffset, request);
      Goto(&done);
    }

    BIND(&loop_next);
    {
      var_current.Bind(next);
      Goto(&loop);
    }
  }
  BIND(&done);
}

Node* AsyncGeneratorBuiltinsAssembler::TakeFirstAsyncGeneratorRequestFromQueue(
    Node* generator) {
  // Removes and returns the first AsyncGeneratorRequest from a
  // JSAsyncGeneratorObject's queue. Asserts that the queue is not empty.
  CSA_ASSERT(this, HasInstanceType(generator, JS_ASYNC_GENERATOR_OBJECT_TYPE));
  Node* request =
      LoadObjectField(generator, JSAsyncGeneratorObject::kQueueOffset);
  CSA_ASSERT(this, WordNotEqual(request, UndefinedConstant()));

  Node* next = LoadObjectField(request, AsyncGeneratorRequest::kNextOffset);

  StoreObjectField(generator, JSAsyncGeneratorObject::kQueueOffset, next);
  return request;
}
}  // namespace

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-asyncgenerator-prototype-next
TF_BUILTIN(AsyncGeneratorPrototypeNext, AsyncGeneratorBuiltinsAssembler) {
  Node* const generator = Parameter(Descriptor::kReceiver);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);
  AsyncGeneratorEnqueue(context, generator, value,
                        JSAsyncGeneratorObject::kNext,
                        "[AsyncGenerator].prototype.next");
}

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-asyncgenerator-prototype-return
TF_BUILTIN(AsyncGeneratorPrototypeReturn, AsyncGeneratorBuiltinsAssembler) {
  Node* generator = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  AsyncGeneratorEnqueue(context, generator, value,
                        JSAsyncGeneratorObject::kReturn,
                        "[AsyncGenerator].prototype.return");
}

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-asyncgenerator-prototype-throw
TF_BUILTIN(AsyncGeneratorPrototypeThrow, AsyncGeneratorBuiltinsAssembler) {
  Node* generator = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  AsyncGeneratorEnqueue(context, generator, value,
                        JSAsyncGeneratorObject::kThrow,
                        "[AsyncGenerator].prototype.throw");
}

TF_BUILTIN(AsyncGeneratorAwaitResolveClosure, AsyncGeneratorBuiltinsAssembler) {
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  AsyncGeneratorAwaitResumeClosure(context, value,
                                   JSAsyncGeneratorObject::kNext);
}

TF_BUILTIN(AsyncGeneratorAwaitRejectClosure, AsyncGeneratorBuiltinsAssembler) {
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  AsyncGeneratorAwaitResumeClosure(context, value,
                                   JSAsyncGeneratorObject::kThrow);
}

TF_BUILTIN(AsyncGeneratorAwaitUncaught, AsyncGeneratorBuiltinsAssembler) {
  const bool kIsCatchable = false;
  AsyncGeneratorAwait<Descriptor>(kIsCatchable);
}

TF_BUILTIN(AsyncGeneratorAwaitCaught, AsyncGeneratorBuiltinsAssembler) {
  const bool kIsCatchable = true;
  AsyncGeneratorAwait<Descriptor>(kIsCatchable);
}

TF_BUILTIN(AsyncGeneratorResumeNext, AsyncGeneratorBuiltinsAssembler) {
  typedef AsyncGeneratorResumeNextDescriptor Descriptor;
  Node* const generator = Parameter(Descriptor::kGenerator);
  Node* const context = Parameter(Descriptor::kContext);

  // The penultimate step of proposal-async-iteration/#sec-asyncgeneratorresolve
  // and proposal-async-iteration/#sec-asyncgeneratorreject both recursively
  // invoke AsyncGeneratorResumeNext() again.
  //
  // This implementation does not implement this recursively, but instead
  // performs a loop in AsyncGeneratorResumeNext, which  continues as long as
  // there is an AsyncGeneratorRequest in the queue, and as long as the
  // generator is not suspended due to an AwaitExpression.
  VARIABLE(var_state, MachineRepresentation::kTaggedSigned,
           LoadGeneratorState(generator));
  VARIABLE(var_next, MachineRepresentation::kTagged,
           LoadFirstAsyncGeneratorRequestFromQueue(generator));
  Variable* labels[] = {&var_state, &var_next};
  Label start(this, 2, labels);
  Goto(&start);
  BIND(&start);

  CSA_ASSERT(this, IsGeneratorNotExecuting(generator));

  // Stop resuming if suspended for Await.
  ReturnIf(IsGeneratorSuspendedForAwait(generator), UndefinedConstant());

  // Stop resuming if request queue is empty.
  ReturnIf(IsUndefined(var_next.value()), UndefinedConstant());

  Node* const next = var_next.value();
  Node* const resume_type = LoadResumeTypeFromAsyncGeneratorRequest(next);

  Label if_abrupt(this), if_normal(this), resume_generator(this);
  Branch(IsAbruptResumeType(resume_type), &if_abrupt, &if_normal);
  BIND(&if_abrupt);
  {
    Label settle_promise(this), fulfill_promise(this), reject_promise(this);
    GotoIfNot(IsGeneratorStateSuspendedAtStart(var_state.value()),
              &settle_promise);
    CloseGenerator(generator);
    var_state.Bind(SmiConstant(JSGeneratorObject::kGeneratorClosed));

    Goto(&settle_promise);
    BIND(&settle_promise);

    GotoIfNot(IsGeneratorStateClosed(var_state.value()), &resume_generator);

    Branch(SmiEqual(resume_type, SmiConstant(JSGeneratorObject::kReturn)),
           &fulfill_promise, &reject_promise);

    BIND(&fulfill_promise);
    CallBuiltin(Builtins::kAsyncGeneratorResolve, context, generator,
                LoadValueFromAsyncGeneratorRequest(next), TrueConstant());
    var_next.Bind(LoadFirstAsyncGeneratorRequestFromQueue(generator));
    Goto(&start);

    BIND(&reject_promise);
    CallBuiltin(Builtins::kAsyncGeneratorReject, context, generator,
                LoadValueFromAsyncGeneratorRequest(next));
    var_next.Bind(LoadFirstAsyncGeneratorRequestFromQueue(generator));
    Goto(&start);
  }

  BIND(&if_normal);
  {
    GotoIfNot(IsGeneratorStateClosed(var_state.value()), &resume_generator);
    CallBuiltin(Builtins::kAsyncGeneratorResolve, context, generator,
                UndefinedConstant(), TrueConstant());
    var_state.Bind(LoadGeneratorState(generator));
    var_next.Bind(LoadFirstAsyncGeneratorRequestFromQueue(generator));
    Goto(&start);
  }

  BIND(&resume_generator);
  {
    CallStub(CodeFactory::ResumeGenerator(isolate()), context,
             LoadValueFromAsyncGeneratorRequest(next), generator, resume_type,
             SmiConstant(static_cast<int>(SuspendFlags::kAsyncGeneratorYield)));
    var_state.Bind(LoadGeneratorState(generator));
    var_next.Bind(LoadFirstAsyncGeneratorRequestFromQueue(generator));
    Goto(&start);
  }
}

TF_BUILTIN(AsyncGeneratorResolve, AsyncGeneratorBuiltinsAssembler) {
  Node* const generator = Parameter(Descriptor::kGenerator);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const done = Parameter(Descriptor::kDone);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_SLOW_ASSERT(this,
                  HasInstanceType(generator, JS_ASYNC_GENERATOR_OBJECT_TYPE));
  CSA_ASSERT(this, IsGeneratorNotSuspendedForAwait(generator));

  Node* const next = TakeFirstAsyncGeneratorRequestFromQueue(generator);
  Node* const promise = LoadPromiseFromAsyncGeneratorRequest(next);

  Node* const wrapper = AllocateAndInitJSPromise(context);
  CallBuiltin(Builtins::kResolveNativePromise, context, wrapper, value);

  Node* const on_fulfilled =
      CreateUnwrapClosure(LoadNativeContext(context), done);

  // Per spec, AsyncGeneratorResolve() returns undefined. However, for the
  // benefit of %TraceExit(), return the Promise.
  Return(CallBuiltin(Builtins::kPerformNativePromiseThen, context, wrapper,
                     on_fulfilled, UndefinedConstant(), promise));
}

TF_BUILTIN(AsyncGeneratorReject, AsyncGeneratorBuiltinsAssembler) {
  typedef AsyncGeneratorRejectDescriptor Descriptor;
  Node* const generator = Parameter(Descriptor::kGenerator);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const next = TakeFirstAsyncGeneratorRequestFromQueue(generator);
  Node* const promise = LoadPromiseFromAsyncGeneratorRequest(next);

  Return(CallBuiltin(Builtins::kRejectNativePromise, context, promise, value,
                     TrueConstant()));
}

}  // namespace internal
}  // namespace v8
