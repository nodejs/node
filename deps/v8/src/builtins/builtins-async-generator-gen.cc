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

  inline Node* TaggedIsAsyncGenerator(Node* tagged_object) {
    TNode<BoolT> if_notsmi = TaggedIsNotSmi(tagged_object);
    return Select<BoolT>(if_notsmi,
                         [=] {
                           return HasInstanceType(
                               tagged_object, JS_ASYNC_GENERATOR_OBJECT_TYPE);
                         },
                         [=] { return if_notsmi; });
  }
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

  inline Node* IsGeneratorAwaiting(Node* const generator) {
    Node* is_generator_awaiting =
        LoadObjectField(generator, JSAsyncGeneratorObject::kIsAwaitingOffset);
    return SmiEqual(is_generator_awaiting, SmiConstant(1));
  }

  inline void SetGeneratorAwaiting(Node* const generator) {
    CSA_ASSERT(this, Word32BinaryNot(IsGeneratorAwaiting(generator)));
    StoreObjectFieldNoWriteBarrier(
        generator, JSAsyncGeneratorObject::kIsAwaitingOffset, SmiConstant(1));
    CSA_ASSERT(this, IsGeneratorAwaiting(generator));
  }

  inline void SetGeneratorNotAwaiting(Node* const generator) {
    CSA_ASSERT(this, IsGeneratorAwaiting(generator));
    StoreObjectFieldNoWriteBarrier(
        generator, JSAsyncGeneratorObject::kIsAwaitingOffset, SmiConstant(0));
    CSA_ASSERT(this, Word32BinaryNot(IsGeneratorAwaiting(generator)));
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

  void AsyncGeneratorEnqueue(CodeStubArguments* args, Node* context,
                             Node* generator, Node* value,
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
    CodeStubArguments* args, Node* context, Node* generator, Node* value,
    JSAsyncGeneratorObject::ResumeMode resume_mode, const char* method_name) {
  // AsyncGeneratorEnqueue produces a new Promise, and appends it to the list
  // of async generator requests to be executed. If the generator is not
  // presently executing, then this method will loop through, processing each
  // request from front to back.
  // This loop resides in AsyncGeneratorResumeNext.
  Node* promise = AllocateAndInitJSPromise(context);

  Label enqueue(this), if_receiverisincompatible(this, Label::kDeferred);

  Branch(TaggedIsAsyncGenerator(generator), &enqueue,
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
    args->PopAndReturn(promise);
  }

  BIND(&if_receiverisincompatible);
  {
    Node* const error =
        MakeTypeError(MessageTemplate::kIncompatibleMethodReceiver, context,
                      StringConstant(method_name), generator);

    CallBuiltin(Builtins::kRejectPromise, context, promise, error,
                TrueConstant());
    args->PopAndReturn(promise);
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
  CSA_SLOW_ASSERT(this, TaggedIsAsyncGenerator(generator));

  SetGeneratorNotAwaiting(generator);

  CSA_SLOW_ASSERT(this, IsGeneratorSuspended(generator));

  // Remember the {resume_mode} for the {generator}.
  StoreObjectFieldNoWriteBarrier(generator,
                                 JSGeneratorObject::kResumeModeOffset,
                                 SmiConstant(resume_mode));

  CallStub(CodeFactory::ResumeGenerator(isolate()), context, value, generator);

  TailCallBuiltin(Builtins::kAsyncGeneratorResumeNext, context, generator);
}

template <typename Descriptor>
void AsyncGeneratorBuiltinsAssembler::AsyncGeneratorAwait(bool is_catchable) {
  Node* generator = Parameter(Descriptor::kGenerator);
  Node* value = Parameter(Descriptor::kAwaited);
  Node* context = Parameter(Descriptor::kContext);

  CSA_SLOW_ASSERT(this, TaggedIsAsyncGenerator(generator));

  Node* const request = LoadFirstAsyncGeneratorRequestFromQueue(generator);
  CSA_ASSERT(this, IsNotUndefined(request));

  ContextInitializer init_closure_context = [&](Node* context) {
    StoreContextElementNoWriteBarrier(context, AwaitContext::kGeneratorSlot,
                                      generator);
  };

  Node* outer_promise =
      LoadObjectField(request, AsyncGeneratorRequest::kPromiseOffset);

  const int resolve_index = Context::ASYNC_GENERATOR_AWAIT_RESOLVE_SHARED_FUN;
  const int reject_index = Context::ASYNC_GENERATOR_AWAIT_REJECT_SHARED_FUN;

  SetGeneratorAwaiting(generator);
  Await(context, generator, value, outer_promise, AwaitContext::kLength,
        init_closure_context, resolve_index, reject_index, is_catchable);
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
  CSA_ASSERT(this, TaggedIsAsyncGenerator(generator));
  Node* request =
      LoadObjectField(generator, JSAsyncGeneratorObject::kQueueOffset);
  CSA_ASSERT(this, IsNotUndefined(request));

  Node* next = LoadObjectField(request, AsyncGeneratorRequest::kNextOffset);

  StoreObjectField(generator, JSAsyncGeneratorObject::kQueueOffset, next);
  return request;
}
}  // namespace

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-asyncgenerator-prototype-next
TF_BUILTIN(AsyncGeneratorPrototypeNext, AsyncGeneratorBuiltinsAssembler) {
  const int kValueArg = 0;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* generator = args.GetReceiver();
  Node* value = args.GetOptionalArgumentValue(kValueArg);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  AsyncGeneratorEnqueue(&args, context, generator, value,
                        JSAsyncGeneratorObject::kNext,
                        "[AsyncGenerator].prototype.next");
}

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-asyncgenerator-prototype-return
TF_BUILTIN(AsyncGeneratorPrototypeReturn, AsyncGeneratorBuiltinsAssembler) {
  const int kValueArg = 0;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* generator = args.GetReceiver();
  Node* value = args.GetOptionalArgumentValue(kValueArg);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  AsyncGeneratorEnqueue(&args, context, generator, value,
                        JSAsyncGeneratorObject::kReturn,
                        "[AsyncGenerator].prototype.return");
}

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-asyncgenerator-prototype-throw
TF_BUILTIN(AsyncGeneratorPrototypeThrow, AsyncGeneratorBuiltinsAssembler) {
  const int kValueArg = 0;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* generator = args.GetReceiver();
  Node* value = args.GetOptionalArgumentValue(kValueArg);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  AsyncGeneratorEnqueue(&args, context, generator, value,
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
  Variable* loop_variables[] = {&var_state, &var_next};
  Label start(this, 2, loop_variables);
  Goto(&start);
  BIND(&start);

  CSA_ASSERT(this, IsGeneratorNotExecuting(generator));

  // Stop resuming if suspended for Await.
  ReturnIf(IsGeneratorAwaiting(generator), UndefinedConstant());

  // Stop resuming if request queue is empty.
  ReturnIf(IsUndefined(var_next.value()), UndefinedConstant());

  Node* const next = var_next.value();
  Node* const resume_type = LoadResumeTypeFromAsyncGeneratorRequest(next);

  Label if_abrupt(this), if_normal(this), resume_generator(this);
  Branch(IsAbruptResumeType(resume_type), &if_abrupt, &if_normal);
  BIND(&if_abrupt);
  {
    Label settle_promise(this), if_return(this), if_throw(this);
    GotoIfNot(IsGeneratorStateSuspendedAtStart(var_state.value()),
              &settle_promise);
    CloseGenerator(generator);
    var_state.Bind(SmiConstant(JSGeneratorObject::kGeneratorClosed));
    Goto(&settle_promise);

    BIND(&settle_promise);
    Node* next_value = LoadValueFromAsyncGeneratorRequest(next);
    Branch(SmiEqual(resume_type, SmiConstant(JSGeneratorObject::kReturn)),
           &if_return, &if_throw);

    BIND(&if_return);
    // For "return" completions, await the sent value. If the Await succeeds,
    // and the generator is not closed, resume the generator with a "return"
    // completion to allow `finally` blocks to be evaluated. Otherwise, perform
    // AsyncGeneratorResolve(awaitedValue, true). If the await fails and the
    // generator is not closed, resume the generator with a "throw" completion.
    // If the generator was closed, perform AsyncGeneratorReject(thrownValue).
    // In all cases, the last step is to call AsyncGeneratorResumeNext.
    Node* is_caught = CallRuntime(Runtime::kAsyncGeneratorHasCatchHandlerForPC,
                                  context, generator);
    TailCallBuiltin(Builtins::kAsyncGeneratorReturn, context, generator,
                    next_value, is_caught);

    BIND(&if_throw);
    GotoIfNot(IsGeneratorStateClosed(var_state.value()), &resume_generator);
    CallBuiltin(Builtins::kAsyncGeneratorReject, context, generator,
                next_value);
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
    // Remember the {resume_type} for the {generator}.
    StoreObjectFieldNoWriteBarrier(
        generator, JSGeneratorObject::kResumeModeOffset, resume_type);
    CallStub(CodeFactory::ResumeGenerator(isolate()), context,
             LoadValueFromAsyncGeneratorRequest(next), generator);
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

  CSA_SLOW_ASSERT(this, TaggedIsAsyncGenerator(generator));
  CSA_ASSERT(this, Word32BinaryNot(IsGeneratorAwaiting(generator)));

  // If this assertion fails, the `value` component was not Awaited as it should
  // have been, per https://github.com/tc39/proposal-async-iteration/pull/102/.
  CSA_SLOW_ASSERT(this, TaggedDoesntHaveInstanceType(value, JS_PROMISE_TYPE));

  Node* const next = TakeFirstAsyncGeneratorRequestFromQueue(generator);
  Node* const promise = LoadPromiseFromAsyncGeneratorRequest(next);

  // Let iteratorResult be CreateIterResultObject(value, done).
  Node* const iter_result = Allocate(JSIteratorResult::kSize);
  {
    Node* map = LoadContextElement(LoadNativeContext(context),
                                   Context::ITERATOR_RESULT_MAP_INDEX);
    StoreMapNoWriteBarrier(iter_result, map);
    StoreObjectFieldRoot(iter_result, JSIteratorResult::kPropertiesOrHashOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldRoot(iter_result, JSIteratorResult::kElementsOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldNoWriteBarrier(iter_result, JSIteratorResult::kValueOffset,
                                   value);
    StoreObjectFieldNoWriteBarrier(iter_result, JSIteratorResult::kDoneOffset,
                                   done);
  }

  // Perform Call(promiseCapability.[[Resolve]], undefined, «iteratorResult»).
  CallBuiltin(Builtins::kResolvePromise, context, promise, iter_result);

  // Per spec, AsyncGeneratorResolve() returns undefined. However, for the
  // benefit of %TraceExit(), return the Promise.
  Return(promise);
}

TF_BUILTIN(AsyncGeneratorReject, AsyncGeneratorBuiltinsAssembler) {
  typedef AsyncGeneratorRejectDescriptor Descriptor;
  Node* const generator = Parameter(Descriptor::kGenerator);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const next = TakeFirstAsyncGeneratorRequestFromQueue(generator);
  Node* const promise = LoadPromiseFromAsyncGeneratorRequest(next);

  Return(CallBuiltin(Builtins::kRejectPromise, context, promise, value,
                     TrueConstant()));
}

TF_BUILTIN(AsyncGeneratorYield, AsyncGeneratorBuiltinsAssembler) {
  Node* const generator = Parameter(Descriptor::kGenerator);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const is_caught = Parameter(Descriptor::kIsCaught);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const request = LoadFirstAsyncGeneratorRequestFromQueue(generator);
  Node* const outer_promise = LoadPromiseFromAsyncGeneratorRequest(request);

  ContextInitializer init_closure_context = [&](Node* context) {
    StoreContextElementNoWriteBarrier(context, AwaitContext::kGeneratorSlot,
                                      generator);
  };

  const int on_resolve = Context::ASYNC_GENERATOR_YIELD_RESOLVE_SHARED_FUN;
  const int on_reject = Context::ASYNC_GENERATOR_AWAIT_REJECT_SHARED_FUN;

  SetGeneratorAwaiting(generator);
  Await(context, generator, value, outer_promise, AwaitContext::kLength,
        init_closure_context, on_resolve, on_reject, is_caught);
  Return(UndefinedConstant());
}

TF_BUILTIN(AsyncGeneratorYieldResolveClosure, AsyncGeneratorBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const generator =
      LoadContextElement(context, AwaitContext::kGeneratorSlot);

  SetGeneratorNotAwaiting(generator);

  // Per proposal-async-iteration/#sec-asyncgeneratoryield step 9
  // Return ! AsyncGeneratorResolve(_F_.[[Generator]], _value_, *false*).
  CallBuiltin(Builtins::kAsyncGeneratorResolve, context, generator, value,
              FalseConstant());

  TailCallBuiltin(Builtins::kAsyncGeneratorResumeNext, context, generator);
}

TF_BUILTIN(AsyncGeneratorReturn, AsyncGeneratorBuiltinsAssembler) {
  // AsyncGeneratorReturn is called when resuming requests with "return" resume
  // modes. It is similar to AsyncGeneratorAwait(), but selects different
  // resolve/reject closures depending on whether or not the generator is marked
  // as closed.
  //
  // In particular, non-closed generators will resume the generator with either
  // "return" or "throw" resume modes, allowing finally blocks or catch blocks
  // to be evaluated, as if the `await` were performed within the body of the
  // generator. (per proposal-async-iteration/#sec-asyncgeneratoryield step 8.b)
  //
  // Closed generators do not resume the generator in the resolve/reject
  // closures, but instead simply perform AsyncGeneratorResolve or
  // AsyncGeneratorReject with the awaited value
  // (per proposal-async-iteration/#sec-asyncgeneratorresumenext step 10.b.i)
  //
  // In all cases, the final step is to jump back to AsyncGeneratorResumeNext.
  Node* const generator = Parameter(Descriptor::kGenerator);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const is_caught = Parameter(Descriptor::kIsCaught);
  Node* const req = LoadFirstAsyncGeneratorRequestFromQueue(generator);
  CSA_ASSERT(this, IsNotUndefined(req));

  Label perform_await(this);
  VARIABLE(var_on_resolve, MachineType::PointerRepresentation(),
           IntPtrConstant(
               Context::ASYNC_GENERATOR_RETURN_CLOSED_RESOLVE_SHARED_FUN));
  VARIABLE(
      var_on_reject, MachineType::PointerRepresentation(),
      IntPtrConstant(Context::ASYNC_GENERATOR_RETURN_CLOSED_REJECT_SHARED_FUN));

  Node* const state = LoadGeneratorState(generator);
  GotoIf(IsGeneratorStateClosed(state), &perform_await);
  var_on_resolve.Bind(
      IntPtrConstant(Context::ASYNC_GENERATOR_RETURN_RESOLVE_SHARED_FUN));
  var_on_reject.Bind(
      IntPtrConstant(Context::ASYNC_GENERATOR_AWAIT_REJECT_SHARED_FUN));
  Goto(&perform_await);

  BIND(&perform_await);

  ContextInitializer init_closure_context = [&](Node* context) {
    StoreContextElementNoWriteBarrier(context, AwaitContext::kGeneratorSlot,
                                      generator);
  };

  SetGeneratorAwaiting(generator);
  Node* const context = Parameter(Descriptor::kContext);
  Node* const outer_promise = LoadPromiseFromAsyncGeneratorRequest(req);
  Await(context, generator, value, outer_promise, AwaitContext::kLength,
        init_closure_context, var_on_resolve.value(), var_on_reject.value(),
        is_caught);

  Return(UndefinedConstant());
}

// On-resolve closure for Await in AsyncGeneratorReturn
// Resume the generator with "return" resume_mode, and finally perform
// AsyncGeneratorResumeNext. Per
// proposal-async-iteration/#sec-asyncgeneratoryield step 8.e
TF_BUILTIN(AsyncGeneratorReturnResolveClosure,
           AsyncGeneratorBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const value = Parameter(Descriptor::kValue);
  AsyncGeneratorAwaitResumeClosure(context, value, JSGeneratorObject::kReturn);
}

// On-resolve closure for Await in AsyncGeneratorReturn
// Perform AsyncGeneratorResolve({awaited_value}, true) and finally perform
// AsyncGeneratorResumeNext.
TF_BUILTIN(AsyncGeneratorReturnClosedResolveClosure,
           AsyncGeneratorBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const generator =
      LoadContextElement(context, AwaitContext::kGeneratorSlot);

  SetGeneratorNotAwaiting(generator);

  // https://tc39.github.io/proposal-async-iteration/
  //    #async-generator-resume-next-return-processor-fulfilled step 2:
  //  Return ! AsyncGeneratorResolve(_F_.[[Generator]], _value_, *true*).
  CallBuiltin(Builtins::kAsyncGeneratorResolve, context, generator, value,
              TrueConstant());

  TailCallBuiltin(Builtins::kAsyncGeneratorResumeNext, context, generator);
}

TF_BUILTIN(AsyncGeneratorReturnClosedRejectClosure,
           AsyncGeneratorBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const generator =
      LoadContextElement(context, AwaitContext::kGeneratorSlot);

  SetGeneratorNotAwaiting(generator);

  // https://tc39.github.io/proposal-async-iteration/
  //    #async-generator-resume-next-return-processor-rejected step 2:
  // Return ! AsyncGeneratorReject(_F_.[[Generator]], _reason_).
  CallBuiltin(Builtins::kAsyncGeneratorReject, context, generator, value);

  TailCallBuiltin(Builtins::kAsyncGeneratorResumeNext, context, generator);
}

}  // namespace internal
}  // namespace v8
